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
 * Create: 2023-10-10
 */
#include "feature_loader.h"
#include "spod_info.h"

#include "svm_msg_client.h"
#include "svm_define.h"
#include "devmm_channel.h"
#include "svm_rbtree.h"
#include "svm_dev_res_mng.h"
#include "svm_master_proc_mng.h"
#include "svm_phy_addr_blk_mng.h"
#include "svm_master_mem_create.h"
#include "svm_master_mem_share.h"
#include "svm_mem_create.h"
#include "svm_mem_share.h"
#ifdef CFG_SOC_PLATFORM_CLOUD_V2
#include "svm_shmem_node_pod.h"
#endif
#include "svm_ref_server_occupier.h"
#include "svm_recycle_thread.h"

struct devmm_pid_list_node {
    ka_rb_node_t node;
    ka_pid_t pid;
    u64 set_time;
    bool is_share[DEVMM_MAX_DEVICE_NUM];
    u32 permission;
};

struct devmm_pid_list_mng {
    ka_rw_semaphore_t rw_sem;
    ka_rb_root_t rbtree;
    u32 pid_cnt;
    bool need_set_wlist; /* True means that the user needs to actively call the interface to set pid */
};

struct devmm_share_mem_info {
    int id;
    int share_id;
    int hostpid;
    u32 sdid;
    u32 devid;

    u64 pg_num; /* for record p2p msg alloc size */
    u32 module_id;
    u32 side;
    u32 pg_type;
    u32 mem_type;
};

struct devmm_share_phy_addr_agent_blk {
    ka_rb_node_t dev_res_mng_node;

    ka_kref_t ref;
    struct devmm_ref_server_occupier_mng server_occupier_mng;

    u32 pre_release_flag;

    int share_id;

    u32 devid;
    int id;

    u64 pg_num; /* for record p2p msg alloc size */
    u32 module_id;
    u32 side;
    u32 type;
    u32 pg_type;
    u32 mem_type;

    ka_spinlock_t status_lock;
    u32 occupied_cnt;
    u32 status;

    int export_pid;
    struct devmm_pid_list_mng pid_list_mng;
};

#define SHARE_AGENT_BLK_STATUS_IDLE 0
#define SHARE_AGENT_BLK_STATUS_OCCUPIED 1
#define SHARE_AGENT_BLK_STATUS_RELEASING 2

static void devmm_share_agent_blk_put(struct devmm_share_phy_addr_agent_blk *blk);
static int devmm_share_mem_release(int side, u32 share_devid, int share_id, u32 total_pg_num, u32 free_type);

static u64 rb_handle_of_pid_list_node(ka_rb_node_t *node)
{
    struct devmm_pid_list_node *list_node = ka_base_rb_entry(node, struct devmm_pid_list_node, node);
    return (u64)list_node->pid;
}

static ka_rb_node_t *_devmm_pid_list_insert(struct devmm_pid_list_mng *mng, ka_pid_t pid)
{
    u64 current_time = (u64)ka_system_ktime_to_ns(ka_system_ktime_get());
    struct devmm_pid_list_node *new_node = NULL;
    struct devmm_pid_list_node *exist_node = NULL;
    ka_rb_node_t *node = NULL;
    u32 i;

    new_node = devmm_kvzalloc_ex(sizeof(struct devmm_pid_list_node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (new_node == NULL) {
        devmm_drv_err("Alloc pid_list_node fail.\n");
        return NULL;
    }
    new_node->pid = pid;
    new_node->set_time = current_time;
    for (i = 0; i < DEVMM_MAX_DEVICE_NUM; i++) {
        new_node->is_share[i] = false;
    }

    /* same pid insert, should update set time. */
    if (mng->pid_cnt >= DEVMM_SHARE_MEM_MAX_PID_CNT) {
        devmm_drv_err("Pid_num is out of limit. (pid_num=%u; pid=%d; limit=%u)\n", mng->pid_cnt, pid, DEVMM_SHARE_MEM_MAX_PID_CNT);
        devmm_kvfree_ex(new_node);
        return NULL;
    }

    node = devmm_rb_search(&mng->rbtree, (u64)pid, rb_handle_of_pid_list_node);
    if (node != NULL) {
        exist_node = ka_base_rb_entry(node, struct devmm_pid_list_node, node);
        exist_node->set_time = current_time;
        devmm_kvfree_ex(new_node);
        devmm_drv_debug("Update pid set time. (pid=%d)\n", pid);
        return node;
    }

    mng->pid_cnt++;
    (void)devmm_rb_insert(&mng->rbtree, &new_node->node, rb_handle_of_pid_list_node);
    return &new_node->node;
}

static int devmm_pid_list_insert(struct devmm_pid_list_mng *mng, ka_pid_t pid)
{
    int ret;

    ka_task_down_write(&mng->rw_sem);
    if (mng->need_set_wlist == false) {
        devmm_drv_err("Had set no need wlist, operation not permitted.\n");
        ka_task_up_write(&mng->rw_sem);
        return -EPERM;
    }

    ret = (_devmm_pid_list_insert(mng, pid) != NULL) ? 0 : -EINVAL;
    ka_task_up_write(&mng->rw_sem);
    return ret;
}

static int devmm_pid_set_share_status(struct devmm_share_phy_addr_agent_blk *blk, ka_pid_t pid, u32 devid, bool is_share)
{
    struct devmm_pid_list_mng *mng = &blk->pid_list_mng;
    u64 start_time = devmm_get_tgid_start_time();
    struct devmm_pid_list_node *list_node = NULL;
    ka_rb_node_t *node = NULL;

    if (devid >= DEVMM_MAX_DEVICE_NUM) {
        devmm_drv_err("Invalid devid. (pid=%d; devid=%u; id=%d)\n", pid, devid, blk->id);
        return -EINVAL;
    }

    ka_task_down_write(&mng->rw_sem);
    node = devmm_rb_search(&mng->rbtree, (u64)pid, rb_handle_of_pid_list_node);
    if (node == NULL) {
        if (is_share && (mng->need_set_wlist == false)) {
            node = _devmm_pid_list_insert(mng, pid);
            if (node == NULL) {
                ka_task_up_write(&mng->rw_sem);
                return -EINVAL;
            }
        } else {
            ka_task_up_write(&mng->rw_sem);
            return -EACCES;
        }
    }

    list_node = ka_base_rb_entry(node, struct devmm_pid_list_node, node);
    if (is_share) {
        if (list_node->is_share[devid]) {
            ka_task_up_write(&mng->rw_sem);
            /* The log cannot be modified, because in the failure mode library. */
            devmm_drv_err("Can not import multi times. (pid=%d; devid=%u)\n", pid, devid);
            /* EFAULT cann't change, user will get invalid handle err code. */
            return -EFAULT;
        }

        if (list_node->set_time < start_time) {
            ka_task_up_write(&mng->rw_sem);
            devmm_drv_err("Time check fail. (thread=%d; thread_start_time=%llu)\n", pid, start_time);
            return -EACCES;
        }
    }

    list_node->is_share[devid] = is_share;
    ka_task_up_write(&mng->rw_sem);
    return 0;
}

static void devmm_pid_list_erase(struct devmm_pid_list_mng *mng, ka_pid_t pid)
{
    struct devmm_pid_list_node *list_node = NULL;
    ka_rb_node_t *node = NULL;

    ka_task_down_write(&mng->rw_sem);
    node = devmm_rb_search(&mng->rbtree, (u64)pid, rb_handle_of_pid_list_node);
    if (node != NULL) {
        mng->pid_cnt--;
        list_node = ka_base_rb_entry(node, struct devmm_pid_list_node, node);
        (void)devmm_rb_erase(&mng->rbtree, node);
        devmm_kvfree_ex(list_node);
    }
    ka_task_up_write(&mng->rw_sem);
}

static void devmm_pid_list_release_func(ka_rb_node_t *node)
{
    struct devmm_pid_list_node *list_node = ka_base_rb_entry(node, struct devmm_pid_list_node, node);

    devmm_kvfree_ex(list_node);
}

static void devmm_pid_list_erase_all(struct devmm_pid_list_mng *mng)
{
    ka_task_down_write(&mng->rw_sem);
    devmm_rb_erase_all_node(&mng->rbtree, devmm_pid_list_release_func);
    ka_task_up_write(&mng->rw_sem);
}

static void devmm_share_agent_blk_status_init(struct devmm_share_phy_addr_agent_blk *blk)
{
    ka_task_spin_lock_init(&blk->status_lock);
    blk->occupied_cnt = 0;
    blk->status = SHARE_AGENT_BLK_STATUS_IDLE;
}

static int devmm_share_agent_blk_occupied_cnt_add(struct devmm_share_phy_addr_agent_blk *blk, u64 num)
{
    ka_task_spin_lock(&blk->status_lock);
    if (blk->status == SHARE_AGENT_BLK_STATUS_RELEASING) {
        ka_task_spin_unlock(&blk->status_lock);
        return -EBUSY;
    }

    blk->status = SHARE_AGENT_BLK_STATUS_OCCUPIED;
    blk->occupied_cnt += num;
    ka_task_spin_unlock(&blk->status_lock);
    return 0;
}

static void devmm_share_agent_blk_occupied_cnt_sub(struct devmm_share_phy_addr_agent_blk *blk,
    u64 num, bool *is_blk_no_occupied)
{
    ka_task_spin_lock(&blk->status_lock);
    blk->occupied_cnt -= num;
    if (blk->occupied_cnt == 0) {
        blk->status = SHARE_AGENT_BLK_STATUS_RELEASING;
        *is_blk_no_occupied = true;
    }
    ka_task_spin_unlock(&blk->status_lock);
}

static int _devmm_share_agent_blk_occupied_cnt_inc(struct devmm_share_phy_addr_agent_blk *blk)
{
    return devmm_share_agent_blk_occupied_cnt_add(blk, 1ULL);
}

static void _devmm_share_agent_blk_occupied_cnt_dec(struct devmm_share_phy_addr_agent_blk *blk,
    bool *is_blk_no_occupied)
{
    devmm_share_agent_blk_occupied_cnt_sub(blk, 1ULL, is_blk_no_occupied);
}

static int devmm_share_agent_blk_occupied_cnt_inc(struct devmm_share_phy_addr_agent_blk *blk, u32 dst_sdid)
{
    bool is_blk_no_occupied;
    int ret;

    ret = _devmm_share_agent_blk_occupied_cnt_inc(blk);
    if (ret != 0) {
        return ret;
    }

    if (dst_sdid != SVM_INVALID_SDID) {
        ret = devmm_ref_server_occupier_add(&blk->server_occupier_mng, dst_sdid);
        if (ret != 0) {
            _devmm_share_agent_blk_occupied_cnt_dec(blk, &is_blk_no_occupied);
            return ret;
        }
    }

    return 0;
}

static void devmm_share_agent_blk_occupied_cnt_dec(struct devmm_share_phy_addr_agent_blk *blk, u32 dst_sdid,
    bool *is_blk_no_occupied)
{
    if (dst_sdid != SVM_INVALID_SDID) {
        (void)devmm_ref_server_occupier_del(&blk->server_occupier_mng, dst_sdid);
    }
   _devmm_share_agent_blk_occupied_cnt_dec(blk, is_blk_no_occupied);
}

static u64 rb_handle_of_share_id_map_node(ka_rb_node_t *node)
{
    struct devmm_share_id_map_node *map_node = ka_base_rb_entry(node, struct devmm_share_id_map_node, proc_node);
    return (u64)map_node->shid_map_node_info.id;
}

static struct devmm_share_id_map_node *devmm_erase_one_share_id_map_node(struct devmm_share_id_map_mng *mng,
    rb_erase_condition condition)
{
    ka_rb_node_t *node = NULL;

    ka_task_down_write(&mng->sem);
    node = devmm_rb_erase_one_node(&mng->rbtree, condition);
    ka_task_up_write(&mng->sem);

    return ((node == NULL) ? NULL : ka_base_rb_entry(node, struct devmm_share_id_map_node, proc_node));
}

static struct devmm_share_id_map_mng *devmm_get_share_id_map_mng(struct devmm_svm_proc_master *master_data, u32 devid)
{
    return (devid == uda_get_host_id()) ? &master_data->master_share_id_map_mng : &master_data->share_id_map_mng[devid];
}

void devmm_share_id_map_node_destroy_by_devid(struct devmm_svm_process *svm_proc, u32 devid, bool need_pid_erase)
{
    struct devmm_svm_proc_master *master_data = svm_proc->priv_data;
    struct devmm_share_id_map_mng *mng = NULL;
    struct devmm_share_id_map_node *map_node = NULL;
    u32 stamp = (u32)ka_jiffies;

    mng = devmm_get_share_id_map_mng(master_data, devid);

    while (1) {
        bool is_local_blk = true;

        map_node = devmm_erase_one_share_id_map_node(mng, NULL);
        if (map_node == NULL) {
            break;
        }

#ifdef CFG_SOC_PLATFORM_CLOUD_V2
        if ((map_node->blk_type == SVM_PYH_ADDR_BLK_IMPORT_TYPE) &&
            (!svm_is_sdid_in_local_server(devid, map_node->shid_map_node_info.share_sdid))) {
            is_local_blk = false;
        }
#endif

        if (is_local_blk) {
            (void)devmm_share_agent_blk_put_with_share_id(map_node->shid_map_node_info.share_devid,
                map_node->shid_map_node_info.share_id, map_node->hostpid, map_node->shid_map_node_info.devid,
                need_pid_erase, SVM_INVALID_SDID);
        } else {
#ifdef CFG_SOC_PLATFORM_CLOUD_V2
            int ret = devmm_put_remote_share_mem_info(devid, map_node->shid_map_node_info.share_id,
                map_node->shid_map_node_info.share_sdid, map_node->shid_map_node_info.share_devid);
            if (ret != 0) {
                devmm_drv_err("Put share id failed. (devid=%u; share_devid=%u; share_id=%u; share_sdid=%u)\n",
                    devid, map_node->shid_map_node_info.share_id,
                    map_node->shid_map_node_info.share_sdid, map_node->shid_map_node_info.share_devid);
            }
#endif
        }
        devmm_share_id_map_node_destroy(svm_proc, map_node->shid_map_node_info.devid, map_node);
        devmm_try_cond_resched(&stamp);
    }
}


void devmm_share_id_map_node_destroy_all(struct devmm_svm_process *svm_proc)
{
    u32 stamp = (u32)ka_jiffies;
    u32 devid;

    for (devid = 0; devid < DEVMM_MAX_DEVICE_NUM; devid++) {
        devmm_share_id_map_node_destroy_by_devid(svm_proc, devid, true);
        devmm_try_cond_resched(&stamp);
    }
}

static void devmm_share_id_map_node_release(ka_kref_t *kref)
{
    struct devmm_share_id_map_node *map_node = ka_container_of(kref, struct devmm_share_id_map_node, ref);

    devmm_kvfree_ex(map_node);
}

void devmm_share_id_map_node_put(struct devmm_share_id_map_node *map_node)
{
    ka_base_kref_put(&map_node->ref, devmm_share_id_map_node_release);
}

struct devmm_share_id_map_node *devmm_share_id_map_node_get(struct devmm_svm_process *svm_proc, u32 devid, int id)
{
    struct devmm_svm_proc_master *master_data = svm_proc->priv_data;
    struct devmm_share_id_map_mng *mng = devmm_get_share_id_map_mng(master_data, devid);
    struct devmm_share_id_map_node *map_node = NULL;
    ka_rb_node_t *node = NULL;

    ka_task_down_read(&mng->sem);
    node = devmm_rb_search(&mng->rbtree, (u64)id, rb_handle_of_share_id_map_node);
    if (node != NULL) {
        map_node = ka_base_rb_entry(node, struct devmm_share_id_map_node, proc_node);
        ka_base_kref_get(&map_node->ref);
    }
    ka_task_up_read(&mng->sem);
    return map_node;
}

int devmm_share_agent_blk_set_pre_release(u32 devid, int share_id);
void devmm_share_id_map_node_destroy(struct devmm_svm_process *svm_proc,
    u32 devid, struct devmm_share_id_map_node *map_node)
{
    struct devmm_svm_proc_master *master_data = svm_proc->priv_data;
    struct devmm_share_id_map_mng *mng = devmm_get_share_id_map_mng(master_data, devid);

    ka_task_down_write(&mng->sem);
    devmm_rb_erase(&mng->rbtree, &map_node->proc_node);
    ka_task_up_write(&mng->sem);

    if (map_node->blk_type == SVM_PYH_ADDR_BLK_EXPORT_TYPE) {
        devmm_share_agent_blk_set_pre_release(map_node->shid_map_node_info.devid,
            map_node->shid_map_node_info.share_id);
    }

    ka_base_kref_put(&map_node->ref, devmm_share_id_map_node_release);
}

static struct devmm_share_id_map_node *devmm_share_id_map_node_create(struct devmm_svm_process *svm_proc,
    struct devmm_shid_map_node_info *info, u32 blk_type)
{
    struct devmm_svm_proc_master *master_data = svm_proc->priv_data;
    struct devmm_share_id_map_mng *mng = devmm_get_share_id_map_mng(master_data, info->devid);
    struct devmm_share_id_map_node *map_node = NULL;
    int ret;

    map_node = devmm_kvzalloc_ex(sizeof(struct devmm_share_id_map_node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (map_node == NULL) {
        devmm_drv_err("Alloc devmm_share_id_map_node fail.\n");
        return NULL;
    }
    ka_base_kref_init(&map_node->ref);
    map_node->shid_map_node_info = *info;
    map_node->blk_type = blk_type;
    map_node->hostpid = svm_proc->process_id.hostpid;

    ka_task_down_write(&mng->sem);
    ret = devmm_rb_insert(&mng->rbtree, &map_node->proc_node, rb_handle_of_share_id_map_node);
    ka_task_up_write(&mng->sem);
    if (ret != 0) {
        devmm_drv_err("Current proc export or import repeatly. (devid=%u; id=%d; blk_type=%u)\n",
            info->devid, info->id, blk_type);
        devmm_kvfree_ex(map_node);
        map_node = NULL;
    }
    return map_node;
}

static u64 rb_handle_of_share_agent_blk_node(ka_rb_node_t *node)
{
    struct devmm_share_phy_addr_agent_blk *blk = ka_base_rb_entry(node, struct devmm_share_phy_addr_agent_blk,
        dev_res_mng_node);
    return (u64)blk->share_id;
}

static void devmm_share_agent_blk_node_uninit(struct devmm_share_phy_addr_agent_blk *blk)
{
    devmm_ref_server_occupier_mng_uninit(&blk->server_occupier_mng);
}

static void _devmm_share_agent_blk_release(struct devmm_share_phy_addr_agent_blk *blk)
{
    devmm_pid_list_erase_all(&blk->pid_list_mng);
    devmm_share_agent_blk_node_uninit(blk);
    devmm_kvfree_ex(blk);
}

static void devmm_share_agent_blk_release_func(ka_rb_node_t *node)
{
    struct devmm_share_phy_addr_agent_blk *blk = ka_base_rb_entry(node, struct devmm_share_phy_addr_agent_blk,
        dev_res_mng_node);
    _devmm_share_agent_blk_release(blk);
}

void devmm_share_agent_blk_destroy_all(struct devmm_share_phy_addr_agent_blk_mng *blk_mng)
{
    ka_task_down_write(&blk_mng->rw_sem);
    devmm_rb_erase_all_node(&blk_mng->rbtree, devmm_share_agent_blk_release_func);
    ka_task_up_write(&blk_mng->rw_sem);
}

static void devmm_share_agent_blk_release(ka_kref_t *kref)
{
    struct devmm_share_phy_addr_agent_blk *blk = ka_container_of(kref, struct devmm_share_phy_addr_agent_blk, ref);

    _devmm_share_agent_blk_release(blk);
}

static void devmm_share_agent_blk_destroy(struct devmm_share_phy_addr_agent_blk_mng *blk_mng,
    struct devmm_share_phy_addr_agent_blk *blk)
{
    int ret;

    ka_task_down_write(&blk_mng->rw_sem);
    ret = devmm_rb_erase(&blk_mng->rbtree, &blk->dev_res_mng_node);
    ka_task_up_write(&blk_mng->rw_sem);
    if (ret == 0) {
        ka_base_kref_put(&blk->ref, devmm_share_agent_blk_release);
    }
}

static void devmm_share_agent_blk_node_init(struct devmm_share_mem_info *info,
    struct devmm_share_phy_addr_agent_blk *blk)
{
    blk->pre_release_flag = 0;
    ka_base_kref_init(&blk->ref);
    blk->id = info->id;
    blk->devid = info->devid;
    blk->pg_num = info->pg_num;
    blk->side = info->side;
    blk->module_id = info->module_id;
    blk->pg_type = info->pg_type;
    blk->mem_type = info->mem_type;
    devmm_share_agent_blk_status_init(blk);

    blk->share_id = info->share_id;

    blk->export_pid = devmm_get_current_pid();
    blk->pid_list_mng.rbtree = KA_RB_ROOT;
    ka_task_init_rwsem(&blk->pid_list_mng.rw_sem);
    blk->pid_list_mng.pid_cnt = 0;
    blk->pid_list_mng.need_set_wlist = true;

    blk->status = SHARE_AGENT_BLK_STATUS_OCCUPIED;
    blk->occupied_cnt = 1;

    devmm_ref_server_occupier_mng_init(&blk->server_occupier_mng);
}

static int devmm_share_agent_blk_create(struct devmm_share_mem_info *info)
{
    struct devmm_share_phy_addr_agent_blk *blk = NULL;
    struct devmm_dev_res_mng *dev_res_mng = NULL;
    struct svm_id_inst id_inst;
    int ret;

    blk = devmm_kvzalloc_ex(sizeof(struct devmm_share_phy_addr_agent_blk), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (blk == NULL) {
        devmm_drv_err("Alloc devmm_share_phy_addr_agent_blk fail.\n");
        return -ENOMEM;
    }
    devmm_share_agent_blk_node_init(info, blk);

    svm_id_inst_pack(&id_inst, blk->devid, 0);
    dev_res_mng = devmm_dev_res_mng_get(&id_inst);
    if (dev_res_mng == NULL) {
        devmm_drv_err("Get dev res mng fail. (devid=%u)\n", blk->devid);
        devmm_share_agent_blk_node_uninit(blk);
        devmm_kvfree_ex(blk);
        return -ENODEV;
    }

    ka_task_down_write(&dev_res_mng->share_agent_blk_mng.rw_sem);
    ret = devmm_rb_insert(&dev_res_mng->share_agent_blk_mng.rbtree, &blk->dev_res_mng_node,
        rb_handle_of_share_agent_blk_node);
    ka_task_up_write(&dev_res_mng->share_agent_blk_mng.rw_sem);
    devmm_dev_res_mng_put(dev_res_mng);
    if (ret != 0) {
        devmm_drv_err("Share handle already exists. (devid=%u; share_id=%d)\n",
            blk->devid, blk->share_id);
        devmm_share_agent_blk_node_uninit(blk);
        devmm_kvfree_ex(blk);
    }
    return ret;
}
#ifndef EMU_ST
static struct devmm_share_phy_addr_agent_blk *devmm_share_agent_blk_first_get(
    struct devmm_share_phy_addr_agent_blk_mng *mng)
{
    struct devmm_share_phy_addr_agent_blk *blk = NULL;
    ka_rb_node_t *node = NULL;

    ka_task_down_read(&mng->rw_sem);
    node = devmm_rb_first(&mng->rbtree);
    if (node != NULL) {
        blk = ka_base_rb_entry(node, struct devmm_share_phy_addr_agent_blk, dev_res_mng_node);
        ka_base_kref_get(&blk->ref);
    }
    ka_task_up_read(&mng->rw_sem);

    return blk;
}

static struct devmm_share_phy_addr_agent_blk *devmm_share_agent_blk_next_get(
    struct devmm_share_phy_addr_agent_blk_mng *mng, struct devmm_share_phy_addr_agent_blk *blk)
{
    struct devmm_share_phy_addr_agent_blk *next_blk = NULL;
    ka_rb_node_t *next_node = NULL;

    ka_task_down_read(&mng->rw_sem);
    next_node = devmm_rb_next(&blk->dev_res_mng_node);
    if (next_node != NULL) {
        next_blk = ka_base_rb_entry(next_node, struct devmm_share_phy_addr_agent_blk, dev_res_mng_node);
        ka_base_kref_get(&next_blk->ref);
    }
    ka_task_up_read(&mng->rw_sem);

    return next_blk;
}
#endif
static struct devmm_share_phy_addr_agent_blk *devmm_share_agent_blk_get(u32 devid, int share_id)
{
    struct devmm_share_phy_addr_agent_blk *blk = NULL;
    struct devmm_dev_res_mng *dev_res_mng = NULL;
    ka_rb_node_t *node = NULL;
    struct svm_id_inst id_inst;

    svm_id_inst_pack(&id_inst, devid, 0);
    dev_res_mng = devmm_dev_res_mng_get(&id_inst);
    if (dev_res_mng == NULL) {
        devmm_drv_err("Get dev res mng fail. (devid=%u)\n", devid);
        return NULL;
    }

    ka_task_down_read(&dev_res_mng->share_agent_blk_mng.rw_sem);
    node = devmm_rb_search(&dev_res_mng->share_agent_blk_mng.rbtree, (u64)share_id,
        rb_handle_of_share_agent_blk_node);
    if (node == NULL) {
        goto get_from_dev_fail;
    }

    blk = ka_base_rb_entry(node, struct devmm_share_phy_addr_agent_blk, dev_res_mng_node);
    ka_base_kref_get(&blk->ref);

get_from_dev_fail:
    ka_task_up_read(&dev_res_mng->share_agent_blk_mng.rw_sem);
    devmm_dev_res_mng_put(dev_res_mng);
    return blk;
}

static void devmm_share_agent_blk_put(struct devmm_share_phy_addr_agent_blk *blk)
{
    ka_base_kref_put(&blk->ref, devmm_share_agent_blk_release);
}

int devmm_share_agent_blk_put_with_share_id(u32 share_devid, int share_id, int hostpid,
    u32 devid, bool need_pid_erase, u32 dst_sdid)
{
    struct devmm_share_phy_addr_agent_blk *blk = NULL;
    struct devmm_dev_res_mng *dev_res_mng = NULL;
    struct svm_id_inst id_inst;
    bool is_blk_no_occupied = false;

    devmm_drv_debug("Share agent blk release. (devid=%u; share_id=%d)\n", share_devid, share_id);
    svm_id_inst_pack(&id_inst, share_devid, 0);
    dev_res_mng = devmm_dev_res_mng_get(&id_inst);
    if (dev_res_mng == NULL) {
        devmm_drv_err("Get dev res mng fail. (devid=%u)\n", share_devid);
        return -ENXIO;
    }

    blk = devmm_share_agent_blk_get(share_devid, share_id);
    if (blk == NULL) {
        devmm_dev_res_mng_put(dev_res_mng);
        devmm_drv_err("Share handle doesn't exist. (share_id=%d; devid=%u)\n", share_id, share_devid);
        return -EFAULT;
    }

    if (devid < DEVMM_MAX_DEVICE_NUM) {
        (void)devmm_pid_set_share_status(blk, hostpid, devid, false);
    }
    if (need_pid_erase && (hostpid != blk->export_pid)) {
        /* only erase in process release */
        devmm_pid_list_erase(&blk->pid_list_mng, hostpid);
    }

    devmm_share_agent_blk_occupied_cnt_dec(blk, dst_sdid, &is_blk_no_occupied);
    if (is_blk_no_occupied) {
        int ret = devmm_share_mem_release(blk->side, blk->devid, blk->share_id,
            (u32)blk->pg_num, SVM_PYH_ADDR_BLK_NORMAL_FREE);
        if (ret != 0) {
            devmm_drv_err("Share mem release fail. (devid=%u; share_id=%d)\n", blk->devid, blk->share_id);
        }
        devmm_share_agent_blk_destroy(&dev_res_mng->share_agent_blk_mng, blk);
    }

    devmm_share_agent_blk_put(blk);
    devmm_dev_res_mng_put(dev_res_mng);
    return 0;
}

int devmm_share_agent_blk_set_pre_release(u32 devid, int share_id)
{
    struct devmm_share_phy_addr_agent_blk *blk = NULL;

    blk = devmm_share_agent_blk_get(devid, share_id);
    if (blk == NULL) {
        return -EINVAL;
    }

    blk->pre_release_flag = 1U;
    devmm_share_agent_blk_put(blk);
    return 0;
}
#ifndef EMU_ST
static ka_atomic64_t g_remote_occupy_num = KA_BASE_ATOMIC64_INIT(0);

struct devmm_share_agent_blk_del_no_response_occupier_data {
    struct devmm_share_phy_addr_agent_blk *blk;
    bool is_blk_no_occupied;
};

static int devmm_share_agent_blk_del_no_response_occupier(
    struct devmm_ref_server_occupier *occupier, void *priv)
{
#ifdef CFG_SOC_PLATFORM_CLOUD_V2
    struct devmm_share_agent_blk_del_no_response_occupier_data *data =
        (struct devmm_share_agent_blk_del_no_response_occupier_data *)priv;
    struct devmm_share_phy_addr_agent_blk *blk = data->blk;
    u32 devid = (blk->devid == uda_get_host_id()) ? 0 : blk->devid; /* Host pa handle's sdid is got by device0. */
    u64 occupy_num = 0;
    int ret;

    ret = devdrv_s2s_npu_link_check(devid, occupier->sdid);
    if (ret != 0) {
        devmm_drv_warn("s2s link abnormal. (ret=%d; devid=%u; sdid=%u)\n", ret, devid, occupier->sdid);
        occupy_num = (u64)ka_base_atomic64_read(&occupier->occupy_num);
        devmm_share_agent_blk_occupied_cnt_sub(blk, occupy_num, &data->is_blk_no_occupied);
        ka_base_atomic64_sub(occupy_num, &occupier->occupy_num);
        ka_base_atomic64_sub(occupy_num, &g_remote_occupy_num);
    }
#endif
    return 0;
}

static void devmm_share_agent_blk_del_each_no_response_occupier(struct devmm_share_phy_addr_agent_blk *blk,
    bool *is_blk_no_occupied)
{
    struct devmm_share_agent_blk_del_no_response_occupier_data data = {.blk = blk, .is_blk_no_occupied = false};

    devmm_for_each_ref_server_occupier(&blk->server_occupier_mng, devmm_share_agent_blk_del_no_response_occupier, (void *)&data);
    *is_blk_no_occupied = data.is_blk_no_occupied;
}

static int _devmm_share_agent_blk_recycle_by_dev(struct devmm_share_phy_addr_agent_blk_mng *mng)
{
    struct devmm_share_phy_addr_agent_blk *cur_blk = NULL;
    struct devmm_share_phy_addr_agent_blk *next_blk = NULL;
    u64 num = 0;
    u32 stamp = (u32)ka_jiffies;
    bool is_blk_no_occupied = false;

    cur_blk = devmm_share_agent_blk_first_get(mng);
    while (cur_blk != NULL) {
        devmm_try_cond_resched(&stamp);
        next_blk = devmm_share_agent_blk_next_get(mng, cur_blk);

        if (cur_blk->pre_release_flag == 1U) { /* Check only the pa handles that have called halMemRelease. */
            devmm_share_agent_blk_del_each_no_response_occupier(cur_blk, &is_blk_no_occupied);
            if (is_blk_no_occupied) {
                (void)devmm_share_mem_release(cur_blk->side, cur_blk->devid,
                    cur_blk->share_id, (u32)cur_blk->pg_num, SVM_PYH_ADDR_BLK_NORMAL_FREE);
                devmm_share_agent_blk_destroy(mng, cur_blk);
                num++;
            }
        }

        devmm_share_agent_blk_put(cur_blk);
        cur_blk = next_blk;
    }

    if (num != 0) {
        devmm_drv_info("Recycle share agent blk. (num=%llu)\n", num);
    }
    return 0;
}

static int devmm_share_agent_blk_recycle_by_dev(u32 devid)
{
    struct devmm_dev_res_mng *dev_res_mng = NULL;
    struct svm_id_inst id_inst;
    int ret;

    svm_id_inst_pack(&id_inst, devid, 0);
    dev_res_mng = devmm_dev_res_mng_get(&id_inst);
    if (dev_res_mng == NULL) {
        return -ENODEV;
    }

    ret = _devmm_share_agent_blk_recycle_by_dev(&dev_res_mng->share_agent_blk_mng);
    devmm_dev_res_mng_put(dev_res_mng);
    return ret;
}

static void devmm_share_agent_blk_recycle_handle(void)
{
    u32 stamp = (u32)ka_jiffies;
    u32 devid;

    if (ka_base_atomic64_read(&g_remote_occupy_num) == 0) {
        return;
    }

    for (devid = 0; devid < DEVMM_MAX_DEVICE_NUM; devid++) {
        (void)devmm_share_agent_blk_recycle_by_dev(devid);
        devmm_try_cond_resched(&stamp);
    }
}

static int devmm_share_agent_blk_recycle_handle_register(void)
{
    svm_recycle_handle_register(devmm_share_agent_blk_recycle_handle);
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(devmm_share_agent_blk_recycle_handle_register, FEATURE_LOADER_STAGE_3);
#endif
static int devmm_get_share_devid(u32 share_logic_devid, u32 *share_devid)
{
    u32 share_vfid;

    if (share_logic_devid == uda_get_host_id()) {
        *share_devid = share_logic_devid;
        return 0;
    }

    if (share_logic_devid >= DEVMM_MAX_DEVICE_NUM) {
        devmm_drv_err("Invalid devid. (share_logic_devid=%u)\n", share_logic_devid);
        return -ENODEV;
    }
    return devmm_container_vir_to_phs_devid(share_logic_devid, share_devid, &share_vfid);
}

static int devmm_agent_mem_import(struct devmm_svm_process *svm_proc, struct devmm_devid *devids,
    struct devmm_share_mem_info *info, int *id)
{
    struct devmm_chan_mem_import msg = {{{0}}};
    u64 total_pg_num, created_num, tmp_num;
    int ret;

    if ((info->side == DEVMM_SIDE_MASTER) && (info->pg_type == MEM_NORMAL_PAGE_TYPE)) {
        devmm_drv_run_info("Not support import.\n");
        return -EOPNOTSUPP;
    }

    msg.head.msg_id = DEVMM_CHAN_MEM_IMPORT_H2D_ID;
    msg.head.process_id.hostpid = svm_proc->process_id.hostpid;
    msg.head.process_id.vfid = (u16)devids->vfid;
    msg.head.dev_id = (u16)devids->devid;
    msg.side = info->side;
    msg.share_id = info->share_id;
    msg.share_sdid = info->sdid;
    msg.host_did = info->devid;
    msg.total_pg_num = (u32)info->pg_num;
    msg.module_id = info->module_id;
    msg.pg_type = info->pg_type;
    msg.mem_type = info->mem_type;

    total_pg_num = info->pg_num;
    for (created_num = 0; created_num < total_pg_num; created_num += tmp_num) {
        tmp_num = ka_base_min((u64)DEVMM_PAGE_NUM_PER_MSG, (total_pg_num - created_num));

        msg.to_create_pg_num = (u32)tmp_num;
        msg.is_create_to_new_blk = (created_num == 0) ? 1 : 0;
        ret = devmm_chan_msg_send(&msg, sizeof(struct devmm_chan_mem_import), sizeof(struct devmm_chan_mem_import));
        if (ret != 0) {
            devmm_drv_err("Import msg send failed. (ret=%d; hostpid=%d; devid=%u; id=%d; host_did=%u)\n",
                ret, svm_proc->process_id.hostpid, devids->devid, info->share_id, info->devid);
            goto agent_mem_release;
        }
        *id = msg.id;
    }
    return 0;

agent_mem_release:
    if (created_num != 0) {
        (void)devmm_agent_mem_release(svm_proc, devids, total_pg_num, *id, SVM_PYH_ADDR_BLK_FREE_NO_PAGE);
    }
    return ret;
}

#ifdef CFG_SOC_PLATFORM_CLOUD_V2
static int devmm_s2s_host_addr_trans(u32 sdid, u64 addr, u64 *trans_addr)
{
    struct sdid_parse_info parse = {0};
    int ret;

    ret = dbl_parse_sdid(sdid, &parse);
    if (ret != 0) {
        devmm_drv_err("Parse sdid fail. (ret=%d; owner_sdid=%u;)\n", ret, sdid);
        return ret;
    }

    ret = devmm_get_s2s_host_global_addr(parse.server_id, addr, trans_addr);
    if (ret != 0) {
        devmm_drv_err("Get global addr failed. (server_id=%u; addr=0x%llx)\n", parse.server_id, addr);
        return ret;
    }

    return 0;
}

static int devmm_get_agent_target_blk_info(struct devmm_share_mem_info *info, struct devmm_phy_addr_blk *blk)
{
    struct devmm_chan_target_blk_query *query_msg = NULL;
    u64 num_pre_msg, saved_num, i, offset, msg_len;
    int ret;

    msg_len = sizeof(struct devmm_chan_target_blk_query) +
        DEVMM_P2P_PAGE_MAX_NUM_QUERY_MSG * sizeof(struct devmm_target_blk);
    query_msg = devmm_kvzalloc_ex(msg_len, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (query_msg == NULL) {
        devmm_drv_err("Kzalloc query_msg is NULL.\n");
        return -ENOMEM;
    }
    query_msg->head.dev_id = blk->attr.devid;
    query_msg->head.process_id.hostpid = info->hostpid;
    query_msg->head.process_id.vfid = 0;
    query_msg->head.msg_id = DEVMM_CHAN_GET_BLK_INFO_H2D_ID;
    query_msg->msg.share_devid = blk->attr.devid;
    query_msg->msg.share_id = info->share_id;

    offset = 0;
    for (saved_num = 0; saved_num < info->pg_num;) {
        num_pre_msg = ka_base_min((u64)DEVMM_P2P_PAGE_MAX_NUM_QUERY_MSG, (info->pg_num - saved_num));
        query_msg->msg.num = num_pre_msg;
        query_msg->msg.offset = offset;
        query_msg->head.extend_num = num_pre_msg;
        msg_len = sizeof(struct devmm_chan_target_blk_query) + num_pre_msg * sizeof(struct devmm_target_blk);

        ret = devmm_chan_msg_send(query_msg, msg_len, msg_len);
        if (ret != 0) {
            devmm_drv_err("Get_agent_target_blk failed. (ret=%d; devid=%u; saved_num=%u; pg_num=%u)\n",
                ret, query_msg->head.dev_id, saved_num, info->pg_num);
            devmm_kvfree_ex(query_msg);
            return ret;
        }

        /* record the dma num for the first msg */
        if ((saved_num == 0) && (query_msg->msg.dma_saved <= blk->addr_info.total_num)) {
            blk->dma_blk_info.saved_num = query_msg->msg.dma_saved;
        }

        for (i = 0; i < num_pre_msg; i++) {
            blk->dma_blk_info.dma_blks[offset] = query_msg->msg.blk[i].dma_blk;
            ret = devdrv_devmem_addr_d2h(blk->attr.devid, (phys_addr_t)query_msg->msg.blk[i].target_addr,
                &blk->addr_info.target_addr[offset]);
            if (ret != 0) {
                devmm_kvfree_ex(query_msg);
                devmm_drv_err("Addr trans fail. (devid=%u; sdid=%u)\n", blk->attr.devid, info->sdid);
                return ret;
            }
            offset++;
            saved_num++;
        }
    }
    blk->addr_info.saved_num += saved_num;
    devmm_kvfree_ex(query_msg);

    return 0;
}

static int devmm_get_cs_host_target_blk_info(struct devmm_share_mem_info *info, struct devmm_phy_addr_blk *blk)
{
    struct devmm_chan_target_blk_query_msg *query_msg = NULL;
    struct devmm_ipc_pod_msg_data msg;
    u64 num_pre_msg, saved_num, i, offset;
    int ret;

    query_msg = (struct devmm_chan_target_blk_query_msg *)msg.payload;
    query_msg->share_devid = info->devid;
    query_msg->share_id = info->share_id;

    msg.header.devid = blk->attr.devid;
    msg.header.cmdtype = DEVMM_GET_BLK_INFO;
    msg.header.result = 0;
    msg.header.valid = DEVMM_IPC_POD_MSG_SEND_MAGIC;

    offset = 0;
    for (saved_num = 0; saved_num < info->pg_num;) {
        num_pre_msg = ka_base_min((u64)SVM_CS_HOST_BLK_MAX_NUM, (info->pg_num - saved_num));
        /* other dev return real num */
        query_msg->num = num_pre_msg;
        query_msg->offset = offset;

        ret = devmm_s2s_msg_sync_send(blk->attr.devid, info->sdid, &msg, sizeof(struct devmm_ipc_pod_msg_data));
        if ((ret != 0) || (msg.header.result != 0) || (msg.header.valid != DEVMM_IPC_POD_MSG_RCV_MAGIC)) {
            devmm_drv_err("Send fail. (ret=%d; result=%d; valid=0x%x; devid=%u; sdid=%u)\n",
                ret, msg.header.result, msg.header.valid, blk->attr.devid, info->sdid);
            return -EFAULT;
        }

        /* record the dma num for the first msg */
        if ((saved_num == 0) && (query_msg->dma_saved <= blk->addr_info.total_num)) {
            blk->dma_blk_info.saved_num = query_msg->dma_saved;
        }

        for (i = 0; i < num_pre_msg; i++) {
            blk->dma_blk_info.dma_blks[offset] = query_msg->blk[i].dma_blk;
            ret = devmm_s2s_host_addr_trans(info->sdid,
                query_msg->blk[i].target_addr, &blk->addr_info.target_addr[offset]);
            if (ret != 0) {
                devmm_drv_err("Addr trans fail. (devid=%u; sdid=%u)\n", blk->attr.devid, info->sdid);
                return ret;
            }
            offset++;
            saved_num++;
        }
    }

    blk->addr_info.saved_num += saved_num;

    return 0;
}

int devmm_get_cs_host_chan_target_blk_info_process(u32 devid, struct devmm_ipc_pod_msg_data *msg)
{
    struct devmm_chan_target_blk_query_msg *query_msg = (struct devmm_chan_target_blk_query_msg *)msg->payload;
    return devmm_target_blk_query_pa_process(devid, query_msg, MEM_HOST_SIDE);
}

static int devmm_master_import_mem_init_in_diff_os(struct devmm_share_mem_info *info, struct devmm_phy_addr_blk *blk)
{
    int ret;

    if (info->side == DEVMM_SIDE_MASTER) {
        ret = devmm_get_cs_host_target_blk_info(info, blk);
    } else {
        ret = devmm_get_agent_target_blk_info(info, blk);
    }

    if (ret != 0) {
        return ret;
    }

    return devmm_share_phy_addr_blk_init(blk, NULL, info->pg_num, SVM_PYH_ADDR_BLK_IMPORT_TYPE);
}

static int devmm_master_import_mem_init(struct devmm_share_mem_info *info, struct devmm_phy_addr_blk *blk)
{
    u32 devid = blk->attr.devid;

    if ((info->side == DEVMM_SIDE_MASTER) && svm_is_sdid_in_local_server(devid, info->sdid)) {
        /* devid use 0 to occupy share_phy_addr_blk_mng */
        return devmm_phy_addr_blk_init_in_same_os(blk, 0, info->share_id, info->pg_num);
    } else {
        return devmm_master_import_mem_init_in_diff_os(info, blk);
    }
}

static int devmm_master_mem_import(struct devmm_svm_process *svm_proc, struct devmm_devid *devids,
    struct devmm_share_mem_info *info, int *id)
{
    struct devmm_phy_addr_blk *blk = NULL;
    struct devmm_phy_addr_attr attr = {0};
    int ret, tmp_id;

    if ((info->side != DEVMM_SIDE_MASTER) && !svm_is_sdid_in_local_server(devids->devid, info->sdid)) {
        devmm_drv_err("Remote server can't import dev side mem. (side=%u)\n", info->side, devids->devid, info->sdid);
        return -EINVAL;
    }

    attr.devid = (info->side == DEVMM_SIDE_MASTER) ? devids->devid : info->devid;
    attr.vfid = devids->vfid;
    attr.side = DEVMM_SIDE_MASTER;
    attr.module_id = info->module_id;
    attr.pg_type = (info->pg_type == MEM_GIANT_PAGE_TYPE && info->side != DEVMM_SIDE_MASTER)
        ? MEM_HUGE_PAGE_TYPE : info->pg_type;
    attr.mem_type = info->mem_type;
    attr.is_giant_page = (info->pg_type == MEM_GIANT_PAGE_TYPE) ? true : false;

    blk = devmm_phy_addr_blk_create(&svm_proc->phy_addr_blk_mng, &attr, info->pg_num, &tmp_id);
    if (blk == NULL) {
        return -ENOMEM;
    }

    ret = devmm_master_import_mem_init(info, blk);
    if (ret != 0) {
        devmm_phy_addr_blk_destroy(&svm_proc->phy_addr_blk_mng, blk);
        return ret;
    }

    *id = tmp_id;
    return 0;
}
#else
static int devmm_master_mem_import(struct devmm_svm_process *svm_proc, struct devmm_devid *devids,
    struct devmm_share_mem_info *info, int *id)
{
    return -EOPNOTSUPP;
}
#endif

static int devmm_share_mem_import(struct devmm_svm_process *svm_proc, struct devmm_devid *devids,
    struct devmm_share_mem_info *info, int *id)
{
    if (devids->devid == uda_get_host_id()) {
        return devmm_master_mem_import(svm_proc, devids, info, id);
    } else {
        return devmm_agent_mem_import(svm_proc, devids, info, id);
    }
}

static int devmm_share_mem_de_import(struct devmm_svm_process *svm_proc, struct devmm_devid *devids,
    u64 pg_num, int id)
{
    if (devids->devid == uda_get_host_id()) {
        return devmm_master_mem_release(svm_proc, pg_num, id, SVM_PYH_ADDR_BLK_FREE_NO_PAGE);
    } else {
        return devmm_agent_mem_release(svm_proc, devids, pg_num, id, SVM_PYH_ADDR_BLK_FREE_NO_PAGE);
    }
}

static void devmm_shid_map_node_info_pack(struct devmm_shid_map_node_info *info, u32 devid, int id,
    u32 share_sdid, u32 share_devid, int share_id)
{
    info->devid = devid;
    info->id = id;
    info->share_sdid = share_sdid;
    info->share_devid = share_devid;
    info->share_id = share_id;
}

static int devmm_ioctl_mem_import_local_server(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_import_para *para = &arg->data.mem_import_para;
    struct devmm_share_phy_addr_agent_blk *blk = NULL;
    struct devmm_share_id_map_node *map_node = NULL;
    struct devmm_share_mem_info share_info;
    struct devmm_shid_map_node_info info;
    bool is_blk_no_occupied = false;
    u32 share_devid;
    int ret, id = -1;

    devmm_drv_debug("Mem import enter. (devid=%u; share_sdid=%u; share_logic_devid=%u; share_id=%d)\n",
        arg->head.devid, para->share_sdid, para->share_devid, para->share_id);

    ret = devmm_get_share_devid(para->share_devid, &share_devid);
    if (ret != 0) {
        return ret;
    }

    blk = devmm_share_agent_blk_get(share_devid, para->share_id);
    if (blk == NULL) {
        devmm_drv_err("Share handle doesn't exist. (devid=%u; share_id=%d)\n", share_devid, para->share_id);
        return -EFAULT;
    }

    if (blk->export_pid == devmm_get_current_pid() && blk->devid == arg->head.devid) {
        devmm_drv_err("Export process can not import. (pid=%d; devId:%u)\n", blk->export_pid, blk->devid);
        ret = -EACCES;
        goto share_agent_blk_put;
    }

    ret = devmm_share_agent_blk_occupied_cnt_inc(blk, SVM_INVALID_SDID);
    if (ret != 0) {
        devmm_drv_err("Share handle is released. (share_id=%u; share_logic_devid=%u)\n",
            para->share_id, para->share_devid);
        goto share_agent_blk_put;
    }

    share_info.share_id = para->share_id;
    share_info.sdid = para->share_sdid;
    share_info.devid = blk->devid;
    share_info.pg_num = blk->pg_num;
    share_info.module_id = blk->module_id;
    share_info.side = blk->side;
    share_info.pg_type = blk->pg_type;
    share_info.mem_type = blk->mem_type;

    ret = devmm_share_mem_import(svm_proc, &arg->head, &share_info, &id);
    if (ret != 0) {
        goto mem_import_fail;
    }

    devmm_shid_map_node_info_pack(&info, arg->head.devid, id, para->share_sdid, share_devid, blk->share_id);
    map_node = devmm_share_id_map_node_create(svm_proc, &info, SVM_PYH_ADDR_BLK_IMPORT_TYPE);
    if (map_node == NULL) {
        ret = -EINVAL;
        goto share_id_map_node_fail;
    }

    ret = devmm_pid_set_share_status(blk, devmm_get_current_pid(), arg->head.devid, true);
    if (ret != 0) {
        devmm_drv_err("Current process not add in pid list. (pid=%d)\n", devmm_get_current_pid());
        goto set_share_status_fail;
    }

    para->id = id;
    para->module_id = blk->module_id;
    para->pg_num = blk->pg_num;
    para->pg_type = blk->pg_type;
    para->side = (info.devid == uda_get_host_id()) ? MEM_HOST_SIDE : MEM_DEV_SIDE;
    para->mem_type = blk->mem_type;
    devmm_share_agent_blk_put(blk);
    devmm_drv_debug("Mem import success. (devid=%u; share_devid=%u; share_id=%d; id=%d)\n",
        arg->head.devid, share_devid, para->share_id, id);
    return 0;

set_share_status_fail:
    devmm_share_id_map_node_destroy(svm_proc, info.devid, map_node);
share_id_map_node_fail:
    (void)devmm_share_mem_de_import(svm_proc, &arg->head, blk->pg_num, id);
mem_import_fail:
    devmm_share_agent_blk_occupied_cnt_dec(blk, SVM_INVALID_SDID, &is_blk_no_occupied);
share_agent_blk_put:
    devmm_share_agent_blk_put(blk);
    return ret;
}

#ifdef CFG_SOC_PLATFORM_CLOUD_V2
static int devmm_get_remote_share_mem_info(u32 devid, u32 share_id, u32 share_sdid, u32 share_devid,
    struct devmm_share_mem_info *share_info)
{
    struct devmm_share_mem_info *_share_info = NULL;
    struct devmm_ipc_pod_msg_data msg;
    struct spod_info info;
    u64 *dst_sdid = NULL;
    int ret;

    ret = dbl_get_spod_info(devid, &info);
    if (ret != 0) {
        devmm_drv_err("Get spod info failed. (ret=%d; devid=%u)\n", ret, devid);
        return ret;
    }

    dst_sdid = (u64 *)msg.payload;
    *dst_sdid = info.sdid;

    _share_info = (struct devmm_share_mem_info *)(msg.payload + sizeof(u64));
    _share_info->share_id = share_id;
    _share_info->devid = share_devid;

    msg.header.devid = devid;
    msg.header.cmdtype = DEVMM_GET_SHARE_INFO;
    msg.header.result = 0;
    msg.header.valid = DEVMM_IPC_POD_MSG_SEND_MAGIC;

    ret = devmm_s2s_msg_sync_send(devid, share_sdid, &msg, sizeof(struct devmm_ipc_pod_msg_data));
    if ((ret != 0) || (msg.header.result != 0) || (msg.header.valid != DEVMM_IPC_POD_MSG_RCV_MAGIC)) {
        devmm_drv_err("Send fail. (ret=%d; result=%d; valid=0x%x; devid=%u; sdid=%u)\n",
            ret, msg.header.result, msg.header.valid, devid, share_sdid);
        return -EFAULT;
    }

    *share_info = *_share_info;
    share_info->sdid = share_sdid;

    return 0;
}

int devmm_get_remote_share_mem_info_process(u32 devid, struct devmm_ipc_pod_msg_data *msg)
{
    struct devmm_share_mem_info *share_info = NULL;
    struct devmm_share_phy_addr_agent_blk *blk = NULL;
    u64 dst_sdid;
    int ret;

    dst_sdid = *(u64 *)msg->payload;
    share_info = (struct devmm_share_mem_info *)(msg->payload + sizeof(u64));

    if (share_info->devid >= DEVMM_MAX_DEVICE_NUM) {
        devmm_drv_err("Invalid para. (devid=%u; share_id=%d)\n", share_info->devid, share_info->share_id);
        return -EINVAL;
    }

    blk = devmm_share_agent_blk_get(share_info->devid, share_info->share_id);
    if (blk == NULL) {
        devmm_drv_err("Get blk failed. (devid=%u; share_id=%d)\n", share_info->devid, share_info->share_id);
        return -EINVAL;
    }

    ret = devmm_share_agent_blk_occupied_cnt_inc(blk, dst_sdid);
    if (ret != 0) {
        devmm_drv_err("Share handle is released. (devid=%u; share_id=%u)\n",
            share_info->devid, share_info->share_id);
        devmm_share_agent_blk_put(blk);
        return ret;
    }

    share_info->pg_num = blk->pg_num;
    share_info->module_id = blk->module_id;
    share_info->side = blk->side;
    share_info->pg_type = blk->pg_type;
    share_info->mem_type = blk->mem_type;

    devmm_share_agent_blk_put(blk);
#ifndef EMU_ST
    ka_base_atomic64_inc(&g_remote_occupy_num);
#endif
    devmm_drv_debug("Get remote share mem. (dst_sdid=%llu; devid=%u; share_id=%u)\n", dst_sdid, share_info->devid, share_info->share_id);

    return 0;
}

int devmm_put_remote_share_mem_info(u32 devid, u32 share_id, u32 share_sdid, u32 share_devid)
{
    struct devmm_share_mem_info *_share_info = NULL;
    struct devmm_ipc_pod_msg_data msg;
    struct spod_info info;
    u64 *dst_sdid = NULL;
    int ret;

    ret = dbl_get_spod_info(devid, &info);
    if (ret != 0) {
        devmm_drv_err("Get spod info failed. (ret=%d; devid=%u)\n", ret, devid);
        return ret;
    }

    dst_sdid = (u64 *)msg.payload;
    *dst_sdid = info.sdid;

    _share_info = (struct devmm_share_mem_info *)(msg.payload + sizeof(u64));
    _share_info->share_id = share_id;
    _share_info->devid = share_devid;

    msg.header.devid = devid;
    msg.header.cmdtype = DEVMM_PUT_SHARE_INFO;
    msg.header.result = 0;
    msg.header.valid = DEVMM_IPC_POD_MSG_SEND_MAGIC;

    ret = devmm_s2s_msg_sync_send(devid, share_sdid, &msg, sizeof(struct devmm_ipc_pod_msg_data));
    if ((ret != 0) || (msg.header.result != 0) || (msg.header.valid != DEVMM_IPC_POD_MSG_RCV_MAGIC)) {
        devmm_drv_err("Send fail. (ret=%d; result=%d; valid=0x%x; devid=%u; sdid=%u)\n",
            ret, msg.header.result, msg.header.valid, devid, share_sdid);
        return -EFAULT;
    }

    return 0;
}

int devmm_put_remote_share_mem_info_process(u32 devid, struct devmm_ipc_pod_msg_data *msg)
{
    struct devmm_share_mem_info *share_info = NULL;
    u64 dst_sdid;
    int ret;

    dst_sdid = *(u64 *)msg->payload;
    share_info = (struct devmm_share_mem_info *)(msg->payload + sizeof(u64));

    if (share_info->devid >= DEVMM_MAX_DEVICE_NUM) {
        devmm_drv_err("Invalid para. (devid=%u; share_id=%d)\n", share_info->devid, share_info->share_id);
        return -EINVAL;
    }

    ret = devmm_share_agent_blk_put_with_share_id(share_info->devid, share_info->share_id,
        0, DEVMM_MAX_DEVICE_NUM, false, dst_sdid); /* hostpid and devid not care */
    if (ret != 0) {
        devmm_drv_err("Put share id failed. (devid=%u; share_id=%u)\n", share_info->devid, share_info->share_id);
        return ret;
    }
#ifndef EMU_ST
    ka_base_atomic64_dec(&g_remote_occupy_num);
#endif
    devmm_drv_debug("Put remote share mem. (dst_sdid=%llu; devid=%u; share_id=%u)\n", dst_sdid, share_info->devid, share_info->share_id);

    return 0;
}

static int devmm_ioctl_mem_import_remote_server(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_import_para *para = &arg->data.mem_import_para;
    struct devmm_share_id_map_node *map_node = NULL;
    struct devmm_share_mem_info share_info;
    struct devmm_shid_map_node_info info;
    int ret, id = -1;
    devmm_drv_debug("Mem import enter. (devid=%u; share_sdid=%u; share_logic_devid=%u; share_id=%d)\n",
        arg->head.devid, para->share_sdid, para->share_phy_devid, para->share_id);

    ret = devmm_get_remote_share_mem_info(arg->head.devid,
        para->share_id, para->share_sdid, para->share_phy_devid, &share_info);
    if (ret != 0) {
        devmm_drv_err("Get remote share info failed. (devid=%u; share_sdid=%u; share_devid=%u; share_id=%d)\n",
            arg->head.devid, para->share_sdid, para->share_phy_devid, para->share_id);
        return ret;
    }

    ret = devmm_share_mem_import(svm_proc, &arg->head, &share_info, &id);
    if (ret != 0) {
        (void)devmm_put_remote_share_mem_info(arg->head.devid, para->share_id, para->share_sdid, para->share_phy_devid);
        devmm_drv_err("Import failed. (devid=%u; share_sdid=%u; share_devid=%u; share_id=%d)\n",
            arg->head.devid, para->share_sdid, para->share_phy_devid, para->share_id);
        return ret;
    }

    devmm_shid_map_node_info_pack(&info, arg->head.devid, id, para->share_sdid, para->share_phy_devid, para->share_id);
    map_node = devmm_share_id_map_node_create(svm_proc, &info, SVM_PYH_ADDR_BLK_IMPORT_TYPE);
    if (map_node == NULL) {
        (void)devmm_share_mem_de_import(svm_proc, &arg->head, share_info.pg_num, id);
        (void)devmm_put_remote_share_mem_info(arg->head.devid, para->share_id, para->share_sdid, para->share_phy_devid);
        return -EINVAL;
    }

    para->id = id;
    para->module_id = share_info.module_id;
    para->pg_num = share_info.pg_num;
    para->pg_type = share_info.pg_type;
    para->side = (info.devid == uda_get_host_id()) ? MEM_HOST_SIDE : MEM_DEV_SIDE;
    para->mem_type = share_info.mem_type;

    return 0;
}
#endif

static int devmm_import_para_check(struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_import_para *para = &arg->data.mem_import_para;

    if (para->handle_type >= MEM_HANDLE_TYPE_MAX) {
        devmm_drv_run_info("Invalid handle_type. (handle_type=%d)\n", para->handle_type);
        return -EOPNOTSUPP;
    }

#ifdef CFG_SOC_PLATFORM_CLOUD_V2
    if (svm_is_sdid_in_local_server(arg->head.devid, para->share_sdid) == false) {
        if (arg->head.devid == uda_get_host_id()) {
            devmm_drv_run_info("Invalid direction D2H&H2H. (share_phy_devid=%u)\n", para->share_phy_devid);
            return -EOPNOTSUPP;
        }
    }
#endif
    return 0;
}

static int _devmm_ioctl_mem_import(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
#ifdef CFG_SOC_PLATFORM_CLOUD_V2
    struct devmm_mem_import_para *para = &arg->data.mem_import_para;

    if ((para->handle_type == MEM_HANDLE_TYPE_NONE) ||
        svm_is_sdid_in_local_server(arg->head.devid, para->share_sdid)) {
        return devmm_ioctl_mem_import_local_server(svm_proc, arg);
    } else {
        return devmm_ioctl_mem_import_remote_server(svm_proc, arg);
    }
#else
    return devmm_ioctl_mem_import_local_server(svm_proc, arg);
#endif
}

int devmm_ioctl_mem_import(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    int ret;

    ret = devmm_import_para_check(arg);
    if (ret != 0) {
        return ret;
    }
    return _devmm_ioctl_mem_import(svm_proc, arg);
}

static int devmm_master_mem_export(struct devmm_svm_process *svm_proc, struct devmm_devid *devids,
    struct devmm_mem_export_para *para, struct devmm_share_mem_info *info)
{
    struct devmm_phy_addr_blk *blk = NULL;
    int ret, share_id;
    uint32_t devid = 0; /* devid use 0 to occupy share_phy_addr_blk_mng */

    blk = devmm_phy_addr_blk_get(&svm_proc->phy_addr_blk_mng, para->id);
    if (blk == NULL) {
        devmm_drv_err("Get phy_addr_blk failed. (id=%d)\n", para->id);
        return -EINVAL;
    }
    if (blk->type != SVM_PYH_ADDR_BLK_NORMAL_TYPE) {
        devmm_phy_addr_blk_put(blk);
        /* The log cannot be modified, because in the failure mode library. */
        devmm_drv_err("Blk has been shared. (id=%d)\n", para->id);
        return -EINVAL;
    }

    ret = devmm_share_phy_addr_blk_create(blk, devid, &share_id);
    if (ret == 0) {
        info->hostpid = svm_proc->process_id.hostpid;
        info->devid = devids->devid;
        info->id = para->id;
        info->share_id = share_id;
        info->pg_num = blk->pg_num;
        info->side = blk->attr.side;
        info->module_id = blk->attr.module_id;
        info->pg_type = blk->attr.pg_type;
        info->mem_type = blk->attr.mem_type;

        blk->type = SVM_PYH_ADDR_BLK_EXPORT_TYPE;
    }
    devmm_phy_addr_blk_put(blk);
    return ret;
}

static void devmm_share_mem_info_pack(struct devmm_share_mem_info *info,
    struct devmm_devid *devids, struct devmm_mem_export_para *para, struct devmm_chan_mem_export *msg)
{
    info->hostpid = msg->head.process_id.hostpid;
    info->devid = devids->devid;
    info->id = para->id;
    info->share_id = msg->share_id;
    info->pg_num = msg->pg_num;
    info->side = msg->side;
    info->module_id = msg->module_id;
    info->pg_type = msg->pg_type;
    info->mem_type = msg->mem_type;
}

static int devmm_agent_mem_export(struct devmm_svm_process *svm_proc, struct devmm_devid *devids,
    struct devmm_mem_export_para *para, struct devmm_share_mem_info *info)
{
    struct devmm_chan_mem_export msg = {{{0}}};
    int ret;

    msg.head.msg_id = DEVMM_CHAN_MEM_EXPORT_H2D_ID;
    msg.head.process_id.hostpid = svm_proc->process_id.hostpid;
    msg.head.process_id.vfid = (u16)devids->vfid;
    msg.head.dev_id = (u16)devids->devid;
    msg.id = para->id;
    ret = devmm_chan_msg_send(&msg, sizeof(struct devmm_chan_mem_export), sizeof(struct devmm_chan_mem_export));
    if (ret != 0) {
        devmm_drv_err("Msg send failed. (ret=%d; hostpid=%d; devid=%u; id=%d)\n",
            ret, info->hostpid, info->devid, info->id);
        return ret;
    }

    devmm_share_mem_info_pack(info, devids, para, &msg);
    return 0;
}

static int devmm_share_mem_export(struct devmm_svm_process *svm_proc, struct devmm_devid *devids,
    struct devmm_mem_export_para *para, struct devmm_share_mem_info *info)
{
    if (para->side == DEVMM_SIDE_MASTER) {
        return devmm_master_mem_export(svm_proc, devids, para, info);
    } else {
        return devmm_agent_mem_export(svm_proc, devids, para, info);
    }
}

int devmm_ioctl_mem_export(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_export_para *para = &arg->data.mem_export_para;
    struct devmm_share_id_map_node *map_node = NULL;
    struct devmm_share_mem_info info = {0};
    struct devmm_shid_map_node_info node_info;
    int ret;

    if ((para->side == MEM_HOST_SIDE) && (arg->head.devid != uda_get_host_id())) {
        devmm_drv_err("Invalid side devid. (side=%u; devid=%u)\n", para->side, arg->head.devid);
        return -EINVAL;
    }

    devmm_drv_debug("Mem export enter. (logic_devid=%u; devid=%u; id=%d)\n",
        arg->head.logical_devid, arg->head.devid, para->id);
    ret = devmm_share_mem_export(svm_proc, &arg->head, para, &info);
    if (ret != 0) {
        return ret;
    }

    devmm_shid_map_node_info_pack(&node_info, info.devid, info.id, 0, info.devid, info.share_id);
    map_node = devmm_share_id_map_node_create(svm_proc, &node_info, SVM_PYH_ADDR_BLK_EXPORT_TYPE);
    if (map_node == NULL) {
        (void)devmm_share_mem_release(para->side, info.devid, info.share_id, (u32)info.pg_num,
            SVM_PYH_ADDR_BLK_FREE_NO_PAGE);
        return -ENOMEM;
    }

    ret = devmm_share_agent_blk_create(&info);
    if (ret != 0) {
        devmm_share_id_map_node_destroy(svm_proc, info.devid, map_node);
        (void)devmm_share_mem_release(para->side, info.devid, info.share_id, (u32)info.pg_num,
            SVM_PYH_ADDR_BLK_FREE_NO_PAGE);
        return ret;
    }
    devmm_drv_debug("Mem export success. (hostpid=%d; devid=%u; id=%d; share_id=%d)\n",
        info.hostpid, info.devid, info.id, info.share_id);
    para->share_id = info.share_id;
    return 0;
}

static int devmm_master_share_mem_release(u32 share_devid, int share_id, u32 total_pg_num, u32 free_type)
{
    uint32_t devid = 0; /* devid use 0 to occupy share_phy_addr_blk_mng */
    struct devmm_phy_addr_blk_mng *share_mng = &devmm_svm->share_phy_addr_blk_mng[devid];

    return _devmm_mem_release(NULL, share_mng, share_id, total_pg_num, free_type);
}

static int devmm_agent_share_mem_release(u32 share_devid, int share_id, u32 total_pg_num, u32 free_type)
{
    struct devmm_chan_mem_release msg = {{{0}}};

    msg.head.msg_id = DEVMM_CHAN_SHARE_MEM_RELEASE_H2D_ID;
    msg.head.dev_id = (u16)share_devid;
    msg.id = share_id;
    msg.free_type = free_type;
    msg.to_free_pg_num = total_pg_num;
    return devmm_agent_mem_release_public(&msg);
}

static int devmm_share_mem_release(int side, u32 share_devid, int share_id, u32 total_pg_num, u32 free_type)
{
    if (side == MEM_HOST_SIDE) {
        return devmm_master_share_mem_release(share_devid, share_id, total_pg_num, free_type);
    } else {
        return devmm_agent_share_mem_release(share_devid, share_id, total_pg_num, free_type);
    }
}

static int devmm_set_pid_para_check(struct devmm_mem_set_pid_para *para)
{
    if (para->pid_num == 0) {
        devmm_drv_err("Pid_num is zero.\n");
        return -EINVAL;
    }

    if (para->pid_num > DEVMM_SHARE_MEM_MAX_PID_CNT) {
        devmm_drv_err("Pid_num is invalid. (pid_num=%u)\n", para->pid_num);
        return -EINVAL;
    }

    if (para->pid_list == NULL) {
        devmm_drv_err("Pid list is NULL.\n");
        return -EINVAL;
    }
    return 0;
}

static int devmm_set_pids(struct devmm_share_phy_addr_agent_blk *blk, int *pid_list, u32 pid_num)
{
    u32 stamp = (u32)ka_jiffies;
    u32 set_pid_num = 0;
    u32 i;

    for (i = 0; i < pid_num; i++) {
        int ret;

        if (pid_list[i] == 0) {
            devmm_drv_warn("Invalid pid will not be set to wlist. (pid=%d)\n", pid_list[i]);
            continue;
        } else {
            ret = devmm_pid_list_insert(&blk->pid_list_mng, pid_list[i]);
            devmm_try_cond_resched(&stamp);
        }
        if (ret != 0) {
            /* cannot erase pid, will delete pid which is set.
               devmm_pid_list_erase_all will recycle resource. */
            devmm_drv_err("Set pid fail. (export_pid=%d; ret=%d; i=%u; pid=%d)\n",
                blk->export_pid, ret, i, pid_list[i]);
            return ret;
        }
        set_pid_num++;
    }

    if (ka_unlikely(set_pid_num == 0)) {
        return -EINVAL;
    }

    return 0;
}

int devmm_ioctl_mem_set_pid(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_set_pid_para *para = &arg->data.mem_set_pid_para;
    struct devmm_share_phy_addr_agent_blk *blk = NULL;
    int *pid_list = NULL;
    u64 size;
    int ret;

    ret = devmm_set_pid_para_check(para);
    if (ret != 0) {
        return ret;
    }

    size = para->pid_num * (u64)sizeof(int);
    pid_list = devmm_kvzalloc_ex(size, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (pid_list == NULL) {
        devmm_drv_err("Alloc pid_list fail.\n");
        return -ENOMEM;
    }

    if (ka_base_copy_from_user(pid_list, (void __ka_user *)para->pid_list, size) != 0) {
        devmm_drv_err("Copy_from_user fail. (size=%llu)\n", size);
        ret = -EFAULT;
        goto free_pid_list;
    }

    blk = devmm_share_agent_blk_get(arg->head.devid, para->share_id);
    if (blk == NULL) {
        devmm_drv_err("Share handle doesn't exist. (devid=%u; share_id=%d)\n", arg->head.devid, para->share_id);
        /* EFAULT cann't change, user will get invalid handle err code. */
        ret = -EFAULT;
        goto free_pid_list;
    }

    if (blk->export_pid != devmm_get_current_pid()) {
        devmm_drv_err("Not export process, can not set pid. (export_pid=%d)\n", blk->export_pid);
        ret = -EACCES;
        goto blk_put;
    }

    ret = devmm_set_pids(blk, pid_list, para->pid_num);

blk_put:
    devmm_share_agent_blk_put(blk);
free_pid_list:
    devmm_kvfree_ex(pid_list);
    return ret;
}

static int devmm_share_mem_set_attr_no_wlist_in_server(struct devmm_share_phy_addr_agent_blk *blk,
    struct devmm_mem_set_attr_para *para)
{
    struct devmm_pid_list_mng *pid_mng = &blk->pid_list_mng;

    if ((para->attr.enableFlag != SHR_HANDLE_WLIST_ENABLE) && (para->attr.enableFlag != SHR_HANDLE_NO_WLIST_ENABLE)) {
        devmm_drv_err("Invalid enableFlag. (enableFlag=%u)\n", para->attr.enableFlag);
        return -EINVAL;
    }

    if (blk->export_pid != devmm_get_current_pid()) {
        devmm_drv_err("Not export process, can't set attr. (export_pid=%d)\n", blk->export_pid);
        return -EPERM;
    }

    if (para->attr.enableFlag == SHR_HANDLE_WLIST_ENABLE) {
        devmm_drv_run_info("Not support wlist enable now.\n");
        return -EOPNOTSUPP;
    }

    ka_task_down_write(&pid_mng->rw_sem);
    /* Not allow to set attr if had called halMemSetPidToShareableHandle */
    if ((para->attr.enableFlag == SHR_HANDLE_NO_WLIST_ENABLE) && ((pid_mng->need_set_wlist) && (pid_mng->pid_cnt != 0))) {
        devmm_drv_err("Had set pid not allow to set attr. (devid=%u; share_id=%d; wlist_num=%u)\n",
            blk->devid, blk->share_id, pid_mng->pid_cnt);
        ka_task_up_write(&pid_mng->rw_sem);
        return -EPERM;
    }

    pid_mng->need_set_wlist = (para->attr.enableFlag == SHR_HANDLE_NO_WLIST_ENABLE) ? false : true;
    devmm_drv_run_info("No wlist in server attr set succ. (devid=%u; share_id=%d; need_set_wlist=%u)\n",
        blk->devid, blk->share_id, pid_mng->need_set_wlist);

    ka_task_up_write(&pid_mng->rw_sem);
    return 0;
}

static int(*devmm_share_mem_set_attr_handlers[SHR_HANDLE_ATTR_TYPE_MAX])
    (struct devmm_share_phy_addr_agent_blk *blk, struct devmm_mem_set_attr_para *para) = {
        [SHR_HANDLE_ATTR_NO_WLIST_IN_SERVER] = devmm_share_mem_set_attr_no_wlist_in_server,
};

int devmm_ioctl_mem_set_attr(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_set_attr_para *para = &arg->data.mem_set_attr_para;
    struct devmm_share_phy_addr_agent_blk *blk = NULL;
    int ret;

    devmm_drv_debug("Mem set attr start. (devid=%u; share_id=%d; type=%u)\n",
        arg->head.devid, para->share_id, para->type);

    if ((para->type >= SHR_HANDLE_ATTR_TYPE_MAX) || (devmm_share_mem_set_attr_handlers[para->type] == NULL)) {
        return -EOPNOTSUPP;
    }

    blk = devmm_share_agent_blk_get(arg->head.devid, para->share_id);
    if (blk == NULL) {
        devmm_drv_err("Share handle doesn't exist. (devid=%u; share_id=%d)\n", arg->head.devid, para->share_id);
        return -EFAULT;
    }

    ret = devmm_share_mem_set_attr_handlers[para->type](blk, para);
    devmm_share_agent_blk_put(blk);
    devmm_drv_debug("Mem set attr end. (ret=%d)\n", ret);

    return ret;
}

static int devmm_share_mem_get_attr_no_wlist_in_server(struct devmm_share_phy_addr_agent_blk *blk,
    struct devmm_mem_get_attr_para *para)
{
    struct devmm_pid_list_mng *pid_mng = &blk->pid_list_mng;

    ka_task_down_read(&pid_mng->rw_sem);
    para->attr.enableFlag = pid_mng->need_set_wlist ? SHR_HANDLE_WLIST_ENABLE : SHR_HANDLE_NO_WLIST_ENABLE;
    ka_task_up_read(&pid_mng->rw_sem);

    devmm_drv_debug("No wlist in server attr get succ. (share_devid=%u; share_id=%d; enableFlag=%u)\n",
        blk->devid, para->share_id, para->attr.enableFlag);
    return 0;
}

static int(*devmm_share_mem_get_attr_handlers[SHR_HANDLE_ATTR_TYPE_MAX])
    (struct devmm_share_phy_addr_agent_blk *blk, struct devmm_mem_get_attr_para *para) = {
        [SHR_HANDLE_ATTR_NO_WLIST_IN_SERVER] = devmm_share_mem_get_attr_no_wlist_in_server,
};

int devmm_ioctl_mem_get_attr(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_get_attr_para *para = &arg->data.mem_get_attr_para;
    struct devmm_share_phy_addr_agent_blk *blk = NULL;
    u32 share_devid;
    int ret;

    devmm_drv_debug("Mem get attr start. (log_devid=%u; share_id=%d; type=%u)\n",
        para->share_devid, para->share_id, para->type);

    if ((para->type >= SHR_HANDLE_ATTR_TYPE_MAX) || (devmm_share_mem_get_attr_handlers[para->type] == NULL)) {
        return -EOPNOTSUPP;
    }

    ret = devmm_get_share_devid(para->share_devid, &share_devid);
    if (ret != 0) {
        devmm_drv_err("Get share_devid failed. (log_devid=%u; share_id=%d)\n", para->share_devid, para->share_id);
        return ret;
    }

    blk = devmm_share_agent_blk_get(share_devid, para->share_id);
    if (blk == NULL) {
        devmm_drv_err("Share handle doesn't exist. (devid=%u; share_id=%d)\n", share_devid, para->share_id);
        return -EFAULT;
    }

    ret = devmm_share_mem_get_attr_handlers[para->type](blk, para);
    devmm_share_agent_blk_put(blk);
    devmm_drv_debug("Mem get attr end. (ret=%d)\n", ret);

    return ret;
}

int devmm_ioctl_mem_get_info(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_get_info_para *para = &arg->data.mem_get_info_para;
    struct devmm_share_phy_addr_agent_blk *blk = NULL;
    u32 share_devid;
    int ret;

    devmm_drv_debug("Mem get share mem info enter. (log_devid=%u; share_id=%d)\n", para->share_devid, para->share_id);

    ret = devmm_get_share_devid(para->share_devid, &share_devid);
    if (ret != 0) {
        devmm_drv_err("Get share_devid failed. (log_devid=%u; share_id=%d)\n", para->share_devid, para->share_id);
        return ret;
    }

    blk = devmm_share_agent_blk_get(share_devid, para->share_id);
    if (blk == NULL) {
        devmm_drv_err("Share handle doesn't exist. (devid=%u; share_id=%d)\n", share_devid, para->share_id);
        return -EFAULT;
    }

    para->info.phyDevid = blk->devid;
    devmm_share_agent_blk_put(blk);
    devmm_drv_debug("Mem get share mem info end.\n");

    return 0;
}

int devmm_chan_target_blk_query_pa_process(struct devmm_svm_process *svm_proc,
    struct devmm_svm_heap *heap, void *msg, u32 *ack_len)
{
    struct devmm_chan_target_blk_query *ack_msg = (struct devmm_chan_target_blk_query *)msg;
    int ret = devmm_target_blk_query_pa_process(ack_msg->head.dev_id, &ack_msg->msg, MEM_HOST_SIDE);
    if (ret == 0) {
        *ack_len = (u32)(sizeof(struct devmm_chan_target_blk_query) +
            sizeof(struct devmm_target_blk) * ack_msg->msg.num);
    }

    return ret;
}
