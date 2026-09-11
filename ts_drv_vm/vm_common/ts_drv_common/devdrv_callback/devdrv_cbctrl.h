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

#ifndef DEVDRV_CBCTRL_H
#define DEVDRV_CBCTRL_H

#include "tsdrv_ioctl.h"
#include "tsdrv_ctx.h"

#define MAX_WAIT_CQ_SIZE 32
#define LOGIC_CQ_QUEUE_FULL 1
#define BITS_PER_LONG_LONG_INT 64U

s32 devdrv_ioctl_cbsqcq_alloc(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
s32 devdrv_ioctl_cbsqcq_free(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
s32 devdrv_ioctl_cbsq_send(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
s32 devdrv_ioctl_cbcq_wait(struct tsdrv_ctx *ctx, void __user * arg);
void devdrv_cbcqsq_to_recyclelist(u32 devid, u32 fid, u32 tsnum, struct tsdrv_ctx *ctx);
int devdrv_recycle_cbcqsq_id(struct tsdrv_ctx *ctx);
bool devdrv_cbsqcq_need_recycle(struct tsdrv_ts_ctx *ts_ctx);

int devdrv_cbsqcq_setup(u32 devid);
void devdrv_cbsqcq_cleanup(u32 devid);

#endif

