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

#include "ts_agent_interface.h"
#include "securec.h"
#include "ts_agent_common.h"
#include "ts_agent_log.h"
#include "virtmng_interface.h"
#include "ts_agent_resource.h"
#include "ts_agent_vsq_worker.h"
#include "hvtsdrv_tsagent.h"

#define TS_AGENT_MAILBOX_MESSAGE_VALID 0x5A5AU
#define TS_AGENT_MAILBOX_CREATE_TASK_CMD_SQCQ 1U
#define TS_AGENT_MAILBOX_CREATE_VF_MACHINE 30U
#define TS_AGENT_MAILBOX_RELEASE_VF_MACHINE 31U
#define TS_AGENT_MAILBOX_CREATE_LOGIC_CQ 35U

// define refer to tsch_mailbox.h
typedef struct ts_agent_create_sqcq_info {
    struct devdrv_mailbox_message_header header;
    uint64_t sq_addr;
    uint64_t cq_addr0;
    uint16_t sq_idx;      /* invalid:0xFFFF */
    uint16_t cq_idx0;     /* sq's return */
    uint8_t app_flag : 2; /* 0:offline, 1:online */
    uint8_t vf_id : 6;
    uint8_t sq_cq_side; /* online: bit 0 sq side, bit 1 cq side. device: 0 host: 1  */
    uint8_t sqe_size;
    uint8_t cqe_size;
    uint16_t cqdepth;
    uint16_t sqdepth;
    uint32_t pid;
    uint16_t cq_irq;
    uint16_t smmu_sub_streamid;
    uint32_t info[5]; /* runtime self-define, info[0] is streamid */
} ts_agent_create_sqcq_t;

typedef struct ts_agent_create_logic_cq {
    struct devdrv_mailbox_message_header header;
    uint64_t phy_cq_addr;
    uint16_t cqe_size;
    uint16_t cq_depth;
    uint32_t vpid;
    uint16_t logic_cqid;
    uint16_t phy_cqid;
    uint16_t cq_irq;
    uint8_t app_flag;
    uint8_t thread_bind_irq_flag;
    uint8_t vf_id;
    uint8_t reserved[3]; // 3:for byte align
    uint32_t info[5];
} ts_agent_create_logic_cq_t;

typedef struct ts_agent_create_vf_info {
    struct devdrv_mailbox_message_header header;
    u8 vf_id;
    u8 aic_num; // vm alloc aicore num
    u8 reserved; // reserved 1 for byte alignment
    u8 vfg_id;
    u32 vf_aicpu_bitmap;
    u32 vfg_aicpu_bitmap;
} ts_agent_create_vf_info_t;

typedef struct ts_agent_release_vf_info {
    struct devdrv_mailbox_message_header header;
    u8 vf_id;
    u8 reserved[3]; // reserved 3 for byte alignment
} ts_agent_release_vf_info_t;

static inline bool check_dev_id(u32 dev_id)
{
    if (dev_id >= TS_AGENT_MAX_DEVICE_NUM) {
        ts_agent_err("dev_id is invalid, range[0, %u), dev_id=%u", TS_AGENT_MAX_DEVICE_NUM, dev_id);
        return false;
    }
    return true;
}

static inline bool check_vf_id(u32 vf_id)
{
    if ((vf_id < TS_AGENT_VF_ID_MIN) || (vf_id > TS_AGENT_VF_ID_MAX)) {
        ts_agent_err("vf_id is invalid, range[%u, %u], vf_id=%u",
                     TS_AGENT_VF_ID_MIN, TS_AGENT_VF_ID_MAX, vf_id);
        return false;
    }
    return true;
}

static inline bool check_ts_id(u32 ts_id)
{
    if (ts_id >= TS_AGENT_MAX_TS_NUM) {
        ts_agent_err("ts_id is invalid, range[0, %u), ts_id=%u", TS_AGENT_MAX_TS_NUM, ts_id);
        return false;
    }
    return true;
}

static inline bool check_vsq_type(enum vsqcq_type vsq_type)
{
    if (vsq_type != NORMAL_VSQCQ_TYPE && vsq_type != CALLBACK_VSQCQ_TYPE) {
        ts_agent_err("vsq_type is invalid, only support NORMAL_VSQCQ_TYPE(%d) and CALLBACK_VSQCQ_TYPE(%d), vsq_type=%d",
                     NORMAL_VSQCQ_TYPE, CALLBACK_VSQCQ_TYPE, vsq_type);
        return false;
    }
    return true;
}

static int send_create_vf_to_ts(u32 dev_id, u32 vf_id, u32 ts_id, struct vmng_soc_resource_enquire *vf_res)
{
    int ret;
    struct tsdrv_mbox_data mbox_data = {0};
    ts_agent_create_vf_info_t create_vf_info = {{0}, 0, 0, 0, 0, 0};
    create_vf_info.header.valid = TS_AGENT_MAILBOX_MESSAGE_VALID;
    create_vf_info.header.cmd_type = TS_AGENT_MAILBOX_CREATE_VF_MACHINE;
    create_vf_info.header.result = 0;
    create_vf_info.vf_id = (u8) vf_id;
    create_vf_info.aic_num = (u8) vf_res->each.stars_static.aic;
    create_vf_info.vfg_id = (u8) vf_res->each.vfg.vfg_id;
    create_vf_info.vf_aicpu_bitmap = (u32) vf_res->each.stars_refresh.device_aicpu;
    create_vf_info.vfg_aicpu_bitmap = (u32) vf_res->vfg.stars_refresh.device_aicpu;
    mbox_data.msg = &create_vf_info;
    mbox_data.msg_len = (u32)sizeof(ts_agent_create_vf_info_t);
    ret = tsdrv_mailbox_send_sync(dev_id, ts_id, &mbox_data);
    if (ret != 0) {
        ts_agent_err("send create vf to ts failed, ret=%d, dev_id=%u, ts_id=%u, vf_id=%u, vfg_id =%u, cmd_type=%u",
                     ret, dev_id, ts_id, vf_id, vf_res->each.vfg.vfg_id, TS_AGENT_MAILBOX_CREATE_VF_MACHINE);
        return ret;
    }
    ts_agent_info("send create vf to ts success, dev_id=%u, ts_id=%u, vf_id=%u, vfg_id=%u, cmd_type=%u",
                  dev_id, ts_id, vf_id, vf_res->each.vfg.vfg_id, TS_AGENT_MAILBOX_CREATE_VF_MACHINE);
    return ret;
}

static void send_destroy_vf_to_ts(u32 dev_id, u32 vf_id, u32 ts_id)
{
    int ret;
    struct tsdrv_mbox_data mbox_data = {0};
    ts_agent_release_vf_info_t release_vf_info = {{0}, 0, {0}};
    release_vf_info.header.valid = TS_AGENT_MAILBOX_MESSAGE_VALID;
    release_vf_info.header.cmd_type = TS_AGENT_MAILBOX_RELEASE_VF_MACHINE;
    release_vf_info.header.result = 0;
    release_vf_info.vf_id = (u8) vf_id;
    mbox_data.msg = &release_vf_info;
    mbox_data.msg_len = sizeof(ts_agent_release_vf_info_t);
    ret = tsdrv_mailbox_send_sync(dev_id, ts_id, &mbox_data);
    if (ret != 0) {
        ts_agent_err("send release vf to ts failed, ret=%d, dev_id=%u, ts_id=%u, vf_id=%u, cmd_type=%u",
                     ret, dev_id, ts_id, vf_id, TS_AGENT_MAILBOX_RELEASE_VF_MACHINE);
        return;
    }
    ts_agent_info("send release vf to ts success, dev_id=%u, ts_id=%u, vf_id=%u, cmd_type=%u",
                  dev_id, ts_id, vf_id, TS_AGENT_MAILBOX_RELEASE_VF_MACHINE);
}

void ts_agent_vf_rollback_proc(u32 dev_id, u32 vf_id, bool record[])
{
    u32 ts_id;

    for (ts_id = 0; ts_id < TS_AGENT_MAX_TS_NUM; ++ts_id) {
        if (record[ts_id]) {
            ts_agent_info("need rollback, dev_id=%u, vf_id=%u, ts_id=%u.", dev_id, vf_id, ts_id);
            // rollback
            destroy_vf_worker(dev_id, vf_id, ts_id);
            // send vf destroy msg to TS
            send_destroy_vf_to_ts(dev_id, vf_id, ts_id);
        }
    }
}

int ts_agent_vf_create(u32 dev_id, u32 vf_id)
{
    int ret = 0;
    u32 ts_id;
    u32 vsq_num = 0;
    struct vmng_soc_resource_enquire vf_res;
    bool record[TS_AGENT_MAX_TS_NUM] = {false};
    ts_agent_debug("vf create begin, dev_id=%u, vf_id=%u.", dev_id, vf_id);
    if (!check_dev_id(dev_id)) {
        ts_agent_err("vf create failed as dev_id check failed, dev_id=%u, vf_id=%u.", dev_id, vf_id);
        return -EINVAL;
    }
    if (!check_vf_id(vf_id)) {
        ts_agent_err("vf create failed as vf_id check failed, dev_id=%u, vf_id=%u.", dev_id, vf_id);
        return -EINVAL;
    }
    for (ts_id = 0; ts_id < TS_AGENT_MAX_TS_NUM; ++ts_id) {
        ret = get_vf_vsq_num(dev_id, vf_id, ts_id, &vsq_num);
        if (ret != 0) {
            break;
        }

        ret = create_vf_worker(dev_id, vf_id, ts_id, vsq_num);
        if (ret != 0) {
            ts_agent_err("create vf worker failed, dev_id=%u, vf_id=%u, ret=%d.", dev_id, vf_id, ret);
            break;
        }
        record[ts_id] = true;

        if (vf_id == 0U) {
            /* pf no need to send msg to TS */
            continue;
        }

        (void)memset_s(&vf_res, sizeof(struct vmng_soc_resource_enquire), 0, sizeof(struct vmng_soc_resource_enquire));
        ret = get_vf_info(dev_id, vf_id, &vf_res);
        if (ret != 0) {
            break;
        }

        // send vf create msg to TS
        ret = send_create_vf_to_ts(dev_id, vf_id, ts_id, &vf_res);
        if (ret != 0) {
            break;
        }
    }

    if (ret == 0) {
        ts_agent_info("create vf worker success, dev_id=%u, vf_id=%u, vfg_id=%u, aic_num=%u, vfg_aicpu_bitmap=%u.",
            dev_id, vf_id, vf_res.each.vfg.vfg_id, vf_res.each.stars_static.aic,
            vf_res.vfg.stars_refresh.device_aicpu);
        return 0;
    }

    ts_agent_vf_rollback_proc(dev_id, vf_id, record);

    ts_agent_err("vf create failed, dev_id=%u, vf_id=%u, vfg_id=%u, aic_num=%u, vfg_aicpu_bitmap=%u, ret=%d.", dev_id,
        vf_id, vf_res.each.vfg.vfg_id, vf_res.each.stars_static.aic, vf_res.vfg.stars_refresh.device_aicpu, ret);
    return ret;
}

void ts_agent_vf_destroy(u32 dev_id, u32 vf_id)
{
    u32 ts_id;
    ts_agent_debug("vf destroy begin, dev_id=%u, vf_id=%u.", dev_id, vf_id);
    if (!check_dev_id(dev_id)) {
        ts_agent_err("vf shutdown failed as dev_id check failed, dev_id=%u, vf_id=%u.", dev_id, vf_id);
        return;
    }
    if (!check_vf_id(vf_id)) {
        ts_agent_err("vf shutdown proc failed as vf_id check failed, dev_id=%u, vf_id=%u.", dev_id, vf_id);
        return;
    }
    for (ts_id = 0; ts_id < TS_AGENT_MAX_TS_NUM; ++ts_id) {
        destroy_vf_worker(dev_id, vf_id, ts_id);
        if (vf_id == 0U) {
            /* pf no need to send msg to TS */
            continue;
        }
        // send vf destroy msg to TS
        send_destroy_vf_to_ts(dev_id, vf_id, ts_id);
    }
    ts_agent_info("vf destroy end, dev_id=%u, vf_id=%u.", dev_id, vf_id);
}

int ts_agent_vsq_proc(struct tsdrv_id_inst *id_inst, u32 vsq_id, enum vsqcq_type vsq_type, u32 sqe_num)
{
    u32 dev_id = id_inst->devid;
    u32 vf_id = id_inst->fid;
    u32 ts_id = id_inst->tsid;
    ts_agent_debug("vf proc, dev_id=%u, vf_id=%u, ts_id=%u, vsq_id=%u, vsq_type=%d, sqe_num=%u.",
                   dev_id, vf_id, ts_id, vsq_id, vsq_type, sqe_num);
    if (!check_dev_id(dev_id) || !check_vf_id(vf_id) || !check_ts_id(ts_id) || !check_vsq_type(vsq_type)) {
        ts_agent_err("vsq proc failed as param check failed, dev_id=%u, vf_id=%u, ts_id=%u, vsq_id=%u, vsq_type=%d.",
                     dev_id, vf_id, ts_id, vsq_id, vsq_type);
        return -EINVAL;
    }

    return schedule_vsq_work(id_inst, vsq_id, vsq_type, sqe_num);
}

static int convert_mailbox_sqcq_info(void *mailbox_msg, u32 msg_len, struct hvtsdrv_trans_mailbox_ctx *trans_ctx)
{
    ts_agent_create_sqcq_t *create_sqcq = NULL;
    vsq_base_info_t convert_base_info = {};
    int ret;
    u32 v_stream_id;
    u32 disable_thread;
    u16 stream_id = 0;

    if (msg_len < sizeof(ts_agent_create_sqcq_t)) {
        ts_agent_err("msg_len is not enough for mailbox create sqcq, msg_len=%u, create_sqcq=%zu.", msg_len,
            sizeof(ts_agent_create_sqcq_t));
        return -EINVAL;
    }
    create_sqcq = (ts_agent_create_sqcq_t *)mailbox_msg;

    // 0 is stream_id
    v_stream_id = create_sqcq->info[0];
    // info[3] is disable thread
    disable_thread = create_sqcq->info[3];
    if (v_stream_id > U16_MAX) {
        ts_agent_err("info[0]=%u must be <= U16_MAX.", v_stream_id);
        return -EINVAL;
    }

    if (disable_thread != trans_ctx->disable_thread) {
        ts_agent_err("info[3]=%u must be same as drv disable_thread=%u.", disable_thread, trans_ctx->disable_thread);
        return -EINVAL;
    }

    convert_base_info.dev_id = trans_ctx->dev_id;
    convert_base_info.vf_id = trans_ctx->fid;
    convert_base_info.ts_id = trans_ctx->ts_id;

    ret = convert_stream_id(&convert_base_info, (u16)v_stream_id, &stream_id);
    if (ret != 0) {
        ts_agent_err("convert stream_id failed, v_stream_id=%u, dev_id=%u, vf_id=%u.", v_stream_id,
            convert_base_info.dev_id, convert_base_info.vf_id);
        return ret;
    }
    // disable thread use
    if (trans_ctx->disable_thread != 0) {
        // info[4] is share sq id
        u32 *v_share_sq_id = &(create_sqcq->info[4]);
        u16 share_sq_id = 0;
        if (*v_share_sq_id > U16_MAX) {
            ts_agent_err("info[4]=%u must be <= U16_MAX.", *v_share_sq_id);
            return -EINVAL;
        }
        ret = convert_sq_id(&convert_base_info, *v_share_sq_id, &share_sq_id);
        if (ret != 0) {
            ts_agent_err("convert_sq_id failed, v_share_sq_id=%u, dev_id=%u, vf_id=%u.", *v_share_sq_id,
                convert_base_info.dev_id, convert_base_info.vf_id);
            return ret;
        }
        *v_share_sq_id = share_sq_id;
    }
    // high 16 bits save vir stream id
    create_sqcq->info[0] = (v_stream_id << 16U) | (uint32_t)stream_id;
    return 0;
}

static int convert_mailbox_logic_cq_info(void *mailbox_msg, u32 msg_len, struct hvtsdrv_trans_mailbox_ctx *trans_ctx)
{
    ts_agent_create_logic_cq_t *create_logic_cq = NULL;
    vsq_base_info_t convert_base_info = {};
    int ret;
    u32 v_stream_id;
    u16 stream_id = 0;

    if (msg_len < sizeof(ts_agent_create_logic_cq_t)) {
        ts_agent_err("msg_len is not enough for mailbox create logic cq, msg_len=%u, create_logic_cq=%zu.",
            msg_len, sizeof(ts_agent_create_logic_cq_t));
        return -EINVAL;
    }
    create_logic_cq = (ts_agent_create_logic_cq_t *)mailbox_msg;

    // 0 is stream_id
    v_stream_id = create_logic_cq->info[0];

    // U32_MAX means not shared cq, no need convert
    if (v_stream_id == U32_MAX) {
        ts_agent_debug("info[0]=%u is U32_MAX, no need convert.", v_stream_id);
        return 0;
    }

    if (v_stream_id > U16_MAX) {
        ts_agent_err("logic cq info[0]=%u must be <= U16_MAX.", v_stream_id);
        return -EINVAL;
    }

    convert_base_info.dev_id = trans_ctx->dev_id;
    convert_base_info.vf_id = trans_ctx->fid;
    convert_base_info.ts_id = trans_ctx->ts_id;

    ret = convert_stream_id(&convert_base_info, (u16)v_stream_id, &stream_id);
    if (ret != 0) {
        ts_agent_err("convert logic cq stream_id failed, v_stream_id=%u, dev_id=%u, vf_id=%u.", v_stream_id,
            convert_base_info.dev_id, convert_base_info.vf_id);
        return ret;
    }
    create_logic_cq->info[0] = stream_id;
    return 0;
}

// just trans runtime custom fields
int ts_agent_trans_mailbox_msg(void *mailbox_msg, u32 msg_len, struct hvtsdrv_trans_mailbox_ctx *trans_ctx)
{
    struct devdrv_mailbox_message_header *msg_header = NULL;
    if (mailbox_msg == NULL) {
        ts_agent_err("mailbox_msg is null.");
        return -EINVAL;
    }
    if (trans_ctx == NULL) {
        ts_agent_err("trans_ctx is null.");
        return -EINVAL;
    }
    if (msg_len < sizeof(struct devdrv_mailbox_message_header)) {
        ts_agent_err("msg_len is not enough for mailbox header, msg_len=%u, header=%zu.", msg_len,
            sizeof(struct devdrv_mailbox_message_header));
        return -EINVAL;
    }

    msg_header = (struct devdrv_mailbox_message_header *)mailbox_msg;
    ts_agent_debug("cmd_type=%u, dev_id=%u, vf_id=%u, ts_id=%u, disable_thread=%u, msg_len=%u.",
        msg_header->cmd_type, trans_ctx->dev_id, trans_ctx->fid, trans_ctx->ts_id, trans_ctx->disable_thread, msg_len);

    if (msg_header->cmd_type == TS_AGENT_MAILBOX_CREATE_TASK_CMD_SQCQ) {
        return convert_mailbox_sqcq_info(mailbox_msg, msg_len, trans_ctx);
    }
    if (msg_header->cmd_type == TS_AGENT_MAILBOX_CREATE_LOGIC_CQ) {
        return convert_mailbox_logic_cq_info(mailbox_msg, msg_len, trans_ctx);
    }
    ts_agent_debug("mailbox no need convert, cmd_type=%u.", msg_header->cmd_type);
    return 0;
}
