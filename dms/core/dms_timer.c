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

#include <linux/string.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/time.h>
#include <linux/rculist.h>

#include "ka_system_pub.h"
#include "ka_memory_pub.h"
#include "ka_task_pub.h"
#include "ka_common_pub.h"
#include "ka_list_pub.h"
#include "ka_kernel_def_pub.h"

#ifndef CFG_HOST_ENV
#include "queue_work_affinity.h"
#include "drv_cpu_type.h"
#include "workqueue_affinity.h"
#endif

#include "dms_define.h"
#include "dbl_mem_alloc_interface.h"
#include "dms_timer.h"

#define TIMER_INVALID_ID    0xFFFF
struct dms_sched_task_timer {
    struct list_head node_list;
    struct mutex node_lock;
    struct hrtimer timer;
    struct workqueue_struct *workqueue;
    u32    task_nums;
    u16    ids[MAX_TASK_NUMS];
};
static struct dms_sched_task_timer g_timer = {{0}};

struct devdrv_sched_task_node {
    struct list_head list;
    ka_rcu_head_t rcu;
    u32 expire_count;       // time period: ms / TIMER_STEP_MS
    u32 cur_count;
    u32 node_id;
    u64 run_time;
    u64 user_data;
    int (*exec_task)(u64 data);
    struct work_struct work_data;
    struct workqueue_struct *workqueue;
    timer_task_handler_mode handler_mode;
};

STATIC enum hrtimer_restart dms_timer_irq_handler(struct hrtimer *htr)
{
    struct devdrv_sched_task_node *node = NULL;

    rcu_read_lock();
    ka_list_for_each_entry_rcu(node, &g_timer.node_list, list) {
        if (node->cur_count > 1) {
            node->cur_count--;
            continue;
        }
        node->cur_count = node->expire_count;

        if (node->handler_mode == TIMER_IRQ) {
            node->exec_task(node->user_data);
        } else if (node->handler_mode == INDEPENDENCE_WORK || node->handler_mode == HIGH_PRI_WORK) {
            // Queue in self one thread.
#ifndef CFG_HOST_ENV
            queue_work_affinity(node->workqueue, &node->work_data);
#else
#ifndef DRV_SOFT_UT
            queue_work(node->workqueue, &node->work_data);
#endif
#endif
        } else {
            // Queue in multi common thread.
#ifndef CFG_HOST_ENV
            queue_work_affinity(g_timer.workqueue, &node->work_data);
#else
            queue_work(g_timer.workqueue, &node->work_data);
#endif
        }
    }
    rcu_read_unlock();

    (void)hrtimer_forward_now(htr, ms_to_ktime(TIMER_STEP_MS));
    return HRTIMER_RESTART;
}

STATIC void dms_timer_callback_delay(struct work_struct *work)
{
    struct devdrv_sched_task_node *sched_task = NULL;

    sched_task = ka_container_of(work, struct devdrv_sched_task_node, work_data);
    sched_task->exec_task(sched_task->user_data);
}

STATIC void dms_timer_clear_node_id(u32 node_id)
{
    if (node_id < MAX_TASK_NUMS) {
        g_timer.ids[node_id] = TIMER_INVALID_ID;
    }
}

/* Get valid node id.
 * id Range: 1 ~ MAX_TASK_NUMS
 * */
STATIC u32 dms_timer_get_valid_node_id(void)
{
    int i;
    u32 node_id = TIMER_INVALID_ID;

    for (i = 0; i < MAX_TASK_NUMS; i++) {
        if (g_timer.ids[i] == TIMER_INVALID_ID) {
            g_timer.ids[i] = i;
            node_id = (u32)g_timer.ids[i];
            break;
        }
    }
    return node_id;
}

STATIC int dms_timer_node_init(const struct dms_timer_task *task, struct devdrv_sched_task_node *node)
{
    u32 node_id;
    char queue_name[WORKQUEUE_NAME_LENGTH];
    int ret;

    node_id = dms_timer_get_valid_node_id();
    if (node_id == TIMER_INVALID_ID) {
        /* Node id valid range: 0~MAX_TASK_NUMS-1 */
        dms_err("Timer task list is full.\n");
        return -ENOSPC;
    }

    node->node_id = node_id;
    node->expire_count = task->expire_ms / TIMER_STEP_MS;
    if (task->count_ms >= task->expire_ms) {
        node->cur_count = 0;
    } else {
        node->cur_count = (task->expire_ms - task->count_ms) / TIMER_STEP_MS;
    }
    node->user_data = task->user_data;
    node->exec_task = task->exec_task;
    node->handler_mode = task->handler_mode;
    node->run_time = 0;
    node->workqueue = NULL;
    KA_TASK_INIT_WORK(&node->work_data, dms_timer_callback_delay);
    if ((task->handler_mode != INDEPENDENCE_WORK) && (task->handler_mode != HIGH_PRI_WORK)) {
        return 0;
    }

    ret = snprintf_s(queue_name, sizeof(queue_name), sizeof(queue_name) - 1, "dms_timer_%u", node_id);
    if (ret <= 0) {
        dms_timer_clear_node_id(node_id);
        dms_err("Call snprintf_s failed. (node_id=%u; ret=%d)\n", node_id, ret);
        return -EFAULT;
    }

    if (task->handler_mode == INDEPENDENCE_WORK) {
        node->workqueue = ka_task_create_singlethread_workqueue(queue_name);
    } else {
        /* HIGH_PRI_WORK */
        node->workqueue = alloc_workqueue("%s", WQ_HIGHPRI, 1, queue_name);
    }

    if (node->workqueue == NULL) {
        dms_timer_clear_node_id(node_id);
        dms_err("Timer task queue is NULL.\n");
        return -ENOMEM;
    }
    return 0;
}

int dms_timer_task_register(const struct dms_timer_task *task, u32 *node_id)
{
    struct devdrv_sched_task_node *node = NULL;
    int ret;

    if ((task == NULL) || (node_id == NULL) || task->exec_task == NULL) {
        dms_err("Timer task param invalid. node or task is null.\n");
        return -EINVAL;
    }
    if (task->expire_ms < TIMER_STEP_MS) {
        dms_err("Timer task param invalid. expire_ms(%u) < TIMER_STEP_MS.\n", task->expire_ms);
        return -EINVAL;
    }
    if (g_timer.task_nums >= MAX_TASK_NUMS) {
        dms_err("Timer task list is full.\n");
        return -ENOSPC;
    }

    mutex_lock(&g_timer.node_lock);
    node = dbl_kzalloc(sizeof(struct devdrv_sched_task_node), KA_GFP_ATOMIC | __KA_GFP_ACCOUNT);
    if (node == NULL) {
        mutex_unlock(&g_timer.node_lock);
        dms_err("Timer task node malloc failed.\n");
        return -ENOMEM;
    }

    ret = dms_timer_node_init(task, node);
    if (ret != 0) {
        dbl_kfree(node);
        node = NULL;
        mutex_unlock(&g_timer.node_lock);
        dms_err("Timer task node init failed. (ret=%d)\n", ret);
        return ret;
    }

    *node_id = node->node_id;
    list_add_tail_rcu(&node->list, &g_timer.node_list);
    g_timer.task_nums++;

    mutex_unlock(&g_timer.node_lock);
    dms_info("Timer register success. (node_id=%u; expire_ms=%u)\n", *node_id, task->expire_ms);
    return 0;
}
KA_EXPORT_SYMBOL(dms_timer_task_register);


int dms_timer_task_unregister(u32 node_id)
{
    struct devdrv_sched_task_node *node = NULL;

    mutex_lock(&g_timer.node_lock);
    ka_list_for_each_entry(node, &g_timer.node_list, list) {
        if (node->node_id == node_id) {
            g_timer.task_nums--;
            list_del_rcu(&node->list);
            synchronize_rcu();

            // workqueue 清理
            if (node->workqueue != NULL) {
                flush_workqueue(node->workqueue);
                destroy_workqueue(node->workqueue);
            } else {
                flush_workqueue(g_timer.workqueue);
            }
            dms_timer_clear_node_id(node_id);
            mutex_unlock(&g_timer.node_lock);
            dbl_kfree(node);
            dms_info("Timer unregister success. (node_id=%u)\n", node_id);
            return 0;
        }
    }

    mutex_unlock(&g_timer.node_lock);
    dms_err("Timer node does not exist. (node_id=%u)\n", node_id);
    return -EINVAL;
}
KA_EXPORT_SYMBOL(dms_timer_task_unregister);

#if defined(CFG_SOC_PLATFORM_MDC_V51) && !defined(AOS_LLVM_BUILD)
STATIC void dms_set_timer_workqueue_affinity(struct workqueue_struct *wq)
{
    struct cpumask wq_cpumask;
    int ret;

    ret = drv_get_ctrlcpu_mask(&wq_cpumask);
    if (ret != 0) {
        dms_warn("get ctrl cpu mask warn.(ret=%d)\n", ret);
        return;
    }

    if (set_workqueue_affinity(wq, 0, &wq_cpumask) != 0) {
        dms_warn("bind workqueue dms_timer_common warn\n");
    }
}
#endif
int dms_timer_init(void)
{
    int i;

    INIT_LIST_HEAD_RCU(&g_timer.node_list);
    ka_task_mutex_init(&g_timer.node_lock);
    hrtimer_init(&g_timer.timer, KA_CLOCK_MONOTONIC, HRTIMER_MODE_REL);

    for (i = 0; i < MAX_TASK_NUMS; i++) {
        g_timer.ids[i] = TIMER_INVALID_ID;
    }
    g_timer.timer.function = dms_timer_irq_handler;
    g_timer.task_nums = 0;
#if defined(CFG_SOC_PLATFORM_MDC_V51) && !defined(AOS_LLVM_BUILD)
    g_timer.workqueue = alloc_workqueue("dms_timer_common", WQ_UNBOUND, 1);
#else
    g_timer.workqueue = ka_task_create_workqueue("dms_timer_common");
#endif
    if (g_timer.workqueue == NULL) {
        dms_err("Timer task work queue is NULL.\n");
        return -ENOMEM;
    }

#if defined(CFG_SOC_PLATFORM_MDC_V51) && !defined(AOS_LLVM_BUILD)
    dms_set_timer_workqueue_affinity(g_timer.workqueue);
#endif

    hrtimer_start(&g_timer.timer, ms_to_ktime(TIMER_STEP_MS), HRTIMER_MODE_REL);

    dms_info("Dms timer init success, start timer.\n");
    return 0;
}

void dms_timer_uninit(void)
{
    struct devdrv_sched_task_node *node = NULL;
    struct devdrv_sched_task_node *next = NULL;

    mutex_lock(&g_timer.node_lock);
    (void)hrtimer_cancel(&g_timer.timer);
    dms_info("Dms timer uninit success, cancel timer.\n");

    synchronize_rcu();
    flush_workqueue(g_timer.workqueue);
    destroy_workqueue(g_timer.workqueue);
    ka_list_for_each_entry_safe(node, next, &g_timer.node_list, list) {
        if (node->workqueue != NULL) {
            flush_workqueue(node->workqueue);
            destroy_workqueue(node->workqueue);
        }
        dbl_kfree(node);
        node = NULL;
    }
    mutex_unlock(&g_timer.node_lock);
    mutex_destroy(&g_timer.node_lock);
}
