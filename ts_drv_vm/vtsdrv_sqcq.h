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

#ifndef VTSDRV_SQCQ_H
#define VTSDRV_SQCQ_H

#include "tsdrv_ctx.h"
#include "vtsdrv_common.h"
#include "vtsdrv_init.h"

#define CQ_UMAX 0xFFFFFFFF

#define CQ_HEAD_UPDATE_FLAG 0x1

#define VTSDRV_WAIT_QUEUE_UNINITED 0
#define VTSDRV_WAIT_QUEUE_INITED 1

#define VTSDRV_CQ_HAVE_REPORTS 1

int vtsdrv_sqcq_alloc_id(struct tsdrv_ctx *ctx, struct vtsdrv_msg *msg);
int vtsdrv_sqcq_free_id(struct tsdrv_ctx *ctx, struct vtsdrv_msg *msg);
int vtsdrv_wait_cq_report(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
int vtsdrv_cq_wake_up(u32 devid, struct hvtsdrv_dev_proc *vdev_proc);
void vtsdrv_sq_slot_info_read_test(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
phys_addr_t vtsdrv_get_ts_sq_mem(u32 dev_id, u32 ts_id, u32 vsq_id);
phys_addr_t vtsdrv_get_ts_cq_mem(u32 dev_id, u32 ts_id, u32 vcq_id);
void vtsdrv_proc_remove_all_sqcq(struct tsdrv_ctx *ctx, u32 tsId);


#endif
