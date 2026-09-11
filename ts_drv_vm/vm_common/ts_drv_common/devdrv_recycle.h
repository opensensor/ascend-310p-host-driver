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
#ifndef DEVDRV_RECYCLE_H
#define DEVDRV_RECYCLE_H

#include "devdrv_common.h"
#include "tsdrv_device.h"
#include "tsdrv_set_runtime_conflict_check.h"
#include "tsdrv_delay_recycle.h"

struct devdrv_source {
    u32 stream_num;
    u32 event_num;
    u32 sq_num;
    u32 cq_num;
};

struct devdrv_transmission {
    u32 send_count;
    u32 receive_count;
};

#define DEVDRV_RECYCLE_MAX_EVENT_NUM 25

#define TSDRV_IDS_RECYCLE_STOP 1

struct devdrv_event_recycle {
    struct devdrv_mailbox_message_header header;  // 8
    u16 count;                                    // 2
    u16 event[DEVDRV_RECYCLE_MAX_EVENT_NUM];      // 50
    u8 plat_type;
    u8 reserved[3];
};

struct devdrv_recycle_message {
    u32 invalid_type_num;
    u32 send_doorbell_num;
    u32 recycle_stream[DEVDRV_MAX_STREAM_ID];
    u32 recycle_sq[DEVDRV_MAX_SQ_NUM];
    u32 recycle_cq[DEVDRV_MAX_CQ_NUM];

    u32 recycle_sq_num;
    u32 recycle_cq_num;
    u32 recycle_stream_num;
    u32 recycle_event_num;
    u32 recycle_notify_num;
    u32 recycle_ipc_event_num;
};

/* inform   pid to ts recycle resource */
#define MAX_INFORM_PID_INFO 12
struct pid_exit_info {
    u32 app_cnt;
    int pid[MAX_INFORM_PID_INFO];
    u8 plat_type;
    u8 fid;
    u8 reserved[2];
};

struct recycle_pid_info {
    struct devdrv_mailbox_message_header header;
    struct pid_exit_info pid_info;
};

int tsdrv_dev_res_recycle(u32 devid, struct tsdrv_dev_resource *dev_res, struct tsdrv_ctx *ctx);
void tsdrv_recycle_sq(struct tsdrv_ts_resource *ts_resource, struct tsdrv_ctx *ctx, u32 sqid);
void tsdrv_recycle_notify_id(struct tsdrv_ts_resource *ts_resource, struct tsdrv_ctx *ctx);
void tsdrv_recycle_stream(struct tsdrv_ts_resource *ts_resource, struct tsdrv_ctx *ctx, u32 streamid);
void tsdrv_recycle_event_id(struct tsdrv_ts_resource *ts_resource, struct tsdrv_ctx *ctx);
void tsdrv_recycle_ipc_event_id(struct tsdrv_ts_resource *ts_resource, struct tsdrv_ctx *ctx);
void tsdrv_recycle_model_id(struct tsdrv_ts_resource *ts_resource, struct tsdrv_ctx *ctx);
void tsdrv_recycle_streams(struct tsdrv_ts_resource *ts_resource, struct tsdrv_ctx *ctx);
void tsdrv_recycle_all_sq(struct tsdrv_ts_resource *ts_resource, struct tsdrv_ctx *ctx);
void tsdrv_resource_no_recycle_print(struct tsdrv_ts_resource *ts_resource, struct tsdrv_ctx *ctx);
void tsdrv_resource_recycled_print(struct devdrv_recycle_message *recycle_message);
int tsdrv_proc_recycle(u32 devid, struct tsdrv_ctx *ctx, struct tsdrv_dev_resource *dev_res);
int hvtsdrv_proc_recycle(u32 devid, struct tsdrv_ctx *ctx, struct tsdrv_dev_resource *dev_res, u32 fid);

#endif
