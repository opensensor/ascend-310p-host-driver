/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2022. All rights reserved.
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
 * Create: 2019-10-15
 */

#include <linux/slab.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/pid.h>
#include <linux/kref.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/task.h>
#endif

#include "svm_msg_client.h"
#include "devmm_channel.h"
#include "devdrv_manager_comm.h"
#include "devmm_common.h"
#include "svm_heap_mng.h"
#include "svm_dev_res_mng.h"
#include "svm_shmem_node.h"
#include "svm_shmem_interprocess.h"

static void devmm_ipc_mem_set_ipc_create_flag(struct devmm_svm_process *svm_proc, u64 va)
{
    u32 *bitmap = devmm_get_page_bitmap(svm_proc, va);
    if (bitmap != NULL) {
        devmm_page_bitmap_set_flag(bitmap, DEVMM_PAGE_IPC_MEM_CREATE_MASK);
    }
}

static void devmm_ipc_mem_clear_ipc_create_flag(struct devmm_svm_process *svm_proc, u64 va)
{
    u32 *bitmap = devmm_get_page_bitmap(svm_proc, va);
    if (bitmap != NULL) {
        devmm_page_bitmap_clear_flag(bitmap, DEVMM_PAGE_IPC_MEM_CREATE_MASK);
    }
}

static void devmm_ipc_mem_set_fst_page_ipc_create_flag(struct devmm_svm_process *svm_proc, u64 va)
{
    u32 *bitmap = devmm_get_alloced_va_fst_page_bitmap(svm_proc, va);
    if (bitmap != NULL) {
        devmm_page_bitmap_set_flag(bitmap, DEVMM_PAGE_IPC_MEM_CREATE_MASK);
    }
}

void devmm_try_free_ipc_mem(struct devmm_svm_process *svm_proc, u64 vptr, u64 page_num, u32 page_size)
{
    devmm_ipc_proc_node_free_open_pages(svm_proc, vptr, page_num, page_size);
}

void devmm_destroy_ipc_mem_node_by_proc(struct devmm_svm_process *svm_proc, u32 devid)
{
    devmm_ipc_proc_node_recycle(svm_proc, devid);
}

int devmm_ipc_query_owner_attr_by_va(struct devmm_svm_process *svm_proc, u64 va, void *node_attr,
    struct devmm_ipc_owner_attr *owner_attr)
{
    struct devmm_ipc_node_attr attr;
    u64 alloced_va;
    int ret;

    ret = devmm_get_alloced_va(svm_proc, va, &alloced_va);
    if (ret != 0) {
        devmm_drv_err("Get alloced va failed. (ret=%d; va=0x%llx)\n", ret, va);
        return ret;
    }
    if (node_attr != NULL) {
        ret = memcpy_s(&attr, sizeof(struct devmm_ipc_node_attr), node_attr, sizeof(struct devmm_ipc_node_attr));
        ret = (ret != EOK) ? -ENOMEM : 0;
    } else {
        ret = devmm_ipc_proc_query_attr_by_va(svm_proc, alloced_va, 1, &attr); /* 1 means get attr from open va */
        if (ret != 0) {
            devmm_drv_err("Query attr by va fail. (ret=%d; va=0x%llx; alloced_va=0x%llx)\n", ret, va, alloced_va);
            return ret;
        }
    }

    owner_attr->devid = attr.inst.devid;
    owner_attr->vfid = attr.inst.vfid;
    owner_attr->pid = attr.pid;
    owner_attr->va = attr.vptr + (va - alloced_va);
    owner_attr->sdid = attr.sdid;
    owner_attr->mem_map_route = attr.mem_map_route;

    return 0;
}

struct devmm_svm_process *devmm_ipc_query_owner_info(struct devmm_svm_process *svm_proc,
    u64 va, u64 *owner_va, struct devmm_svm_process_id *id, u32 *sdid)
{
    struct devmm_ipc_node_attr attr;
    u64 alloced_va;
    int ret;

    ret = devmm_get_alloced_va(svm_proc, va, &alloced_va);
    if (ret != 0) {
        devmm_drv_err("Get alloced va failed. (ret=%d; va=%llx)\n", ret, va);
        return NULL;
    }

    ret = devmm_ipc_proc_query_attr_by_va(svm_proc, alloced_va, 1, &attr);  /* 1 means get attr from open va */
    if (ret != 0) {
        devmm_drv_err("Query attr by va fail. (ret=%d; va=0x%llx; alloced_va=0x%llx)\n", ret, va, alloced_va);
        return NULL;
    }

    *owner_va = attr.vptr + (va - alloced_va);
    id->devid = (u16)attr.inst.devid;
    id->vfid = (u16)attr.inst.vfid;
    id->hostpid = attr.pid;

    if (sdid != NULL) {
        *sdid = attr.sdid;
    }

    return attr.svm_proc;
}

int devmm_ipc_get_owner_proc_attr(struct devmm_svm_process *svm_proc, struct devmm_memory_attributes *attr,
    struct devmm_svm_process **owner_proc, struct devmm_svm_heap **heap, struct devmm_memory_attributes *owner_attr)
{
    struct devmm_svm_process_id proc_id;
    u64 owner_va;
    int ret;

    *owner_proc = devmm_ipc_query_owner_info(svm_proc, attr->va, &owner_va, &proc_id, NULL);
    if (*owner_proc == NULL) {
        return -EINVAL;
    }
    ret = devmm_svm_other_proc_occupy_num_add(*owner_proc);
    if (ret != 0) {
        devmm_drv_debug("Process is exiting. (hostpid=%d)\n", (*owner_proc)->process_id.hostpid);
        return ret;
    }

    if (!devmm_va_is_not_svm_process_addr(*owner_proc, owner_va)) {
        *heap = devmm_svm_heap_get(*owner_proc, owner_va);
        if (*heap == NULL) {
            devmm_drv_err("Heap is NULL. (owner_host_pid=%d; va=%llx)\n", (*owner_proc)->process_id.hostpid, owner_va);
            devmm_svm_other_proc_occupy_num_sub(*owner_proc);
            return -EADDRNOTAVAIL;
        }
    }

    ret = devmm_get_memory_attributes(*owner_proc, owner_va, owner_attr);
    if (ret != 0) {
        devmm_drv_err("Query attributes failed. (owner_host_pid=%d; va=%llx)\n",
            (*owner_proc)->process_id.hostpid, owner_va);
#ifndef EMU_ST
        devmm_svm_heap_put(*heap);
        devmm_svm_other_proc_occupy_num_sub(*owner_proc);
#endif
        return ret;
    }

    return 0;
}

void devmm_ipc_put_owner_proc_attr(struct devmm_svm_process *owner_proc, struct devmm_svm_heap *heap)
{
    devmm_svm_heap_put(heap);
    devmm_svm_other_proc_occupy_num_sub(owner_proc);
}

static int devmm_ipc_mem_create_para_check(struct devmm_svm_process *svm_proc,
    u64 vptr, size_t len, struct devmm_memory_attributes *attr)
{
    u32 *bitmap = devmm_get_page_bitmap(svm_proc, vptr);
    u64 page_bitmap_num = devmm_get_pagecount_by_size(vptr, len, (u32)attr->granularity_size);
    u64 i;

    if ((attr->is_svm_device == 0) || devmm_is_host_agent(attr->devid) || (attr->page_size == 0) ||
        ((vptr % attr->page_size) != 0) || attr->is_mem_export || attr->is_mem_import) {
        /* The log cannot be modified, because in the failure mode library. */
        devmm_drv_err("Invalid para. (va=0x%llx; page_size=%u; devid=%d; export=%u; import=%u)\n",
            vptr, attr->page_size, attr->devid, attr->is_mem_export, attr->is_mem_import);
        return -EINVAL;
    }

    if (bitmap == NULL) {
        devmm_drv_err("Get bitmap error. (vptr=0x%llx)\n", vptr);
        return -EINVAL;
    }

    if (devmm_check_status_va_info(svm_proc, vptr, len) != 0) {
        devmm_drv_err("Destination vaddress check error. (vptr=0x%llx)\n", vptr);
        return -EINVAL;
    }

    for (i = 0; i < page_bitmap_num; i++) {
        if (!devmm_page_bitmap_is_page_available(bitmap + i)) {
            devmm_drv_err("Virtual address is invalid. "
                          "(va=0x%llx; page_id=%lld; bitmap=0x%x)\n", vptr, i, *(bitmap + i));
            return -EINVAL;
        }

        if (!devmm_page_bitmap_is_locked_device(bitmap + i)) {
            devmm_drv_err("Virtual address is not device locked. "
                          "(va=0x%llx; page_id=%lld; bitmap=0x%x)\n", vptr, i, *(bitmap + i));
            return -EINVAL;
        }

        if (!devmm_page_bitmap_is_dev_mapped(bitmap + i)) {
            devmm_drv_err("Virtual address is not device maped. "
                          "(va=0x%llx; pageid=%lld; bitmap=0x%x)\n", vptr, i, *(bitmap + i));
            return -EINVAL;
        }
        if (devmm_page_bitmap_is_advise_readonly(bitmap + i)) {
            devmm_drv_err("Readonly mem, not allowed ipc create. "
                          "(va=0x%llx; pageid=%lld; bitmap=0x%x)\n", vptr, i, *(bitmap + i));
            return -EINVAL;
        }
        if (devmm_page_bitmap_is_ipc_open_mem(bitmap + i)) {
            devmm_drv_err("Virtual address is ipc open memory. "
                          "(va=0x%llx; page_id=%lld; bitmap=0x%x)\n", vptr, i, *(bitmap + i));
            return -EINVAL;
        }
    }

    return 0;
}

static int devmm_ipc_vmmas_occupy_inc(struct devmm_svm_process *svm_proc,
    bool is_reserve_addr, u64 va, u64 size)
{
    struct devmm_svm_heap *heap = NULL;
    int ret;

    if (is_reserve_addr == false) {
        return 0;
    }

    heap = devmm_svm_heap_get(svm_proc, va);
    if (heap == NULL) {
        devmm_drv_err("Heap is NULL. (addr=0x%llx)\n", va);
        return -EADDRNOTAVAIL;
    }
    ret = devmm_vmmas_occupy_inc(&heap->vmma_mng, va, size);
    devmm_svm_heap_put(heap);

    return ret;
}

static void devmm_ipc_vmmas_occupy_dec(struct devmm_svm_process *svm_proc,
    bool is_reserve_addr, u64 va, u64 size)
{
    struct devmm_svm_heap *heap = NULL;

    if (is_reserve_addr == false) {
        return;
    }

    heap = devmm_svm_heap_get(svm_proc, va);
    if (heap == NULL) {
        devmm_drv_err("Heap is NULL. (addr=0x%llx)\n", va);
        return;
    }

    devmm_vmmas_occupy_dec(&heap->vmma_mng, va, size);
    devmm_svm_heap_put(heap);
}

static int devmm_ipc_mem_node_init(struct devmm_ipc_node_attr *node_attr)
{
    int ret;

    ret = devmm_ipc_vmmas_occupy_inc(node_attr->svm_proc, node_attr->is_reserve_addr, node_attr->vptr, node_attr->len);
    if (ret != 0) {
        devmm_drv_err("Vmma occupy inc fail. (ret=%d; is_reserve_addr=%d)\n", ret, node_attr->is_reserve_addr);
        return ret;
    }
    devmm_ipc_mem_set_ipc_create_flag(node_attr->svm_proc, node_attr->vptr);
    devmm_ipc_mem_set_fst_page_ipc_create_flag(node_attr->svm_proc, node_attr->vptr); /* for free va fast check */
    return 0;
}

static void devmm_ipc_mem_node_uninit(struct devmm_ipc_node_attr *node_attr)
{
    devmm_ipc_mem_clear_ipc_create_flag(node_attr->svm_proc, node_attr->vptr);
    devmm_ipc_vmmas_occupy_dec(node_attr->svm_proc, node_attr->is_reserve_addr, node_attr->vptr, node_attr->len);
}

static int _devmm_ipc_mem_name_create(struct svm_id_inst *inst, char *name, size_t len)
{
    static u64 g_ipc_name_ref = KA_BASE_ATOMIC64_INIT(0);
    int offset;

    offset = snprintf_s(name, IPC_NAME_SIZE, IPC_NAME_SIZE - 1, "%08x%016llx%02x%02x", ka_task_get_current_tgid(),
        (u64)ka_base_atomic64_inc_return((ka_atomic64_t *)&g_ipc_name_ref), inst->devid, inst->vfid);
    if (offset < 0) {
        devmm_drv_err("Snprintf failed. (offset=%d)\n", offset);
        return -EINVAL;
    }
    name[offset] = '\0';
    return 0;
}

static int devmm_ioctl_ipc_node_attr_pack(struct devmm_svm_process *svm_proc, struct devmm_mem_ipc_create_para *karg,
    struct devmm_ipc_node_attr *node_attr)
{
    struct devmm_memory_attributes mem_attr;
    int ret;

    ret = devmm_get_memory_attributes(svm_proc, karg->vptr, &mem_attr);
    if (ret != 0) {
        devmm_drv_err("Get mem attr fail. (ret=%d; vptr=0x%llx)\n", ret, karg->vptr);
        return ret;
    }

    ret = devmm_ipc_mem_create_para_check(svm_proc, karg->vptr, karg->len, &mem_attr);
    if (ret != 0) {
        devmm_drv_err("Create_para_check fail. (va=0x%llx)\n", karg->vptr);
        return ret;
    }

    svm_id_inst_pack(&node_attr->inst, mem_attr.devid, mem_attr.vfid);
    ret = _devmm_ipc_mem_name_create(&node_attr->inst, node_attr->name, DEVMM_IPC_MEM_NAME_SIZE);
    if (ret != 0) {
        devmm_drv_err("Ipc mem name create fail. (ret=%d)\n", ret);
        return ret;
    }

    node_attr->svm_proc = svm_proc;
    node_attr->pid = ka_task_get_current_tgid();
    node_attr->vptr = karg->vptr;
    node_attr->len = karg->len;
    node_attr->init_fn = devmm_ipc_mem_node_init;
    node_attr->uninit_fn = devmm_ipc_mem_node_uninit;
    node_attr->page_size = mem_attr.page_size;
    node_attr->is_huge = mem_attr.is_svm_huge;
    node_attr->is_reserve_addr = mem_attr.is_reserve_addr;
    node_attr->need_set_wlist = true;
    return 0;
}

int devmm_ioctl_ipc_mem_create(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_ipc_create_para *karg = &arg->data.ipc_create_para;
    struct devmm_ipc_node_attr node_attr = {{0}};
    int ret;

    if (karg->name_len < DEVMM_MAX_NAME_SIZE) {
        devmm_drv_err("Input name len error. (Name_len=%u)\n", karg->name_len);
        return -EINVAL;
    }

    ret = devmm_ioctl_ipc_node_attr_pack(svm_proc, karg, &node_attr);
    if (ret != 0) {
        devmm_drv_err("Ipc node attr pack fail. (ret=%d; vptr=0x%llx; len=%lu)\n", ret, karg->vptr, karg->len);
        return ret;
    }

    ret = devmm_ipc_node_create(&node_attr);
    if (ret != 0) {
        devmm_drv_err("Ipc node create fail. (ret=%d)\n", ret);
        return ret;
    }

    ret = devmm_ipc_proc_node_add(svm_proc, &node_attr, 0);
    if (ret != 0) {
        devmm_ipc_node_destroy(node_attr.name, node_attr.pid, false);
        devmm_drv_err("Ipc node add fail. (ret=%d; name=%s)\n", ret, node_attr.name);
        return ret;
    }

    ret = strncpy_s(karg->name, DEVMM_MAX_NAME_SIZE, node_attr.name, DEVMM_MAX_NAME_SIZE);
    if (ret != EOK) {
        devmm_ipc_proc_node_del(svm_proc, karg->vptr, 0);
        devmm_ipc_node_destroy(node_attr.name, node_attr.pid, false);
        devmm_drv_err("Strcpy name fail. (ret=%d; name=%s)\n", ret, node_attr.name);
        return -ENOMEM;
    }
    return 0;
}

int devmm_ioctl_ipc_mem_destroy(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_ipc_destroy_para *karg = &arg->data.ipc_destroy_para;
    struct devmm_ipc_node_attr node_attr;
    int ret;

    karg->name[DEVMM_MAX_NAME_SIZE - 1] = '\0';
    ret = devmm_ipc_query_node_attr(karg->name, &node_attr);
    if (ret != 0) {
        devmm_drv_err("Query ipc node attr fail. (ret=%d; name=%s)\n", ret, karg->name);
        /* Do not modify return value */
        return -EINVAL;
    }

    ret = devmm_ipc_node_destroy(karg->name, ka_task_get_current_tgid(), false);
    if (ret != 0) {
        devmm_drv_err("Destroy ipc node fail. (ret=%d; name=%s)\n", ret, karg->name);
        return ret;
    }
    devmm_ipc_proc_node_del(svm_proc, node_attr.vptr, 0);

    return 0;
}

int devmm_ioctl_ipc_mem_open(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_ipc_open_para *karg = &arg->data.ipc_open_para;
    struct devmm_ipc_node_attr attr = {{0}};
    u32 devid = arg->head.devid;
    int ret;

    karg->name[DEVMM_MAX_NAME_SIZE - 1] = '\0';
    ret = devmm_ipc_node_open(karg->name, svm_proc, karg->vptr);
    if (ret != 0) {
        devmm_drv_err("Ipc open fail. (ret=%d; name=%s; vptr=0x%llx)\n", ret, karg->name, karg->vptr);
        return ret;
    }

    (void)strncpy_s(attr.name, DEVMM_MAX_NAME_SIZE, karg->name, DEVMM_MAX_NAME_SIZE);
    attr.vptr = karg->vptr;
    attr.len = 0;
    attr.inst.devid = devid;
    ret = devmm_ipc_proc_node_add(svm_proc, &attr, 1);
    if (ret != 0) {
        (void)devmm_ipc_node_close(karg->name, svm_proc, karg->vptr);
        devmm_drv_err("Ipc proc add fail. (ret=%d; name=%s; vptr=0x%llx)\n", ret, karg->name, karg->vptr);
        return ret;
    }

    return 0;
}

int devmm_ioctl_ipc_mem_close(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_ipc_close_para *karg = &arg->data.ipc_close_para;
    char name[DEVMM_MAX_NAME_SIZE];
    int ret;

    ret = devmm_ipc_proc_query_name_by_va(svm_proc, karg->vptr, 1, name);
    if (ret != 0) {
        devmm_drv_err("Query name by va fail. (ret=%d; vptr=0x%llx)\n", ret, karg->vptr);
        return ret;
    }

    ret = devmm_ipc_node_close(name, svm_proc, karg->vptr);
    if (ret != 0) {
        devmm_drv_err("Ipc node close fail. (ret=%d; name=%s; vptr=0x%llx)\n", ret, name, karg->vptr);
        return ret;
    }
    devmm_ipc_proc_node_del(svm_proc, karg->vptr, 1);
    return 0;
}

int devmm_ioctl_ipc_mem_set_pid(struct devmm_svm_process *svm_pro, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_ipc_set_pid_para *karg = &arg->data.ipc_set_pid_para;
    struct devmm_ipc_setpid_attr attr;
    int ret;

    karg->name[DEVMM_MAX_NAME_SIZE - 1] = '\0';
    karg->sdid = KA_UINT_MAX;
    attr.name = karg->name;
    attr.sdid = karg->sdid;
    attr.creator_pid = ka_task_get_current_tgid();
    attr.pid = karg->set_pid;
    attr.pid_num = karg->num;

    /* When attr.send is NULL, attr.inst is not needed */
    attr.send = NULL;

    ret = devmm_ipc_node_set_pids(&attr);
    if (ret != 0) {
        devmm_drv_err("Set pid fail. (ret=%d; name=%s; sdid=%u)\n", ret, karg->name, karg->sdid);
        return ret;
    }
    return 0;
}

int devmm_ioctl_ipc_mem_query(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_query_size_para *karg = &arg->data.query_size_para;
    struct devmm_ipc_node_attr node_attr;
    int ret;

    karg->name[DEVMM_MAX_NAME_SIZE - 1] = '\0';
    ret = devmm_ipc_query_node_attr(karg->name, &node_attr);
    if (ret != 0) {
        devmm_drv_err("Query ipc node attr fail. (ret=%d; name=%s)\n", ret, karg->name);
        return ret;
    }

    karg->len = node_attr.len;
    karg->is_huge = node_attr.is_huge;
    karg->phy_devid = node_attr.inst.devid;

    return 0;
}

static int(*devmm_ipc_set_attr_handlers[SHMEM_ATTR_TYPE_MAX])
    (const char *name, uint64_t flag) = {
        [SHMEM_ATTR_TYPE_MEM_MAP] = devmm_ipc_set_mem_map_attr,
        [SHMEM_ATTR_TYPE_NO_WLIST_IN_SERVER] = devmm_ipc_set_no_wlist_in_server_attr,
};

int devmm_ioctl_ipc_set_attr(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_ipc_set_attr_para *karg = &arg->data.ipc_set_attr_para;

    karg->name[DEVMM_MAX_NAME_SIZE - 1] = '\0';

    if ((karg->type >= SHMEM_ATTR_TYPE_MAX) || (devmm_ipc_set_attr_handlers[karg->type] == NULL)) {
        return -EOPNOTSUPP;
    }

    return devmm_ipc_set_attr_handlers[karg->type](karg->name, karg->attr);
}

static int(*devmm_ipc_get_attr_handlers[SHMEM_ATTR_TYPE_MAX])
    (const char *name, uint64_t *flag) = {
        [SHMEM_ATTR_TYPE_MEM_MAP] = devmm_ipc_get_mem_map_attr,
        [SHMEM_ATTR_TYPE_NO_WLIST_IN_SERVER] = devmm_ipc_get_no_wlist_in_server_attr,
};

int devmm_ioctl_ipc_get_attr(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_ipc_get_attr_para *karg = &arg->data.ipc_get_attr_para;

    karg->name[DEVMM_MAX_NAME_SIZE - 1] = '\0';

    if ((karg->type >= SHMEM_ATTR_TYPE_MAX) || (devmm_ipc_get_attr_handlers[karg->type] == NULL)) {
        return -EOPNOTSUPP;
    }

    return devmm_ipc_get_attr_handlers[karg->type](karg->name, &karg->attr);
}

void devmm_ipc_mem_init(void)
{
    devmm_ipc_node_init();
}

void devmm_ipc_mem_uninit(void)
{
    devmm_ipc_node_uninit();
}
