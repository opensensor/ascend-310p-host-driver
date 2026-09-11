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
#ifndef HVTSDRV_ID_H
#define HVTSDRV_ID_H

#include <linux/slab.h>

#include "ka_task_pub.h"
#include "devdrv_common.h"
#include "tsdrv_device.h"
#include "tsdrv_log.h"
#include "tsdrv_ioctl.h"

#ifndef CFG_SOC_PLATFORM_MDC_V51

#define DONT_SYNC_FROM_DEVICE 0
#define NEED_SYNC_FROM_DEVICE 1

struct res_id_move {
    enum tsdrv_id_type id_type;
    struct tsdrv_id_resource *src_id;
    struct tsdrv_id_resource *des_id;
    u32 sync_flag;
};

struct tsdrv_id_info *tsdrv_dev_res_id_move(u32 devid, u32 fid, u32 tsid, struct res_id_move *move_info);
void tsdrv_res_id_uninit(u32 devid, u32 fid, enum tsdrv_id_type id_type);
s32 tsdrv_res_id_alloc(struct tsdrv_ctx *ctx, enum tsdrv_id_type type, struct devdrv_ioctl_arg *arg);
s32 tsdrv_res_id_free(struct tsdrv_ctx *ctx, enum tsdrv_id_type type, struct devdrv_ioctl_arg *arg);
s32 tsdrv_notify_id_alloc(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
s32 tsdrv_notify_id_free(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
s32 tsdrv_get_pmid(u32 devid, u32 fid, u32 tsid, s32 virtid, enum tsdrv_id_type id_type);
s32 tsdrv_ids_init(u32 devid, u32 fid);
void tsdrv_ids_uninit(u32 devid, u32 fid);
s32 tsdrv_ctx_ids_recycle(u32 devid, u32 fid, struct tsdrv_ctx *ctx);
s32 hvtsdrv_id_info_query(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
struct tsdrv_id_info *tsdrv_get_one_res_id(struct tsdrv_id_resource *id_res);
void tsdrv_wait_dev_ids_recycle_complete(u32 devid, u32 fid);
void hvtsdrv_recycle(u32 devid, u32 fid, ka_pid_t tgid, u64 unique_id, int recycle_status);
#endif
static inline u32 tsdrv_vrit_to_physic_id(struct tsdrv_id_resource *id_res, u32 idx)
{
    return id_res->pm_id[idx];
}

#endif

