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
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/spinlock_types.h>
#include <linux/rbtree.h>

#include "devmm_proc_info.h"
#include "devmm_common.h"
#include "devmm_page_cache.h"
#include "devmm_register_dma.h"
#include "devmm_proc_mem_copy.h"
#include "svm_srcu_work.h"
#include "svm_proc_mng.h"
#include "svm_proc_fs.h"
#include "svm_shmem_interprocess.h"
#include "svm_hot_reset.h"
#include "svm_task_dev_res_mng.h"
#include "svm_master_dma_desc_mng.h"
#include "svm_master_mem_share.h"
#include "svm_mem_stats.h"
#include "svm_master_proc_mng.h"
#include "svm_phy_addr_blk_mng.h"

#ifndef EMU_ST
#define SVM_ASYNC_TASK_MAX_ID 65535
#else
#define SVM_ASYNC_TASK_MAX_ID 10
#endif

u32 devmm_get_proc_dev_async_task_cnt(struct devmm_svm_process *svm_proc, u32 devid)
{
    struct devmm_svm_proc_master *master_data = (struct devmm_svm_proc_master *)svm_proc->priv_data;
    return ka_base_atomic_read(&(master_data->async_copy_record.task_cnt[devid]));
}

bool devmm_proc_dev_async_task_is_empty(struct devmm_svm_process *svm_proc, u32 devid)
{
    struct devmm_svm_proc_master *master_data = (struct devmm_svm_proc_master *)svm_proc->priv_data;
    if (ka_base_atomic_read(&(master_data->async_copy_record.task_cnt[devid])) > 0) {
        return false;
    }
    return true;
}

bool devmm_proc_async_task_is_empty(struct devmm_svm_process *svm_proc)
{
    u32 i;

    for (i = 0; i < DEVMM_MAX_DEVICE_NUM; i++) {
        if ((devmm_is_dma_link_abnormal(i) == false) &&
            (devmm_proc_dev_async_task_is_empty(svm_proc, i) == false)) {
            return false;
        }
    }

    return true;
}

void devmm_proc_dev_set_async_allow(struct devmm_svm_process *svm_proc, u32 devid, bool value)
{
    struct devmm_svm_proc_master *master_data = (struct devmm_svm_proc_master *)svm_proc->priv_data;

    master_data->async_copy_record.is_async_allow[devid] = value;
}

bool devmm_proc_dev_is_async_allow(struct devmm_svm_process *svm_proc, u32 devid)
{
    struct devmm_svm_proc_master *master_data = (struct devmm_svm_proc_master *)svm_proc->priv_data;

    return master_data->async_copy_record.is_async_allow[devid];
}

static inline void devmm_proc_init_async_task_record(struct devmm_svm_proc_master *master_data)
{
    u32 i;

    for (i = 0; i < SVM_MAX_AGENT_NUM; i++) {
        ka_base_atomic_set(&(master_data->async_copy_record.task_cnt[i]), 0);
        master_data->async_copy_record.is_async_allow[i] = false;
    }

    devmm_idr_init(&master_data->async_copy_record.task_idr, SVM_ASYNC_TASK_MAX_ID);
}

static inline void devmm_proc_destroy_async_task_record(struct devmm_svm_process *svm_proc)
{
    struct devmm_svm_proc_master *master_data = (struct devmm_svm_proc_master *)svm_proc->priv_data;

    devmm_idr_destroy(&master_data->async_copy_record.task_idr, devmm_kvfree);
}

static inline void devmm_proc_init_convert_dma_tree(struct devmm_svm_proc_master *master_data)
{
    u32 i;

    for (i = 0; i < DEVMM_CONVERT_TREE_NUM; i++) {
        master_data->convert_dma[i].dma_rbtree = KA_RB_ROOT;
        ka_task_spin_lock_init(&master_data->convert_dma[i].rbtree_lock);
    }
}

static void devmm_init_dma_desc_node_rb_info(struct devmm_dma_desc_node_rb_info *rb_info, u32 array_num)
{
    u32 i;

    for (i = 0; i < array_num; i++) {
        rb_info[i].root = KA_RB_ROOT;
        ka_task_spin_lock_init(&rb_info[i].spinlock);

        rb_info[i].node_num = 0;
        rb_info[i].node_peak_num = 0;
    }
}

static void devmm_init_share_id_map(struct devmm_share_id_map_mng *share_id_map_mng)
{
    u32 i;

    for (i = 0; i < SVM_MAX_AGENT_NUM; i++) {
        share_id_map_mng[i].rbtree = KA_RB_ROOT;
        ka_task_init_rwsem(&share_id_map_mng[i].sem);
    }
}

static void devmm_init_master_share_id_map(struct devmm_share_id_map_mng *share_id_map_mng)
{
    share_id_map_mng->rbtree = KA_RB_ROOT;
    ka_task_init_rwsem(&share_id_map_mng->sem);
}

static void devmm_init_dev_setup_sem(struct devmm_svm_proc_master *master_data)
{
    u32 i;

    for (i = 0; i < SVM_MAX_AGENT_NUM; i++) {
        ka_task_sema_init(&master_data->dev_setup_sem[i], 1);
    }
}

static void devmm_init_svm_proc_register_dma_mng(struct devmm_svm_proc_master *master_data)
{
    u32 i;

    for (i = 0; i < SVM_MAX_AGENT_NUM; i++) {
        master_data->register_dma_mng[i].rbtree = KA_RB_ROOT;
        ka_task_rwlock_init(&master_data->register_dma_mng[i].rbtree_rwlock);
    }
}

static void devmm_init_mem_stats_mng(struct devmm_mem_stats_va_mng *mem_stats_va_mng)
{
    u32 i;

    for (i = 0; i < SVM_MAX_AGENT_NUM; i++) {
        ka_task_sema_init(&mem_stats_va_mng[i].sem, 1);
        mem_stats_va_mng[i].kva = NULL;
        mem_stats_va_mng[i].pages = NULL;
    }
}

static void devmm_init_master_p2p_mem_mng(struct devmm_master_p2p_mem_mng *p2p_mem_mng)
{
    u32 i;

    for (i = 0; i < SVM_MAX_AGENT_NUM; i++) {
        ka_task_mutex_init(&p2p_mem_mng[i].lock);
        KA_INIT_LIST_HEAD(&p2p_mem_mng[i].list_head);
        p2p_mem_mng[i].get_cnt = 0;
        p2p_mem_mng[i].put_cnt = 0;
        p2p_mem_mng[i].free_cb_cnt = 0;
    }
}

void devmm_free_proc_priv_data(struct devmm_svm_process *svm_proc)
{
    devmm_proc_destroy_async_task_record(svm_proc);
    devmm_mem_stats_va_unmap(svm_proc);
    devmm_kvfree(svm_proc->priv_data);
    svm_proc->priv_data = NULL;
}

int devmm_svm_proc_init_private(struct devmm_svm_process *svm_proc)
{
    struct devmm_svm_proc_master *master_data = devmm_kvzalloc(sizeof(struct devmm_svm_proc_master));
    if (master_data == NULL) {
        return -ENOMEM;
    }
    devmm_init_dev_setup_sem(master_data);
    devmm_proc_init_async_task_record(master_data);
    devmm_proc_init_convert_dma_tree(master_data);
    devmm_init_task_dev_res_info(&master_data->task_dev_res_info);
    devmm_init_dma_desc_node_rb_info(master_data->dma_desc_rb_info, DMA_DESC_RB_INFO_CNT);
    devmm_init_share_id_map(master_data->share_id_map_mng);
    devmm_init_master_share_id_map(&master_data->master_share_id_map_mng);
    devmm_init_svm_proc_register_dma_mng(master_data);
    devmm_init_mem_stats_mng(master_data->mem_stats_va_mng);
    devmm_init_master_p2p_mem_mng(master_data->p2p_mem_mng);
    svm_proc->priv_data = (void *)master_data;

    return 0;
}

void devmm_svm_proc_uninit_private(struct devmm_svm_process *svm_proc)
{
}

bool devmm_svm_can_release_private(struct devmm_svm_process *svm_proc)
{
    if (devmm_proc_async_task_is_empty(svm_proc) == false) {
        return false;
    }
    if (devmm_notify_deviceprocess(svm_proc) != 0) {
        return false;
    }
    return true;
}

void devmm_svm_release_private_proc(struct devmm_svm_process *svm_proc)
{
    devmm_drv_run_info("Device process exited. (hostpid=%d; times=%d)\n", svm_proc->process_id.hostpid,
        svm_proc->release_work_cnt);

    devmm_destory_register_dma_mng(svm_proc);
    devmm_share_id_map_node_destroy_all(svm_proc);
    devmm_dma_desc_nodes_destroy_by_task_release(svm_proc);
    devmm_task_dev_res_nodes_destroy_by_task(svm_proc);
    devmm_destroy_all_convert_dma_addr(svm_proc);
    devmm_destroy_ipc_mem_node_by_proc(svm_proc, SVM_MAX_AGENT_NUM);
    devmm_destory_all_heap_by_proc(svm_proc);
    devmm_destroy_pages_cache(svm_proc);
    if (devmm_release_process_notice_pm(svm_proc) != 0) {
        devmm_drv_err("Vm notice pm failed. (hostpid=%d)\n", svm_proc->process_id.hostpid);
    }
    devmm_free_proc_priv_data(svm_proc);
    devmm_remove_pid_from_all_business(svm_proc->process_id.hostpid);
}

void devmm_proc_debug_info_print(struct devmm_svm_process *svm_proc)
{
    devmm_srcu_work_stats_print(&svm_proc->srcu_work);
    devmm_dma_desc_stats_info_print(svm_proc);
}

