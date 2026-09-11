/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2023. All rights reserved.
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
#ifndef TSDRV_SYNC_H
#define TSDRV_SYNC_H

#include "tsdrv_log.h"
#include "tsdrv_id.h"
#include "tsdrv_interface.h"

enum {
    TSDRV_SHAKE_HANDS
};

enum {
    TSDRV_CHAN_SYNC_SSID,
    TSDRV_CHAN_SYNC_RES_ID,
    TSDRV_CHAN_SYNC_STREAM,
    TSDRV_CHAN_SYNC_SQ,
    TSDRV_CHAN_SYNC_CQ,
    TSDRV_CHAN_SYNC_EVENT,
    TSDRV_CHAN_SYNC_MODEL,
    TSDRV_CHAN_SYNC_CMO,
    TSDRV_CHAN_SYNC_NOTIFY,
    TSDRV_CHAN_SYNC_TASK,
    TSDRV_CHAN_SYNC_STREAM_SQ_CQ,
    TSDRV_CHAN_SYNC_CDQM_INIT,
    TSDRV_CHAN_SYNC_CDQM_CREATE,
    TSDRV_CHAN_SYNC_CDQM_DESTROY,
    TSDRV_CHAN_SYNC_CDQM_BATCH_ABNORMAL,
    TSDRV_CHAN_SYNC_PHY_CBCQSQ_INIT,
    TSDRV_CHAN_SYNC_PHY_CBCQSQ_EXIT,
    TSDRV_CHAN_SYNC_ID_MAP,
    TSDRV_CHAN_SYNC_ID_CHECK,
    TSDRV_CHAN_SYNC_MAX
};

#define TSDRV_MSG_SYNC_MAGIC 0x5A5A
#define TSDRV_MSG_SYNC_RSP_MAGIC 0xA5A5

struct tsdrv_msg_sync_stream_sqcq {
    u32 stream_id;
    u32 valid;
    u32 sq_id;
    u32 cq_id;
    int remote_tgid;
};

struct tsdrv_sync_event {
    u32 tsid;
    u32 vfid;
    u32 phy_id;
    u32 virt_id;
};

struct tsdrv_id_map_sync_info {
    enum tsdrv_id_type id_type;
    union {
        struct tsdrv_sync_event event;
    };
};

enum {
    MSG_CMD_TYPE_SYNC = 0x60,
    MSG_CMD_TYPE_ASYNC
};

s32 tsdrv_msg_alloc_sync_stream(u32 dev_id, u32 tsid);
s32 tsdrv_msg_alloc_sync_ipc_event(u32 dev_id, u32 tsid);
s32 tsdrv_msg_alloc_sync_event(u32 dev_id, u32 tsid);
s32 tsdrv_msg_alloc_sync_id(u32 dev_id, u32 tsid, enum tsdrv_id_type id_type);
s32 tsdrv_msg_alloc_sync_cq(u32 dev_id, u32 tsid);
s32 tsdrv_msg_alloc_sync_sq(u32 dev_id, u32 tsid);
s32 tsdrv_msg_alloc_sync_task(u32 dev_id, u32 tsid);
s32 tsdrv_msg_alloc_sync_model(u32 dev_id, u32 tsid);
s32 tsdrv_msg_alloc_sync_notify(u32 dev_id, u32 tsid);
s32 tsdrv_msg_sync_stream_sqcq(u32 dev_id, u32 tsid, u32 stream_id, u32 valid, u32 sq_id, u32 cq_id);
s32 tsdrv_msg_sync_clear_stream_sqcq(u32 dev_id, u32 tsid, u32 stream_id, u32 sq_id, u32 cq_id);
s32 tsdrv_chan_msg_proc(u32 devid, void *msg, u32 msg_len, u32 *ack_len);
void tsdrv_set_send_msg_proc(int (*sync_msg_send)(u32 devid, void *tx, size_t tx_size));
s32 tsdrv_sync_msg_send(u32 dev_id, struct tsdrv_msg_info *msg_info);
s32 tsdrv_msg_sync_id_map(u32 dev_id, struct tsdrv_id_map_sync_info id_map);
s32 tsdrv_msg_check_res_id(u32 dev_id, u32 tsid, u32 id_type, u32 id);

#ifdef CFG_FEATURE_CMO
#include "tsdrv_cmo_id.h"
s32 tsdrv_msg_alloc_sync_cmo(u32 dev_id, u32 tsid);
#endif

int tsdrv_msg_phy_cbcqsq_init(u32 dev_id, u32 tsid);
s32 tsdrv_msg_phy_cbcqsq_exit(u32 dev_id, u32 tsid);

bool tsdrv_set_id_sync_non_idle(struct tsdrv_ts_resource *ts_res, enum tsdrv_id_type id_type, u32 status);
void tsdrv_set_id_sync_idle(struct tsdrv_ts_resource *ts_res, enum tsdrv_id_type id_type);

#endif
