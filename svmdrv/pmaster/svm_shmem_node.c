/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * Description:
 * Author: huawei
 * Create: 2023-12-15
 */
#include <linux/hashtable.h>
#include <linux/kref.h>
#include <linux/compiler.h>

#include "securec.h"

#include "svm_log.h"
#include "svm_define.h"

#include "svm_dev_res_mng.h"
#include "svm_msg_client.h"
#include "devmm_proc_info.h"
#include "devmm_channel.h"
#include "svm_heap_mng.h"
#include "svm_shmem_procfs.h"
#include "svm_master_advise.h"
#include "svm_shmem_node.h"

#define DEVMM_IPC_NODE_ADDR_SHIFT  21 /* 2M mask */

struct ipc_node_name_attr {
    int tgid;
    u64 name_ref;
    struct svm_id_inst inst;
};

static struct devmm_ipc_node_ops g_ipc_node_ops;
static struct ipc_node_wlist *_devmm_ipc_node_wlist_create_and_add(struct devmm_ipc_node *node, u32 sdid, ka_pid_t pid);

void devmm_ipc_node_ops_register(struct devmm_ipc_node_ops *ops)
{
    g_ipc_node_ops = *ops;
}

static inline int devmm_ipc_get_node_bucket(u64 vptr)
{
    return ((vptr >> DEVMM_IPC_NODE_ADDR_SHIFT) & (DEVMM_MAX_IPC_NODE_LIST_NUM - 1));
}

static void _devmm_ipc_node_wlist_show(struct devmm_ipc_node *node)
{
    struct ipc_node_wlist *wlist = NULL;
    u32 stamp = (u32)ka_jiffies;
    int i = 0;

    devmm_drv_info("Ipc mem node:(name=%s)\n", node->attr.name);
    ka_list_for_each_entry(wlist, &node->wlist_head, list) {
        devmm_drv_info("    [%d]: pid=%d; start_time=%lu; vptr=0x%llx\n",
            i, wlist->pid, wlist->set_time, wlist->vptr);
        i++;
        devmm_try_cond_resched(&stamp);
    }
}

static struct ipc_node_wlist *_devmm_ipc_node_find_wlist(struct devmm_ipc_node *node, ka_pid_t pid, u32 sdid)
{
    struct ipc_node_wlist *wlist = NULL;
    u32 stamp = (u32)ka_jiffies;

    ka_list_for_each_entry(wlist, &node->wlist_head, list) {
        if ((wlist->pid == pid) && (wlist->sdid == sdid)) {
            return wlist;
        }
        devmm_try_cond_resched(&stamp);
    }

    return NULL;
}

static inline void _devmm_ipc_node_wlist_add(struct devmm_ipc_node *node, struct ipc_node_wlist *wlist)
{
    ka_list_add_tail(&wlist->list, &node->wlist_head);
    node->wlist_num++;
}

static inline void _devmm_ipc_node_wlist_del(struct devmm_ipc_node *node, struct ipc_node_wlist *wlist)
{
    ka_list_del(&wlist->list);
    node->wlist_num--;
}

static inline struct ipc_node_wlist *_devmm_ipc_node_wlist_create(void)
{
    return devmm_kvzalloc_ex(sizeof(struct ipc_node_wlist), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
}

static inline void _devmm_ipc_node_wlist_destroy(struct ipc_node_wlist *wlist)
{
    devmm_kvfree_ex(wlist);
}

static void devmm_ipc_node_wlist_destroy(struct devmm_ipc_node *node)
{
    struct ipc_node_wlist *wlist = NULL, *n = NULL;
    u32 stamp = (u32)ka_jiffies;

    ka_list_for_each_entry_safe(wlist, n, &node->wlist_head, list) {
        _devmm_ipc_node_wlist_del(node, wlist);
        _devmm_ipc_node_wlist_destroy(wlist);
        devmm_try_cond_resched(&stamp);
    }
}

static inline int devmm_ipc_node_name_to_attr(const char *name, struct ipc_node_name_attr *name_attr)
{
    int ret = sscanf_s(name, "%08x%016llx%02x%02x", &name_attr->tgid, &name_attr->name_ref,
        &name_attr->inst.devid, &name_attr->inst.vfid);
    return (ret == 4) ?  0 : -EFAULT;  /* 4 for para nums */
}

static inline u32 devmm_ipc_node_get_str_elfhash(const char *name)
{
    struct ipc_node_name_attr name_attr;
    return (devmm_ipc_node_name_to_attr(name, &name_attr) == 0) ? (name_attr.name_ref % DEVMM_HASH_LIST_NUM) : 0;
}

int devmm_ipc_node_get_inst_by_name(const char *name, struct svm_id_inst *inst)
{
    struct ipc_node_name_attr name_attr;
    int ret;

    ret = devmm_ipc_node_name_to_attr(name, &name_attr);
    if (ret != 0) {
        devmm_drv_err("Name to attr fail. (ret=%d; name=%s)\n", ret, name);
        return -EINVAL;
    }
    if ((name_attr.inst.devid >= DEVMM_MAX_DEVICE_NUM) || (name_attr.inst.vfid >= DEVMM_MAX_VF_NUM)) {
        devmm_drv_err("Invalid inst. (name=%s; devid=%u; vfid=%u)\n",
            name, name_attr.inst.devid, name_attr.inst.vfid);
        return -EINVAL;
    }
    *inst = name_attr.inst;
    return 0;
}

static inline void _devmm_ipc_node_destroy(struct devmm_ipc_node *node)
{
    ka_task_mutex_destroy(&node->mutex);
    devmm_ipc_node_wlist_destroy(node);
    devmm_kvfree_ex(node);
}

static inline void _devmm_ipc_node_add(struct devmm_dev_res_mng *res_mng, struct devmm_ipc_node *node)
{
    ka_hash_add(res_mng->ipc_mem_node_info.node_htable, &node->link, node->key);
}

static inline void _devmm_ipc_node_del(struct devmm_ipc_node *node)
{
    ka_hash_del(&node->link);
}

static void devmm_ipc_node_del_by_dev_res_mng(struct devmm_ipc_node *node, struct devmm_dev_res_mng *res_mng)
{
    ka_task_write_lock(&res_mng->ipc_mem_node_info.rwlock);
    _devmm_ipc_node_del(node);
    ka_task_write_unlock(&res_mng->ipc_mem_node_info.rwlock);
}

static void devmm_ipc_node_del(struct devmm_ipc_node *node)
{
    struct devmm_dev_res_mng *res_mng = NULL;

    res_mng = devmm_dev_res_mng_get(&node->attr.inst);
    if (res_mng != NULL) {
        devmm_ipc_node_del_by_dev_res_mng(node, res_mng);
        devmm_dev_res_mng_put(res_mng);
    }
}

static struct devmm_ipc_node *_devmm_ipc_node_find(struct devmm_dev_res_mng *res_mng,
    const char *name, struct svm_id_inst *inst)
{
    u32 key = devmm_ipc_node_get_str_elfhash(name);
    struct devmm_ipc_node *node = NULL;

    ka_hash_for_each_possible(res_mng->ipc_mem_node_info.node_htable, node, link, key) {
        if (ka_base_strcmp(node->attr.name, name) == 0) {
            return node;
        }
    }

    return NULL;
}

static inline struct devmm_ipc_node *devmm_ipc_node_find(struct devmm_dev_res_mng *res_mng,
    const char *name, struct svm_id_inst *inst)
{
    struct devmm_ipc_node *node = NULL;

    ka_task_read_lock(&res_mng->ipc_mem_node_info.rwlock);
    node = _devmm_ipc_node_find(res_mng, name, inst);
    ka_task_read_unlock(&res_mng->ipc_mem_node_info.rwlock);
    return node;
}

static int devmm_ipc_node_add(struct devmm_ipc_node *node)
{
    struct devmm_dev_res_mng *res_mng = NULL;

    res_mng = devmm_dev_res_mng_get(&node->attr.inst);
    if (ka_unlikely(res_mng == NULL)) {
        devmm_drv_err("Get res mng fail. (devid=%u; vfid=%u)\n", node->attr.inst.devid, node->attr.inst.vfid);
        return -EINVAL;
    }

    ka_task_write_lock(&res_mng->ipc_mem_node_info.rwlock);
    if (_devmm_ipc_node_find(res_mng, node->attr.name, &node->attr.inst) != NULL) {
        ka_task_write_unlock(&res_mng->ipc_mem_node_info.rwlock);
        devmm_dev_res_mng_put(res_mng);
        return -EACCES;
    }
    _devmm_ipc_node_add(res_mng, node);
    ka_task_write_unlock(&res_mng->ipc_mem_node_info.rwlock);
    devmm_dev_res_mng_put(res_mng);
    devmm_ipc_procfs_add_node(node);
    return 0;
}

static void devmm_ipc_node_release(ka_kref_t * kref)
{
    struct devmm_ipc_node *node = ka_container_of(kref, struct devmm_ipc_node, ref);

    if (g_ipc_node_ops.destory_node != NULL) {
        g_ipc_node_ops.destory_node(node);
    }

    devmm_drv_debug("Ipc node release succeed. (name=%s)\n", node->attr.name);
    devmm_ipc_procfs_del_node(node);
    devmm_ipc_node_del(node);
    _devmm_ipc_node_destroy(node);
}

static struct devmm_ipc_node *_devmm_ipc_node_get(const char *name)
{
    struct devmm_dev_res_mng *res_mng = NULL;
    struct devmm_ipc_node *node = NULL;
    struct svm_id_inst inst;
    int ret;

    ret = devmm_ipc_node_get_inst_by_name(name, &inst);
    if (ka_unlikely(ret != 0)) {
        devmm_drv_err("Get inst by name fail. (ret=%d; name=%s)\n", ret, name);
        return NULL;
    }

    res_mng = devmm_dev_res_mng_get(&inst);
    if (ka_unlikely(res_mng == NULL)) {
        devmm_drv_err("Get res mng fail. (devid=%u; vfid=%u)\n", inst.devid, inst.vfid);
        return NULL;
    }

    ka_task_read_lock(&res_mng->ipc_mem_node_info.rwlock);
    node = _devmm_ipc_node_find(res_mng, name, &inst);
    if (node != NULL) {
        if (ka_base_kref_get_unless_zero(&node->ref) == 0) {
            ka_task_read_unlock(&res_mng->ipc_mem_node_info.rwlock);
            devmm_dev_res_mng_put(res_mng);
            return NULL;
        }
    }
    ka_task_read_unlock(&res_mng->ipc_mem_node_info.rwlock);
    devmm_dev_res_mng_put(res_mng);

    return node;
}

static void _devmm_ipc_node_put(struct devmm_ipc_node *node)
{
    ka_base_kref_put(&node->ref, devmm_ipc_node_release);
}

static void devmm_ipc_node_free_pages(struct devmm_svm_process *svm_proc, u64 vptr, u64 page_num, u32 page_size)
{
    struct devmm_chan_free_pages free_pgs;
    u32 *bitmap = NULL;
    u64 cnt, i;
    int ret;

    bitmap = devmm_get_page_bitmap(svm_proc, vptr);
    if (bitmap == NULL) {
        devmm_drv_err("Va isn't alloced. (va=0x%llx)\n", vptr);
        return;
    }
    /* not clear bitmap when device fault know this memory alloc by ipc open is destoryed. */
    cnt = page_num / DEVMM_FREE_SECTION_NUM;
    if ((page_num % DEVMM_FREE_SECTION_NUM) > 0) {
        cnt++;
    }

    free_pgs.head.msg_id = DEVMM_CHAN_FREE_PAGES_H2D_ID;
    free_pgs.head.process_id.devid = (u16)devmm_page_bitmap_get_phy_devid(svm_proc, bitmap);
    free_pgs.head.process_id.hostpid = svm_proc->process_id.hostpid;
    free_pgs.head.process_id.vfid = (u16)devmm_page_bitmap_get_vfid(svm_proc, bitmap);
    free_pgs.head.dev_id = free_pgs.head.process_id.devid;
    free_pgs.real_size = DEVMM_FREE_SECTION_NUM * page_size;

    for (i = 0; i < cnt; i++) {
        free_pgs.va = vptr + i * DEVMM_FREE_SECTION_NUM * page_size;

        if ((i == cnt - 1) && ((page_num % DEVMM_FREE_SECTION_NUM) > 0)) {
            free_pgs.real_size = (page_num % DEVMM_FREE_SECTION_NUM) * page_size;
        }

        ret = devmm_chan_msg_send(&free_pgs, sizeof(struct devmm_chan_free_pages), 0);
        if (ret != 0) {
            devmm_drv_warn("Free page error. (dev_id=%d; ret=%d; va=0x%llx; realsize=%llu)\n",
                           free_pgs.head.dev_id, ret, free_pgs.va, free_pgs.real_size);
            return;
        }
    }
}

static void _devmm_ipc_node_free_open_pages(struct devmm_ipc_node *node)
{
    struct ipc_node_wlist *wlist = NULL;
    u32 stamp = (u32)ka_jiffies;
    u32 *bitmap = NULL;

    if (node->is_open_page_freed == true) {
        return;
    }

    ka_list_for_each_entry(wlist, &node->wlist_head, list) {
        struct devmm_svm_process *svm_proc = NULL;

        if (wlist->svm_proc == NULL) {
            continue;
        }

        svm_proc = wlist->svm_proc;
        if (devmm_svm_other_proc_occupy_num_add(svm_proc) != 0) {
            devmm_drv_debug("Process is exiting. (hostpid=%d; va=0x%llx)\n",
                svm_proc->process_id.hostpid, wlist->vptr);
            devmm_try_cond_resched(&stamp);
            continue;
        }
        ka_task_down_read(&svm_proc->bitmap_sem);
        bitmap = devmm_get_page_bitmap(svm_proc, wlist->vptr);
        if (bitmap == NULL) {
            devmm_drv_warn("Get bitmap failed. (hostpid=%d; va=0x%llx)\n",
                svm_proc->process_id.hostpid, wlist->vptr);
            goto next;
        }

        if (!devmm_page_bitmap_is_dev_mapped(bitmap)) {
            devmm_drv_debug("Device not map. (hostpid=%d; va=0x%llx)\n",
                svm_proc->process_id.hostpid, wlist->vptr);
            goto next;
        }
        devmm_ipc_node_free_pages(svm_proc, wlist->vptr,
            devmm_get_pagecount_by_size(wlist->vptr, node->attr.len, (u32)node->attr.page_size),
            (u32)node->attr.page_size);
next:
        ka_task_up_read(&svm_proc->bitmap_sem);
        devmm_svm_other_proc_occupy_num_sub(svm_proc);
        devmm_try_cond_resched(&stamp);
    }

    node->is_open_page_freed = true;
}

static int _devmm_ipc_node_init(struct devmm_ipc_node *node)
{
    if (node->attr.init_fn != NULL) {
        return node->attr.init_fn(&node->attr);
    }
    return 0;
}

static bool devmm_is_local_pod(u32 devid, u32 sdid)
{
#ifndef EMU_ST
    if ((g_ipc_node_ops.is_local_pod == NULL) || (sdid == KA_UINT_MAX)) {
        return true;
    }
    return g_ipc_node_ops.is_local_pod(devid, sdid);
#else
    return true;
#endif
}

static bool devmm_is_in_kthread(void)
{
    return ((ka_task_get_current_flags() & KA_TASK_PF_KTHREAD) != 0);
}

static int _devmm_ipc_node_uninit(struct devmm_ipc_node *node, ka_pid_t pid)
{
    if (node->attr.pid != pid) {
        devmm_drv_err("No permission to destroy. (name=%s; pid=%d. creator_pid=%d)\n",
            node->attr.name, pid, node->attr.pid);
        return -EACCES;
    }

    if ((devmm_is_in_kthread() == false) && (devmm_is_local_pod(node->attr.inst.devid, node->attr.sdid) == false)) {
#ifndef EMU_ST
        devmm_drv_err("No permission to destroy. (name=%s; pid=%d; devid=%u; sdid=%u)\n",
            node->attr.name, pid, node->attr.inst.devid, node->attr.sdid);
        return -EACCES;
#endif
    }

    if (node->valid != true) {
        /*
         * Do not add any log here:
         * If a remote server has set many current server's PID in its whitelist,
         * it will send multiple destroy messages.
         */
        return -EFAULT;
    }

    if (node->attr.uninit_fn != NULL) {
        node->attr.uninit_fn(&node->attr);
    }

    node->valid = false;

    return 0;
}

static struct devmm_ipc_node *_devmm_ipc_create_node(struct devmm_ipc_node_attr *attr)
{
    struct devmm_ipc_node *node = NULL;
    int ret;

    node = devmm_kvzalloc_ex(sizeof(struct devmm_ipc_node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (node == NULL) {
        devmm_drv_err("Ipc node create fail. (size=%ld)\n", sizeof(struct devmm_ipc_node));
        return NULL;
    }

    ret = memcpy_s(&node->attr, sizeof(struct devmm_ipc_node_attr), attr, sizeof(struct devmm_ipc_node_attr));
    if (ret != EOK) {
        devmm_kvfree_ex(node);
        devmm_drv_err("Ipc node attr copy fail. (ret=%d; size=%ld; name=%s\n",
            ret, sizeof(struct devmm_ipc_node_attr), attr->name);
        return NULL;
    }
    node->is_open_page_freed = false;
    node->valid = true;
    node->key = devmm_ipc_node_get_str_elfhash(attr->name);
    node->wlist_num = 0;
    KA_INIT_LIST_HEAD(&node->wlist_head);
    node->need_async_recycle = false;
    node->mem_repair_record = false;
    ka_base_kref_init(&node->ref);
    ka_task_mutex_init(&node->mutex);

    return node;
}

static struct devmm_ipc_node *_devmm_ipc_node_create(struct devmm_ipc_node_attr *attr)
{
    struct devmm_ipc_node *node = NULL;
    int ret;

    node = _devmm_ipc_create_node(attr);
    if (node == NULL) {
        devmm_drv_err("Ipc node create fail. (name=%s; pid=%d)\n", attr->name, attr->pid);
        return NULL;
    }

    ret = _devmm_ipc_node_init(node);
    if (ret != 0) {
        _devmm_ipc_node_destroy(node);
        devmm_drv_err("Ipc node init fail. (ret=%d; name=%s; pid=%d)\n", ret, attr->name, attr->pid);
        return NULL;
    }
    ret = devmm_ipc_node_add(node);
    if (ret != 0) {
#ifndef EMU_ST
        (void)_devmm_ipc_node_uninit(node, attr->pid);
#endif
        _devmm_ipc_node_destroy(node);
        devmm_drv_err("Ipc node add fail. (ret=%d; name=%s; pid=%d)\n", ret, attr->name, attr->pid);
        return NULL;
    }
    return node;
}

void devmm_fill_name_zero(char *name)
{
    u32 len = ka_base_strlen(name);
    u32 i;

    for (i = len; i < DEVMM_IPC_MEM_NAME_SIZE - 1; i++) {
        name[i] = '0';
    }
    name[DEVMM_IPC_MEM_NAME_SIZE - 1] = '\0';
}

int devmm_ipc_node_create(struct devmm_ipc_node_attr *attr)
{
    struct devmm_ipc_node *node = NULL;

    if (g_ipc_node_ops.update_node_attr != NULL) {
        int ret = g_ipc_node_ops.update_node_attr(attr);
        if (ret != 0) {
            devmm_drv_err("Update node attr fail. (ret=%d; name=%s)\n", ret, attr->name);
            return ret;
        }
    }
    devmm_fill_name_zero(attr->name);

    node = _devmm_ipc_node_create(attr);
    if (ka_unlikely(node == NULL)) {
        devmm_drv_err("Create ipc node fail. (name=%s)\n", attr->name);
        return -ENOMEM;
    }

    devmm_drv_debug("Ipc node create succeed. (name=%s; sdid=%u; devid=%u; vfid=%u; pid=%d; vptr=0x%llx; len=%ld)\n",
        attr->name, attr->sdid, attr->inst.devid, attr->inst.vfid, attr->pid, attr->vptr, attr->len);
    return 0;
}

int devmm_ipc_node_destroy(const char *name, ka_pid_t pid, bool async_recycle)
{
    struct devmm_ipc_node *node = NULL;
    int ret;

    node = _devmm_ipc_node_get(name);
    if (node == NULL) {
        return 0;
    }
    ka_task_mutex_lock(&node->mutex);
    ret = _devmm_ipc_node_uninit(node, pid);
    if (ret == 0) {
        if (async_recycle) {
            node->need_async_recycle = async_recycle;
        }
        _devmm_ipc_node_free_open_pages(node);
        _devmm_ipc_node_put(node);
    }
    ka_task_mutex_unlock(&node->mutex);
    _devmm_ipc_node_put(node);

    if (ret == 0) {
        devmm_drv_debug("Ipc node destroy succeed. (name=%s; pid=%d)\n", name, pid);
    }
    return ret;
}

int devmm_ipc_set_mem_map_attr(const char *name, u64 attr)
{
    struct devmm_ipc_node *node = NULL;

    if ((attr != MEM_MAP_INBUS) && (attr != MEM_MAP_EXBUS)) {
        devmm_drv_err("Invalid attr. (name=%s; attr=%llu)\n", name, attr);
        return -EINVAL;
    }

    node = _devmm_ipc_node_get(name);
    if (node == NULL) {
        return -EINVAL;
    }

#ifndef EMU_ST
    if (ka_task_get_current_tgid() != node->attr.pid) {
        _devmm_ipc_node_put(node);
        devmm_drv_err("Not creator not allow to set attr. (name=%s; attr=%llu)\n", name, attr);
        return -EPERM;
    }
#endif

    node->attr.mem_map_route = ((attr & MEM_MAP_EXBUS) == MEM_MAP_EXBUS) ? MEM_MAP_EXBUS : MEM_MAP_INBUS;
    _devmm_ipc_node_put(node);

    devmm_drv_debug("Ipc node mem map attribute set. (name=%s; mem_map_route=%llu)\n", name, attr & MEM_MAP_EXBUS);
    return 0;
}

int devmm_ipc_get_mem_map_attr(const char *name, u64 *attr)
{
    struct devmm_ipc_node *node = NULL;

    node = _devmm_ipc_node_get(name);
    if (node == NULL) {
        return -EINVAL;
    }
    *attr = node->attr.mem_map_route;
    _devmm_ipc_node_put(node);

    devmm_drv_debug("Ipc node mem map attribute get. (name=%s; attr=%llu)\n", name, *attr);
    return 0;
}

int devmm_ipc_set_no_wlist_in_server_attr(const char *name, u64 attr)
{
    struct devmm_ipc_node *node = NULL;

    if ((attr != SHMEM_NO_WLIST_ENABLE) && (attr != SHMEM_WLIST_ENABLE)) {
        devmm_drv_err("Invalid attr. (name=%s; attr=%llu)\n", name, attr);
        return -EINVAL;
    }

    node = _devmm_ipc_node_get(name);
    if (node == NULL) {
        return -EINVAL;
    }

    if (node->attr.pid != devmm_get_current_pid()) {
        _devmm_ipc_node_put(node);
        devmm_drv_err("No creator not allow to set attr. (name=%s; attr=%llu)\n", name, attr);
        return -EPERM;
    }

    if (attr == SHMEM_WLIST_ENABLE) {
        _devmm_ipc_node_put(node);
        devmm_drv_run_info("Not support wlist enable now. (name=%s; attr=%llu)\n", name, attr);
        return -EOPNOTSUPP;
    }

    ka_task_mutex_lock(&node->mutex);
    /* Not allow to set attr if had called halShmemSetPid or halShmemSetPodPid */
    if ((attr == SHMEM_NO_WLIST_ENABLE) && ((node->attr.need_set_wlist) && (node->wlist_num != 0))) {
        devmm_drv_err("Had set pid not allow to set attr. (name=%s; attr=%llu; wlist_num=%u)\n",
            name, attr, node->wlist_num);
        ka_task_mutex_unlock(&node->mutex);
        _devmm_ipc_node_put(node);
        return -EPERM;
    }

    node->attr.need_set_wlist = (attr == SHMEM_NO_WLIST_ENABLE) ? false : true;
    devmm_drv_run_info("No wlist in server attr set succ. (name=%s; attr=%llu; need_set_wlist=%u)\n",
        name, attr, node->attr.need_set_wlist);

    ka_task_mutex_unlock(&node->mutex);
    _devmm_ipc_node_put(node);
    return 0;
}

int devmm_ipc_get_no_wlist_in_server_attr(const char *name, u64 *attr)
{
    struct devmm_ipc_node *node = NULL;

    node = _devmm_ipc_node_get(name);
    if (node == NULL) {
        return -EINVAL;
    }
    *attr = node->attr.need_set_wlist ? SHMEM_WLIST_ENABLE : SHMEM_NO_WLIST_ENABLE;
    _devmm_ipc_node_put(node);

    devmm_drv_debug("No wlist in server attr get succ. (name=%s; attr=%llu)\n", name, *attr);
    return 0;
}


static void devmm_ipc_node_free_open_pages(const char *name)
{
    struct devmm_ipc_node *node = NULL;

    node = _devmm_ipc_node_get(name);
    if (node != NULL) {
        ka_task_mutex_lock(&node->mutex);
        _devmm_ipc_node_free_open_pages(node);
        ka_task_mutex_unlock(&node->mutex);
        _devmm_ipc_node_put(node);
    }
}

static int devmm_ipc_node_heap_check(struct devmm_svm_process *svm_proc,
    u64 va, u64 page_num, u64 size, bool is_huge)
{
    struct devmm_svm_heap *heap = NULL;
    bool heap_is_huge = false; /* Local heap type is huge */
    int ret;

    heap = devmm_svm_get_heap(svm_proc, va);
    if (ka_unlikely(heap == NULL)) {
        devmm_drv_err("Get heap fail. (va=0x%llx)\n", va);
        return -ENOMEM;
    }

    heap_is_huge = (heap->heap_type == DEVMM_HEAP_HUGE_PAGE);
    if (ka_unlikely(heap_is_huge != is_huge)) {
        devmm_drv_err("Check heap type fail. (heap_is_huge=%d; is_huge=%d)\n", heap_is_huge, is_huge);
        return -EINVAL;
    }

    ret = devmm_check_va_add_size_by_heap(heap, va, size);
    if (ka_unlikely(ret != 0)) {
        devmm_drv_err("Check va addr fail. (va=0x%llx; size=%llu)\n", va, size);
        return ret;
    }

    return 0;
}

static void devmm_ipc_node_clear_ipc_open_lock_flag(struct devmm_svm_process *svm_proc,
    u64 va, u64 page_num, u64 size, bool is_huge)
{
    u32 *bitmap = devmm_get_page_bitmap(svm_proc, va);
    int ret;
    u64 i;

    if (ka_unlikely(bitmap == NULL)) {
        return;
    }

    ret = devmm_ipc_node_heap_check(svm_proc, va, page_num, size, is_huge);
    if (ka_unlikely(ret != 0)) {
        devmm_drv_err("Heap check fail. (va=0x%llx; page_num=%llu; size=%llu; is_huge=%d)\n",
            va, page_num, size, is_huge);
        return;
    }

    for (i = 0; i < page_num; i++) {
        devmm_page_bitmap_clear_flag(bitmap + i, DEVMM_PAGE_IPC_MEM_OPEN_MASK);
        devmm_page_bitmap_clear_flag(bitmap + i, DEVMM_PAGE_LOCKED_DEVICE_MASK);
    }
}

static void devmm_ipc_node_set_ipc_open_lock_flag(struct devmm_svm_process *svm_proc,
    u64 va, u64 page_num, u64 size, bool is_huge)
{
    u32 *bitmap = devmm_get_page_bitmap(svm_proc, va);
    int ret;
    u64 i;

    if (ka_unlikely(bitmap == NULL)) {
        return;
    }

    ret = devmm_ipc_node_heap_check(svm_proc, va, page_num, size, is_huge);
    if (ka_unlikely(ret != 0)) {
        devmm_drv_err("Heap check fail. (va=0x%llx; page_num=%llu; size=%llu; is_huge=%d)\n",
            va, page_num, size, is_huge);
        return;
    }

    for (i = 0; i < page_num; i++) {
        devmm_page_bitmap_set_flag(bitmap + i, DEVMM_PAGE_IPC_MEM_OPEN_MASK);
        devmm_page_bitmap_set_flag(bitmap + i, DEVMM_PAGE_LOCKED_DEVICE_MASK);
    }
}

static int _devmm_ipc_open_check(struct devmm_ipc_node *node, struct devmm_svm_process *svm_proc, u64 vptr)
{
    struct devmm_memory_attributes attr;
    int ret;

    if (ka_unlikely((node->valid != true) || (node->is_open_page_freed == true))) {
        devmm_drv_err("Ipc node open is not allowed. (name=%s; valid=%d; is_open_page_freed=%d)\n",
            node->attr.name, node->valid, node->is_open_page_freed);
        return -EINVAL;
    }

    ret = devmm_get_memory_attributes(svm_proc, vptr, &attr);
    if (ka_unlikely(ret != 0)) {
        devmm_drv_err("Get_memory_attributes failed. (ret=%d; vptr=0x%llx)\n", ret, vptr);
        return ret;
    }

    if (ka_unlikely(attr.is_ipc_open)) {
        devmm_drv_err("Vptr is ipc memory. (vptr=0x%llx; devid=%d)\n", vptr, attr.devid);
        return -EADDRINUSE;
    }

    if (ka_unlikely(devmm_is_host_agent(attr.devid))) {
        devmm_drv_err("Vptr is host agent, not surport ipc. (vptr=0x%llx; devid=%d)\n", vptr, attr.devid);
        return -EFAULT;
    }

    ret = devmm_check_status_va_info(svm_proc, vptr, (u64)node->attr.len);
    if (ka_unlikely(ret != 0)) {
        devmm_drv_err("Destination address vaddress check error. (ret=%d; vptr=0x%llx; len=%lu)\n",
            ret, vptr, node->attr.len);
        return ret;
    }
    return 0;
}

static struct ipc_node_wlist *_devmm_ipc_node_get_wlist(struct devmm_ipc_node *node, ka_pid_t pid)
{
    struct ipc_node_wlist *wlist = NULL;
    unsigned long start_time = 0;

    wlist = _devmm_ipc_node_find_wlist(node, pid, KA_UINT_MAX);
    if (ka_unlikely(wlist == NULL)) {
        if (node->attr.need_set_wlist) {
            devmm_drv_err("Find wlist fail. (name=%s; pid=%d; creator_pid=%d)\n", node->attr.name, pid, node->attr.pid);
            _devmm_ipc_node_wlist_show(node);
            return NULL;
        } else {
            if (ka_unlikely((node->wlist_num + 1) > IPC_WLIST_NUM)) {
                devmm_drv_err("Too many wlist. (name=%s; wlist_num=%u)\n", node->attr.name, node->wlist_num);
                return NULL;
            }
            wlist = _devmm_ipc_node_wlist_create_and_add(node, KA_UINT_MAX, pid);
            if (wlist == NULL) {
                devmm_drv_err("Prepare wlist fail. (name=%s; pid=%d; creator_pid=%d)\n", node->attr.name, pid, node->attr.pid);
                return NULL;
            }
        }
    }

#ifndef DEVMM_UT
    start_time = ka_task_get_current_group_starttime();
    if (ka_system_time_after(start_time, wlist->set_time)) {
        devmm_drv_err("Invalid task start time. (name=%s; start_time=%lu; wlist_time=%lu)\n",
            node->attr.name, start_time, wlist->set_time);
        return NULL;
    }
#endif

    if (ka_unlikely(wlist->vptr != 0)) {
        devmm_drv_err("Wlist vptr already set. (name=%s; wlist_vptr=0x%llx)\n", node->attr.name, wlist->vptr);
        return NULL;
    }

    return wlist;
}

static int _devmm_ipc_node_open(struct devmm_ipc_node *node, struct devmm_svm_process *svm_proc, u64 vptr)
{
    struct ipc_node_wlist *wlist = NULL;
    u32 page_size;
    u64 page_num;
    int ret;

    ret = _devmm_ipc_open_check(node, svm_proc, vptr);
    if (ka_unlikely(ret != 0)) {
        devmm_drv_err("Ipc open check fail. (ret=%d; name=%s; vptr=0x%llx)\n",
            ret, node->attr.name, vptr);
        return ret;
    }

    wlist = _devmm_ipc_node_get_wlist(node, svm_proc->process_id.hostpid);
    if (ka_unlikely(wlist == NULL)) {
        devmm_drv_err("Wlist verify fail. (pid=%d)\n", svm_proc->process_id.hostpid);
        return -EINVAL;
    }

    wlist->svm_proc = svm_proc;
    wlist->vptr = vptr;
    page_size = (node->attr.is_huge != 0) ? devmm_svm->device_hpage_size : devmm_svm->svm_page_size;
    page_num = devmm_get_pagecount_by_size(vptr, node->attr.len, page_size);
    devmm_ipc_node_set_ipc_open_lock_flag(svm_proc, vptr, page_num, node->attr.len, node->attr.is_huge);

    return 0;
}

int devmm_ipc_node_open(const char *name, struct devmm_svm_process *svm_proc, u64 vptr)
{
    struct devmm_ipc_node *node = NULL;
    int ret;

    node = _devmm_ipc_node_get(name);
    if (ka_unlikely(node == NULL)) {
        devmm_drv_err("Ipc node get fail. (name=%s; vptr=0x%llx)\n", name, vptr);
        return -EINVAL;
    }

    ka_task_mutex_lock(&node->mutex);
    ret = _devmm_ipc_node_open(node, svm_proc, vptr);
    ka_task_mutex_unlock(&node->mutex);
    if (ka_unlikely(ret != 0)) {
        _devmm_ipc_node_put(node);
        return ret;
    }

    devmm_drv_debug("Ipc node open succeed. (name=%s; vptr=0x%llx)\n", name, vptr);
    return 0;
}

static int _devmm_ipc_node_close(struct devmm_ipc_node *node, struct devmm_svm_process *svm_proc, u64 vptr)
{
    struct ipc_node_wlist *wlist = NULL;
    u32 page_size;

    wlist = _devmm_ipc_node_find_wlist(node, svm_proc->process_id.hostpid, KA_UINT_MAX);
    if (ka_unlikely(wlist == NULL)) {
        devmm_drv_err("Wlist check fail. (name=%s; vptr=0x%llx)\n", node->attr.name, vptr);
        _devmm_ipc_node_wlist_show(node);
        return -EPERM;
    }

    if (ka_unlikely(wlist->vptr != vptr)) {
        devmm_drv_err("Invalid vptr. (wlist_vptr=0x%llx; vptr=0x%llx\n)\n", wlist->vptr, vptr);
        return -EFAULT;
    }
    wlist->vptr = 0;
    wlist->svm_proc = NULL;
    page_size = (node->attr.is_huge != 0) ? devmm_svm->device_hpage_size : devmm_svm->svm_page_size;
    devmm_ipc_node_clear_ipc_open_lock_flag(svm_proc, vptr, node->attr.len / page_size,
        node->attr.len, node->attr.is_huge);

    return 0;
}

int devmm_ipc_node_close(const char *name, struct devmm_svm_process *svm_proc, u64 vptr)
{
    struct devmm_ipc_node *node = NULL;
    int ret;

    node = _devmm_ipc_node_get(name);
    if (ka_unlikely(node == NULL)) {
        devmm_drv_err("Ipc node get fail. (name=%s; vptr=0x%llx)\n", name, vptr);
        return -EINVAL;
    }
    ka_task_mutex_lock(&node->mutex);
    ret = _devmm_ipc_node_close(node, svm_proc, vptr);
    ka_task_mutex_unlock(&node->mutex);
    if (ka_unlikely(ret == 0)) {
        _devmm_ipc_node_put(node);
    }
    _devmm_ipc_node_put(node);

    if (ka_likely(ret == 0)) {
        devmm_drv_debug("Ipc node close succeed. (name=%s; vptr=0x%llx)\n", name, vptr);
    }

    return ret;
}

int devmm_ipc_query_node_attr(const char *name, struct devmm_ipc_node_attr *attr)
{
    struct devmm_ipc_node *node = NULL;
    int ret;

    node = _devmm_ipc_node_get(name);
    if (ka_unlikely(node == NULL)) {
        return -EINVAL;
    }
    ka_task_mutex_lock(&node->mutex);
    ret = memcpy_s(attr, sizeof(struct devmm_ipc_node_attr), &node->attr, sizeof(struct devmm_ipc_node_attr));
    ka_task_mutex_unlock(&node->mutex);
    _devmm_ipc_node_put(node);
    ret = (ret != EOK) ? -ENOMEM : 0;

    if (ka_likely(ret == 0)) {
        devmm_drv_debug("Ipc node query attr succeed. (name=%s)\n", name);
    }

    return ret;
}

static void _devmm_ipc_node_wlist_init(struct ipc_node_wlist *wlist, u32 sdid, ka_pid_t pid)
{
    wlist->sdid = sdid;
    wlist->pid = pid;
    wlist->set_time = ka_system_ktime_get_ns();
}

static struct ipc_node_wlist *_devmm_ipc_node_wlist_create_and_add(struct devmm_ipc_node *node, u32 sdid, ka_pid_t pid)
{
    struct ipc_node_wlist *wlist = NULL;

    wlist = _devmm_ipc_node_wlist_create();
    if (wlist == NULL) {
        devmm_drv_err("Ipc node wlist create fail. (name=%s; sdid=%u; pid=%d; size=%ld)\n",
            node->attr.name, sdid, pid, sizeof(struct ipc_node_wlist));
        return NULL;
    }
    _devmm_ipc_node_wlist_add(node, wlist);
    _devmm_ipc_node_wlist_init(wlist, sdid, pid);

    return wlist;
}

static int _devmm_ipc_node_set_pid(struct devmm_ipc_node *node, u32 sdid, ka_pid_t pid)
{
    struct ipc_node_wlist *wlist = NULL;

    wlist = _devmm_ipc_node_find_wlist(node, pid, sdid);
    if (wlist != NULL) {
        _devmm_ipc_node_wlist_init(wlist, sdid, pid);
        return 0;
    }
    wlist = _devmm_ipc_node_wlist_create_and_add(node, sdid, pid);
    if (wlist == NULL) {
        devmm_drv_err("Ipc node wlist create fail. (name=%s; sdid=%u; pid=%d; size=%ld)\n",
            node->attr.name, sdid, pid, sizeof(struct ipc_node_wlist));
        return -ENOMEM;
    }

    return 0;
}

static int devmm_ipc_node_set_pids_check(struct devmm_ipc_node *node, u32 devid, u32 sdid,
    int creator_pid, u32 pid_num)
{
    if (ka_unlikely(node->valid != true)) {
        devmm_drv_err("Ipc node is destroyed. (name=%s)\n", node->attr.name);
        return -EFAULT;
    }

    if (ka_unlikely(node->attr.pid != creator_pid)) {
        /* Only creator have permission to set wlist */
        devmm_drv_err("Node pid check fail. (name=%s; attr.pid=%d; creator_pid=%d)\n",
            node->attr.name, node->attr.pid, creator_pid);
        return -EINVAL;
    }

    if ((devmm_is_in_kthread() == false) && (devmm_is_local_pod(devid, node->attr.sdid) == false)) {
#ifndef EMU_ST
        devmm_drv_err("No permission to set wlist. (name=%s; pid=%d; devid=%u; sdid=%u)\n",
            node->attr.name, creator_pid, devid, node->attr.sdid);
        return -EACCES;
#endif
    }

    if (ka_unlikely((pid_num == 0) || (pid_num > IPC_WLIST_SET_NUM))) {
        devmm_drv_err("Invalid pid_num. (name=%s; attr.pid=%d; creator_pid=%d; pid_num=%u)\n",
            node->attr.name, node->attr.pid, creator_pid, pid_num);
        return -EINVAL;
    }

    if (ka_unlikely((node->wlist_num + pid_num) > IPC_WLIST_NUM)) {
        devmm_drv_err("Too many wlist. (name=%s; wlist_num=%u; pid_num=%u)\n",
            node->attr.name, node->wlist_num, pid_num);
        return -EINVAL;
    }
    return 0;
}

static int _devmm_ipc_node_set_pids(struct devmm_ipc_node *node, u32 devid, u32 sdid,
    ka_pid_t pid[], u32 pid_num)
{
    bool is_local_pod = false;
    u32 stamp = (u32)ka_jiffies;
    u32 set_pid_num = 0;
    u32 sdid_to_set;
    int i;

    is_local_pod = devmm_is_local_pod(devid, sdid);
    sdid_to_set = devmm_is_in_kthread() ? KA_UINT_MAX : sdid;
    for (i = 0; i < pid_num; i++) {
        int ret;

        if ((pid[i] == 0) || ((pid[i] == node->attr.pid) && is_local_pod)) {
            devmm_drv_warn("Invalid pid will not be set to wlist. (pid=%d)\n", pid[i]);
            continue;
        }
        ret = _devmm_ipc_node_set_pid(node, sdid_to_set, pid[i]);
        if (ret != 0) {
            devmm_drv_err("Set pid fail. (name=%s; i=%d; pid=%d; pid_num=%d)\n", node->attr.name, i, pid[i], pid_num);
            return ret;
        }
        set_pid_num++;
        devmm_try_cond_resched(&stamp);
    }

    if (ka_unlikely(set_pid_num == 0)) {
        return -EINVAL;
    }

    return 0;
}

int devmm_ipc_node_set_pids(struct devmm_ipc_setpid_attr *attr)
{
    struct devmm_ipc_node *node = NULL;
    int ret;

    /* IpcCreate takes one, DEVMM_SVM_MAX_PROCESS_NUM - 1 left */
    if (ka_unlikely(attr->pid_num > IPC_WLIST_SET_NUM)) {
        devmm_drv_err("Invalid pid num. (pid_num=%u; max_wlist_num=%u)\n", attr->pid_num, IPC_WLIST_SET_NUM);
        return -EINVAL;
    }
    node = _devmm_ipc_node_get(attr->name);
    if (ka_unlikely(node == NULL)) {
        devmm_drv_err("Ipc node get fail. (name=%s)\n", attr->name);
        return -EINVAL;
    }
    ka_task_mutex_lock(&node->mutex);
    if (node->attr.need_set_wlist == false) {
        ka_task_mutex_unlock(&node->mutex);
        devmm_drv_err("Had set no need wlist, operation not permitted. (name=%s)\n", attr->name);
        _devmm_ipc_node_put(node);
        return -EPERM;
    }
    ret = devmm_ipc_node_set_pids_check(node, node->attr.inst.devid, attr->sdid, attr->creator_pid, attr->pid_num);
    if (ret == 0) {
        ret = _devmm_ipc_node_set_pids(node, node->attr.inst.devid, attr->sdid, attr->pid, attr->pid_num);
    }
    ka_task_mutex_unlock(&node->mutex);
    if ((ret == 0) && (attr->send != NULL)) {
        /* S2S msg should be sent before put ipc mem node */
        ret = attr->send(attr->name, &attr->inst, attr->sdid, attr->pid, attr->pid_num);
    }
    _devmm_ipc_node_put(node);

    if (ka_likely(ret == 0)) {
        devmm_drv_debug("Ipc node set pid succeed. (name=%s; sdid=%u; creator_pid=%d)\n",
            attr->name, attr->sdid, attr->creator_pid);
    }

    return ret;
}

static struct devmm_ipc_node *devmm_ipc_node_get(struct devmm_dev_res_mng *res_mng, struct devmm_ipc_node_attr *attr)
{
    struct devmm_ipc_node *node = NULL;

    /* The whole process is locked by g_htable[inst_id].mutex */
    node = devmm_ipc_node_find(res_mng, attr->name, &attr->inst);
    if (node == NULL) {
        node = _devmm_ipc_node_create(attr);
    }
    if (node != NULL) {
        if (ka_base_kref_get_unless_zero(&node->ref) == 0) {
            return NULL;
        }
    }
    return node;
}

int devmm_ipc_node_set_pids_ex(struct devmm_ipc_node_attr *attr, u32 sdid,
    int creator_pid, ka_pid_t pid[], u32 pid_num)
{
    struct devmm_dev_res_mng *res_mng = NULL;
    struct devmm_ipc_node *node = NULL;
    int ret = -ENODEV;

    res_mng = devmm_dev_res_mng_get(&attr->inst);
    if (ka_unlikely(res_mng == NULL)) {
        devmm_drv_err("Get res mng fail. (devid=%u; vfid=%u)\n", attr->inst.devid, attr->inst.vfid);
        return -EINVAL;
    }

    ka_task_mutex_lock(&res_mng->ipc_mem_node_info.mutex);
    node = devmm_ipc_node_get(res_mng, attr);
    if (ka_unlikely(node != NULL)) {
        ka_task_mutex_lock(&node->mutex);
        ret = devmm_ipc_node_set_pids_check(node, attr->inst.devid, node->attr.sdid, creator_pid, pid_num);
        if (ret == 0) {
            ret = _devmm_ipc_node_set_pids(node, attr->inst.devid, node->attr.sdid, pid, pid_num);
        }
        ka_task_mutex_unlock(&node->mutex);
        _devmm_ipc_node_put(node);
    }
    ka_task_mutex_unlock(&res_mng->ipc_mem_node_info.mutex);
    devmm_dev_res_mng_put(res_mng);

    if (ka_likely(ret == 0)) {
        devmm_drv_debug("Ipc node set pid succeed. (ret=%d; name=%s; devid=%u; sdid=%u; pid=%d)\n",
            ret, attr->name, attr->inst.devid, sdid, attr->pid);
    }

    return ret;
}

static ka_list_head_t *_devmm_ipc_get_proc_node_head(struct devmm_svm_process *svm_proc, u64 vptr, int op)
{
    int bkt = devmm_ipc_get_node_bucket(vptr);
    return (op == 0) ? &svm_proc->ipc_node.create_head[bkt] : &svm_proc->ipc_node.open_head[bkt];
}

static struct devmm_ipc_proc_node *_devmm_ipc_proc_node_find(struct devmm_svm_process *svm_proc,
    u64 vptr, int op)
{
    ka_list_head_t *head = _devmm_ipc_get_proc_node_head(svm_proc, vptr, op);
    struct devmm_ipc_proc_node *proc_node = NULL;

    ka_list_for_each_entry(proc_node, head, list) {
        if ((proc_node->pid == svm_proc->process_id.hostpid) && (proc_node->vptr == vptr)) {
            return proc_node;
        }
    }
    return NULL;
}

#ifndef DEVMM_UT
#define IPC_MEM_MAX_NUM 480000
#else
#define IPC_MEM_MAX_NUM 8 /* for ut to cover edges */
#endif

static struct devmm_ipc_proc_node *_devmm_ipc_proc_node_create(struct devmm_svm_process *svm_proc,
    const char *name, u64 vptr, size_t len, u32 devid)
{
    struct devmm_ipc_proc_node *proc_node = devmm_kvmalloc_ex(sizeof(struct devmm_ipc_proc_node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (ka_likely(proc_node != NULL)) {
        int i;
        for (i = 0; i < DEVMM_IPC_MEM_NAME_SIZE; i++) {
            proc_node->name[i] = name[i];
        }
        proc_node->pid = svm_proc->process_id.hostpid;
        proc_node->vptr = vptr;
        proc_node->len = len;
        proc_node->devid = devid;
    }
    return proc_node;
}

static inline void _devmm_ipc_proc_node_destroy(struct devmm_ipc_proc_node *proc_node)
{
    if (proc_node != NULL) {
        devmm_kvfree_ex(proc_node);
    }
}

static void _devmm_ipc_proc_node_add(struct devmm_svm_process *svm_proc,
    struct devmm_ipc_proc_node *proc_node, int op)
{
    ka_list_head_t *head = NULL;
    head = _devmm_ipc_get_proc_node_head(svm_proc, proc_node->vptr, op);
    ka_list_add_tail(&proc_node->list, head);
    svm_proc->ipc_node.node_cnt++;
}

static inline void _devmm_ipc_proc_node_del(struct devmm_svm_process *svm_proc, struct devmm_ipc_proc_node *proc_node)
{
    ka_list_del(&proc_node->list);
    svm_proc->ipc_node.node_cnt--;
}

int devmm_ipc_proc_node_add(struct devmm_svm_process *svm_proc, struct devmm_ipc_node_attr *attr, int op)
{
    struct devmm_ipc_proc_node *proc_node = NULL;

    ka_task_mutex_lock(&svm_proc->ipc_node.node_mutex);
    if (ka_unlikely(svm_proc->ipc_node.node_cnt >= IPC_MEM_MAX_NUM)) {
        ka_task_mutex_unlock(&svm_proc->ipc_node.node_mutex);
        devmm_drv_err("Ipc node cnt invalid. (node_cnt=%u)\n", svm_proc->ipc_node.node_cnt);
        return -EFAULT;
    }
    proc_node = _devmm_ipc_proc_node_create(svm_proc, attr->name, attr->vptr, attr->len, attr->inst.devid);
    if (ka_unlikely(proc_node == NULL)) {
        ka_task_mutex_unlock(&svm_proc->ipc_node.node_mutex);
        devmm_drv_err("Ipc node create fail. (name=%s)\n", attr->name);
        return -ENOMEM;
    }
    _devmm_ipc_proc_node_add(svm_proc, proc_node, op);
    ka_task_mutex_unlock(&svm_proc->ipc_node.node_mutex);
    return 0;
}

void devmm_ipc_proc_node_del(struct devmm_svm_process *svm_proc, u64 vptr, int op)
{
    struct devmm_ipc_proc_node *proc_node = NULL;

    ka_task_mutex_lock(&svm_proc->ipc_node.node_mutex);
    proc_node = _devmm_ipc_proc_node_find(svm_proc, vptr, op);
    if (proc_node != NULL) {
        _devmm_ipc_proc_node_del(svm_proc, proc_node);
    }
    ka_task_mutex_unlock(&svm_proc->ipc_node.node_mutex);
    _devmm_ipc_proc_node_destroy(proc_node);
}

static bool devmm_ipc_proc_node_is_within_addr_range(struct devmm_ipc_proc_node *proc_node, u64 vptr, u64 size)
{
    return (proc_node->vptr >= vptr) && ((proc_node->vptr + proc_node->len) <= (vptr + size));
}

void devmm_ipc_proc_node_free_open_pages(struct devmm_svm_process *svm_proc, u64 vptr, u64 page_num, u32 page_size)
{
    struct devmm_ipc_proc_node *proc_node = NULL, *n = NULL;
    u32 stamp = (u32)ka_jiffies;
    int bkt;

    ka_task_mutex_lock(&svm_proc->ipc_node.node_mutex);
    for (bkt = 0; bkt < DEVMM_MAX_IPC_NODE_LIST_NUM; bkt++) {
        ka_list_for_each_entry_safe(proc_node, n, &svm_proc->ipc_node.create_head[bkt], list) {
            if ((proc_node->pid == svm_proc->process_id.hostpid) && devmm_ipc_proc_node_is_within_addr_range(proc_node, vptr, page_num * (u64)page_size)) {
                devmm_ipc_node_free_open_pages(proc_node->name); /* One va may correspond to many names. */
            }
            devmm_try_cond_resched(&stamp);
        }
        devmm_try_cond_resched(&stamp);
    }
    ka_task_mutex_unlock(&svm_proc->ipc_node.node_mutex);
}

static void devmm_ipc_proc_node_opend_recycle(struct devmm_svm_process *svm_proc, u32 devid)
{
    struct devmm_ipc_proc_node *proc_node = NULL, *n = NULL;
    u32 stamp = (u32)ka_jiffies;
    int bkt;

    ka_task_mutex_lock(&svm_proc->ipc_node.node_mutex);
    for (bkt = 0; bkt < DEVMM_MAX_IPC_NODE_LIST_NUM; bkt++) {
        ka_list_for_each_entry_safe(proc_node, n, &svm_proc->ipc_node.open_head[bkt], list) {
            if ((devid == SVM_MAX_AGENT_NUM) || (proc_node->devid == devid)) {
                (void)devmm_ipc_node_close(proc_node->name, svm_proc, proc_node->vptr);
                _devmm_ipc_proc_node_del(svm_proc, proc_node);
                _devmm_ipc_proc_node_destroy(proc_node);
#ifndef EMU_ST
                devmm_try_cond_resched(&stamp);
#endif
            }
        }
        devmm_try_cond_resched(&stamp);
    }
    ka_task_mutex_unlock(&svm_proc->ipc_node.node_mutex);
}

static void devmm_ipc_proc_node_create_recycle(struct devmm_svm_process *svm_proc, u32 devid)
{
#ifndef EMU_ST
    bool async_recycle = (devid == SVM_MAX_AGENT_NUM) ? true : false;
#else
    bool async_recycle = true;
#endif
    struct devmm_ipc_proc_node *proc_node = NULL, *n = NULL;
    u32 stamp = (u32)ka_jiffies;
    int bkt;

    ka_task_mutex_lock(&svm_proc->ipc_node.node_mutex);
    for (bkt = 0; bkt < DEVMM_MAX_IPC_NODE_LIST_NUM; bkt++) {
        ka_list_for_each_entry_safe(proc_node, n, &svm_proc->ipc_node.create_head[bkt], list) {
            if ((devid == SVM_MAX_AGENT_NUM) || (proc_node->devid == devid)) {
                (void)devmm_ipc_node_destroy(proc_node->name, proc_node->pid, async_recycle);
                _devmm_ipc_proc_node_del(svm_proc, proc_node);
                _devmm_ipc_proc_node_destroy(proc_node);
#ifndef EMU_ST
                devmm_try_cond_resched(&stamp);
#endif
            }
        }
        devmm_try_cond_resched(&stamp);
    }
    if (g_ipc_node_ops.clear_fault_sdid != NULL) {
        g_ipc_node_ops.clear_fault_sdid(svm_proc->process_id.hostpid, async_recycle);
    }

    ka_task_mutex_unlock(&svm_proc->ipc_node.node_mutex);
}

void devmm_ipc_proc_node_recycle(struct devmm_svm_process *svm_proc, u32 devid)
{
    devmm_ipc_proc_node_opend_recycle(svm_proc, devid);
    devmm_ipc_proc_node_create_recycle(svm_proc, devid);
}

int devmm_ipc_proc_query_attr_by_va(struct devmm_svm_process *svm_proc, u64 vptr, int op,
    struct devmm_ipc_node_attr *attr)
{
    struct devmm_ipc_proc_node *proc_node = NULL;
    int ret;

    ka_task_mutex_lock(&svm_proc->ipc_node.node_mutex);
    proc_node = _devmm_ipc_proc_node_find(svm_proc, vptr, op);
    if (ka_unlikely(proc_node == NULL)) {
        ka_task_mutex_unlock(&svm_proc->ipc_node.node_mutex);
        devmm_drv_err("Proc node find fail. (vptr=0x%llx; op=%d)\n", vptr, op);
        return -ENODEV;
    }
    ret = devmm_ipc_query_node_attr(proc_node->name, attr);
    ka_task_mutex_unlock(&svm_proc->ipc_node.node_mutex);

    return ret;
}

int devmm_ipc_proc_query_name_by_va(struct devmm_svm_process *svm_proc, u64 vptr, int op, char *name)
{
    struct devmm_ipc_proc_node *proc_node = NULL;
    int ret;

    ka_task_mutex_lock(&svm_proc->ipc_node.node_mutex);
    proc_node = _devmm_ipc_proc_node_find(svm_proc, vptr, op);
    if (ka_unlikely(proc_node == NULL)) {
        ka_task_mutex_unlock(&svm_proc->ipc_node.node_mutex);
        devmm_drv_err("Proc node find fail. (vptr=0x%llx)\n", vptr);
#ifndef EMU_ST
        return -EINVAL;
#endif
    }
    ret = strncpy_s(name, DEVMM_IPC_MEM_NAME_SIZE, proc_node->name, DEVMM_IPC_MEM_NAME_SIZE - 1);
    ka_task_mutex_unlock(&svm_proc->ipc_node.node_mutex);

    ret = (ret != EOK) ? -ENOMEM : 0;
    if (ret == 0) {
        devmm_drv_debug("Ipc node query name succeed. (name=%s; vptr=0x%llx; op=%d)\n", name, vptr, op);
    }

    return ret;
}

struct devmm_range_match_ipc_node {
    struct devmm_ipc_node *ipc_node;
    ka_list_head_t list;
};

static void devmm_ipc_nodes_put(ka_list_head_t *head)
{
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;
    u32 stamp = (u32)ka_jiffies;

    if ((head != NULL) && (!ka_list_empty_careful(head))) {
        ka_list_for_each_safe(pos, n, head) {
            struct devmm_range_match_ipc_node *match_ipc_node = ka_list_entry(pos, struct devmm_range_match_ipc_node, list);
            ka_list_del(&match_ipc_node->list);
            _devmm_ipc_node_put(match_ipc_node->ipc_node);
            devmm_kvfree_ex(match_ipc_node);
            devmm_try_cond_resched(&stamp);
        }
    }
}

static bool devmm_ipc_node_addr_range_is_overlap(struct devmm_ipc_node *node, u64 vptr, size_t len)
{
    if ((vptr + len <= node->attr.vptr) || (vptr >= node->attr.vptr + node->attr.len)) {
        return false;
    }
    return true;
}

static int devmm_ipc_nodes_get_by_range(struct devmm_svm_process *svm_proc, u64 vptr,
    size_t len, ka_list_head_t *head)
{
    struct devmm_ipc_proc_node *proc_node = NULL, *n = NULL;
    struct devmm_range_match_ipc_node *match_node = NULL;
    struct devmm_ipc_node *node = NULL;
    u32 stamp = (u32)ka_jiffies;
    int bkt;

    ka_task_mutex_lock(&svm_proc->ipc_node.node_mutex);
    for (bkt = 0; bkt < DEVMM_MAX_IPC_NODE_LIST_NUM; bkt++) {
        ka_list_for_each_entry_safe(proc_node, n, &svm_proc->ipc_node.create_head[bkt], list) {
            node = _devmm_ipc_node_get(proc_node->name);
            if ((node != NULL) && devmm_ipc_node_addr_range_is_overlap(node, vptr, len)) {
                match_node = devmm_kvzalloc_ex(sizeof(struct devmm_range_match_ipc_node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
                if (match_node == NULL) {
                    _devmm_ipc_node_put(node);
                    ka_task_mutex_unlock(&svm_proc->ipc_node.node_mutex);
                    devmm_ipc_nodes_put(head);
                    return -ENOMEM;
                }
                match_node->ipc_node = node;
                ka_list_add_tail(&match_node->list, head);
                devmm_drv_debug("Ipc node query name succeed. (name=%s; vptr=0x%llx 0x%llx; len=%lu %lu)\n",
                    node->attr.name, node->attr.vptr, vptr, node->attr.len, len);
            } else {
                if (node != NULL) {
                    _devmm_ipc_node_put(node);
                }
            }
            devmm_try_cond_resched(&stamp);
        }
        devmm_try_cond_resched(&stamp);
    }
    ka_task_mutex_unlock(&svm_proc->ipc_node.node_mutex);

    return 0;
}

static int devmm_ipc_node_create_open_page_per_wlist(struct ipc_node_wlist *wlist, struct devmm_ipc_node *ipc_node)
{
    struct devmm_svm_process *svm_proc = wlist->svm_proc;
    struct devmm_ioctl_arg arg = {{0}};
    struct devmm_svm_heap *heap = NULL;
    int try_max_cnt = 3; /* 3 times */
    u32 *bitmap = NULL;
    int ret, i;

    if ((svm_proc == NULL) || (devmm_svm_other_proc_occupy_num_add(svm_proc) != 0)) {
        return 0;
    }

    for (i = 0; i < try_max_cnt; i++) {
        if (ka_task_down_read_trylock(&svm_proc->bitmap_sem) != 0) {
            break;
        }
        ka_system_usleep_range(100, 200); /* 100~200us */
    }
    if (i == try_max_cnt) {
        return -EBUSY;
    }

    heap = devmm_svm_get_heap(svm_proc, wlist->vptr);
    if (heap == NULL) {
#ifndef EMU_ST
        ka_task_up_read(&svm_proc->bitmap_sem);
#endif
        devmm_svm_other_proc_occupy_num_sub(svm_proc);
        return -EINVAL;
    }

    bitmap = devmm_get_page_bitmap_with_heap(heap, wlist->vptr);
    if (bitmap == NULL) {
#ifndef EMU_ST
        ka_task_up_read(&svm_proc->bitmap_sem);
#endif
        devmm_svm_other_proc_occupy_num_sub(svm_proc);
        return -EFAULT;
    }

    arg.data.prefetch_para.ptr = wlist->vptr;
    arg.data.prefetch_para.count = ipc_node->attr.len;
    arg.head.logical_devid = devmm_page_bitmap_get_devid(bitmap);
    arg.head.vfid = devmm_get_vfid_from_svm_process(svm_proc, arg.head.logical_devid);
    arg.head.devid = devmm_get_phyid_devid_from_svm_process(svm_proc, arg.head.logical_devid);
    devmm_page_bitmap_clear_flag(bitmap, DEVMM_PAGE_DEV_MAPPED_MASK);
    ret = devmm_ipc_page_table_create_process(svm_proc, heap, bitmap, &arg, &ipc_node->attr);
    if (ret != 0) {
        devmm_drv_err("create_open_pages failed. (hostpid=%d; va=0x%llx; ret=%d)\n",
            svm_proc->process_id.hostpid, wlist->vptr, ret);
#ifndef EMU_ST
        ka_task_up_read(&svm_proc->bitmap_sem);
#endif
        devmm_svm_other_proc_occupy_num_sub(svm_proc);
        return ret;
    }
    devmm_drv_info("Create_open_pages succ. (hostpid=%d; va=0x%llx; name=%s; create_va=0x%llx; len=0x%llx)\n",
        svm_proc->process_id.hostpid, wlist->vptr, ipc_node->attr.name, ipc_node->attr.vptr, ipc_node->attr.len);

    ka_task_up_read(&svm_proc->bitmap_sem);
    devmm_svm_other_proc_occupy_num_sub(svm_proc);
    return 0;
}

static int devmm_ipc_node_create_open_pages(struct devmm_ipc_node *ipc_node)
{
    struct ipc_node_wlist *wlist = NULL;
    u32 stamp = (u32)ka_jiffies;
    int ret;

    if (ipc_node->is_open_page_freed == false) {
        devmm_drv_err("Can not create open pages. (name=%s; src_vptr=0x%llx; len=0x%llx)\n",
            ipc_node->attr.name, ipc_node->attr.vptr, ipc_node->attr.len);
        return -EPERM;
    }

    ka_list_for_each_entry(wlist, &ipc_node->wlist_head, list) {
        ret = devmm_ipc_node_create_open_page_per_wlist(wlist, ipc_node);
        if (ret != 0) {
            devmm_drv_err("create_open_pages failed. (va=0x%llx; name=%s; create_va=0x%llx; len=0x%llx; ret=%d)\n",
                wlist->vptr, ipc_node->attr.name, ipc_node->attr.vptr, ipc_node->attr.len, ret);
            return ret;
        }
        devmm_try_cond_resched(&stamp);
    }
    ipc_node->is_open_page_freed = false;

    return 0;
}
/* only repair local pod */
int devmm_ipc_node_mem_repair_by_name(const char *name, ka_pid_t pid)
{
    struct devmm_ipc_node *node = NULL;
    int ret;

    node = _devmm_ipc_node_get(name);
    if (node == NULL) {
        devmm_drv_err("Ipc node get fail. (name=%s; pid=%u)\n", name, pid);
        return -EINVAL;
    }
#ifndef EMU_ST
    ka_task_mutex_lock(&node->mutex);
#endif
    if (node->attr.pid != pid) {
        devmm_drv_err("No permission to mem_repair. (name=%s; pid=%d; creator_pid=%d; va=0x%llx; len=0x%llx)\n",
            name, pid, node->attr.pid, node->attr.vptr, node->attr.len);
        ret = -EACCES;
        goto OUT;
    }
    _devmm_ipc_node_free_open_pages(node);
    ret = devmm_ipc_node_create_open_pages(node);
    if (ret == 0) {
        node->mem_repair_record = true;
    }
OUT:
#ifndef EMU_ST
    ka_task_mutex_unlock(&node->mutex);
#endif
    _devmm_ipc_node_put(node);
    return ret;
}

static int devmm_per_ipc_node_mem_repair(struct devmm_ipc_node *ipc_node)
{
    int ret;

    ka_task_mutex_lock(&ipc_node->mutex);
    _devmm_ipc_node_free_open_pages(ipc_node);
    ret = devmm_ipc_node_create_open_pages(ipc_node);
    if (ret == 0) {
        if (g_ipc_node_ops.pod_mem_repair != NULL) {
            ret = g_ipc_node_ops.pod_mem_repair(ipc_node);
        }
    }

    if (ret == 0) {
        ipc_node->mem_repair_record = true;
    }
    ka_task_mutex_unlock(&ipc_node->mutex);

    return ret;
}

static int devmm_ipc_nodes_mem_repair(ka_list_head_t *head)
{
    struct devmm_ipc_node *ipc_node = NULL;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;
    u32 stamp = (u32)ka_jiffies;
    int ret;

    ka_list_for_each_safe(pos, n, head) {
        struct devmm_range_match_ipc_node *match_ipc_node = ka_list_entry(pos, struct devmm_range_match_ipc_node, list);
        ipc_node = match_ipc_node->ipc_node;
        ret = devmm_per_ipc_node_mem_repair(match_ipc_node->ipc_node);
        if (ret != 0) {
            devmm_drv_err("Ipc node mem repair failed. (name=%s; vptr=0x%llx; len=0x%llx; ret=%d)\n",
                ipc_node->attr.name, ipc_node->attr.vptr, ipc_node->attr.len, ret);
            return -EINVAL;
        }
        devmm_try_cond_resched(&stamp);
    }

    return 0;
}

int devmm_ipc_create_mem_repair_post_process(struct devmm_svm_process *svm_proc, u64 addr, u64 len)
{
    ka_list_head_t head = KA_LIST_HEAD_INIT(head);
    int ret;

    ret = devmm_ipc_nodes_get_by_range(svm_proc, addr, len, &head);
    if (ret == 0) {
        ret = devmm_ipc_nodes_mem_repair(&head);
        devmm_ipc_nodes_put(&head);
    }
    return ret;
}

/* res_mng had been erased, can not call devmm_dev_res_mng_get */
static void _devmm_ipc_node_clean_all_by_dev_res_mng(struct devmm_dev_res_mng *res_mng)
{
    struct devmm_ipc_node *node = NULL;
    ka_hlist_node_t *tmp = NULL;
    u32 stamp = (u32)ka_jiffies;
    u32 bkt;

    ka_hash_for_each_safe(res_mng->ipc_mem_node_info.node_htable, bkt, tmp, node, link) {
        devmm_ipc_procfs_del_node(node);
        devmm_ipc_node_del_by_dev_res_mng(node, res_mng);
        _devmm_ipc_node_destroy(node);
        devmm_try_cond_resched(&stamp);
    }
}

void devmm_ipc_node_clean_all_by_dev_res_mng(struct devmm_dev_res_mng *res_mng)
{
    ka_task_mutex_lock(&res_mng->ipc_mem_node_info.mutex);
    _devmm_ipc_node_clean_all_by_dev_res_mng(res_mng);
    ka_task_mutex_unlock(&res_mng->ipc_mem_node_info.mutex);
}

void devmm_ipc_node_init(void)
{
    devmm_ipc_profs_init();
}

void devmm_ipc_node_uninit(void)
{
    devmm_ipc_profs_uninit();
}

