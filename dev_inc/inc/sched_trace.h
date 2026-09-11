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
* Create: 2023-2-28
*/

#ifndef SCHED_TRACE_H
#define SCHED_TRACE_H

typedef enum {
    SCHED_TRACE_TIME_ENUQE_START,
    SCHED_TRACE_TIME_EVENT_SEND,
    SCHED_TRACE_TIME_EVENT_PUBLISH_USER,
    SCHED_TRACE_TIME_EVENT_PUBLISH_KERNEL,
    SCHED_TRACE_TIME_EVENT_PUBLISH_FINISH,
    SCHED_TRACE_TIME_THREAD_WAIT,
    SCHED_TRACE_TIME_PUBLISH_WAKEUP,
    SCHED_TRACE_TIME_THREAD_WAKED,
    SCHED_TRACE_TIME_ITEM_MAX
} SchedTraceTimeItem;

typedef struct {
    unsigned long timeStamp[SCHED_TRACE_TIME_ITEM_MAX];
} SchedTraceTimeInfo;

drvError_t RegisterEschedTraceRecordFunc(unsigned int grpId, unsigned int event_id,
    void (*finishFunc)(unsigned int grpId, unsigned int event_id, unsigned int subevent_id,
        SchedTraceTimeInfo *sched_trace_time_info));
#endif

