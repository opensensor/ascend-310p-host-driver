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
 * Create: 2022-2-20
 */
#ifndef TSDRV_RTS_STREAMID_H
#define TSDRV_RTS_STREAMID_H

#include "tsdrv_interface.h"
#include "tsdrv_ctx.h"
#include "devdrv_common.h"
#include "tsdrv_ioctl.h"

struct tsdrv_mailbox_alloc_streamid {
    struct devdrv_mailbox_message_header header;
    u32 priority;
    u32 stream_id;
    u32 vf_id;
    u32 pid;
};

struct tsdrv_mailbox_free_streamid {
    struct devdrv_mailbox_message_header header;
    u32 stream_id;
};

int tsdrv_alloc_rts_streamid(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
int tsdrv_free_rts_streamid(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
void tsdrv_recycle_rts_streamid(struct tsdrv_ts_resource *ts_resource, struct tsdrv_ctx *ctx);

#endif
