/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
 * Create: 2025-07-23
 */

#ifndef _UDIS_TIMER_H_
#define _UDIS_TIMER_H_

#include <linux/list.h>
#include <linux/rculist.h>
#include <linux/mutex.h>
#include <linux/hrtimer.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/time.h>
#include <linux/atomic.h>

#define UDIS_TIMER_TASK_MAX_NUM 2048
#define TASK_NAME_MAX_LEN 16

enum udis_timer_work_type {
    COMMON_WORK = 0,
    INDEPENDENCE_WORK,
    UDIS_WORK_TYPE_MAX
};

struct udis_timer_task {
    unsigned int period_ms;
    unsigned int cur_ms;
    enum udis_timer_work_type work_type;
    char task_name[TASK_NAME_MAX_LEN];
    unsigned long privilege_data;
    int (*period_task_func)(unsigned int udevid, unsigned long privilege_data);
};

struct udis_period_task_node {
    struct list_head node;
    struct rcu_head rcu;
    char task_name[TASK_NAME_MAX_LEN];
    unsigned long privilege_data;
    int (*period_task_func)(unsigned int udevid, unsigned long privilege_data);
    unsigned int original_expired_cnt;
    atomic_t expired_cnt;
    unsigned int cur_cnt;
    unsigned int udevid;
    enum udis_timer_work_type work_type;
    atomic_t queueflag;
    struct workqueue_struct *workqueue;
    struct work_struct work;
};

struct udis_timer {
    struct hrtimer timer;
    struct mutex task_list_lock;
    struct list_head period_task_list;
    unsigned int task_num;
    struct workqueue_struct *common_wq;
};

int udis_timer_init(void);
void udis_timer_uninit(void);
int hal_kernel_register_period_task(unsigned int udevid, const struct udis_timer_task *timer_task);
int hal_kernel_unregister_period_task(unsigned int udevid, const char *task_name);

#endif