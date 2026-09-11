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

#ifndef _HVTSDRV_TSAGENT_H_
#define _HVTSDRV_TSAGENT_H_

#include "tsdrv_interface.h"

enum vsqcq_type {
    NORMAL_VSQCQ_TYPE = 0,
    CALLBACK_VSQCQ_TYPE
};

struct hvtsdrv_vsq_info {
    const void *vsq_base_addr; // vsq base addr alloced by drv
    u32 vsq_dep;  // 1024
    u32 vsq_slot_size; // 64
};

struct hvtsdrv_vsq_head_tail {
    u32 head;
    u32 tail;
};

struct hvtsdrv_vsq_slot_info {
    u32 vsq_id;
    const void *vsq_slot_addr; // alloced by tsagent
    u32 vsq_slot_size;
};

struct hvtsdrv_id_v2p {
    enum tsdrv_id_type id_type;
    u32 virt_id;
    u32 phy_id;
};

struct hvtsdrv_trans_mailbox_ctx {
    u32 dev_id;
    u32 fid;
    u32 ts_id;
    u32 disable_thread;  // 0:with thread, 1:disable thread
};

struct hvtsdrv_tsagent_ops {
    int (*tsagent_vf_create)(u32 devid, u32 fid);
    void (*tsagent_vf_destroy)(u32 devid, u32 fid);
    int (*tsagent_vsq_proc)(struct tsdrv_id_inst *id_inst, u32 vsq_id, enum vsqcq_type type, u32 sqe_num);
    // just trans runtime custom fields
    int (*tsagent_trans_mailbox_msg)(void *mailbox_msg, u32 msg_len, struct hvtsdrv_trans_mailbox_ctx *trans_ctx);
};

struct hvtsdrv_tsagent_ops *hvtsdrv_get_tsagent_ops(void);

/* inferface for tsagent */
void hvtsdrv_tsagent_register(struct hvtsdrv_tsagent_ops *ops);
void hvtsdrv_tsagent_unregister(void);
int hvtsdrv_sq_write(u32 devid, u32 fid, u32 tsid, struct hvtsdrv_vsq_slot_info *data);
void hvtsdrv_sq_irq_trigger(u32 devid, u32 fid, u32 tsid, u32 vsq_id);
int hvtsdrv_resid_v2p(u32 devid, u32 fid, u32 tsid, struct hvtsdrv_id_v2p *data);
int hvtsdrv_get_res_num(u32 devid, u32 fid, u32 tsid, enum tsdrv_id_type id_type, u32 *num);
int hvtsdrv_get_vsq_head_and_tail(u32 devid, u32 fid, u32 tsid, u32 vsq_id, struct hvtsdrv_vsq_head_tail *out);
int hvtsdrv_get_vsq_info(u32 devid, u32 fid, u32 tsid, u32 vsq_id, struct hvtsdrv_vsq_info *vsq_info);
int hvtsdrv_event_id_v2p(u32 devid, u32 tsid, u32 fid, u32 virt, u32 *phy);
int hvdevmng_get_aicore_num(u32 devid, u32 fid, u32 *aicore_num);
int hvdevmng_get_aicpu_num(u32 devid, u32 fid, u32 *aicpu_num, u32 *aicpu_bitmap);
int hvdevmng_get_chip_type(u32 devid, u32 *chip_type);
void hvdevmng_set_dev_ts_resource(u32 devid, u32 fid, u32 tsid, void *data);
void hvtsdrv_fill_trans_info_mbox(u32 devid, u32 fid, u32 tsid, u32 disable_thread,
    struct hvtsdrv_trans_mailbox_ctx *trans_mbox);
int hvtsdrv_trans_info_msg(struct hvtsdrv_trans_mailbox_ctx *trans_mbox, void *info, size_t msg_len);
int tsdrv_cdqid_is_belong_to_proc(struct tsdrv_id_inst *id_inst, pid_t tgid, u32 id);
int tsdrv_id_is_belong_to_proc(struct tsdrv_id_inst *id_inst, pid_t tgid, u32 id, enum tsdrv_id_type id_type);

#endif

