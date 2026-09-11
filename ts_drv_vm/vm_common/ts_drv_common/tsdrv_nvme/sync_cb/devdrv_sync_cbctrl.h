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
#ifndef DEVDRV_SYNC_CBCTRL_H
#define DEVDRV_SYNC_CBCTRL_H

#include "devdrv_cbsqcq_common.h"
#include "tsdrv_ioctl.h"
#include "tsdrv_ctx.h"
#include "ka_task_pub.h"
#include "ka_base_pub.h"
#include "ka_list_pub.h"

int devdrv_ioctl_cbsq_send(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
int devdrv_ioctl_cbcq_wait(struct tsdrv_ctx *ctx, void __user * arg);
void devdrv_cbcqsq_to_recyclelist(u32 devid, u32 fid, u32 tsnum, struct tsdrv_ctx *ctx);
int devdrv_recycle_cbcqsq_id(struct tsdrv_ctx *ctx);
void tsdrv_cbsqcq_no_recycle_print(struct tsdrv_ts_ctx *ts_ctx);
int callback_ctx_init(struct tsdrv_ctx *ctx, u32 tsnum);
void callback_ctx_exit(struct tsdrv_ctx *ctx, u32 tsnum);
int callback_dev_init(u32 devid, u32 fid, u32 tsnum);
void callback_dev_exit(u32 devid, u32 fid, u32 tsnum);
bool devdrv_cbsqcq_need_recycle(struct tsdrv_ts_ctx *ts_ctx);

void callback_g_info_exit_all_fids(u32 devid, u32 tsnum);

#endif /* __DEVDRV_SYNC_CBCTRL_H */
