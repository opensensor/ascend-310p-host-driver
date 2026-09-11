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
#include <linux/atomic.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include "ka_list_pub.h"
#include "ka_task_pub.h"
#include "ka_base_pub.h"
#include "ka_system_pub.h"
#include "devdrv_cqsq.h"
#include "devdrv_id.h"
#include "devdrv_recycle.h"
#include "tsdrv_rts_streamid.h"
#include "tsdrv_drvops.h"
#include "tsdrv_device.h"
#include "tsdrv_nvme.h"
#include "shm_sqcq.h"
#if defined(CFG_SOC_PLATFORM_MINI) && !defined(CFG_SOC_PLATFORM_MINIV2) && !defined(CFG_SOC_PLATFORM_MINIV3)
#include "devdrv_sync_cbctrl.h"
#else
#include "devdrv_cb.h"
#endif
#include "hvtsdrv_id.h"
#include "tsdrv_vsq.h"

#define TS_REPORT_TYPE_RECYCLE_SQ_FINISHED     3
#define TS_REPORT_TYPE_RECYCLE_STREAM_FINISHED 4
#if defined(CFG_SOC_PLATFORM_MINI) && !defined(CFG_SOC_PLATFORM_MINIV2) && !defined(CFG_SOC_PLATFORM_MINIV3)
#define TS_REPORT_TYPE_RECYCLE_CBSQCQ_FINISHED 5
#else
#define TS_REPORT_TYPE_RECYCLE_NOTIFY_FINISHED 5
#endif
#define TS_REPORT_TYPE_RECYCLE_EVENT_FINISHED  6
#define TS_REPORT_TYPE_RECYCLE_CQ_FINISHED     7
#define TS_DFX_SHOW_NUM                        20
#define TS_DFX_SHOW_REPORT_NUM                 2
#define TS_MSECS_TO_JIFFIES                   1000

void tsdrv_recycle_model_id(struct tsdrv_ts_resource *ts_resource, struct tsdrv_ctx *ctx)
{
    struct tsdrv_id_info *model_info = NULL;
    ka_list_head_t *pos = NULL, *n = NULL;
    u32 tsid;

    tsid = ts_resource->tsid;
    if (ka_list_empty_careful(&ctx->ts_ctx[tsid].id_ctx[TSDRV_MODEL_ID].id_list) != 0) {
        return;
    }

    ka_list_for_each_safe(pos, n, &ctx->ts_ctx[tsid].id_ctx[TSDRV_MODEL_ID].id_list) {
        model_info = ka_list_entry(pos, struct tsdrv_id_info, list);
        (void)devdrv_free_model_id(ctx, tsid, model_info->phy_id);
    }

    return;
}

void tsdrv_recycle_notify_id(struct tsdrv_ts_resource *ts_resource, struct tsdrv_ctx *ctx)
{
    struct tsdrv_id_info *notify_info = NULL;
    ka_list_head_t *pos = NULL, *n = NULL;
    u32 tsid = ts_resource->tsid;

    if (ka_list_empty_careful(&ctx->ts_ctx[tsid].id_ctx[TSDRV_NOTIFY_ID].id_list) != 0) {
        return;
    }

    ka_list_for_each_safe(pos, n, &ctx->ts_ctx[tsid].id_ctx[TSDRV_NOTIFY_ID].id_list) {
        notify_info = ka_list_entry(pos, struct tsdrv_id_info, list);
        (void)devdrv_free_one_notify_id(ts_resource, ctx, notify_info->phy_id, DEVDRV_NOTIFY_NOT_INFORM_TS);
    }

    return;
}

void tsdrv_recycle_event_id(struct tsdrv_ts_resource *ts_resource, struct tsdrv_ctx *ctx)
{
    struct tsdrv_id_info *event_info = NULL;
    ka_list_head_t *pos = NULL, *n = NULL;
    u32 tsid = ts_resource->tsid;

    if (ka_list_empty_careful(&ctx->ts_ctx[tsid].id_ctx[TSDRV_EVENT_SW_ID].id_list) != 0) {
        return;
    }

    ka_list_for_each_safe(pos, n, &ctx->ts_ctx[tsid].id_ctx[TSDRV_EVENT_SW_ID].id_list) {
        event_info = ka_list_entry(pos, struct tsdrv_id_info, list);
        (void)devdrv_free_event_id(ctx, tsid, event_info->phy_id);
    }

    return;
}

void tsdrv_recycle_ipc_event_id(struct tsdrv_ts_resource *ts_resource, struct tsdrv_ctx *ctx)
{
    struct tsdrv_id_info *event_info = NULL;
    ka_list_head_t *pos = NULL, *n = NULL;
    u32 tsid = ts_resource->tsid;

    /* The hw event and sw event all link sw type ctx list, and ipc event use hw type ctx list.
     * If ipc event and event id all link sw type ctx list, the id info struct should contains some info to
     * decide the which brach it should to go. So, in here, it uses the unused hw type ctx list to do recycle.
    */
    if (ka_list_empty_careful(&ctx->ts_ctx[tsid].id_ctx[TSDRV_EVENT_HW_ID].id_list) != 0) {
        return;
    }
    ka_list_for_each_safe(pos, n, &ctx->ts_ctx[tsid].id_ctx[TSDRV_EVENT_HW_ID].id_list) {
        event_info = ka_list_entry(pos, struct tsdrv_id_info, list);
        (void)devdrv_free_ipc_event_id(ctx, tsid, event_info->id, DEVDRV_NOTIFY_NOT_INFORM_TS);
    }

    return;
}

static void tsdrv_clear_sq_map_info(struct devdrv_ts_sq_info *sq_info)
{
#ifndef TSDRV_UT
    sq_info->sqDbVaddr = 0;
    sq_info->sqHeadVaddr = 0;
    sq_info->sqTailVaddr = 0;
#endif
}

void tsdrv_recycle_all_sq(struct tsdrv_ts_resource *ts_resource, struct tsdrv_ctx *ctx)
{
    struct tsdrv_id_info *sq_id_info = NULL;
    struct devdrv_ts_sq_info *sq_info = NULL;
    ka_list_head_t *pos = NULL, *n = NULL;
    u32 tsid = ts_resource->tsid;
    phys_addr_t info_mem_addr;

    if (ka_list_empty_careful(&ctx->ts_ctx[tsid].id_ctx[TSDRV_SQ_ID].id_list) != 0) {
        return;
    }
    info_mem_addr = ts_resource->mem_info[DEVDRV_INFO_MEM].virt_addr;

#ifndef TSDRV_UT
    ka_list_for_each_safe(pos, n, &ctx->ts_ctx[tsid].id_ctx[TSDRV_SQ_ID].id_list) {
        sq_id_info = ka_list_entry(pos, struct tsdrv_id_info, list);
        sq_info = devdrv_calc_sq_info(info_mem_addr, sq_id_info->phy_id);
        (void)devdrv_exit_sq(ts_resource, ctx, sq_info);
        tsdrv_clear_sq_map_info(sq_info);
        tsdrv_free_sq(ts_resource, ctx, sq_info);
#ifdef CFG_FEATURE_SUPPORT_VSQ_MNG
        tsdrv_vsq_mem_destroy(ts_resource, (u32)sq_id_info->phy_id);
        tsdrv_clear_vsq_info(ts_resource, (u32)sq_id_info->phy_id);
#endif
    }
#endif
}

void tsdrv_recycle_sq(struct tsdrv_ts_resource *ts_resource, struct tsdrv_ctx *ctx, u32 sqid)
{
#ifndef TSDRV_UT
    struct devdrv_ts_sq_info *sq_info = NULL;
    struct tsdrv_id_info *sq_id_info = NULL;
    u32 tsid = ts_resource->tsid;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;
    phys_addr_t info_mem_addr;

    if (ka_list_empty_careful(&ctx->ts_ctx[tsid].id_ctx[TSDRV_SQ_ID].id_list) != 0) {
        return;
    }
    info_mem_addr = ts_resource->mem_info[DEVDRV_INFO_MEM].virt_addr;

    ka_list_for_each_safe(pos, n, &ctx->ts_ctx[tsid].id_ctx[TSDRV_SQ_ID].id_list) {
        sq_id_info = ka_list_entry(pos, struct tsdrv_id_info, list);
        if (sq_id_info->phy_id == sqid) {
            sq_info = devdrv_calc_sq_info(info_mem_addr, sqid);
            (void)devdrv_exit_sq(ts_resource, ctx, sq_info);
            tsdrv_free_sq(ts_resource, ctx, sq_info);
#ifdef CFG_FEATURE_SUPPORT_VSQ_MNG
            tsdrv_vsq_mem_destroy(ts_resource, (u32)sq_id_info->phy_id);
            tsdrv_clear_vsq_info(ts_resource, (u32)sq_id_info->phy_id);
#endif
        }
    }
#endif
}

void tsdrv_recycle_streams(struct tsdrv_ts_resource *ts_resource, struct tsdrv_ctx *ctx)
{
    struct tsdrv_id_info *stream_info = NULL;
    ka_list_head_t *pos = NULL, *n = NULL;
    u32 tsid = ts_resource->tsid;

    if (ka_list_empty_careful(&ctx->ts_ctx[tsid].id_ctx[TSDRV_STREAM_ID].id_list) != 0) {
#ifdef CFG_SOC_PLATFORM_MDC_V51
        tsdrv_recycle_rts_streamid(ts_resource, ctx);
#endif
        return;
    }

    ka_list_for_each_safe(pos, n, &ctx->ts_ctx[tsid].id_ctx[TSDRV_STREAM_ID].id_list) {
        stream_info = ka_list_entry(pos, struct tsdrv_id_info, list);
        (void)devdrv_free_stream(ctx, tsid, stream_info->phy_id, DEVDRV_FREE_BY_RECYCLE);
    }
}

void tsdrv_recycle_stream(struct tsdrv_ts_resource *ts_resource, struct tsdrv_ctx *ctx, u32 streamid)
{
#ifndef TSDRV_UT
    struct tsdrv_id_info *stream_info = NULL;
    u32 tsid = ts_resource->tsid;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;

    if (ka_list_empty_careful(&ctx->ts_ctx[tsid].id_ctx[TSDRV_STREAM_ID].id_list) != 0) {
#ifdef CFG_SOC_PLATFORM_MDC_V51
        tsdrv_recycle_rts_streamid(ts_resource, ctx);
#endif
        return;
    }

    ka_list_for_each_safe(pos, n, &ctx->ts_ctx[tsid].id_ctx[TSDRV_STREAM_ID].id_list) {
        stream_info = ka_list_entry(pos, struct tsdrv_id_info, list);
        if (stream_info->phy_id == streamid) {
            (void)devdrv_free_stream(ctx, tsid, stream_info->phy_id, DEVDRV_FREE_BY_RECYCLE);
        }
    }
#endif /* TSDRV_UT */
}

STATIC void tsdrv_dfx_show_report(struct tsdrv_ts_resource *ts_resource, struct devdrv_cq_sub_info *cq_sub)
{
#ifndef TSDRV_UT
    struct devdrv_ts_cq_info *cq_info = NULL;
    struct devdrv_report *report = NULL;
    phys_addr_t info_mem_addr;
    int show_num = TS_DFX_SHOW_NUM;
    u32 streamID;
    u32 taskID;
    int idx;
    u32 phase;
    u32 len;
    int i;

    info_mem_addr = ts_resource->mem_info[DEVDRV_INFO_MEM].virt_addr;
    cq_info = devdrv_calc_cq_info(info_mem_addr, cq_sub->index);
    len = cq_info->slot_size * cq_info->depth;
    tsdrv_get_drv_ops()->flush_cache((u64)cq_sub->virt_addr, len);

    for (i = 0; i < show_num; ++i) {
        idx = (DEVDRV_MAX_CQ_DEPTH + (int)cq_info->tail - (show_num / (int)TS_DFX_SHOW_REPORT_NUM) + i) % cq_info->depth;
        report = (struct devdrv_report *)(uintptr_t)((unsigned long)cq_sub->virt_addr +
           ((unsigned long)cq_info->slot_size * idx));
        if (report != NULL) {  // for coverity check
            phase = devdrv_report_get_phase(report);
            streamID = devdrv_report_get_stream_index(report);
            taskID = devdrv_report_get_task_index(report);
            TSDRV_PRINT_EVENT("idx:%d, slot phase:%d, cq info phase:%u, streamID:%u, taskID:%u, type:%u.\n",
                idx, phase, cq_info->phase, streamID, taskID, devdrv_report_get_type(report));
        }
    }
#endif
}

STATIC void tsdrv_set_cq_invalid(struct tsdrv_ts_resource *ts_resource, struct devdrv_cq_sub_info *cq_sub)
{
#ifndef TSDRV_UT
    struct devdrv_ts_cq_info *cq_info = NULL;
    phys_addr_t info_mem_addr;

    info_mem_addr = ts_resource->mem_info[DEVDRV_INFO_MEM].virt_addr;
    cq_info = devdrv_calc_cq_info(info_mem_addr, cq_sub->index);
    if (cq_sub->index != cq_info->index) {
        TSDRV_PRINT_WARN("cq_info index(%u), cq_sub index(%u)\n", cq_sub->index, cq_info->index);
        return;
    }

    ka_task_spin_lock_irq(&cq_sub->spinlock);
    cq_sub->ctx = NULL;
    ka_task_spin_unlock_irq(&cq_sub->spinlock);
#endif
}

void tsdrv_resource_no_recycle_print(struct tsdrv_ts_resource *ts_resource, struct tsdrv_ctx *ctx)
{
#ifndef TSDRV_UT
    struct devdrv_cq_sub_info *cq_sub_info = NULL;
    struct tsdrv_id_info *stream_info = NULL;
    struct tsdrv_id_info *sq_id_info = NULL;
    struct tsdrv_id_info *cq_id_info = NULL;
    struct devdrv_ts_cq_info *cq_info = NULL;
    ka_list_head_t *pos = NULL, *n = NULL;
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    u32 tsid = ts_resource->tsid;
    phys_addr_t info_mem_addr;

    info_mem_addr = ts_resource->mem_info[DEVDRV_INFO_MEM].virt_addr;

    TSDRV_PRINT_ERR("SOME RESOURCE are not recycle. PID: %d, TGID: %d, devid(%u) tsid(%u)\n",
        ctx->pid, ctx->tgid, devid, tsid);

    if (!ka_list_empty_careful(&ctx->ts_ctx[tsid].id_ctx[TSDRV_EVENT_SW_ID].id_list)) {
        TSDRV_PRINT_ERR("some event id are not released. event num = %u\n",
            ctx->ts_ctx[tsid].id_ctx[TSDRV_EVENT_SW_ID].id_num);
    }

    if (!ka_list_empty_careful(&ctx->ts_ctx[tsid].id_ctx[TSDRV_EVENT_HW_ID].id_list)) {
        TSDRV_PRINT_ERR("some ipc event id are not released. ipc event num = %u\n",
            ctx->ts_ctx[tsid].id_ctx[TSDRV_EVENT_HW_ID].id_num);
    }

    if (!ka_list_empty_careful(&ctx->ts_ctx[tsid].id_ctx[TSDRV_NOTIFY_ID].id_list)) {
        TSDRV_PRINT_ERR("some notify id are not released. notify num = %u\n",
            ctx->ts_ctx[tsid].id_ctx[TSDRV_NOTIFY_ID].id_num);
    }

    if (!ka_list_empty_careful(&ctx->ts_ctx[tsid].id_ctx[TSDRV_MODEL_ID].id_list)) {
        TSDRV_PRINT_ERR("some model id are not released. model num = %u\n",
            ctx->ts_ctx[tsid].id_ctx[TSDRV_MODEL_ID].id_num);
    }

    if (!ka_list_empty_careful(&ctx->ts_ctx[tsid].id_ctx[TSDRV_STREAM_ID].id_list)) {
        TSDRV_PRINT_ERR("some stream id are not released. stream num = %u\n",
            ctx->ts_ctx[tsid].id_ctx[TSDRV_STREAM_ID].id_num);
        ka_list_for_each_safe(pos, n, &ctx->ts_ctx[tsid].id_ctx[TSDRV_STREAM_ID].id_list) {
            stream_info = ka_list_entry(pos, struct tsdrv_id_info, list);
            TSDRV_PRINT_ERR("stream id = %d\n", stream_info->phy_id);
        }
    }
    if (!ka_list_empty_careful(&ctx->ts_ctx[tsid].id_ctx[TSDRV_SQ_ID].id_list)) {
        TSDRV_PRINT_ERR("some sq id are not recycle. tsid(%u) sq num = %u\n", tsid,
            ctx->ts_ctx[tsid].id_ctx[TSDRV_SQ_ID].id_num);
        ka_list_for_each_safe(pos, n, &ctx->ts_ctx[tsid].id_ctx[TSDRV_SQ_ID].id_list) {
            sq_id_info = ka_list_entry(pos, struct tsdrv_id_info, list);
            TSDRV_PRINT_ERR("sq id = %d\n", sq_id_info->phy_id);
        }
    }
    if (!ka_list_empty_careful(&ctx->ts_ctx[tsid].id_ctx[TSDRV_CQ_ID].id_list)) {
        TSDRV_PRINT_ERR("some cq id are not recycle. tsid(%u) cq num = %u\n", tsid,
            ctx->ts_ctx[tsid].id_ctx[TSDRV_CQ_ID].id_num);
        ka_list_for_each_safe(pos, n, &ctx->ts_ctx[tsid].id_ctx[TSDRV_CQ_ID].id_list) {
            cq_id_info = ka_list_entry(pos, struct tsdrv_id_info, list);
            cq_info = devdrv_calc_cq_info(info_mem_addr, cq_id_info->phy_id);
            tsdrv_dfx_show_report(ts_resource, cq_info->cq_sub);
            TSDRV_PRINT_ERR("cq id = %d\n", cq_id_info->phy_id);
            tsdrv_set_cq_invalid(ts_resource, cq_info->cq_sub);
            cq_sub_info = (struct devdrv_cq_sub_info *)cq_info->cq_sub;
            tsdrv_add_mem_delay_free(devid, (void *)(uintptr_t)cq_sub_info->virt_addr, cq_sub_info->size);
        }
    }

#if defined(CFG_SOC_PLATFORM_MINI) && !defined(CFG_SOC_PLATFORM_MINIV2) && !defined(CFG_SOC_PLATFORM_MINIV3)
    tsdrv_cbsqcq_no_recycle_print(&ctx->ts_ctx[tsid]);
#endif
#endif /* TSDRV_UT */
}

void tsdrv_resource_recycled_print(struct devdrv_recycle_message *recycle_message)
{
#ifndef TSDRV_UT
    u32 i;

    TSDRV_PRINT_INFO("resource had been recycled print:\n");
    TSDRV_PRINT_INFO("invalid_type_num(%u), send_doorbell_num(%u).\n",
        recycle_message->invalid_type_num, recycle_message->send_doorbell_num);
    TSDRV_PRINT_INFO("recycle_sq_num(%u),recycle_stream_num(%u),recycle_cq_num(%u),"
                     "recycle_event_num(%u),recycle_notify_num(%u), recycle_ipc_event_num(%u).\n",
                     recycle_message->recycle_sq_num, recycle_message->recycle_stream_num,
                     recycle_message->recycle_cq_num, recycle_message->recycle_event_num,
                     recycle_message->recycle_notify_num, recycle_message->recycle_ipc_event_num);
    for (i = 0; i < recycle_message->recycle_sq_num; i++) {
        TSDRV_PRINT_INFO("sq_index:%u ", recycle_message->recycle_sq[i % DEVDRV_MAX_SQ_NUM]);
    }

    for (i = 0; i < recycle_message->recycle_stream_num; i++) {
        TSDRV_PRINT_INFO("stream_id:%u ", recycle_message->recycle_stream[i % DEVDRV_MAX_STREAM_ID]);
    }

    for (i = 0; i < recycle_message->recycle_cq_num; i++) {
        TSDRV_PRINT_INFO("cq_index:%u ", recycle_message->recycle_cq[i % DEVDRV_MAX_CQ_NUM]);
    }
#endif
}

STATIC int devdrv_send_recycle_info_to_ts(u32 dev_id, u8 fid, u32 tsid, int tgid, u32 cmd)
{
    struct recycle_pid_info msg;
    struct tsdrv_mbox_data data;
    enum tsdrv_env_type env_type;

    env_type = tsdrv_get_env_type();
    msg.header.valid = DEVDRV_MAILBOX_MESSAGE_VALID;
    msg.header.cmd_type = cmd;
    msg.header.result = 0;
    msg.pid_info.app_cnt = 1;
    msg.pid_info.pid[0] = tgid;
    msg.pid_info.plat_type = (u8)env_type;
    msg.pid_info.fid = fid;

    data.msg = &msg;
    data.msg_len = sizeof(struct recycle_pid_info);
    data.out_data = NULL;
    data.out_len = 0;

    TSDRV_PRINT_INFO("Send recycle mailbox to ts."
        "cmd[%u], devid[%u], fid[%u], tsid[%u], tgid(%d), msg_len(%u), evn_type[%d].\n",
        (u32)msg.header.cmd_type, dev_id, (u32)fid, tsid, tgid, (u32)sizeof(struct recycle_pid_info), (int)env_type);

    return tsdrv_mailbox_send_sync(dev_id, tsid, &data);
}

#ifdef CFG_FEATURE_USE_MAILBOX_RECYCLE
static void tsdrv_recycle_all_cq(struct tsdrv_ts_resource *ts_resource, struct tsdrv_ctx *ctx)
{
    struct devdrv_ts_cq_info *cq_info = NULL;
    struct tsdrv_id_info *cq_id_info = NULL;
    u32 tsid = ts_resource->tsid;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;
    phys_addr_t info_mem_addr;

    if (ka_list_empty_careful(&ctx->ts_ctx[tsid].id_ctx[TSDRV_CQ_ID].id_list) != 0) {
        return;
    }
#ifndef TSDRV_UT
    info_mem_addr = ts_resource->mem_info[DEVDRV_INFO_MEM].virt_addr;
    ka_list_for_each_safe(pos, n, &ctx->ts_ctx[tsid].id_ctx[TSDRV_CQ_ID].id_list) {
        cq_id_info = ka_list_entry(pos, struct tsdrv_id_info, list);
        cq_info = devdrv_calc_cq_info(info_mem_addr, cq_id_info->phy_id);
        (void)devdrv_exit_cq(ctx, ts_resource->tsid, cq_info->cq_sub);
        devdrv_free_cq(ts_resource, ctx, cq_info);
    }
#endif
}

static void tsdrv_recycle_resource_use_mailbox(struct tsdrv_ts_resource *ts_resource, struct tsdrv_ctx *ctx)
{
#define CUR_TIMEOUT_CYCLE_MAX  309
#define CUR_TIMEOUT_CYCLE_MIN  10
#define CUR_MSLEEP_SHORT       100
#define CUR_MSLEEP_LONG        1000

    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    u32 fid = tsdrv_get_fid_by_ctx(ctx);
    u32 tsid = ts_resource->tsid;
    u32 cnt = 0;
    int ret;

    while (1) {
        /* use the result of mailbox to check the recycle status of ts */
        ret = devdrv_send_recycle_info_to_ts(devid, fid, tsid, ctx->tgid,  DEVDRV_MAILBOX_RECYCLE_CHECK);
        if (ret == 0) {
            TSDRV_PRINT_INFO("Ts recycle is success. devid[%u], fid[%u], tsid[%u], tgid[%d], wait_cnt[%u].\n",
                devid, fid, tsid, ctx->tgid, cnt);
            (void)devdrv_recycle_cbcqsq_id(ctx);
            tsdrv_recycle_streams(ts_resource, ctx);
            tsdrv_recycle_notify_id(ts_resource, ctx);
            tsdrv_recycle_event_id(ts_resource, ctx);
            tsdrv_recycle_ipc_event_id(ts_resource, ctx);
            tsdrv_recycle_all_sq(ts_resource, ctx);
            tsdrv_recycle_all_cq(ts_resource, ctx);
            break;
        }
         /* recycle timeout 300s, stop recycle. */
        if (cnt >= CUR_TIMEOUT_CYCLE_MAX) {
            TSDRV_PRINT_ERR("Recycle timeout, devid[%u], fid[%u], tsid[%u], tgid[%d], wait_cnt[%u].\n",
                devid, fid, tsid, ctx->tgid, cnt);
            break;
        }

         /* dev_status of pm fid is not active. stop recycle. */
        if (tsdrv_get_dev_status(devid, TSDRV_PM_FID) != TSDRV_DEV_ACTIVE) {
            TSDRV_PRINT_ERR("Dev status is not TSDRV_DEV_ACTIVE,"
                "devid[%u], fid[%u], tsid[%u], tgid[%d], wait_cnt[%u].\n",
                devid, fid, tsid, ctx->tgid, cnt);
            break;
        }

         /* ctx_status of other fid is TSDRV_CTX_STOP_RECYCLE. stop recycle. */
        if ((fid != TSDRV_PM_FID) && ((enum tsdrv_ctx_status)ka_base_atomic_read(&ctx->status) == TSDRV_CTX_STOP_RECYCLE)) {
            TSDRV_PRINT_ERR("In vm, ctx status is TSDRV_CTX_STOP_RECYCLE,"
                "devid[%u], fid[%u], tsid[%u], tgid[%d], wait_cnt[%u].\n",
                devid, fid, tsid, ctx->tgid, cnt);
            break;
        }

        (cnt++ < CUR_TIMEOUT_CYCLE_MIN) ? ka_system_msleep(CUR_MSLEEP_SHORT) : ka_system_msleep(CUR_MSLEEP_LONG);
    }
}

static int devdrv_recycle_process(struct tsdrv_ts_resource *ts_resource, struct tsdrv_ts_ctx *ts_ctx)
{
    struct tsdrv_ctx *ctx = tsdrv_tsctx_to_ctx(ts_ctx);
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    u32 fid = tsdrv_get_fid_by_ctx(ctx);
    u32 tsid = ts_ctx->tsid;
    int ret;

    /* recycle notify, event, stream, sqcq */
    tsdrv_recycle_resource_use_mailbox(ts_resource, ctx);
    tsdrv_recycle_model_id(ts_resource, ctx);

    if (tsdrv_ts_ctx_id_res_chk(ts_ctx, ctx->tgid, ctx->pid, LEAK_CHK_NO_PRINT) == true) {
#ifndef TSDRV_UT
        tsdrv_resource_no_recycle_print(ts_resource, ctx);
        ka_base_atomic_set(&ts_ctx->status, TSDRV_TS_CTX_RECYCLE_ERR);

        /* do not put ctx into rb again, no one can use this ctx anymore */
        tsdrv_dev_set_ctx_recycle_status(ctx, TSDRV_CTX_RECYCLE_ERR);
        ret = tsdrv_set_runtime_available_conflict_check(devid);
        if (ret != 0) {
            TSDRV_PRINT_ERR("Failed to set runtime status. devid[%u], ret[%d].\n", devid, ret);
        }

        return -EBUSY;
#endif
    }

    ka_base_atomic_set(&ts_ctx->status, TSDRV_TS_CTX_IDLE);
    TSDRV_PRINT_INFO("Recycle succeed. devid[%u], fid[%u], tsid[%u], tgid[%d], pid[%d]\n",
        devid, fid, tsid, ctx->tgid, ctx->pid);
    return 0;
}

#define SEND_REC_MSG_TIMES    3

static void tsdrv_send_rec_msg_to_ts(u32 devid, struct tsdrv_dev_resource *dev_res, struct tsdrv_ctx *ctx)
{
    u32 tsnum = tsdrv_get_dev_tsnum(devid);
    struct tsdrv_dev_ctx *dev_ctx = NULL;
    int ret, cnt;
    u32 tsid;

    if (unlikely(ctx->ctx_index >= TSDRV_MAX_CTX_NUM)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Invalid ctx_index. (ctx_index=%u)\n", ctx->ctx_index);
        return;
#endif
    }

    dev_ctx = ka_container_of(ctx, struct tsdrv_dev_ctx, ctx[ctx->ctx_index]);
    if (unlikely(dev_ctx->fid >= TSDRV_MAX_FID_NUM)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Invalid fid. (fid=%u)\n", dev_ctx->fid);
        return;
#endif
    }

    for (tsid = 0; tsid < tsnum; tsid++) {
        for (cnt = 0; cnt < SEND_REC_MSG_TIMES; cnt++) {
            ret = devdrv_send_recycle_info_to_ts(devid, dev_ctx->fid, tsid, ctx->tgid, DEVDRV_MAILBOX_RECYCLE_PID_ID);
            if (ret != 0) {
#ifndef TSDRV_UT
                TSDRV_PRINT_WARN("Send recycle info to ts failed. (tsid=%u; cnt=%d; ret=%d)\n", tsid, cnt, ret);
#endif
            } else {
                break;
            }
        }
    }
}
#else
STATIC void tsdrv_recycle_cq(struct tsdrv_ts_resource *ts_resource, struct tsdrv_ctx *ctx, u32 cqid)
{
#ifndef TSDRV_UT
    struct devdrv_ts_cq_info *cq_info = NULL;
    struct tsdrv_id_info *cq_id_info = NULL;
    u32 tsid = ts_resource->tsid;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;
    phys_addr_t info_mem_addr;

    if (ka_list_empty_careful(&ctx->ts_ctx[tsid].id_ctx[TSDRV_CQ_ID].id_list) != 0) {
        return;
    }

    info_mem_addr = ts_resource->mem_info[DEVDRV_INFO_MEM].virt_addr;
    ka_list_for_each_safe(pos, n, &ctx->ts_ctx[tsid].id_ctx[TSDRV_CQ_ID].id_list) {
        cq_id_info = ka_list_entry(pos, struct tsdrv_id_info, list);
        if (cq_id_info->phy_id == cqid) {
            cq_info = devdrv_calc_cq_info(info_mem_addr, cqid);
            (void)devdrv_exit_cq(ctx, ts_resource->tsid, cq_info->cq_sub);
            devdrv_free_cq(ts_resource, ctx, cq_info);
        }
    }
#endif
}

STATIC void tsdrv_recycle_sources(struct tsdrv_ts_resource *ts_resource, struct tsdrv_ctx *ctx,
    struct devdrv_ts_cq_info *cq_info, struct devdrv_recycle_message *recycle_message)
{
#ifndef TSDRV_UT
    struct tsdrv_device *tsdrv_dev = tsdrv_res_to_dev(ts_resource);
    struct devdrv_cq_sub_info *cq_sub_info = (struct devdrv_cq_sub_info *)cq_info->cq_sub;
    struct tsdrv_db_hwinfo_t *db_hwinfo = NULL;
    struct devdrv_report *report = NULL;
    u32 tsid = ts_resource->tsid;
    u32 streamid, sqid, cqid;
    u32 *cq_doorbell = NULL;
    u16 report_type;

    db_hwinfo = tsdrv_get_db_hwinfo_t(tsdrv_dev->devid, tsid, TSDRV_CQ_DB);
    cq_doorbell = db_hwinfo->dbs + ((long)(unsigned)cq_info->index * db_hwinfo->db_stride) + DEVDRV_CQ_HEAD_OFFSET;

    while (cq_info->head != cq_info->tail) {
        report = (struct devdrv_report *)(uintptr_t)((unsigned long)(uintptr_t)cq_sub_info->virt_addr +
            ((unsigned long)cq_info->slot_size * cq_info->head));
        report_type = devdrv_report_get_type(report);
        cq_info->head = (cq_info->head + 1) % cq_info->depth;
        *cq_doorbell = cq_info->head;
        recycle_message->send_doorbell_num++;

        switch (report_type) {
            case TS_REPORT_TYPE_RECYCLE_SQ_FINISHED:
                sqid = devdrv_report_get_sqcq_index(report);
                recycle_message->recycle_sq[recycle_message->recycle_sq_num % DEVDRV_MAX_SQ_NUM] = sqid;
                recycle_message->recycle_sq_num++;
                tsdrv_recycle_sq(ts_resource, ctx, sqid);
                break;
            case TS_REPORT_TYPE_RECYCLE_STREAM_FINISHED:
                streamid = devdrv_report_get_stream_index(report);
                recycle_message->recycle_stream[recycle_message->recycle_stream_num % DEVDRV_MAX_STREAM_ID] = streamid;
                recycle_message->recycle_stream_num++;
                tsdrv_recycle_stream(ts_resource, ctx, streamid);
                break;
#if defined(CFG_SOC_PLATFORM_MINI) && !defined(CFG_SOC_PLATFORM_MINIV2) && !defined(CFG_SOC_PLATFORM_MINIV3)
            case TS_REPORT_TYPE_RECYCLE_CBSQCQ_FINISHED:
                (void)devdrv_recycle_cbcqsq_id(ctx);
                break;
#else
            case TS_REPORT_TYPE_RECYCLE_NOTIFY_FINISHED:
                recycle_message->recycle_notify_num++;
                tsdrv_recycle_notify_id(ts_resource, ctx);
                break;
#endif
            case TS_REPORT_TYPE_RECYCLE_EVENT_FINISHED:
                recycle_message->recycle_event_num++;
                tsdrv_recycle_event_id(ts_resource, ctx);
                recycle_message->recycle_ipc_event_num++;
                tsdrv_recycle_ipc_event_id(ts_resource, ctx);
                break;
            case TS_REPORT_TYPE_RECYCLE_CQ_FINISHED:
                cqid = devdrv_report_get_sqcq_index(report);
                recycle_message->recycle_cq[recycle_message->recycle_cq_num % DEVDRV_MAX_CQ_NUM] = cqid;
                recycle_message->recycle_cq_num++;
                tsdrv_recycle_cq(ts_resource, ctx, cqid);
                return;
            default:
                recycle_message->invalid_type_num++;
                break;
        }
    }
#endif /* TSDRV_UT */
}

STATIC void tsdrv_update_report(struct devdrv_ts_cq_info *cq_info)
{
#ifndef TSDRV_UT
    struct devdrv_cq_sub_info *cq_sub_info = (struct devdrv_cq_sub_info *)cq_info->cq_sub;
    struct devdrv_report *report = NULL;
    u32 next_tail;
    u32 phase;
    u32 len;

    len = cq_info->slot_size * cq_info->depth;
    tsdrv_get_drv_ops()->flush_cache((u64)cq_sub_info->virt_addr, len);
    report = (struct devdrv_report *)(uintptr_t)((unsigned long)cq_sub_info->virt_addr +
                                                 ((unsigned long)cq_info->slot_size * cq_info->tail));
    phase = devdrv_report_get_phase(report);

    while (phase == cq_info->phase) {
        next_tail = (cq_info->tail + 1) % cq_info->depth;
        if (next_tail == cq_info->head) {
            break;
        }
        next_tail = cq_info->tail + 1;
        if (next_tail > (cq_info->depth - 1)) {
            cq_info->phase = ((cq_info->phase == 0) ? 1 : 0);
            cq_info->tail = 0;
        } else {
            cq_info->tail++;
        }
        report = (struct devdrv_report *)(uintptr_t)((unsigned long)(uintptr_t)cq_sub_info->virt_addr +
                                                     ((unsigned long)cq_info->slot_size * cq_info->tail));
        phase = devdrv_report_get_phase(report);
    }
    TSDRV_PRINT_INFO("head = %d, tail = %d\n", cq_info->head, cq_info->tail);
#endif
}

STATIC void tsdrv_recycle_resource_report_by_ts(struct tsdrv_ts_resource *ts_resource,
    struct tsdrv_ctx *ctx, unsigned long timeout, struct devdrv_recycle_message *recycle_message)
{
    struct devdrv_cq_sub_info *cq_sub_info = NULL;
    struct tsdrv_id_info *cq_id_info = NULL;
    struct devdrv_ts_cq_info *cq_info = NULL;
    ka_list_head_t *pos = NULL, *n = NULL;
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    u32 fid = tsdrv_get_fid_by_ctx(ctx);
    struct devdrv_report *report = NULL;
    u32 tsid = ts_resource->tsid;
    phys_addr_t info_mem_addr;
    unsigned long jiffy;
    u32 len;

    while (1) {
        if (ka_list_empty_careful(&ctx->ts_ctx[tsid].id_ctx[TSDRV_CQ_ID].id_list) != 0) {
            TSDRV_PRINT_WARN("tsid(%u) cq_list is null, start recycle all streams,"
                "event_id, notify_id, sq.\n", tsid);
            (void)devdrv_recycle_cbcqsq_id(ctx);
            tsdrv_recycle_streams(ts_resource, ctx);
            tsdrv_recycle_notify_id(ts_resource, ctx);
            tsdrv_recycle_event_id(ts_resource, ctx);
            tsdrv_recycle_ipc_event_id(ts_resource, ctx);
            tsdrv_recycle_all_sq(ts_resource, ctx);
            break;
        }
#ifndef TSDRV_UT
        ka_list_for_each_safe(pos, n, &ctx->ts_ctx[tsid].id_ctx[TSDRV_CQ_ID].id_list) {
            cq_id_info = ka_list_entry(pos, struct tsdrv_id_info, list);
            if (cq_id_info == NULL) {
                TSDRV_PRINT_ERR("cq_sub_info is null.\n");
                continue;
            }

            info_mem_addr = ts_resource->mem_info[DEVDRV_INFO_MEM].virt_addr;
            cq_info = devdrv_calc_cq_info(info_mem_addr, cq_id_info->phy_id);
            if ((cq_info == NULL) || (cq_info->index >= DEVDRV_MAX_CQ_NUM)) {
                TSDRV_PRINT_ERR("param is null.\n");
                continue;
            }

            cq_sub_info = (struct devdrv_cq_sub_info *)cq_info->cq_sub;
            len = cq_info->slot_size * cq_info->depth;
            tsdrv_get_drv_ops()->flush_cache((u64)cq_sub_info->virt_addr, len);
            report = (struct devdrv_report *)(uintptr_t)((unsigned long)cq_sub_info->virt_addr +
                                                         ((unsigned long)cq_info->slot_size * cq_info->tail));

            if ((devdrv_report_get_phase(report)) == cq_info->phase) {
                tsdrv_update_report(cq_info);
            } else {
                jiffy = ka_system_msecs_to_jiffies(TS_MSECS_TO_JIFFIES);
                ctx->ts_ctx[tsid].cq_tail_updated = 0;
                if (ka_task_wait_event_interruptible_timeout(ctx->ts_ctx[tsid].report_wait,
                    ctx->ts_ctx[tsid].cq_tail_updated == CQ_HEAD_UPDATE_FLAG, jiffy)) {
                    ctx->ts_ctx[tsid].cq_tail_updated = 0;
                    tsdrv_update_report(cq_info);
                }
            }
            tsdrv_recycle_sources(ts_resource, ctx, cq_info, recycle_message);
        }
        /*
         * When the resource recovery is complete, or the recovery timeout, or dev_status of pm fid is not active,
         * or ctx_status of other fid is TSDRV_CTX_STOP_RECYCLE, stop recovering the resource.
         */
        if ((tsdrv_ts_ctx_id_res_chk(&ctx->ts_ctx[tsid], ctx->tgid, ctx->pid, LEAK_CHK_NO_PRINT) == false) ||
            ka_system_time_after(jiffies, timeout) || (tsdrv_get_dev_status(devid, TSDRV_PM_FID) != TSDRV_DEV_ACTIVE) ||
            ((fid != TSDRV_PM_FID) && ((enum tsdrv_ctx_status)ka_base_atomic_read(&ctx->status) == TSDRV_CTX_STOP_RECYCLE))) {
            break;
        }
#endif
    }
}

STATIC int devdrv_recycle_process(struct tsdrv_ts_resource *ts_resource, struct tsdrv_ts_ctx *ts_ctx)
{
    struct devdrv_recycle_message *recycle_message = NULL;
    struct tsdrv_ctx *ctx = NULL;
    u32 devid, tsid;
    unsigned long timeout;
    ka_pid_t tgid;
    ka_pid_t pid;
    u32 fid;
    int ret;

    recycle_message = tsdrv_kzalloc(sizeof(struct devdrv_recycle_message), KA_GFP_KERNEL);
    if (recycle_message == NULL) {
        TSDRV_PRINT_ERR("kmalloc recycle_message fail.\n");
        return -ENOMEM;
    }

    ctx = tsdrv_tsctx_to_ctx(ts_ctx);
    tgid = ctx->tgid;
    pid = ctx->pid;
    fid = tsdrv_get_fid_by_ctx(ctx);
    tsid = ts_ctx->tsid;

    devid = tsdrv_get_devid_by_ctx(ctx);

    /* process do not need clear pte table */
    timeout = jiffies + (300 * KA_HZ); // recycle resources in 300 s

    /* recycle notify, event, stream, sqcq */
    tsdrv_recycle_resource_report_by_ts(ts_resource, ctx, timeout, recycle_message);
    tsdrv_recycle_model_id(ts_resource, ctx);

    if (tsdrv_ts_ctx_id_res_chk(ts_ctx, ctx->tgid, ctx->pid, LEAK_CHK_NO_PRINT) == true) {
#ifndef TSDRV_UT
        tsdrv_resource_recycled_print(recycle_message);
        tsdrv_kfree(recycle_message);
        recycle_message = NULL;
        tsdrv_resource_no_recycle_print(ts_resource, ctx);
        ka_base_atomic_set(&ts_ctx->status, TSDRV_TS_CTX_RECYCLE_ERR);
        /* do not put ctx into rb again, no one can use this ctx anymore */
        tsdrv_dev_set_ctx_recycle_status(ctx, TSDRV_CTX_RECYCLE_ERR);
        ret = tsdrv_set_runtime_available_conflict_check(devid);
        if (ret != 0) {
            TSDRV_PRINT_ERR("set runtime status fail, devid=%u ret=%d.\n", devid, ret);
        }
        return -EBUSY;
#endif
    }
    tsdrv_kfree(recycle_message);
    recycle_message = NULL;
    ka_base_atomic_set(&ts_ctx->status, TSDRV_TS_CTX_IDLE);

    TSDRV_PRINT_INFO("recycle succeed, devid(%u), fid(%u), tsid(%u), tgid(%d), pid(%d)\n", devid, fid, tsid, tgid, pid);
    return 0;
}
#endif

STATIC void devdrv_recycle_work(ka_work_struct_t *work)
{
#ifndef TSDRV_UT
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct tsdrv_ts_ctx *ts_ctx = NULL;
    struct tsdrv_ctx *ctx = NULL;
    u32 devid, fid, tsnum, i;
    int ret;

    ts_ctx = ka_container_of(work, struct tsdrv_ts_ctx, recycle_work);
    ctx = tsdrv_tsctx_to_ctx(ts_ctx);
    ts_resource = tsdrv_ctx_to_ts_res(ctx, ts_ctx->tsid);

    devid = tsdrv_get_devid_by_ctx(ctx);
    fid = tsdrv_get_fid_by_ctx(ctx);
    tsnum = tsdrv_get_dev_tsnum(devid);

    TSDRV_PRINT_DEBUG("devid[%u], fid[%u], tgid[%u].\n", devid, fid, ctx->tgid);

    ret = devdrv_recycle_process(ts_resource, ts_ctx);
#ifndef CFG_SOC_PLATFORM_MDC_V51
    // send recycle vpc msg to vm.
    hvtsdrv_recycle(devid, fid, ctx->tgid, ctx->unique_id, ret);
#endif
    if (ret != 0) {
        return;
    }

    for (i = 0; i < tsnum; i++) {
        if (ka_base_atomic_read(&ctx->ts_ctx[i].status) != (int)TSDRV_TS_CTX_IDLE) {
            break;
        }
    }
    if (i == tsnum) {
        tsdrv_dev_set_ctx_recycle_status(ctx, TSDRV_CTX_INVALID);
        ret = tsdrv_set_runtime_available_conflict_check(devid);
        if (ret != 0) {
            TSDRV_PRINT_ERR("set runtime status fail, devid=%u ret=%d.\n", devid, ret);
        }
    }
#endif
}

int tsdrv_dev_res_recycle(u32 devid, struct tsdrv_dev_resource *dev_res, struct tsdrv_ctx *ctx)
{
    u32 tsnum = tsdrv_get_dev_tsnum(devid);
    struct tsdrv_dev_ctx *dev_ctx = NULL;
    int recycle_err = 0;
    u32 tsid;
    int ret;

    if (unlikely(ctx->ctx_index >= TSDRV_MAX_CTX_NUM)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid ctx_index(%u)\n", ctx->ctx_index);
        return -EINVAL;
#endif
    }
    dev_ctx = ka_container_of(ctx, struct tsdrv_dev_ctx, ctx[ctx->ctx_index]);
    if (unlikely(dev_ctx->fid >= TSDRV_MAX_FID_NUM)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid fid(%u)\n", dev_ctx->fid);
        return -EINVAL;
#endif
    }

    for (tsid = 0; tsid < tsnum; tsid++) {
        struct tsdrv_ts_resource *ts_resource = &dev_res->ts_resource[tsid];
        struct tsdrv_ts_ctx *ts_ctx = &ctx->ts_ctx[tsid];
        if (tsdrv_ts_ctx_id_res_chk(ts_ctx, ctx->tgid, ctx->pid, LEAK_CHK_NO_PRINT) == false) {
            continue;
        }
        ka_base_atomic_set(&ts_ctx->status, TSDRV_TS_CTX_START_RECYCLE);
        ret = devdrv_send_recycle_info_to_ts(devid, dev_ctx->fid, tsid, ctx->tgid,  DEVDRV_MAILBOX_RECYCLE_PID_ID);
        if (ret != 0) {
            TSDRV_PRINT_WARN("devdrv_send_recycle_info_to_ts failed, ret=%d\n", ret);
        }
        if (tsdrv_is_in_vm(devid) == false) {
            ret = devdrv_recycle_process(ts_resource, ts_ctx);
            if (ret != 0) {
#ifndef TSDRV_UT
                TSDRV_PRINT_ERR("recycle failed, devid(%u), fid(%u), tsid(%u), tgid(%d), pid(%d)\n", devid,
                    dev_ctx->fid, tsid, ctx->tgid, ctx->pid);
                recycle_err++;
#endif
            }
        } else {
#ifndef TSDRV_UT
            ka_task_mutex_lock(&dev_res->dev_res_lock);
            if (ts_resource->wq != NULL) {
                KA_TASK_INIT_WORK(&ts_ctx->recycle_work, devdrv_recycle_work);
                (void)ka_task_queue_work(ts_resource->wq, &ts_ctx->recycle_work);
            }
            ka_task_mutex_unlock(&dev_res->dev_res_lock);
#endif
        }
    }
    return (recycle_err > 0) ? -EBUSY : 0;
}

#ifdef CFG_FEATURE_USE_MAILBOX_RECYCLE
int tsdrv_proc_recycle(u32 devid, struct tsdrv_ctx *ctx, struct tsdrv_dev_resource *dev_res)
{
    int ret;
    u32 tsnum = tsdrv_get_dev_tsnum(devid);

    /* check source leak */
    if (tsdrv_ctx_id_res_chk(ctx, LEAK_CHK_PRINT) == true) {
        shm_sqcq_ctx_exit(ctx, tsnum);
        ret = tsdrv_dev_res_recycle(devid, dev_res, ctx);
        if (ret != 0) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("Recycle failed. (devid=%u; ret=%d)\n", devid, ret);
            return ret;
#endif
        }
    } else {
        tsdrv_send_rec_msg_to_ts(devid, dev_res, ctx);

        callback_ctx_exit(ctx, tsnum);
        shm_sqcq_ctx_exit(ctx, tsnum);
    }

    return 0;
}

int hvtsdrv_proc_recycle(u32 devid, struct tsdrv_ctx *ctx, struct tsdrv_dev_resource *dev_res, u32 fid)
{
    int ret;
    u32 tsnum = tsdrv_get_dev_tsnum(devid);

    /* check source leak */
    if (tsdrv_ctx_id_res_chk(ctx, LEAK_CHK_PRINT) == true) {
#ifndef TSDRV_UT
        shm_sqcq_ctx_exit(ctx, tsnum);
        ret = tsdrv_dev_res_recycle(devid, dev_res, ctx);
        if (ret != 0) {
            TSDRV_PRINT_ERR("Recycle failed. (devid=%u; fid=%u; ret=%d)\n", devid, fid, ret);
            return ret;
        }
        if (tsdrv_is_in_vm(devid) == true) {
            return TSDRV_IDS_RECYCLE_STOP;
        }
#endif
    } else {
        tsdrv_send_rec_msg_to_ts(devid, dev_res, ctx);
    }

    return 0;
}
#else
int tsdrv_proc_recycle(u32 devid, struct tsdrv_ctx *ctx, struct tsdrv_dev_resource *dev_res)
{
    int ret;
    u32 tsnum = tsdrv_get_dev_tsnum(devid);

    /* check source leak */
    if (tsdrv_ctx_id_res_chk(ctx, LEAK_CHK_PRINT) == true) {
        if (tsdrv_ctx_is_nothread_type(ctx) == true) {
            ret = shm_sqcq_dev_res_recycle(devid, dev_res, ctx);
        } else {
            shm_sqcq_ctx_exit(ctx, tsnum);
            ret = tsdrv_dev_res_recycle(devid, dev_res, ctx);
        }
        if (ret != 0) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("Recycle failed. (devid=%u; ret=%d)\n", devid, ret);
            return ret;
#endif
        }
    } else {
        callback_ctx_exit(ctx, tsnum);
        shm_sqcq_ctx_exit(ctx, tsnum);
    }

    return 0;
}

int hvtsdrv_proc_recycle(u32 devid, struct tsdrv_ctx *ctx, struct tsdrv_dev_resource *dev_res, u32 fid)
{
    int ret;
    u32 tsnum = tsdrv_get_dev_tsnum(devid);

    /* check source leak */
    if (tsdrv_ctx_id_res_chk(ctx, LEAK_CHK_PRINT) == true) {
#ifndef TSDRV_UT
        if (tsdrv_ctx_is_nothread_type(ctx) == true) {
            ret = shm_sqcq_dev_res_recycle(devid, dev_res, ctx);
        } else {
            shm_sqcq_ctx_exit(ctx, tsnum);
            ret = tsdrv_dev_res_recycle(devid, dev_res, ctx);
        }
        if (ret != 0) {
            TSDRV_PRINT_ERR("Recycle failed. (devid=%u; fid=%u; ret=%d)\n", devid, fid, ret);
            return ret;
        }
        if (tsdrv_is_in_vm(devid) == true) {
            return TSDRV_IDS_RECYCLE_STOP;
        }
#endif
    }

    return 0;
}
#endif

