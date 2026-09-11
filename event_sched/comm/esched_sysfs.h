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

#ifndef EVENT_ESCHED_SYSFS_H
#define EVENT_ESCHED_SYSFS_H

#include "ka_task_pub.h"
#include "ka_fs_pub.h"
#include "esched_fops.h"

#define MAX_LENTH 256
#define INFO_TO_SYSFS  0
#define INFO_TO_LOG    1
#define MILLISECOND_TO_MICROSECOND 1000

#define SYS_SCHED_PERIOD_TIME_1S 1000  /* 1s */
#define SYS_SCHED_PERIOD_TIME_10S 10000  /* 10s */
#define SYS_SCHED_PERIOD_TIME_60S 60000  /* 60s */
#define SYS_SCHED_PERIOD_TIME_300S 300000  /* 300s */

typedef enum abnormal_event_type {
    PROC_ABNORMAL_EVENT = 0,
    WAKEUP_ABNORMAL_EVENT,
    PUBLISH_SUBSCRIBE_ABNORMAL_EVENT,
    NORMAL_EVENT
}ABNORMAL_EVENT_TYPE_VALUE;

void sched_sysfs_show_proc_info(u32 chip_id, int pid);
void sched_sysfs_record_sample_data(struct sched_numa_node *node);
void sched_sysfs_clear_sample_data(struct sched_numa_node *node);

void sched_sysfs_init(ka_device_t *dev);
void sched_sysfs_uninit(ka_device_t *dev);

unsigned int sched_sysfs_record_num_data(void);
void sched_sysfs_node_debug_uninit(struct sched_numa_node *node);
#endif

