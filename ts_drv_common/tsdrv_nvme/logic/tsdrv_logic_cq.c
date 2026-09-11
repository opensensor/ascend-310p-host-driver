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

#include <linux/mutex.h>

#include "ka_list_pub.h"
#include "tsdrv_device.h"
#include "devdrv_mailbox.h"
#include "logic_cq.h"
#include "devdrv_id.h"
#include "tsdrv_vsq.h"
#include "tsdrv_logic_cq.h"
#include "tsdrv_sqcq_timestamp.h"

int logic_sqcq_mbox_send(u32 devid, u32 tsid, u8 *mbox_msg)
{
#ifdef CFG_FEATURE_STARS
    return 0;
#else
    struct tsdrv_ts_resource *pm_ts_res = NULL;
    int mbox_ret = 0;
    int err;

    pm_ts_res = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, tsid);
    if (pm_ts_res == NULL) {
        return -EINVAL;
    }

    err = devdrv_mailbox_kernel_sync_no_feedback(&pm_ts_res->mailbox, mbox_msg,
        sizeof(struct logic_cqsq_mbox_msg), &mbox_ret);
    if ((err != 0) || (mbox_ret != 0)) {
        TSDRV_PRINT_ERR("logic sqcq mbox send fail, err=%d mbox_ret=%d devid=%u tsid=%u\n", err, mbox_ret, devid, tsid);
        return -ENODEV;
    }
    return 0;
#endif
}

int logic_cq_phy_cq_clean(struct tsdrv_id_inst *id_inst, u32 phy_cq, u32 logic_cqid, u32 type)
{
#ifdef CFG_FEATURE_STARS
        return 0;
#else

    struct tag_ts_logic_cq_report_msg *report = NULL;
    struct devdrv_cq_sub_info *cq_sub = NULL;
    struct devdrv_ts_cq_info *cq_info = NULL;
    struct logic_cq_info *logic_cq = NULL;
    u32 phy_cq_phase;
    u32 phy_cq_head;
    size_t offset;

    if (type == CTRL_SQCQ_TYPE) {
        return 0;
    }

    cq_info = tsdrv_get_cq_info(id_inst->devid, id_inst->fid, id_inst->tsid, phy_cq);
    if ((cq_info == NULL) || (cq_info->cq_sub == NULL) || (cq_info->alloc_status != SQCQ_ACTIVE)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Get phy cq_info fail. (devid=%u; fid=%u; tsid=%u)\n",
            id_inst->devid, id_inst->fid, id_inst->tsid);
        if (cq_info != NULL) {
            TSDRV_PRINT_ERR("(cqid=%u; cq_sub=%d; alloc_status=%d)\n", cq_info->index,
                cq_info->cq_sub == NULL, (int)cq_info->alloc_status);
        }
        return -EFAULT;
#endif
    }
    cq_sub = cq_info->cq_sub;
    ka_task_spin_lock_irq(&cq_sub->spinlock);
    logic_cq = logic_cq_info_get(id_inst->devid, id_inst->fid, id_inst->tsid);
    if (logic_cq == NULL) {
#ifndef TSDRV_UT
        ka_task_spin_unlock_irq(&cq_sub->spinlock);
        TSDRV_PRINT_ERR("get logic_cq fail, devid=%u tsid=%u\n", id_inst->devid, id_inst->tsid);
        return -ENOMEM;
#endif
    }
    offset = cq_sub->slot_size * cq_info->head;
    report = (struct tag_ts_logic_cq_report_msg *)((uintptr_t)cq_sub->virt_addr + offset);
    phy_cq_head = cq_info->head;
    phy_cq_phase = cq_info->phase;
    while (report->phase == phy_cq_phase) {
#ifndef TSDRV_UT
        /**
         * In the ARM environment:
         * 1.Out-of-order may cause the TS driver to read other members of the report structure (read instruction 1)
         *   to be executed before the phase comparison instruction (read instruction 2);
         * 2.The time when TS writes report is between [read instruction 1] and [read instruction 2];
         * This error timing may cause [Read instruction 1] to read the old value.
         */
        ka_rmb();
        phy_cq_head = phy_cq_head + 1;
        if (phy_cq_head == cq_sub->depth) {
            phy_cq_head = 0;
            phy_cq_phase = !phy_cq_phase;
        }
        if (report->logic_cq_id == logic_cqid) {
            TSDRV_PRINT_DEBUG("clean logic_cqid=%u phy_cqid=%u phy_cq_head=%u\n", logic_cqid, cq_sub->index,
                phy_cq_head);
            report->logic_cq_id = LOGIC_CQ_CLEANED_CQID;
        }
        report = (struct tag_ts_logic_cq_report_msg *)(uintptr_t)(cq_sub->virt_addr +
            (phy_cq_head * cq_sub->slot_size));
#endif
    }
    ka_task_spin_unlock_irq(&cq_sub->spinlock);
    return 0;
#endif
}

#ifndef TSDRV_UT
static int logic_sqcq_phy_cq_handle_chk(u32 devid, u32 tsid, struct devdrv_ts_cq_info *cq_info)
{
    struct devdrv_cq_sub_info *cq_sub = NULL;

    if ((devid >= TSDRV_MAX_DAVINCI_NUM) || (tsid >= tsdrv_get_dev_tsnum(devid))) {
        TSDRV_PRINT_ERR("phy cq handle fail, devid=%u tsid=%u\n", devid, tsid);
        return -EINVAL;
    }
    if ((cq_info == NULL) || (cq_info->cq_sub == NULL)) {
        TSDRV_PRINT_ERR("invalid cq info, devid=%u tsid=%u\n", devid, tsid);
        return -EFAULT;
    }
    if (cq_info->alloc_status != SQCQ_ACTIVE) {
        TSDRV_PRINT_ERR("cq is not allocated, cqid=%u devid=%u tsid=%u\n", cq_info->index, devid, tsid);
        return -ENODEV;
    }
    cq_sub = (struct devdrv_cq_sub_info *)(uintptr_t)cq_info->cq_sub;
    if (!cq_sub->virt_addr) {
        TSDRV_PRINT_ERR("cq que addr is NULL, cqid=%u devid=%u tsid=%u\n", cq_info->index, devid, tsid);
        return -EFAULT;
    }
    return 0;
}
#endif

void logic_sqcq_phy_cq_handler(u32 devid, u32 tsid, struct devdrv_ts_cq_info *cq_info)
{
#ifndef TSDRV_UT
    struct tag_ts_logic_cq_report_msg *report = NULL;
    struct devdrv_cq_sub_info *cq_sub = NULL;
    struct logic_cq_info *logic_cq = NULL;
    struct tsdrv_ts_resource *ts_res = NULL;
    u32 report_cnt = 0;
    size_t offset;
    u32 fid;
    int err;

    err = logic_sqcq_phy_cq_handle_chk(devid, tsid, cq_info);
    if (err != 0) {
        return;
    }
    cq_sub = (struct devdrv_cq_sub_info *)cq_info->cq_sub;
    fid = cq_info->vfid;
    if (fid >= TSDRV_MAX_FID_NUM) {
        TSDRV_PRINT_ERR("Invalid fid. (fid=%u)\n", fid);
        return;
    }

    ts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
    logic_cq = logic_cq_info_get(devid, fid, tsid);
    if (logic_cq == NULL) {
        TSDRV_PRINT_ERR("get logic_cq fail, devid=%u tsid=%u cqid=%u\n", devid, tsid, cq_info->index);
        return;
    }
    ka_task_spin_lock_bh(&logic_cq->lock);
    offset = cq_sub->slot_size * cq_info->head;
    report = (struct tag_ts_logic_cq_report_msg *)(uintptr_t)(cq_sub->virt_addr + offset);
    TSDRV_PRINT_DEBUG("(phy_cqid=%u; cq_head=%u; cq_size=0x%lx; cq_depth=%u; report->phase=%u; cq_info->phase=%u)\n",
        cq_info->index, cq_info->head, cq_sub->slot_size, cq_sub->depth, (u32)report->phase, cq_info->phase);
    while ((u32)report->phase == cq_info->phase) {
        /**
         * In the ARM environment:
         * 1.Out-of-order may cause the TS driver to read other members of the report structure (read instruction 1)
         *   to be executed before the phase comparison instruction (read instruction 2);
         * 2.The time when TS writes report is between [read instruction 1] and [read instruction 2];
         * This error timing may cause [Read instruction 1] to read the old value.
         */
        ka_rmb();
        tsdrv_sqcq_dfx_set_logic_cq_dispatch_tick(devid, fid, tsid, (u32)report->logic_cq_id, cq_info->head);
        err = logic_cq_dispatch(devid, fid, tsid, cq_info->index, logic_cq, report);
        if (err != 0) {
            TSDRV_PRINT_DEBUG("Failed to dispatch. (phy_cqid=%u; cq_head=%u; phase=%u)\n",
                cq_info->index, cq_info->head, cq_info->phase);
            break;
        }
        logic_cq_send_event(ts_res, report->stream_id);
        TSDRV_PRINT_DEBUG("Get logic type cq report. (logic_cqid=%u; SOP=%u; MOP=%u; EOP=%u; stream_id=%u; "
            "task_id=%u; error_type=%u; match_flag=%u; cq_info_tail=%u, slot_size=%u, phase=%u)\n",
            (u32)report->logic_cq_id, (u32)report->SOP, (u32)report->MOP, (u32)report->EOP,
            (u32)report->stream_id, (u32)report->task_id, (u32)report->error_type, (u32)report->match_flag,
            cq_info->tail, (u32)cq_info->slot_size, cq_info->phase);
        cq_info->head = cq_info->head + 1;
        if (cq_info->head == cq_sub->depth) {
            cq_info->head = 0;
            cq_info->phase = !cq_info->phase;
            TSDRV_PRINT_DEBUG("Attach cq depth and reverse phase. (phy_cqid=%u; cq_head=%d; phase_before=%u; "
                "phase_cur=%u)\n", cq_info->index, cq_info->head, !cq_info->phase, cq_info->phase);
        }
        offset = cq_sub->slot_size * cq_info->head;
        report = (struct tag_ts_logic_cq_report_msg *)(uintptr_t)(cq_sub->virt_addr + offset);
        report_cnt++;
    }
    ka_task_spin_unlock_bh(&logic_cq->lock);

    if (report_cnt != 0) {
        (void)tsdrv_ring_phy_cq_doorbell(devid, tsid, cq_info->index, cq_info->head);
        TSDRV_PRINT_DEBUG("report_cnt=%u phy_cqid=%u cq_head=%u\n", report_cnt, cq_info->index, cq_info->head);
    }
#endif
}

static bool is_logic_cq_belong_to_proc(struct logic_sqcq_ts_ctx *logic_ctx, u32 tsid, u32 logic_cqid)
{
    struct logic_cq_sub *cq_sub = NULL;

    if (!ka_list_empty_careful(&logic_ctx->logic_cq_list)) {
        ka_list_for_each_entry(cq_sub, &logic_ctx->logic_cq_list, cq_list) {
            if (cq_sub->cqid == logic_cqid) {
                return true;
            }
        }
    }

    return false;
}

bool tsdrv_is_logic_cq_belong_to_proc(struct tsdrv_ctx *ctx, u32 tsid, u32 logic_cqid)
{
    struct logic_sqcq_ts_ctx *logic_ctx = (struct logic_sqcq_ts_ctx *)ctx->ts_ctx[tsid].logic_sqcq_ctx;

    ka_task_mutex_lock(&logic_ctx->lock);

    if (!is_logic_cq_belong_to_proc(logic_ctx, tsid, logic_cqid)) {
#ifndef TSDRV_UT
        ka_task_mutex_unlock(&logic_ctx->lock);
        TSDRV_PRINT_ERR("logic cq is not belong to this ctx. (tsid=%u; logic_cqid=%u)\n", tsid, logic_cqid);
        return false;
#endif
    }

    ka_task_mutex_unlock(&logic_ctx->lock);
    return true;
}

static void tsdrv_construct_logic_cqe(struct devdrv_ctrl_report *ctrl_cqe, logic_cq_report_msg_t *logic_cqe)
{
    logic_cqe->stream_id = ctrl_cqe->stream_id;
    logic_cqe->task_id = ctrl_cqe->task_id;
    logic_cqe->sqId = ctrl_cqe->sq_id;
    logic_cqe->sqHead = ctrl_cqe->sq_head;
    logic_cqe->errorCode = ctrl_cqe->err_code;
    logic_cqe->match_flag = ctrl_cqe->match_flag;
    TSDRV_PRINT_DEBUG("stream_id=%u; task_id=%u; sqid=%u; sq_head=%u; err_code=%u; match_flag=%u\n",
        logic_cqe->stream_id, logic_cqe->task_id, logic_cqe->sqId, logic_cqe->sqHead,
        logic_cqe->errorCode, logic_cqe->match_flag);
}

struct devdrv_ctrl_report *tsdrv_get_ctrl_report(struct devdrv_ts_cq_info *cq_info)
{
    struct devdrv_cq_sub_info *cq_sub = (struct devdrv_cq_sub_info*)cq_info->cq_sub;
    struct devdrv_ctrl_report *report = (struct devdrv_ctrl_report *)(uintptr_t)(cq_sub->virt_addr +
        cq_info->slot_size * cq_info->head);

    TSDRV_PRINT_DEBUG("cqid=%u; slot_size=%u; cq_head=%u; cq_tail=%u; cq_phase=%u; cq_type=%d: phase=%u; "
        "sqid=%u; match_flag=%u; sq_head=%u; stream_id=%u; task_id=%u; err_code=%u\n",
        cq_info->index, cq_info->slot_size, cq_info->head, cq_info->tail, cq_info->phase, (int)cq_info->type,
        (u32)report->phase, (u32)report->sq_id, (u32)report->match_flag, (u32)report->sq_head,
        (u32)report->stream_id, (u32)report->task_id, report->err_code);

    return report;
}

void tsdrv_parse_cqe_to_logic_cq(struct tsdrv_ts_resource *ts_res, struct devdrv_ts_cq_info *cq_info)
{
    struct devdrv_ctrl_report *report = NULL;
    struct logic_cq_info *logic_cq = NULL;
    struct devdrv_cq_sub_info *cq_sub_info = NULL;
    struct tag_logic_cq_report_msg logic_cqe;
    u32 devid, fid, tsid, logic_cqid, sqid, sq_head;
    int ret, report_cnt = 0;

    devid = tsdrv_get_devid_by_res(ts_res);
    tsid = ts_res->tsid;
    fid = cq_info->vfid;

    cq_sub_info = (struct devdrv_cq_sub_info*)cq_info->cq_sub;
    report = tsdrv_get_ctrl_report(cq_info);

    logic_cq = logic_cq_info_get(devid, fid, tsid);
    logic_cqid = cq_info->bind_logic_cqid;
    ka_task_spin_lock_bh(&logic_cq->lock);

    while (report->phase == cq_info->phase) {
        ka_rmb();

        tsdrv_construct_logic_cqe(report, &logic_cqe);
        tsdrv_sqcq_dfx_set_logic_cq_dispatch_tick(devid, fid, tsid, logic_cqid, cq_info->head);
        ret = tsdrv_fill_logic_cqe(devid, fid, tsid, logic_cqid, logic_cq, &logic_cqe);
        if (ret != 0) {
            TSDRV_PRINT_WARN("Failed to dispatch. (phy_cqid=%u; cq_head=%u; phase=%u)\n",
                cq_info->index, cq_info->head, cq_info->phase);
            break;
        }

        cq_info->head = (cq_info->head + 1);
        if (cq_info->head == cq_info->depth) {
            cq_info->head = 0;
            cq_info->phase = !cq_info->phase;
            TSDRV_PRINT_DEBUG("Attach cq depth and reverse phase. (phy_cqid=%u; cq_head=%d; phase_before=%u; "
                "phase_cur=%u)\n", cq_info->index, cq_info->head, !cq_info->phase, cq_info->phase);
        }

        sqid = report->sq_id;
        sq_head = report->sq_head;
        (void)tsdrv_phy_sq_head_update(devid, fid, tsid, sqid, sq_head);
#ifdef CFG_FEATURE_SUPPORT_VSQ_MNG
        tsdrv_update_vsq_head(ts_res, sqid, sq_head);
#endif

        report = (struct devdrv_ctrl_report *)(uintptr_t)(cq_sub_info->virt_addr + cq_info->slot_size * cq_info->head);
        report_cnt++;
    }

    ka_task_spin_unlock_bh(&logic_cq->lock);

    if (report_cnt != 0) {
        TSDRV_PRINT_DEBUG("update normal cq head. cqid=%u, head=%u, tail=%u, report_cnt=%u\n",
            cq_info->index, cq_info->head, cq_info->tail, report_cnt);
        tsdrv_set_cq_doorbell(devid, tsid, cq_info->index, cq_info->head);
    }
}
