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

#ifndef HVTSDRV_SHM_SQCQ_H
#define HVTSDRV_SHM_SQCQ_H

#include "ka_task_pub.h"
#include "tsdrv_ioctl.h"
#include "devdrv_cqsq.h"
#include "drv_type.h"

int hvtsdrv_shm_sqcq_alloc(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
int hvtsdrv_shm_sqcq_free(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
int hvtsdrv_get_shm_phy_sq(u32 devid, u32 fid, u32 tsid, struct phy_sq_alloc_para *phy_sq);
int hvtsdrv_shm_offset_get(u32 devid, u32 tsid, u32 fid, ka_pid_t tgid, u8 *msg);

#endif /* HVTSDRV_SHM_SQCQ_H__ */
