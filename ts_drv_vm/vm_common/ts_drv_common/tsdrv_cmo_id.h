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

#ifndef TSDRV_CMO_ID_H
#define TSDRV_CMO_ID_H

#include "tsdrv_interface.h"
#include "tsdrv_ctx.h"
#include "devdrv_common.h"
#include "tsdrv_ioctl.h"

#ifdef CFG_FEATURE_CMO
int tsdrv_cmo_id_init(u32 devid, u32 fid, u32 tsid, u32 cmo_id_num);
void tsdrv_cmo_id_destroy(u32 devid, u32 fid, u32 tsid);
int tsdrv_add_cmo_msg_chan(u32 devid, u32 tsid, struct tsdrv_msg_resource_id *dev_msg_resource_id);
int tsdrv_alloc_cmo_msg_chan(u32 devid, u32 tsid, struct tsdrv_msg_resource_id *dev_msg_resource_id);
int tsdrv_ioctl_alloc_cmo(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
int tsdrv_ioctl_free_cmo(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
#else
static int tsdrv_cmo_id_init(u32 devid, u32 fid, u32 tsid, u32 cmo_id_num)
{
    return 0;
}

static void tsdrv_cmo_id_destroy(u32 devid, u32 fid, u32 tsid)
{
    return;
}

static int tsdrv_ioctl_alloc_cmo(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    return -EOPNOTSUPP;
}

static int tsdrv_ioctl_free_cmo(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    return -EOPNOTSUPP;
}
#endif /* CFG_FEATURE_CMO */
#endif

