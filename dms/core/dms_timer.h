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

#ifndef __DMS_TIMER_H__
#define __DMS_TIMER_H__

#include <linux/workqueue.h>
#include "drv_type.h"

#define WORKQUEUE_NAME_LENGTH  32
#define S_TO_US_M           1000000
#define TIMER_STEP_MS       100
#define MAX_TASK_NUMS       1024

typedef enum {
    TIMER_IRQ = 0,
    COMMON_WORK,
    INDEPENDENCE_WORK,
    HIGH_PRI_WORK,
}timer_task_handler_mode;

struct dms_timer_task {
    u32 expire_ms;
    u32 count_ms;
    u64 user_data;
    u32 handler_mode;
    int (*exec_task)(u64 data);
};

int dms_timer_init(void);
void dms_timer_uninit(void);
int dms_timer_task_register(const struct dms_timer_task *task, u32 *node_id);
int dms_timer_task_unregister(u32 node_id);

#endif

