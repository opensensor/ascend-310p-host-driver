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

#ifndef __DMS_EVENT_DISTRIBUTE_H__
#define __DMS_EVENT_DISTRIBUTE_H__
#include <linux/kfifo.h>

#include "dms_cmd_def.h"

#define DMS_EVENT_DISTRIBUTE_FUNC_MAX 64
#define DISTRIBUTE_FUNC_HEAD_INDEX(priority) (DMS_EVENT_DISTRIBUTE_FUNC_MAX /               \
                                              DMS_DISTRIBUTE_PRIORITY_MAX * (priority))
#define DISTRIBUTE_FUNC_TAIL_INDEX(priority) (DMS_EVENT_DISTRIBUTE_FUNC_MAX /               \
                                              DMS_DISTRIBUTE_PRIORITY_MAX * (priority + 1))

#define DMS_EVENT_MAX_PROC_NUM_DSMI 64
/* means max 64 device, each device has 64 process */
#define DMS_EVENT_MAX_PROC_NUM_HAL (64 * 64)

/* halGetFaultEvent occupy 1 */
#define DMS_EVENT_EXCEPTION_PROCESS_MAX (DMS_EVENT_MAX_PROC_NUM_DSMI + DMS_EVENT_MAX_PROC_NUM_HAL + 1)

#define DMS_EVENT_EXCEPTION_PROCESS_DSMI_START 0
#define DMS_EVENT_EXCEPTION_PROCESS_DSMI_END (DMS_EVENT_EXCEPTION_PROCESS_DSMI_START + DMS_EVENT_MAX_PROC_NUM_DSMI)
#define DMS_EVENT_EXCEPTION_PROCESS_HAL_START DMS_EVENT_EXCEPTION_PROCESS_DSMI_END
#define DMS_EVENT_EXCEPTION_PROCESS_HAL_END (DMS_EVENT_EXCEPTION_PROCESS_HAL_START + DMS_EVENT_MAX_PROC_NUM_HAL)
#define DMS_EVENT_EXCEPTION_PROCESS_KERNEL_START DMS_EVENT_EXCEPTION_PROCESS_HAL_END
#define DMS_EVENT_EXCEPTION_PROCESS_KERNEL_END (DMS_EVENT_EXCEPTION_PROCESS_KERNEL_START + 1)

#define DMS_EVENT_PROCESS_STATUS_IDLE 0
#define DMS_EVENT_PROCESS_STATUS_WORK 1

#define DMS_EVENT_NO_TIMEOUT_FLAG (-1)
#define DMS_EVENT_WAIT_TIME 1000 /* 1000ms */
#define DMS_EVENT_WAIT_TIME_MAX 30000 /* 30000ms(30s) */

typedef enum {
    DMS_DISTRIBUTE_PRIORITY0 = 0,
    DMS_DISTRIBUTE_PRIORITY1,
    DMS_DISTRIBUTE_PRIORITY2,
    DMS_DISTRIBUTE_PRIORITY3,
    DMS_DISTRIBUTE_PRIORITY_MAX
} DMS_DISTRIBUTE_PRIORITY;

typedef struct {
    struct dms_event_para event;
    struct list_head node;
} DMS_EVENT_NODE_STRU;

struct dms_device_event {
    struct list_head head;
    int event_num; /* max 128 */
    struct mutex lock;
};

typedef struct {
    struct kfifo event_fifo;
    wait_queue_head_t event_wait;
    struct mutex process_mutex;

    u32 exception_num;
    pid_t process_pid;
    pid_t process_tid;
    u64 start_time; /* pid start time, not tgid */
    u8 process_status;
} DMS_EVENT_PROCESS_STRU;

typedef struct {
    DMS_EVENT_PROCESS_STRU event_process[DMS_EVENT_EXCEPTION_PROCESS_MAX];
    unsigned int process_num;
    u32 ns_id;
} DMS_EVENT_STRU_T;

typedef int (*DMS_EVENT_DISTRIBUTE_HANDLE_T)(DMS_EVENT_NODE_STRU *exception_node);

struct dms_device_event* dms_get_device_event(u32 dev_id);
int dms_event_get_exception(struct dms_event_para *fault_event, int timeout, enum cmd_source cmd_src);
int dms_event_clear_exception(u32 devid);
int dms_event_distribute_to_process(DMS_EVENT_NODE_STRU *exception_node);
int dms_event_distribute_handle(DMS_EVENT_NODE_STRU *exception_node, DMS_DISTRIBUTE_PRIORITY priority);
int dms_event_subscribe_register(DMS_EVENT_DISTRIBUTE_HANDLE_T handle_func,
    DMS_DISTRIBUTE_PRIORITY priority);
void dms_event_subscribe_unregister(DMS_EVENT_DISTRIBUTE_HANDLE_T handle_func);
void dms_event_subscribe_unregister_all(void);
void dms_event_distribute_stru_init(void);
void dms_event_distribute_stru_exit(void);
void dms_device_fault_event_init(void);
void dms_device_fault_event_exit(void);
void dms_release_one_device_remote_event(unsigned int dev_id);

#endif
