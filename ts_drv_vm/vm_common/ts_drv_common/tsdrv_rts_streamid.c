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

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/gfp.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <asm/page.h>

#include "tsdrv_rts_streamid.h"
#include "tsdrv_ctx.h"
#include "tsdrv_device.h"
#include "tsdrv_sync.h"
#include "tsdrv_id.h"
#include "tsdrv_common.h"

#define TSDRV_STREAM_ID_INSUFFICIENT_RESOURCE 0x219 // Ts insufficient resource errorNum

int tsdrv_alloc_rts_streamid(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    struct tsdrv_mailbox_alloc_streamid alloc_streamid;
    struct tsdrv_mbox_data data;
    int ret;
    u32 devid;

    devid = tsdrv_get_devid_by_ctx(ctx);

    alloc_streamid.header.cmd_type = TSDRV_MBOX_ALLOC_RUNTIME_STREAM_ID;
    alloc_streamid.header.valid = DEVDRV_MAILBOX_MESSAGE_VALID;
    alloc_streamid.header.result = 0;

    alloc_streamid.priority = arg->rts_stream_para.priority;
    alloc_streamid.stream_id = 0;
    alloc_streamid.vf_id = arg->rts_stream_para.vf_id;
    alloc_streamid.pid = (u32)ctx->tgid;

    data.msg = &alloc_streamid;
    data.msg_len = sizeof(struct tsdrv_mailbox_alloc_streamid);
    data.out_data = &alloc_streamid;
    data.out_len = sizeof(struct tsdrv_mailbox_alloc_streamid);

    ret = tsdrv_mailbox_send_sync(devid, arg->tsid, &data);
    if (ret != 0) {
        TSDRV_PRINT_ERR("Send mailbox sync failed(ret=%d). (devid=%u, tsid=%u, pid=%u; priority=%u)\n",
            ret, devid, arg->tsid, alloc_streamid.pid, alloc_streamid.priority);
        if (ret == TSDRV_STREAM_ID_INSUFFICIENT_RESOURCE) {
            arg->result = ID_IS_EXHAUSTED;
            arg->id_para.res_id = tsdrv_get_stream_id_max_num(devid, arg->tsid);
            return 0;
        }
        return -EREMOTE;
    }

    arg->rts_stream_para.stream_id = alloc_streamid.stream_id;
    TSDRV_PRINT_DEBUG("alloc steam success. (devid=%u, tsid=%u, pid=%u, stream id=%u, priority=%u)\n",
        devid, arg->tsid, alloc_streamid.pid, alloc_streamid.stream_id, alloc_streamid.priority);
    return 0;
}

int tsdrv_free_rts_streamid(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    struct tsdrv_mailbox_free_streamid free_streamid;
    struct tsdrv_mbox_data data;
    int ret;
    u32 devid;

    devid = tsdrv_get_devid_by_ctx(ctx);

    free_streamid.header.cmd_type = TSDRV_MBOX_FREE_RUNTIME_STREAM_ID;
    free_streamid.header.valid = DEVDRV_MAILBOX_MESSAGE_VALID;
    free_streamid.header.result = 0;
    free_streamid.stream_id = arg->id_para.res_id;

    data.msg = &free_streamid;
    data.msg_len = sizeof(struct tsdrv_mailbox_free_streamid);
    data.out_data = NULL;
    data.out_len = 0;

    ret = tsdrv_mailbox_send_sync(devid, arg->tsid, &data);
    if (ret != 0) {
        TSDRV_PRINT_ERR("Send mailbox sync failed(ret=%d). (devid=%u, tsid=%u, pid=%u; streamid=%u)\n",
            ret, devid, arg->tsid, ctx->pid, free_streamid.stream_id);
        return -EINVAL;
    }

    TSDRV_PRINT_DEBUG("free steam success. (devid=%u, tsid=%u, pid=%u, stream id=%u)\n",
        devid, arg->tsid, ctx->pid, free_streamid.stream_id);
    return 0;
}

#ifndef TSDRV_UT
void tsdrv_recycle_rts_streamid(struct tsdrv_ts_resource *ts_resource, struct tsdrv_ctx *ctx)
{
    u32 tsid = ts_resource->tsid;
    if (tsdrv_get_chip_type() == CHIP_TYPE_MDC_BS9SX1A) {
        ka_task_spin_lock(&ts_resource->id_res[TSDRV_STREAM_ID].spinlock);
        ts_resource->id_res[TSDRV_STREAM_ID].id_available_num += ctx->ts_ctx[tsid].id_ctx[TSDRV_STREAM_ID].id_num;
        ctx->ts_ctx[tsid].id_ctx[TSDRV_STREAM_ID].id_num = 0;
        ka_task_spin_unlock(&ts_resource->id_res[TSDRV_STREAM_ID].spinlock);
    }
    return;
}
#else
void tsdrv_rts_streamid_stub(void)
{
}
#endif
