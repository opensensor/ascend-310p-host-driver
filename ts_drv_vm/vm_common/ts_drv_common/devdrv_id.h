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
#ifndef DEVDRV_ID_H
#define DEVDRV_ID_H

#include "tsdrv_interface.h"
#include "tsdrv_ctx.h"
#include "tsdrv_ioctl.h"
#include "devdrv_common.h"
#include "devdrv_cqsq.h"
#include "ka_memory_pub.h"
#include "ka_task_pub.h"
#define DEVDRV_INVALID_INDEX 0xFEFE

enum devdrv_uio_type {
    DEVDRV_INIT_SQ_UIO,
    DEVDRV_INIT_CQ_UIO
};

enum devdrv_free_flag {
    DEVDRV_FREE_BY_RECYCLE,
    DEVDRV_FREE_BY_USER
};

#define DEVDRV_NOTIFY_INFORM_TS 0
#define DEVDRV_NOTIFY_NOT_INFORM_TS 1

struct devdrv_ts_notify_msg {
    struct devdrv_mailbox_message_header header;
    u16 notifyId;
    u16 plat_type;
    u32 tgid;
    u16 virt_notifyId; /* ts use for covert id to notifyid */
    u8  fid;
    u8 notify_type; /* 0 notify id, 1 event id */
    u8 reserved[44];
};

struct tsdrv_free_event_id_msg {
    struct devdrv_mailbox_message_header header;
    u32 event_id;
};

static inline enum tsdrv_id_type devdrv_manager_id_type_replace(enum tsdrv_id_type id_type)
{
    return (id_type == TSDRV_EVENT_HW_ID) ? TSDRV_EVENT_SW_ID : id_type;
}

void tsdrv_ids_release(u32 devid, u32 fid, u32 tsnum);
struct tsdrv_id_info *devdrv_find_one_id(struct tsdrv_id_resource *id_res, int id);
int devdrv_stream_id_init(u32 devid, u32 fid, u32 tsid, u32 streams);
int devdrv_event_id_init(u32 devid, u32 fid, u32 tsid, u32 events);
void devdrv_event_id_destroy(u32 devid, u32 fid, u32 tsid);
void devdrv_stream_id_destroy(u32 devid, u32 fid, u32 tsid);
int devdrv_model_id_init(u32 devid, u32 fid, u32 tsid, u32 model_id_num);
void devdrv_model_id_destroy(u32 devid, u32 fid, u32 tsid);
int devdrv_notify_id_init(u32 devid, u32 fid, u32 tsid, u32 notify_id_num);
void devdrv_notify_id_destroy(u32 devid, u32 fid, u32 tsid);

struct tsdrv_id_info *devdrv_alloc_stream(struct tsdrv_ctx *ctx, u32 tsid, int *no_stream);
int devdrv_free_stream(struct tsdrv_ctx *ctx, u32 tsid, int id, int flag);
struct tsdrv_id_info *devdrv_alloc_event_id(struct tsdrv_ctx *ctx, u32 tsid);
int devdrv_free_event_id(struct tsdrv_ctx *ctx, u32 tsid, int id);

int devdrv_alloc_model_id(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
int devdrv_free_model_id(struct tsdrv_ctx *ctx, u32 tsid, int id);

int devdrv_alloc_notify_id(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
int devdrv_free_notify_id(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);


int devdrv_init_cq(struct tsdrv_ctx *ctx, u32 tsid, struct devdrv_cq_sub_info *cq_sub_info, u32 size);
int devdrv_exit_cq(struct tsdrv_ctx *ctx, u32 tsid, struct devdrv_cq_sub_info *cq_sub_info);
int devdrv_init_sq(struct tsdrv_ts_resource *ts_resource, struct tsdrv_ctx *ctx,
    struct devdrv_sq_sub_info *sq_sub_info, u32 size);
int devdrv_exit_sq(struct tsdrv_ts_resource *ts_resource, struct tsdrv_ctx *ctx,
    struct devdrv_ts_sq_info *sq_info);

int devdrv_free_one_notify_id(struct tsdrv_ts_resource *ts_resource, struct tsdrv_ctx *ctx, int id,
    int inform_type);
int devdrv_notify_ts_msg(u32 devid, u32 tsid, u32 notify_type, int notifyId, struct tsdrv_id_info *notify_info);

int devdrv_alloc_stream_msg_chan(u32 devid, u32 tsid, struct tsdrv_msg_resource_id *dev_msg_resource_id);
int devdrv_alloc_model_msg_chan(u32 devid, u32 tsid, struct tsdrv_msg_resource_id *dev_msg_resource_id);
int devdrv_alloc_notify_msg_chan(u32 devid, u32 tsid, struct tsdrv_msg_resource_id *dev_msg_resource_id);
int devdrv_alloc_id_msg_chan(u32 devid, u32 tsid, struct tsdrv_msg_resource_id *dev_msg_resource_id);
int devdrv_alloc_event_msg_chan(u32 devid, u32 tsid, struct tsdrv_msg_resource_id *dev_msg_resource_id,
    u32 *sync_hw_event_num, u32 *sync_sw_event_num);


int devdrv_add_stream_msg_chan(u32 devid, u32 tsid, struct tsdrv_msg_resource_id *dev_msg_resource_id);
int devdrv_add_event_msg_chan(u32 devid, u32 tsid, struct tsdrv_msg_resource_id *dev_msg_resource_id);
int devdrv_add_model_msg_chan(u32 devid, u32 tsid, struct tsdrv_msg_resource_id *dev_msg_resource_id);
int devdrv_add_notify_msg_chan(u32 devid, u32 tsid, struct tsdrv_msg_resource_id *dev_msg_resource_id);
int devdrv_add_id_msg_chan(u32 devid, u32 tsid, struct tsdrv_msg_resource_id *dev_msg_resource_id);

int devdrv_alloc_ipc_event_id(struct tsdrv_ctx *ctx, u32 tsid, u32 *id);
int devdrv_free_ipc_event_id(struct tsdrv_ctx *ctx, u32 tsid, int id, int inform_type);
int tsdrv_ioctl_id_info_query(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
int tsdrv_id_is_belong_to_proc(struct tsdrv_id_inst *id_inst, ka_pid_t tgid,
    u32 id, enum tsdrv_id_type id_type);
u32 devdrv_get_available_id_num(struct tsdrv_ts_resource *ts_resource, enum tsdrv_id_type type);

struct tsdrv_stream_logic_cq_info {
    ka_task_spinlock_t lock;
    u16 logic_cq[DEVDRV_MAX_STREAM_ID];
};

int tsdrv_ioctl_res_config(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
struct tsdrv_id_info *devdrv_get_one_stream_id(struct tsdrv_ts_resource *ts_resource);
int devdrv_alloc_stream_check(struct tsdrv_ts_resource *ts_resource, int *no_stream);
#endif
