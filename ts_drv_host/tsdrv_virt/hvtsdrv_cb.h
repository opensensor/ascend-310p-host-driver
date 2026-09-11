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

#ifndef HVTSDRV_CB_H
#define HVTSDRV_CB_H

#include "vtsdrv_common.h"
#include "ka_task_pub.h"
#include "ka_common_pub.h"
s32 hvtsdrv_generate_cb_sq(u32 devid, u32 tsid, u32 fid, ka_pid_t tgid, u8 *msg);
s32 hvtsdrv_generate_cb_cq(u32 devid, u32 tsid, u32 fid, ka_pid_t tgid, u8 *msg);
void hvtsdrv_destroy_cb_sqcq(u32 devid, u32 fid);
s32 hvtsdrv_cb_mailbox_to_ts(u32 devid, u32 tsid, u32 fid, ka_pid_t tgid, u8 *msg);
s32 hvtsdrv_cb_mailbox_logic_to_ts(u32 devid, u32 tsid, u32 fid, ka_pid_t tgid, u8 *msg);
s32 hvtsdrv_cb_set_doorbell(u32 devid, u32 tsid, u32 fid, ka_pid_t tgid, u8 *msg);
void hvtsdrv_cb_update_cq_report(ka_work_struct_t *work);
void hvtsdrv_cb_cq_handler(u32 devid, u32 fid, u32 tsid, u32 cq_id);
s32 hvtsdrv_cb_inform_tsagent_vsq_tail(u32 devid, u32 fid, u32 vsqid, u32 tail);
void hvtsdrv_cb_mutex_init(u32 devid, u32 fid);
void hvtsdrv_cb_mutex_destroy(u32 devid, u32 fid);
s32 hvtsdrv_ioctl_cbsqcq_alloc(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
s32 hvtsdrv_ioctl_cbsqcq_free(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
s32 hvtsdrv_ioctl_cbsq_send(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);


#endif
