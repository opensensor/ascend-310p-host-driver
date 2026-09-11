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

#include "dbl/runenv_config.h"
#include "devdrv_manager_comm.h"

#include "runenv_config.h"
#include "devmm_adapt.h"
#include "svm_srcu_work.h"
#include "devmm_common.h"
#include "devmm_proc_info.h"
#include "devmm_dev.h"
#include "svm_page_cnt_stats.h"
#include "svm_phy_addr_blk_mng.h"
#include "devmm_mem_alloc_interface.h"
#include "svm_proc_mng.h"
#include "svm_heap_mng.h"
#include "svm_vmma_mng.h"
#include "svm_dynamic_addr.h"

struct devmm_proc_node {
    struct devmm_svm_process svm_proc;
    ka_hlist_node_t link;
};

#define DEVMM_RELEASE_WAITTIME 1000
#define DEVMM_RELEASE_WAIT_TIMES_OUT 604800 /* one week */

#define DEVMM_HOSTPID_LIST_MAX   (1U << DEVMM_HOSTPID_LIST_SHIFT)
#define DEVMM_HOSTPID_LIST_SHIFT 6  /* 64 1<<6 */

static ka_rwlock_t svm_proc_hash_spinlock[DEVMM_HOSTPID_LIST_MAX];
STATIC KA_DEFINE_HASHTABLE(svm_proc_hashtable, DEVMM_HOSTPID_LIST_SHIFT);

STATIC void devmm_del_process_id_list(ka_list_head_t *head)
{
    struct svm_proc_id *proc_id_node = NULL;
    struct svm_proc_id *n = NULL;

    ka_list_for_each_entry_safe(proc_id_node, n, head, proc_list) {
        ka_list_del(&proc_id_node->proc_list);
        devmm_kvfree(proc_id_node);
    }
}

STATIC u32 devmm_get_node_hash_tag(int host_pid)
{
    return (u32)host_pid % DEVMM_HOSTPID_LIST_MAX;
}

int devmm_add_to_svm_proc_hashtable(struct devmm_svm_process *svm_proc)
{
    u32 node_tag = devmm_get_node_hash_tag(svm_proc->process_id.hostpid);
    u32 bucket = ka_hash_min(node_tag, KA_HASH_BITS(svm_proc_hashtable));
    struct devmm_proc_node *node = NULL;

    ka_task_write_lock_bh(&svm_proc_hash_spinlock[bucket]);
    if (devmm_get_end_type() == DEVMM_END_DEVICE) {
        u32 devid, vfid, proc_idx;
        int hostpid;

        ka_hash_for_each_possible(svm_proc_hashtable, node, link, node_tag) {
            if (node->svm_proc.devpid == svm_proc->devpid) {
#ifndef EMU_ST
                hostpid = node->svm_proc.process_id.hostpid;
                devid = node->svm_proc.process_id.devid;
                vfid = node->svm_proc.process_id.vfid;
                proc_idx = node->svm_proc.proc_idx;
                ka_task_write_unlock_bh(&svm_proc_hash_spinlock[bucket]);
                devmm_drv_err("Repeat pid info. (hostpid=%d; devid=%u; vfid=%u; proc_idx=%u)\n",
                    hostpid, devid, vfid, proc_idx);
                return -EEXIST;
#endif
            }
        }
    } else {
        ka_hash_for_each_possible(svm_proc_hashtable, node, link, node_tag) {
            if ((node->svm_proc.process_id.hostpid == svm_proc->process_id.hostpid) &&
                (node->svm_proc.process_id.vm_id == svm_proc->process_id.vm_id)) {
                ka_task_write_unlock_bh(&svm_proc_hash_spinlock[bucket]);
                return -EEXIST;
            }
        }
    }
    /* devdrv_check_hostpid to make sure just one proc can go here */
    node = ka_list_entry(svm_proc, struct devmm_proc_node, svm_proc);
    ka_hash_add(svm_proc_hashtable, &node->link, node_tag);
    ka_task_write_unlock_bh(&svm_proc_hash_spinlock[bucket]);

    return 0;
}

STATIC void devmm_del_from_svm_proc_hashtable(struct devmm_svm_process *svm_proc)
{
    struct devmm_proc_node *node = ka_list_entry(svm_proc, struct devmm_proc_node, svm_proc);

    ka_hash_del(&node->link);
}

void devmm_del_from_svm_proc_hashtable_lock(struct devmm_svm_process *svm_proc)
{
    u32 bucket = ka_hash_min(devmm_get_node_hash_tag(svm_proc->process_id.hostpid), KA_HASH_BITS(svm_proc_hashtable));

    ka_task_write_lock_bh(&svm_proc_hash_spinlock[bucket]);
    devmm_del_from_svm_proc_hashtable(svm_proc);
    ka_task_write_unlock_bh(&svm_proc_hash_spinlock[bucket]);
}

static void devmm_init_svm_proc_process_id(struct devmm_svm_process *svm_proc)
{
    svm_proc->process_id.hostpid = DEVMM_SVM_INVALID_PID;
    svm_proc->process_id.devid = DEVMM_MAX_DEVICE_NUM;
    svm_proc->process_id.vfid = DEVMM_MAX_VF_NUM;
    svm_proc->devpid = DEVMM_SVM_INVALID_PID;
    KA_INIT_LIST_HEAD(&svm_proc->proc_id_head);
}

static void devmm_init_svm_proc_heaps(struct devmm_svm_process *svm_proc)
{
    u32 i;

    svm_proc->max_heap_use = 0;
    for (i = 0; i < DEVMM_MAX_HEAP_NUM; i++) {
        svm_proc->heaps[i] = NULL;
    }
    svm_proc->alloced_heap_size = 0;
    ka_task_init_rwsem(&svm_proc->heap_sem);
}

static void devmm_init_svm_proc_vmas(struct devmm_svm_process *svm_proc)
{
    u32 i;

    svm_proc->vma_num = 0;
    for (i = 0; i < DEVMM_MAX_VMA_NUM; i++) {
        svm_proc->vma[i] = NULL;
    }
}

static void devmm_init_svm_proc_fault_state(struct devmm_svm_process *svm_proc)
{
    u32 i;

    for (i = 0; i < DEVMM_SVM_MAX_AICORE_NUM; i++) {
        svm_proc->fault_err[i].fault_addr = DEVMM_INVALID_ADDR;
        svm_proc->fault_err[i].fault_cnt = 0;
    }

    svm_proc->device_fault_printf = 1;
    ka_task_sema_init(&svm_proc->huge_fault_sem, 1);
    ka_task_sema_init(&svm_proc->p2p_fault_sem, 1);
    ka_task_sema_init(&svm_proc->fault_sem, 1);
    ka_task_init_rwsem(&svm_proc->host_fault_sem);
}

static void devmm_init_svm_proc_ipc_node(struct devmm_svm_process *svm_proc)
{
    u32 i;

    for (i = 0; i < DEVMM_MAX_IPC_NODE_LIST_NUM; i++) {
        KA_INIT_LIST_HEAD(&svm_proc->ipc_node.create_head[i]);
        KA_INIT_LIST_HEAD(&svm_proc->ipc_node.open_head[i]);
    }
    svm_proc->ipc_node.node_cnt = 0;
    ka_task_mutex_init(&svm_proc->ipc_node.node_mutex);
}

static void devmm_init_svm_proc_convert_res(struct devmm_svm_process *svm_proc)
{
    u32 i;

    ka_base_atomic64_set(&svm_proc->convert_res.convert_id, 0);
     ka_task_mutex_init(&svm_proc->convert_res.hlist_mutex);
    for (i = 0; i < DEVMM_CONVERT_RES_HLIST_NUM; i++) {
        KA_INIT_HLIST_HEAD(&svm_proc->convert_res.hlist[i]);
    }
}

static void devmm_init_svm_proc_state(struct devmm_svm_process *svm_proc)
{
    u32 i;

    ka_task_mutex_init(&svm_proc->proc_lock);
    for (i = 0; i < HOST_REGISTER_MAX_TPYE; i++) {
        ka_task_sema_init(&svm_proc->register_sem[i], 1);
    }
    ka_task_init_rwsem(&svm_proc->bitmap_sem);
    ka_task_init_rwsem(&svm_proc->ioctl_rwsem);
    ka_task_init_rwsem(&svm_proc->msg_chan_sem);

    svm_proc->normal_exited = DEVMM_SVM_ABNORMAL_EXITED_FLAG;
    svm_proc->start_addr = DEVMM_SVM_MEM_START;
    svm_proc->end_addr = DEVMM_SVM_MEM_START + DEVMM_SVM_MEM_SIZE - 1;
    svm_proc->host_pin_start_addr = DEVMM_HOST_PIN_START;
    svm_proc->host_pin_end_addr = DEVMM_HOST_PIN_START + DEVMM_HOST_PIN_SIZE - 1;
    svm_proc->host_pin_heap = NULL;

    for (i = 0; i < SVM_MAX_AGENT_NUM; i++) {
        svm_proc->deviceinfo[i].devpid = DEVMM_SETUP_INVAL_PID;
        svm_proc->phy_devid[i] = DEVMM_INVALID_DEVICE_PHYID;
        svm_proc->vfid[i] = DEVMM_INVALID_DEVICE_PHYID;
        svm_proc->real_phy_devid[i] = DEVMM_INVALID_DEVICE_PHYID;
    }
    svm_proc->phy_devid[SVM_HOST_AGENT_ID] = SVM_HOST_AGENT_ID;
    svm_proc->vfid[SVM_HOST_AGENT_ID] = 0;

    for (i = 0; i < SVM_UAGENT_MAX_NUM; i++) {
        (void)memset_s(&svm_proc->proc_states_info[i], sizeof(struct devmm_proc_states_info), 0,
            sizeof(struct devmm_proc_states_info));
        ka_task_init_rwsem(&svm_proc->proc_states_info[i].rw_sem);
        ka_base_atomic64_set(&svm_proc->proc_states_info[i].oom_ref, 0);
    }

    svm_proc->other_proc_occupying = 0;
    svm_proc->msg_processing = 0;
    svm_proc->proc_status = 0; /* clear CodeDEX alarm */
    svm_proc->release_work_timeout = DEVMM_RELEASE_WAIT_TIMES_OUT;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
    svm_proc->notifier = NULL;
#endif
    svm_proc->mm = ka_task_get_current_mm();
    svm_proc->tsk = ka_task_get_current();
    svm_proc->is_enable_svm_mem = false;
    svm_proc->notifier_reg_flag = DEVMM_SVM_UNINITED_FLAG;
    ka_base_atomic_set(&svm_proc->ref, 0);
}

static void devmm_init_svm_proc_custom(struct devmm_svm_process *svm_proc)
{
    u32 i;

    for (i = 0; i < DEVMM_CUSTOM_PROCESS_NUM; i++) {
        (void)memset_s(&svm_proc->custom[i], sizeof(svm_proc->custom[i]), 0, sizeof(svm_proc->custom[i]));
        ka_task_mutex_init(&svm_proc->custom[i].proc_lock);
    }
}

static void devmm_init_svm_proc_memnode(struct devmm_svm_process *svm_proc)
{
    ka_task_mutex_init(&svm_proc->mem_node_mutex);
#ifdef CFG_FEATURE_SURPORT_DMA_SVA
    svm_proc->addr_mng.is_need_dma_map = false;
#else
    svm_proc->addr_mng.is_need_dma_map = true;
#endif
    svm_proc->addr_mng.rbtree = KA_RB_ROOT;
    ka_task_init_rwsem(&svm_proc->addr_mng.rbtree_mutex);
}

static void devmm_init_svm_proc_host_pa_list(struct devmm_svm_process *svm_proc)
{
    svm_proc->host_pa_list.pa_rbtree = KA_RB_ROOT;
    ka_task_mutex_init(&svm_proc->host_pa_list.rbtree_mutex);
}

static void devmm_init_svm_proc_support_host_giant_page(struct devmm_svm_process *svm_proc)
{
    svm_proc->is_enable_host_giant_page = devmm_support_host_giant_page();
}

static int devmm_init_svm_proc_host_pin_heap(struct devmm_svm_process *svm_proc)
{
    int ret;
    u64 used_mask_size;
    struct devmm_svm_heap *heap;

    used_mask_size = sizeof(unsigned long) * KA_BASE_BITS_TO_LONGS((u64)(DEVMM_HOST_PIN_SIZE / HEAP_USED_PER_MASK_SIZE));
    heap = devmm_kvzalloc(sizeof(struct devmm_svm_heap) + used_mask_size);
    if (heap == NULL) {
        return -ENOMEM;
    }

    heap->heap_type = DEVMM_HEAP_PINNED_HOST;
    heap->heap_sub_type = SUB_HOST_TYPE;
    heap->used_mask = (unsigned long *)(void *)(heap + 1);
    heap->chunk_page_size = DEVMM_HUGE_PAGE_SIZE;
    heap->heap_size = DEVMM_HOST_PIN_SIZE;
    heap->start = DEVMM_HOST_PIN_START;
    ret = devmm_vmma_mng_init(&heap->vmma_mng, heap->start, heap->heap_size);
    if (ret != 0) {
        devmm_kvfree(heap);
        return ret;
    }

    if (dbl_get_deployment_mode() == DBL_HOST_DEPLOYMENT) {
        if (devmm_alloc_new_heap_pagebitmap(heap) != 0) {
            devmm_drv_err("Devmm_alloc_new_heap_pagebitmap for host pin error.\n");
            devmm_kvfree(heap);
            return ret;
        }
    }

    svm_proc->host_pin_heap = heap;
    return 0;
}

void devmm_destroy_svm_proc_host_pin_heap(struct devmm_svm_process *svm_proc)
{
    struct devmm_svm_heap *heap;

    heap = svm_proc->host_pin_heap;
    svm_proc->host_pin_heap = NULL;
    if (heap == NULL) {
        return;
    }

    if (dbl_get_deployment_mode() == DBL_HOST_DEPLOYMENT) {
        devmm_free_heap_pagebitmap_ref(heap);
    }
    devmm_kvfree(heap);
}

STATIC int devmm_init_svm_proc(struct devmm_svm_process *svm_proc)
{
    int ret;

    devmm_init_svm_proc_state(svm_proc);
    devmm_init_svm_proc_process_id(svm_proc);
    devmm_init_svm_proc_custom(svm_proc);
    devmm_init_svm_proc_heaps(svm_proc);
    devmm_init_svm_proc_vmas(svm_proc);
    devmm_init_svm_proc_fault_state(svm_proc);
    devmm_init_svm_proc_ipc_node(svm_proc);
    devmm_init_svm_proc_convert_res(svm_proc);
    devmm_init_svm_proc_memnode(svm_proc);
    devmm_init_svm_proc_host_pa_list(svm_proc);
    devmm_init_svm_proc_support_host_giant_page(svm_proc);

    devmm_page_cnt_stats_init(&svm_proc->pg_cnt_stats);
    devmm_srcu_work_init(&svm_proc->srcu_work);
    devmm_phy_addr_blk_mng_init(&svm_proc->phy_addr_blk_mng);
    svm_da_init(svm_proc);

    ret = devmm_init_svm_proc_host_pin_heap(svm_proc);
    if (ret != 0) {
        devmm_drv_err("devmm init host pin heap failed : %d\n", ret);
        return ret;
    }

    ret = devmm_svm_proc_init_private(svm_proc);
    if (ret != 0) {
        devmm_destroy_svm_proc_host_pin_heap(svm_proc);
        return ret;
    }

    return 0;
}

struct devmm_svm_process *devmm_alloc_svm_proc(void)
{
    struct devmm_proc_node *svm_proc_node = NULL;
    struct devmm_svm_process *svm_proc = NULL;
    static ka_atomic_t proc_idx = KA_BASE_ATOMIC_INIT(0);
    int ret;

    svm_proc_node = devmm_kvzalloc(sizeof(struct devmm_proc_node));
    if (svm_proc_node == NULL) {
        devmm_drv_err("Devmm_kvzalloc svm process fail.\n");
        return NULL;
    }

    svm_proc = &svm_proc_node->svm_proc;
    ret = devmm_init_svm_proc(svm_proc);
    if (ret != 0) {
        devmm_drv_err("Svm proc init fail. (ret=%d)\n", ret);
        devmm_kvfree(svm_proc_node);
        return NULL;
    }
    svm_proc->proc_idx = (u32)ka_base_atomic_inc_return(&proc_idx);

    return svm_proc;
}

void devmm_free_svm_proc(struct devmm_svm_process *svm_proc)
{
    struct devmm_proc_node *node = ka_list_entry(svm_proc, struct devmm_proc_node, svm_proc);

    devmm_drv_debug("Devmm_free_process details. (hostpid=%d; devid=%d; vfid=%d; devpid=%d; status=%u; pro_idx=%u)\n",
        svm_proc->process_id.hostpid, svm_proc->process_id.devid, svm_proc->process_id.vfid,
        svm_proc->devpid, svm_proc->notifier_reg_flag, svm_proc->proc_idx);

    devmm_destroy_svm_proc_host_pin_heap(svm_proc);
    devmm_del_process_id_list(&svm_proc->proc_id_head);
    devmm_kvfree(node);
}

void devmm_set_svm_proc_state(struct devmm_svm_process *svm_proc, u32 state)
{
    svm_proc->inited = state;
}

STATIC bool devmm_cmp_list_process_id(ka_list_head_t *head,
    struct devmm_svm_process_id *process_id)
{
    struct svm_proc_id *proc_id_node = NULL;

    ka_list_for_each_entry(proc_id_node, head, proc_list) {
        if (devmm_get_end_type() == DEVMM_END_DEVICE) {
            if ((proc_id_node->proc_id.hostpid == process_id->hostpid) &&
                (proc_id_node->proc_id.devid == process_id->devid) &&
                (proc_id_node->proc_id.vfid == process_id->vfid)) {
                return true;
            }
        } else {
            /* host do not need vfid, one host app may support many vfid */
            if ((proc_id_node->proc_id.hostpid == process_id->hostpid) &&
                (proc_id_node->proc_id.vm_id == process_id->vm_id)) {
                return true;
            }
        }
    }

    return false;
}

int devmm_add_svm_proc_pid(struct devmm_svm_process *svm_proc,
    struct devmm_svm_process_id *process_id, int dev_pid)
{
    struct svm_proc_id *proc_id_node = NULL;

    if (devmm_cmp_list_process_id(&svm_proc->proc_id_head, process_id) == true) {
        return -EUSERS;
    }
    proc_id_node = devmm_kvzalloc(sizeof(struct svm_proc_id));
    if (proc_id_node == NULL) {
        return -ENOMEM;
    }
    if (svm_proc->process_id.hostpid == DEVMM_SVM_INVALID_PID) {
        svm_proc->process_id = *process_id;
    } else {
        /* hostpid must same */
        if (svm_proc->process_id.hostpid != process_id->hostpid) {
#ifndef DRV_UT
            devmm_kvfree(proc_id_node);
            return -EUSERS;
#endif
        }
    }
    proc_id_node->proc_id = *process_id;
    ka_list_add(&proc_id_node->proc_list, &svm_proc->proc_id_head);
    svm_proc->devpid = dev_pid;

    return 0;
}

int devmm_add_svm_proc_pid_lock(struct devmm_svm_process *svm_proc,
    struct devmm_svm_process_id *process_id, int dev_pid)
{
    int ret;

    ka_task_mutex_lock(&svm_proc->proc_lock);
    ret = devmm_add_svm_proc_pid(svm_proc, process_id, dev_pid);
    ka_task_mutex_unlock(&svm_proc->proc_lock);

    return ret;
}

void devmm_del_first_svm_proc_pid(struct devmm_svm_process *svm_proc)
{
    struct svm_proc_id *proc_id_node = NULL;

    proc_id_node = ka_list_first_entry(&svm_proc->proc_id_head,
        typeof(struct svm_proc_id), proc_list);
    /* first is head just return */
    if (&svm_proc->proc_id_head == &proc_id_node->proc_list) {
        return;
    }
    ka_list_del(&proc_id_node->proc_list);
    devmm_kvfree(proc_id_node);

    return;
}

void devmm_del_first_svm_proc_pid_lock(struct devmm_svm_process *svm_proc)
{
    ka_task_mutex_lock(&svm_proc->proc_lock);
    devmm_del_first_svm_proc_pid(svm_proc);
    ka_task_mutex_unlock(&svm_proc->proc_lock);
    return;
}

static bool _devmm_svm_proc_devid_is_inited(struct devmm_svm_process *svm_proc, u32 phy_devid, u32 vfid)
{
    int i;

    for (i = 0; i < SVM_MAX_AGENT_NUM; i++) {
        /* svm_proc->phy_devid may not set up by current proc. */
        if ((svm_proc->deviceinfo[i].devpid != DEVMM_SETUP_INVAL_PID) &&
            (svm_proc->phy_devid[i] == phy_devid) && (svm_proc->vfid[i] == vfid)) {
            return true;
        }
    }

    return false;
}

static int devmm_svm_proc_cmp_phy_devid(struct devmm_svm_process *svm_proc, u32 real_phy_devid)
{
    int i;
    for (i = 0; i < SVM_MAX_AGENT_NUM; i++) {
        if ((svm_proc->deviceinfo[i].devpid != DEVMM_SETUP_INVAL_PID) &&
            svm_proc->real_phy_devid[i] == real_phy_devid) {
            return 0;
        }
    }

    return -ENODEV;
}

static bool devmm_svm_proc_devid_is_inited(struct devmm_svm_process *svm_proc, u32 docker_id, bool in_normal_docker,
    u32 phy_devid, u32 vfid, u32 real_phy_devid)
{
    int ret;

    if (in_normal_docker) {
        if ((svm_proc->docker_id == docker_id) && _devmm_svm_proc_devid_is_inited(svm_proc, phy_devid, vfid)) {
            return true;
        }
    } else { /* phy device or privileged docker */
        ret = devmm_svm_proc_cmp_phy_devid(svm_proc, real_phy_devid);
        if (ret == 0) {
            return true;
        }
    }

    return false;
}

int devmm_get_hostpid_by_docker_id(u32 docker_id, u32 phy_devid, u32 vfid, int *pids, u32 cnt)
{
    u32 bucket_occupy[DEVMM_HOSTPID_LIST_MAX] = {0};
    struct devmm_svm_process *svm_proc = NULL;
    struct devmm_proc_node *proc_node = NULL;
    bool in_normal_docker = false;
    int got_cnt = 0, ret, i;
    u32 real_phy_devid;
    u32 bucket;

    ret = devmm_get_real_phy_devid(phy_devid, vfid, &real_phy_devid);
    if (ret != 0) {
        devmm_drv_err("Get real phy devid fail. (ret=%d; phy_devid=%u; vfid=%u)\n", ret, phy_devid, vfid);
        return ret;
    }

    in_normal_docker = run_in_normal_docker();
    for (i = 0; i < DEVMM_HOSTPID_LIST_MAX; i++) {
        bucket = ka_hash_min(i, KA_HASH_BITS(svm_proc_hashtable));
        if (bucket_occupy[bucket] == 1) {
            continue;
        }

        ka_task_read_lock_bh(&svm_proc_hash_spinlock[bucket]);
        ka_hash_for_each_possible(svm_proc_hashtable, proc_node, link, i) {
            svm_proc = &proc_node->svm_proc;
            if ((svm_proc->inited == DEVMM_SVM_INITED_FLAG) &&
                devmm_svm_proc_devid_is_inited(svm_proc, docker_id, in_normal_docker,
                    phy_devid, vfid, real_phy_devid)) {
                pids[got_cnt] = svm_proc->process_id.hostpid;
                got_cnt++;
                if (got_cnt >= cnt) {
                    ka_task_read_unlock_bh(&svm_proc_hash_spinlock[bucket]);
                    return got_cnt;
                }
            }
        }
        ka_task_read_unlock_bh(&svm_proc_hash_spinlock[bucket]);
        bucket_occupy[bucket] = 1;
    }

    return got_cnt;
}

STATIC struct devmm_svm_process *devmm_sreach_svm_proc_each(proc_mng_cmp_fun cmp_fun, u64 cmp_arg, bool ref_inc)
{
    u32 bucket_occupy[DEVMM_HOSTPID_LIST_MAX] = {0};
    struct devmm_proc_node *proc_node = NULL;
    u32 bucket, i;

    for (i = 0; i < DEVMM_HOSTPID_LIST_MAX; i++) {
        bucket = ka_hash_min(i, KA_HASH_BITS(svm_proc_hashtable));
        if (bucket_occupy[bucket] == 1) {
            continue;
        }

        ka_task_read_lock_bh(&svm_proc_hash_spinlock[bucket]);
        ka_hash_for_each_possible(svm_proc_hashtable, proc_node, link, i) {
            if (cmp_fun(&proc_node->svm_proc, cmp_arg) == true) {
                if (ref_inc) {
                    ka_base_atomic_inc(&proc_node->svm_proc.ref);
                }
                ka_task_read_unlock_bh(&svm_proc_hash_spinlock[bucket]);
                return &proc_node->svm_proc;
            }
        }
        ka_task_read_unlock_bh(&svm_proc_hash_spinlock[bucket]);
        bucket_occupy[bucket] = 1;
    }

    return NULL;
}

static struct devmm_svm_process *devmm_svm_proc_get(proc_mng_cmp_fun_by_both_pid cmp_fun, ka_pid_t devpid, ka_pid_t hostpid)
{
    u32 bucket_occupy[DEVMM_HOSTPID_LIST_MAX] = {0};
    struct devmm_proc_node *proc_node = NULL;
    u32 bucket, i;

    for (i = 0; i < DEVMM_HOSTPID_LIST_MAX; i++) {
        bucket = ka_hash_min(i, KA_HASH_BITS(svm_proc_hashtable));
        if (bucket_occupy[bucket] == 1) {
            continue;
        }

        ka_task_read_lock_bh(&svm_proc_hash_spinlock[bucket]);
        ka_hash_for_each_possible(svm_proc_hashtable, proc_node, link, i) {
            if (cmp_fun(&proc_node->svm_proc, devpid, hostpid) == true) {
                ka_base_atomic_inc(&proc_node->svm_proc.ref);
                ka_task_read_unlock_bh(&svm_proc_hash_spinlock[bucket]);
                return &proc_node->svm_proc;
            }
        }
        ka_task_read_unlock_bh(&svm_proc_hash_spinlock[bucket]);
        bucket_occupy[bucket] = 1;
    }

    return NULL;
}

void devmm_svm_procs_get(struct devmm_svm_process *svm_procs[], u32 *num)
{
    u32 bucket_occupy[DEVMM_HOSTPID_LIST_MAX] = {0};
    struct devmm_proc_node *proc_node = NULL;
    u32 bucket, tmp_num = 0;
    u64 i;

    for (i = 0; i < DEVMM_HOSTPID_LIST_MAX; i++) {
        bucket = ka_hash_min(i, KA_HASH_BITS(svm_proc_hashtable));
        if (bucket_occupy[bucket] == 1) {
            continue;
        }

        ka_task_read_lock_bh(&svm_proc_hash_spinlock[bucket]);
        ka_hash_for_each_possible(svm_proc_hashtable, proc_node, link, i) {
            ka_base_atomic_inc(&proc_node->svm_proc.ref);
            svm_procs[tmp_num] = &proc_node->svm_proc;
            tmp_num++;
            if (tmp_num == *num) {
                ka_task_read_unlock_bh(&svm_proc_hash_spinlock[bucket]);
                return;
            }
        }
        ka_task_read_unlock_bh(&svm_proc_hash_spinlock[bucket]);
        bucket_occupy[bucket] = 1;
    }

    *num = tmp_num;
}

struct devmm_svm_process *devmm_sreach_svm_proc_each_get_and_del(proc_mng_cmp_fun cmp_fun, u64 cmp_arg)
{
    u32 bucket_occupy[DEVMM_HOSTPID_LIST_MAX] = {0};
    struct devmm_proc_node *proc_node = NULL;
    ka_hlist_node_t *tmp = NULL;
    u32 bucket, i;

    for (i = 0; i < DEVMM_HOSTPID_LIST_MAX; i++) {
        bucket = ka_hash_min(i, KA_HASH_BITS(svm_proc_hashtable));
        if (bucket_occupy[bucket] == 1) {
            continue;
        }

        ka_task_write_lock_bh(&svm_proc_hash_spinlock[bucket]);
        ka_hash_for_each_possible_safe(svm_proc_hashtable, proc_node, tmp, link, i) {
            if (cmp_fun(&proc_node->svm_proc, cmp_arg) == true) {
                ka_hash_del(&proc_node->link);
                ka_task_write_unlock_bh(&svm_proc_hash_spinlock[bucket]);
                return &proc_node->svm_proc;
            }
        }
        ka_task_write_unlock_bh(&svm_proc_hash_spinlock[bucket]);
        bucket_occupy[bucket] = 1;
    }

    return NULL;
}

static struct devmm_svm_process *devmm_sreach_svm_proc_each_possible(u32 tag, proc_mng_cmp_fun cmp_fun, u64 cmp_arg)
{
    u32 bucket = ka_hash_min(tag, KA_HASH_BITS(svm_proc_hashtable));
    struct devmm_proc_node *proc_node = NULL;

    ka_task_read_lock_bh(&svm_proc_hash_spinlock[bucket]);
    ka_hash_for_each_possible(svm_proc_hashtable, proc_node, link, tag) {
        if (cmp_fun(&proc_node->svm_proc, cmp_arg) == true) {
            ka_base_atomic_inc(&proc_node->svm_proc.ref);
            ka_task_read_unlock_bh(&svm_proc_hash_spinlock[bucket]);
            return &proc_node->svm_proc;
        }
    }
    ka_task_read_unlock_bh(&svm_proc_hash_spinlock[bucket]);

    return NULL;
}

STATIC bool devmm_cmp_proc_mm(struct devmm_svm_process *svm_pro, u64 cmp_arg)
{
    ka_mm_struct_t *mm = (ka_mm_struct_t *)(uintptr_t)cmp_arg;

    if (svm_pro->mm == mm) {
        return true;
    }

    return false;
}

struct devmm_svm_process *devmm_get_svm_proc_by_mm(ka_mm_struct_t *mm)
{
    return devmm_sreach_svm_proc_each(devmm_cmp_proc_mm, (u64)(uintptr_t)mm, false);
}

struct devmm_svm_process *devmm_svm_proc_get_by_mm(ka_mm_struct_t *mm)
{
    return devmm_sreach_svm_proc_each(devmm_cmp_proc_mm, (u64)(uintptr_t)mm, true);
}

STATIC bool devmm_cmp_proc_both_pid(struct devmm_svm_process *svm_pro, ka_pid_t devpid, int hostpid)
{
    if (svm_pro->devpid == devpid) {
        if ((hostpid == 0) || (svm_pro->process_id.hostpid == hostpid)) {
            return true;
        } else {
            devmm_drv_warn("Devmm cmp proc by hostpid failed, svm proc detail. "
                "(hostpid=%d; devid=%d; vfid=%d; devpid=%d; status=%u; proc_idx=%u)\n",
                svm_pro->process_id.hostpid, svm_pro->process_id.devid, svm_pro->process_id.vfid,
                svm_pro->devpid, svm_pro->notifier_reg_flag, svm_pro->proc_idx);
        }
    }

    return false;
}

struct devmm_svm_process *devmm_svm_proc_get_by_devpid(int dev_pid)
{
    return devmm_svm_proc_get(devmm_cmp_proc_both_pid, dev_pid, 0);
}

struct devmm_svm_process *devmm_svm_proc_get_by_both_pid(int dev_pid, int hostpid)
{
    return devmm_svm_proc_get(devmm_cmp_proc_both_pid, dev_pid, hostpid);
}

STATIC bool devmm_cmp_host_id(struct devmm_svm_process *svm_proc, u64 cmp_arg)
{
    struct devmm_svm_process_id *process_id = (struct devmm_svm_process_id *)(uintptr_t)cmp_arg;
    if (devmm_get_end_type() == DEVMM_END_DEVICE) {
        if ((svm_proc->inited == DEVMM_SVM_INITED_FLAG) &&
            (svm_proc->process_id.hostpid == process_id->hostpid)) {
            return true;
        }
    }

    return false;
}

STATIC bool devmm_cmp_process_id(struct devmm_svm_process *svm_proc, u64 cmp_arg)
{
    struct devmm_svm_process_id *process_id = (struct devmm_svm_process_id *)(uintptr_t)cmp_arg;
    if (devmm_get_end_type() == DEVMM_END_DEVICE) {
        if ((svm_proc->inited == DEVMM_SVM_INITED_FLAG) &&
            (svm_proc->process_id.hostpid == process_id->hostpid) &&
            (svm_proc->process_id.devid == process_id->devid) &&
            (svm_proc->process_id.vfid == process_id->vfid)) {
            return true;
        }
    } else {
        /* host do not need vfid, one host app may support many vfid */
        if ((svm_proc->inited == DEVMM_SVM_INITED_FLAG) &&
            (svm_proc->process_id.hostpid == process_id->hostpid) &&
            (svm_proc->process_id.vm_id == process_id->vm_id)) {
            return true;
        }
    }
    return false;
}

static struct devmm_svm_process *devmm_sreach_svm_proc(u32 node_tag, struct devmm_svm_process_id *process_id)
{
    struct devmm_svm_process *svm_proc = NULL;
    /* first sreach by process id */
    svm_proc = devmm_sreach_svm_proc_each_possible(node_tag, devmm_cmp_process_id, (u64)(uintptr_t)process_id);
    if (svm_proc != NULL) {
        return svm_proc;
    }
    /* not find sreach by host pid, and cmp proc id list */
    svm_proc = devmm_sreach_svm_proc_each_possible(node_tag, devmm_cmp_host_id, (u64)(uintptr_t)process_id);
    if (svm_proc == NULL) {
        return NULL;
    }

    ka_task_mutex_lock(&svm_proc->proc_lock);
    if (devmm_cmp_list_process_id(&svm_proc->proc_id_head, process_id) == false) {
        ka_task_mutex_unlock(&svm_proc->proc_lock);
        devmm_svm_proc_put(svm_proc);
        return NULL;
    }
    ka_task_mutex_unlock(&svm_proc->proc_lock);
    return svm_proc;
}

struct devmm_svm_process *devmm_svm_proc_get_by_process_id_ex(struct devmm_svm_process_id *process_id)
{
    u32 node_tag = devmm_get_node_hash_tag(process_id->hostpid);

    return devmm_sreach_svm_proc(node_tag, process_id);
}

void devmm_svm_proc_list_get_by_dev(u32 devid, ka_list_head_t *list)
{
#ifndef EMU_ST
    struct devmm_svm_proc_list_node *list_node = NULL;
    struct devmm_proc_node *proc_node = NULL;
    u32 bucket_occupy[DEVMM_HOSTPID_LIST_MAX] = {0};
    u32 bucket, i;

    for (i = 0; i < DEVMM_HOSTPID_LIST_MAX; i++) {
        bucket = ka_hash_min(i, KA_HASH_BITS(svm_proc_hashtable));
        if (bucket_occupy[bucket] == 1) {
            continue;
        }

        ka_task_read_lock_bh(&svm_proc_hash_spinlock[bucket]);
        ka_hash_for_each_possible(svm_proc_hashtable, proc_node, link, i) {
            if (proc_node->svm_proc.process_id.devid == devid) {
                list_node = devmm_kmalloc_ex(sizeof(struct devmm_svm_proc_list_node), KA_GFP_ATOMIC | __KA_GFP_ACCOUNT);
                if (list_node != NULL) {
                    ka_base_atomic_inc(&proc_node->svm_proc.ref);
                    list_node->svm_proc = &proc_node->svm_proc;
                    ka_list_add(&list_node->list, list);
                }
            }
        }
        ka_task_read_unlock_bh(&svm_proc_hash_spinlock[bucket]);
        bucket_occupy[bucket] = 1;
    }
#endif
}

void devmm_svm_proc_list_put(ka_list_head_t *list, proc_mng_put_hander handle)
{
#ifndef EMU_ST
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;

    if (ka_list_empty_careful(list) == 0) {
        ka_list_for_each_safe(pos, n, list) {
            struct devmm_svm_proc_list_node *list_node = ka_list_entry(pos, struct devmm_svm_proc_list_node, list);
            if (handle != NULL) {
                handle(list_node->svm_proc);
            }
            ka_base_atomic_dec(&list_node->svm_proc->ref);
            ka_list_del(&list_node->list);
            devmm_kfree_ex(list_node);
        }
    }
#endif
}

struct devmm_svm_process *devmm_svm_proc_get_by_process_id(struct devmm_svm_process_id *process_id)
{
    u32 node_tag = devmm_get_node_hash_tag(process_id->hostpid);
    u32 bucket = ka_hash_min(node_tag, KA_HASH_BITS(svm_proc_hashtable));
    struct devmm_proc_node *proc_node = NULL;

    ka_task_read_lock_bh(&svm_proc_hash_spinlock[bucket]);
    ka_hash_for_each_possible(svm_proc_hashtable, proc_node, link, node_tag) {
        if (devmm_cmp_process_id(&proc_node->svm_proc, (u64)(uintptr_t)process_id) == true) {
            ka_base_atomic_inc(&proc_node->svm_proc.ref);
            ka_task_read_unlock_bh(&svm_proc_hash_spinlock[bucket]);
            return &proc_node->svm_proc;
        }
    }
    ka_task_read_unlock_bh(&svm_proc_hash_spinlock[bucket]);

    return NULL;
}

/* svm_proc will be freed until other_proc_occupying==0 (Optimized to kref later) */
void devmm_svm_proc_put(struct devmm_svm_process *svm_proc)
{
    if (svm_proc != NULL) {
        ka_base_atomic_dec(&svm_proc->ref);
    }
}

void devmm_svm_procs_put(struct devmm_svm_process *svm_procs[], u32 num)
{
    u32 idx;

    for (idx = 0; idx < num; ++idx) {
        if (svm_procs[idx] == NULL) {
            return;
        }
        ka_base_atomic_dec(&svm_procs[idx]->ref);
    }
}

void devmm_svm_proc_wait_exit(struct devmm_svm_process *svm_proc)
{
    int retry;

    do {
        retry = 0;
        ka_task_mutex_lock(&svm_proc->proc_lock);
        if ((svm_proc->msg_processing > 0) || (svm_proc->other_proc_occupying > 0) ||
            ka_base_atomic_read(&svm_proc->ref) > 0) {
            svm_proc->proc_status |= DEVMM_SVM_THREAD_WAIT_EXIT;
            ka_task_mutex_unlock(&svm_proc->proc_lock);
            ka_system_usleep_range(DEVMM_SVM_MMU_MIN_SLEEP, DEVMM_SVM_MMU_MAX_SLEEP);
            retry = 1;
        }
    } while (retry != 0);
    ka_task_mutex_unlock(&svm_proc->proc_lock);
}

void devmm_wait_exit_and_del_from_hashtable_lock(struct devmm_svm_process *svm_proc)
{
    devmm_del_from_svm_proc_hashtable_lock(svm_proc);
    devmm_svm_proc_wait_exit(svm_proc);
}

STATIC void devmm_svm_release_work(ka_work_struct_t *work)
{
    struct devmm_svm_process *svm_proc = ka_container_of(work, struct devmm_svm_process, release_work.work);
    /* mmu notifier release may call by init thread(1), so need wait mmu notifier release finish */
    if ((devmm_svm_can_release_private(svm_proc) == false) ||
        (svm_proc->notifier_reg_flag != DEVMM_SVM_UNINITED_FLAG)) {
        svm_proc->release_work_cnt++;
        if (svm_proc->release_work_cnt < svm_proc->release_work_timeout) {
            (void)ka_task_schedule_delayed_work(&svm_proc->release_work, ka_system_msecs_to_jiffies(DEVMM_RELEASE_WAITTIME));
            return;
        }
    }

    if (svm_proc->release_work_cnt >= svm_proc->release_work_timeout) {
        devmm_drv_err("Svm proc in use can not release. "
            "(hostpid=%d; devid=%d; vfid=%d; devpid=%d; status=%u; proc_idx=%u; timeout=%u)\n",
            svm_proc->process_id.hostpid, svm_proc->process_id.devid, svm_proc->process_id.vfid,
            svm_proc->devpid, svm_proc->notifier_reg_flag, svm_proc->proc_idx, svm_proc->release_work_timeout);
        return;
    }
    svm_proc->release_work_cnt = 0;
    devmm_proc_debug_info_print(svm_proc);
    devmm_svm_release_private_proc(svm_proc);
    svm_da_recycle(svm_proc);
    devmm_free_svm_proc(svm_proc);

    return;
}

void devmm_svm_release_proc(struct devmm_svm_process *svm_proc)
{
    devmm_drv_debug("Devmm release create work. (hostpid=%d; timeout=%u)\n",
        svm_proc->process_id.hostpid, svm_proc->release_work_timeout);
    devmm_srcu_work_uninit(&svm_proc->srcu_work);
    KA_TASK_INIT_DELAYED_WORK(&svm_proc->release_work, devmm_svm_release_work);
    svm_proc->release_work_cnt = 0;
    (void)ka_task_schedule_delayed_work(&svm_proc->release_work, ka_system_msecs_to_jiffies(0));
}

struct devmm_svm_process *devmm_get_svm_proc_from_file(ka_file_t *file)
{
    if ((ka_fs_get_file_private_data(file) == NULL) || (((struct devmm_private_data *)ka_fs_get_file_private_data(file))->process == NULL)) {
        devmm_drv_err("Svm not inited, private_data is NULL, or svm proc is NULL.\n");
        return NULL;
    }
    return (struct devmm_svm_process *)((struct devmm_private_data *)ka_fs_get_file_private_data(file))->process;
}

bool devmm_test_and_set_init_flag(ka_file_t *file)
{
    int init_flag = ka_base_atomic_inc_return(&((struct devmm_private_data *)ka_fs_get_file_private_data(file))->init_flag);
    if (init_flag != 1) {
        ka_base_atomic_dec(&((struct devmm_private_data *)ka_fs_get_file_private_data(file))->init_flag);
        return true;
    }
    return false;
}

int devmm_alloc_svm_proc_set_to_file(ka_file_t *file)
{
    struct devmm_svm_process *svm_proc = NULL;

    if (devmm_test_and_set_init_flag(file) == true) {
        devmm_drv_err("Svm already inited.\n");
        return -EALREADY;
    }
    /*
     * user drvMemInitSvm:
     * 1)DEVMM_SVM_PRE_INITING_FLAG(alloc svm proc) -> DEVMM_SVM_INITING_FLAG(mmap) -> DEVMM_SVM_INITED_FLAG(ioctl init)
     * 2)DEVMM_SVM_PRE_INITING_FLAG(alloc svm proc) -> DEVMM_SVM_INITED_FLAG(ioctl init)
     */
    svm_proc = devmm_alloc_svm_proc();
    if (svm_proc == NULL) {
        devmm_drv_err("Alloc svm_proc struct fail.\n");
        return -EBUSY;
    }

    devmm_set_svm_proc_state(svm_proc, DEVMM_SVM_PRE_INITING_FLAG);
    ((struct devmm_private_data *)ka_fs_get_file_private_data(file))->process = svm_proc;
    devmm_drv_debug("Devmm_set_porcess details. (status=%u; proc_idx=%u)\n",
        svm_proc->notifier_reg_flag, svm_proc->proc_idx);
    return 0;
}

STATIC u32 devmm_get_custom_mm_id(struct devmm_svm_process *svm_pro, ka_mm_struct_t *mm)
{
    u32 i;

    for (i = 0; i < DEVMM_CUSTOM_PROCESS_NUM; i++) {
        if (svm_pro->custom[i].mm == mm) {
            return i;
        }
    }

    return DEVMM_CUSTOM_PROCESS_NUM;
}

STATIC bool devmm_cmp_custom_mm(struct devmm_svm_process *svm_pro, u64 cmp_arg)
{
    ka_mm_struct_t *mm = (ka_mm_struct_t *)(uintptr_t)cmp_arg;

    if (devmm_get_custom_mm_id(svm_pro, mm) != DEVMM_CUSTOM_PROCESS_NUM) {
        return true;
    }
    return false;
}

struct devmm_svm_process *devmm_get_svm_proc_by_custom_mm(ka_mm_struct_t *mm)
{
    return devmm_sreach_svm_proc_each(devmm_cmp_custom_mm, (u64)(uintptr_t)mm, false);
}
#ifndef EMU_ST
struct devmm_svm_process *devmm_svm_proc_get_by_custom_mm(ka_mm_struct_t *mm)
{
    return devmm_sreach_svm_proc_each(devmm_cmp_custom_mm, (u64)(uintptr_t)mm, true);
}
#endif
struct devmm_custom_process *devmm_get_svm_custom_proc_by_mm(ka_mm_struct_t *mm)
{
    struct devmm_svm_process *svm_proc = NULL;
    u32 custom_idx;

    svm_proc = devmm_get_svm_proc_by_custom_mm(mm);
    if (svm_proc == NULL) {
        return NULL;
    }
    custom_idx = devmm_get_custom_mm_id(svm_proc, mm);
    return &svm_proc->custom[custom_idx];
}

int devmm_svm_proc_mng_init(void)
{
    u32 i;
    for (i = 0; i < DEVMM_HOSTPID_LIST_MAX; i++) {
        ka_task_rwlock_init(&svm_proc_hash_spinlock[i]);
    }

    return 0;
}

void devmm_svm_proc_mng_uinit(void)
{
}

static struct devmm_proc_states_info *devmm_get_proc_states_info(
    struct devmm_svm_process *svm_proc, u32 devid, u32 vfid)
{
    struct uda_mia_dev_para mia_para;
    u32 udevid;
    int ret;

    if (vfid != 0) {
        mia_para.phy_devid = devid;
        mia_para.sub_devid = vfid - 1;
        ret = uda_mia_devid_to_udevid(&mia_para, &udevid);
        if (ret != 0) {
            devmm_drv_err("UDA mia devid to udevid failed. (ret=%d; devid=%u; vfid=%u)\n", ret, devid, vfid);
            return NULL;
        }
    } else {
        udevid = devid;
    }

    return &svm_proc->proc_states_info[udevid];
}

void devmm_modify_process_status(
    struct devmm_svm_process *svm_proc, u32 devid, u32 vfid, processStatus_t pid_status, bool new_state)
{
    struct devmm_proc_states_info *info = devmm_get_proc_states_info(svm_proc, devid, vfid);

    if ((info != NULL) && (pid_status >= STATUS_NOMEM) && (pid_status < STATUS_MAX)) {
        ka_task_down_write(&info->rw_sem);
        if (pid_status == STATUS_SVM_PAGE_FALUT_ERR_CNT) {
            if (new_state) {
                info->state[pid_status]++;
            } else {
                info->state[pid_status] = 0;
            }
        } else {
            info->state[pid_status] = (u32)new_state;
        }
        ka_task_up_write(&info->rw_sem);
    }
}

u32 devmm_get_process_status(struct devmm_svm_process *svm_proc, u32 devid, u32 vfid, processStatus_t status)
{
    struct devmm_proc_states_info *info = devmm_get_proc_states_info(svm_proc, devid, vfid);
    u32 state = 0;

    if ((info != NULL) && (status >= STATUS_NOMEM) && (status < STATUS_MAX)) {
        ka_task_down_read(&info->rw_sem);
        state = info->state[status];
        ka_task_up_read(&info->rw_sem);
    }

    return state;
}

void devmm_oom_ref_inc(struct devmm_svm_process *svm_proc)
{
    struct devmm_proc_states_info *info = NULL;

    info = devmm_get_proc_states_info(svm_proc, svm_proc->process_id.devid, svm_proc->process_id.vfid);
    if (info != NULL) {
        ka_base_atomic64_inc(&info->oom_ref);
    }
}

void devmm_set_oom_ref(struct devmm_svm_process *svm_proc, u64 value)
{
    struct devmm_proc_states_info *info = NULL;

    info = devmm_get_proc_states_info(svm_proc, svm_proc->process_id.devid, svm_proc->process_id.vfid);
    if (info != NULL) {
        ka_base_atomic64_set(&info->oom_ref, (s64)value);
    }
}

u64 devmm_get_oom_ref(struct devmm_svm_process *svm_proc)
{
    struct devmm_proc_states_info *info = NULL;
    u64 ref = 0;

    info = devmm_get_proc_states_info(svm_proc, svm_proc->process_id.devid, svm_proc->process_id.vfid);
    if (info != NULL) {
        ref = (u64)ka_base_atomic64_read(&info->oom_ref);
    }

    return ref;
}

bool devmm_thread_is_run_in_docker(void)
{
    if ((ka_task_get_current_mnt_ns() != ka_task_get_init_task()->nsproxy->mnt_ns) ||
        (ka_task_get_current_mnt_ns() == NULL)) {
        return true;
    }

    return false;
}

