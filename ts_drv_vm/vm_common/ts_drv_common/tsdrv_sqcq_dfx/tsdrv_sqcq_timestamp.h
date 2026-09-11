/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
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
 * Create: 2024-2-1
 */
#ifndef TSDRV_SQCQ_TIMESTAMP_H
#define TSDRV_SQCQ_TIMESTAMP_H

#include "devdrv_common.h"
#include "tsdrv_timestamp_track.h"
#include "ka_task_pub.h"

struct tsdrv_logic_cq_dispatch_dfx {
    u32 logic_cq_dispatch_head;
    u64 logic_cq_dispatch_tick;
    u64 logic_cq_wakeup_tick;
};

struct tsdrv_sqcq_dfx {
    u64 cq_irq_tick;
    u64 cq_tasklet_tick;
    u64 phy_cq_tick[DEVDRV_MAX_CQ_NUM];
    struct tsdrv_logic_cq_dispatch_dfx logic_cq_dfx[TSDRV_MAX_LOGIC_CQ_NUM];
};

struct tsdrv_cqe_report_info_dfx {
    u32 logic_cq_id;
    u32 stream_id;
    u32 task_id;
    u32 error_type;
    u32 match_flag;
};

#ifdef TSDRV_UT
#ifdef __cplusplus
extern "C" {
#endif
#endif

int tsdrv_sqcq_dfx_dev_init(u32 devid, u32 fid, u32 tsnum);
void tsdrv_sqcq_dfx_dev_exit(u32 devid, u32 fid, u32 tsnum);
void tsdrv_sqcq_dfx_set_cq_irq_tick(u32 devid, u32 fid, u32 tsid);
void tsdrv_sqcq_dfx_set_cq_tasklet_tick(u32 devid, u32 fid, u32 tsid);
void tsdrv_sqcq_dfx_set_phy_cq_tick(u32 devid, u32 fid, u32 tsid, u32 cqid);
void tsdrv_sqcq_dfx_set_logic_cq_dispatch_tick(u32 devid, u32 fid, u32 tsid, u32 logic_cqid, u32 head);
void tsdrv_sqcq_dfx_set_wakeup_tick(u32 devid, u32 fid, u32 tsid, u32 logic_cqid);
void tsdrv_sqcq_dfx_show(struct tsdrv_ctx *ctx, u32 tsid, u32 logic_cq_id);

void tsdrv_sqcq_dfx_set_enable_status(u32 devid, u32 fid, bool status);
bool tsdrv_sqcq_dfx_is_enabled(u32 devid, u32 fid);
bool tsdrv_sqcq_dfx_ratelimited(void);

#ifdef TSDRV_UT
#ifdef __cplusplus
}
#endif
#endif

#endif