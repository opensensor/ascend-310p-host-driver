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
 * Create: 2022-7-20
 */
#ifndef TSDRV_VSQ_H
#define TSDRV_VSQ_H

#include "drv_type.h"

#include "tsdrv_device.h"
#include "devdrv_cqsq.h"

int tsdrv_vsq_info_mem_create(u32 devid, u32 fid, u32 tsid);
void tsdrv_vsq_info_mem_destroy(u32 devid, u32 fid, u32 tsid);
void tsdrv_clear_vsq_info(struct tsdrv_ts_resource *ts_res, u32 vsq_id);

int tsdrv_vsq_mem_create(struct tsdrv_ts_resource *ts_res, u32 vsq_id, u32 slot_size, u32 detph);
void tsdrv_vsq_mem_destroy(struct tsdrv_ts_resource *ts_res, u32 vsq_id);

int tsdrv_get_vsq_phy_addr(struct tsdrv_ctx *ctx, u32 tsId, u32 vsqId, struct tsdrv_phy_addr_get *info);
u32 tsdrv_get_vsq_head(struct tsdrv_ts_resource *ts_res, u32 vsq_id);
void tsdrv_update_vsq_head(struct tsdrv_ts_resource *ts_res, u32 vsq_id, u32 vsq_head);

void hvtsdrv_update_vsq_tail(struct tsdrv_ts_resource *ts_res, u32 vsq_id, u32 vsq_tail);
s32 tsdrv_vsq_msg_send(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);

#endif
