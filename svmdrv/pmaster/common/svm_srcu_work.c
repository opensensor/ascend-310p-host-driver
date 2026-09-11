/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
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
 * Create: 2022-11-29
 */

#include "svm_log.h"
#include "devmm_adapt.h"
#include "devmm_common.h"
#include "devmm_mem_alloc_interface.h"
#include "svm_srcu_work.h"

struct devmm_srcu_node {
    srcu_subwork_func work_func;
    u64 *arg;
    u64 arg_size;

    ka_list_head_t node;

    u32 type;
};

struct devmm_srcu_work default_srcu_work;

void devmm_srcu_work_stats_print(struct devmm_srcu_work *srcu_work)
{
    devmm_drv_info("Srcu work stats info. (subwork_num=%llu; subwork_peak_num=%llu)\n",
        srcu_work->subwork_num, srcu_work->subwork_peak_num);
}

static struct devmm_srcu_node *_devmm_erase_one_srcu_node(struct devmm_srcu_work *work)
{
    struct devmm_srcu_node *srcu_node = NULL;

    if (ka_list_empty(&work->head) != 0) {
        return NULL;
    }

    srcu_node = ka_list_last_entry(&work->head, struct devmm_srcu_node, node);
    ka_list_del(&srcu_node->node);
    work->subwork_num--;
    return srcu_node;
}

static struct devmm_srcu_node *devmm_erase_one_srcu_node(struct devmm_srcu_work *srcu_work)
{
    struct devmm_srcu_node *srcu_node = NULL;

    ka_task_spin_lock_bh(&srcu_work->lock);
    srcu_node = _devmm_erase_one_srcu_node(srcu_work);
    ka_task_spin_unlock_bh(&srcu_work->lock);

    return srcu_node;
}

static void devmm_srcu_base_work(ka_work_struct_t *work)
{
    struct devmm_srcu_work *srcu_work = ka_container_of(work, struct devmm_srcu_work, dwork.work);
    struct devmm_srcu_node *srcu_node = NULL;
    bool is_empty;

    srcu_node = devmm_erase_one_srcu_node(srcu_work);
    if (srcu_node == NULL) {
        return;
    }

    srcu_node->work_func(srcu_node->arg, srcu_node->arg_size);
    devmm_kvfree_ex(srcu_node->arg);
    devmm_kvfree_ex(srcu_node);
    srcu_node = NULL;

    is_empty = (bool)ka_list_empty(&srcu_work->head);
    if (is_empty == false) {
        /* schedule work can just one on working, one standing, list not empty need schedule again */
        (void)ka_task_schedule_delayed_work(&srcu_work->dwork, ka_system_msecs_to_jiffies(0));
#ifndef EMU_ST
        ka_task_cond_resched();
#endif
    }
}

static void _devmm_srcu_subwork_add(struct devmm_srcu_work *work, struct devmm_srcu_node *subwork)
{
    ka_list_add(&subwork->node, &work->head);
    work->subwork_num++;
    work->subwork_peak_num = (work->subwork_num > work->subwork_peak_num) ? work->subwork_num : work->subwork_peak_num;
}

/* srcu_work == NULL will use default_srcu_work */
int devmm_srcu_subwork_add(struct devmm_srcu_work *srcu_work, u32 type, srcu_subwork_func func, u64 *arg, u64 arg_size)
{
    struct devmm_srcu_work *srcu_work_tmp = srcu_work;
    struct devmm_srcu_node *srcu_node = NULL;
    ka_gfp_t flags = (in_softirq() != 0) ? (KA_GFP_ATOMIC | __KA_GFP_ACCOUNT) : (KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    int ret;

    if (srcu_work_tmp == NULL) {
        srcu_work_tmp = &default_srcu_work;
    }

    srcu_node = devmm_kvzalloc_ex(sizeof(struct devmm_srcu_node), flags);
    if (srcu_node == NULL) {
        return -ENOMEM;
    }

    srcu_node->arg = devmm_kvzalloc_ex(arg_size, flags);
    if (srcu_node->arg == NULL) {
        devmm_kvfree_ex(srcu_node);
        return -ENOMEM;
    }

    ret = memcpy_s(srcu_node->arg, arg_size, arg, arg_size);
    if (ret != 0) {
        devmm_kvfree_ex(srcu_node->arg);
        devmm_kvfree_ex(srcu_node);
        return ret;
    }

    srcu_node->work_func = func;
    srcu_node->arg_size = arg_size;
    srcu_node->type = type;
    ka_task_spin_lock_bh(&srcu_work_tmp->lock);
    _devmm_srcu_subwork_add(srcu_work_tmp, srcu_node);
    ka_task_spin_unlock_bh(&srcu_work_tmp->lock);
    (void)ka_task_schedule_delayed_work(&srcu_work_tmp->dwork, ka_system_msecs_to_jiffies(0));

    return 0;
}

void devmm_srcu_work_init(struct devmm_srcu_work *srcu_work)
{
    ka_task_spin_lock_init(&srcu_work->lock);
    KA_INIT_LIST_HEAD(&srcu_work->head);
    KA_TASK_INIT_DELAYED_WORK(&srcu_work->dwork, devmm_srcu_base_work);

    srcu_work->subwork_num = 0;
    srcu_work->subwork_peak_num = 0;
}

static void devmm_srcu_nodes_destroy(struct devmm_srcu_work *srcu_work)
{
    struct devmm_srcu_node *srcu_node = NULL;
    u32 stamp = (u32)ka_jiffies;

    while (1) {
        srcu_node = devmm_erase_one_srcu_node(srcu_work);
        if (srcu_node == NULL) {
            return;
        }

        if (srcu_node->type == DEVMM_SRCU_SUBWORK_ENSURE_EXEC_TYPE) {
            srcu_node->work_func(srcu_node->arg, srcu_node->arg_size);
        }

        devmm_kvfree_ex(srcu_node->arg);
        devmm_kvfree_ex(srcu_node);

        devmm_try_cond_resched(&stamp);
    }
}

void devmm_srcu_work_uninit(struct devmm_srcu_work *srcu_work)
{
    if (ka_task_cancel_delayed_work_sync(&srcu_work->dwork) == true) {
        devmm_drv_debug("Cancel delayed_work return true.\n");
    }
    devmm_srcu_nodes_destroy(srcu_work);
}

void devmm_default_srcu_work_init(void)
{
    devmm_srcu_work_init(&default_srcu_work);
}

