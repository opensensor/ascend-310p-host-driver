/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
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
 * Create: 2024-08-05
 */
#include <linux/idr.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/gfp.h>
#include <linux/ratelimit.h>

#ifndef CFG_MANAGER_HOST_ENV
#include <linux/securec.h>
#else
#include "securec.h"
#endif

#include "tsdrv_timestamp_track.h"
#include "devdrv_common.h"
#include "tsdrv_device.h"
#include "logic_cq.h"
#include "tsdrv_sqcq_timestamp.h"

static bool g_sqcq_dfx_enable_status[TSDRV_MAX_DAVINCI_NUM][TSDRV_MAX_FID_NUM];
static struct tsdrv_sqcq_dfx *g_tsdrv_sqcq_dfx[TSDRV_MAX_DAVINCI_NUM][TSDRV_MAX_FID_NUM][DEVDRV_MAX_TS_NUM];

static struct tsdrv_sqcq_dfx *tsdrv_get_sqcq_dfx_mng(u32 devid, u32 fid, u32 tsid)
{
    if ((devid >= TSDRV_MAX_DAVINCI_NUM) || (fid >= TSDRV_MAX_FID_NUM) || (tsid >= DEVDRV_MAX_TS_NUM)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Invalid param. (devid=%u; fid=%u; tsid=%u)\n", devid, fid, tsid);
        return NULL;
#endif
    }

    return g_tsdrv_sqcq_dfx[devid][fid][tsid];
}

void tsdrv_sqcq_dfx_set_enable_status(u32 devid, u32 fid, bool status)
{
    g_sqcq_dfx_enable_status[devid][fid] = status;
}

bool tsdrv_sqcq_dfx_is_enabled(u32 devid, u32 fid)
{
    return g_sqcq_dfx_enable_status[devid][fid];
}

#define TSDRV_SQCQ_RATELIMIT_INTERVAL (5 * KA_HZ)
#define TSDRV_SQCQ_RATELIMIT_BURST 10
bool tsdrv_sqcq_dfx_ratelimited(void)
{
#ifndef TSDRV_UT
    static KA_BASE_DEFINE_RATELIMIT_STATE(tsdrv_sqcq_dfx_rate_limit, TSDRV_SQCQ_RATELIMIT_INTERVAL, TSDRV_SQCQ_RATELIMIT_BURST);

    if (__ka_base_ratelimit(&tsdrv_sqcq_dfx_rate_limit)) {
        return false;
    }
#endif
    return true;
}

static int tsdrv_sqcq_dfx_init(u32 devid, u32 fid, u32 tsid)
{
    if (g_tsdrv_sqcq_dfx[devid][fid][tsid] != NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Repeate init. (devid=%u; fid=%u; tsid=%u)\n", devid, fid, tsid);
        return -EINVAL;
#endif
    }

    g_tsdrv_sqcq_dfx[devid][fid][tsid] = tsdrv_vzalloc(sizeof(struct tsdrv_sqcq_dfx));
    if (g_tsdrv_sqcq_dfx[devid][fid][tsid] == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Alloc mem failed. (devid=%u; fid=%u; tsid=%u)\n", devid, fid, tsid);
        return -ENOMEM;
#endif
    }

    TSDRV_PRINT_INFO("Sqcq dfx init success. (devid=%u; fid=%u; tsid=%u; sizeof_sqcq_dfx=0x%lx)\n", devid, fid, tsid, sizeof(struct tsdrv_sqcq_dfx));
    return 0;
}

static void tsdrv_sqcq_dfx_exit(u32 devid, u32 fid, u32 tsid)
{
    if (g_tsdrv_sqcq_dfx[devid][fid][tsid] == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_WARN("Maynot init. (devid=%u; fid=%u; tsid=%u)\n", devid, fid, tsid);
        return;
#endif
    }

    tsdrv_vfree(g_tsdrv_sqcq_dfx[devid][fid][tsid]);
    g_tsdrv_sqcq_dfx[devid][fid][tsid] = NULL;

    TSDRV_PRINT_INFO("Sqcq dfx exit success. (devid=%u; fid=%u; tsid=%u)\n", devid, fid, tsid);
}

int tsdrv_sqcq_dfx_dev_init(u32 devid, u32 fid, u32 tsnum)
{
    int ret, tsid;

    for (tsid = 0; tsid < (int)tsnum; tsid++) {
        ret = tsdrv_sqcq_dfx_init(devid, fid, (u32)tsid);
        if (ret != 0) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("Sqcq dfx init failed. (devid=%u; fid=%u; tsid=%u; ret=%d)\n", devid, fid, tsid, ret);
            goto err;
#endif
        }
    }

    return 0;

err:
    for (tsid = tsid - 1; tsid >= 0; tsid--) {
        tsdrv_sqcq_dfx_exit(devid, fid, (u32)tsid);
    }
    return ret;
}

void tsdrv_sqcq_dfx_dev_exit(u32 devid, u32 fid, u32 tsnum)
{
    u32 tsid;

    for (tsid = 0; tsid < tsnum; tsid++) {
        tsdrv_sqcq_dfx_exit(devid, fid, tsid);
    }
}

void tsdrv_sqcq_dfx_set_cq_irq_tick(u32 devid, u32 fid, u32 tsid)
{
#ifndef TSDRV_UT
    struct tsdrv_sqcq_dfx *sqcq_dfx_mng = NULL;

    sqcq_dfx_mng = tsdrv_get_sqcq_dfx_mng(devid, fid, tsid);
    if (sqcq_dfx_mng == NULL) {
        return;
    }

    sqcq_dfx_mng->cq_irq_tick = tsdrv_get_cur_cpu_tick();
#endif
}

static inline u64 tsdrv_sqcq_dfx_get_cq_irq_tick(struct tsdrv_sqcq_dfx *sqcq_dfx_mng)
{
    return sqcq_dfx_mng->cq_irq_tick;
}

void tsdrv_sqcq_dfx_set_cq_tasklet_tick(u32 devid, u32 fid, u32 tsid)
{
    struct tsdrv_sqcq_dfx *sqcq_dfx_mng = NULL;

    sqcq_dfx_mng = tsdrv_get_sqcq_dfx_mng(devid, fid, tsid);
    if (sqcq_dfx_mng == NULL) {
        return;
    }

    sqcq_dfx_mng->cq_tasklet_tick = tsdrv_get_cur_cpu_tick();
}

static inline u64 tsdrv_sqcq_dfx_get_cq_tasklet_tick(struct tsdrv_sqcq_dfx *sqcq_dfx_mng)
{
    return sqcq_dfx_mng->cq_tasklet_tick;
}

void tsdrv_sqcq_dfx_set_phy_cq_tick(u32 devid, u32 fid, u32 tsid, u32 cqid)
{
    struct tsdrv_sqcq_dfx *sqcq_dfx_mng = NULL;

    sqcq_dfx_mng = tsdrv_get_sqcq_dfx_mng(devid, fid, tsid);
    if (sqcq_dfx_mng == NULL) {
        return;
    }

    if (cqid >= DEVDRV_MAX_CQ_NUM) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Invalid phy cqid. (devid=%u; fid=%u; tsid=%u; phy_cqid=%u)\n", devid, fid, tsid, cqid);
        return;
#endif
    }

    sqcq_dfx_mng->phy_cq_tick[cqid] = tsdrv_get_cur_cpu_tick();
}

static inline u64 tsdrv_sqcq_dfx_get_phy_cq_tick(struct tsdrv_sqcq_dfx *sqcq_dfx_mng, u32 cqid)
{
    return sqcq_dfx_mng->phy_cq_tick[cqid];
}

void tsdrv_sqcq_dfx_set_logic_cq_dispatch_tick(u32 devid, u32 fid, u32 tsid, u32 logic_cqid, u32 head)
{
    struct tsdrv_sqcq_dfx *sqcq_dfx_mng = NULL;

    sqcq_dfx_mng = tsdrv_get_sqcq_dfx_mng(devid, fid, tsid);
    if (sqcq_dfx_mng == NULL) {
        return;
    }

    if (logic_cqid >= TSDRV_MAX_LOGIC_CQ_NUM) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Invalid logic cqid. (devid=%u; fid=%u; tsid=%u; logic_cqid=%u)\n",
            devid, fid, tsid, logic_cqid);
        return;
#endif
    }

    sqcq_dfx_mng->logic_cq_dfx[logic_cqid].logic_cq_dispatch_head = head;
    sqcq_dfx_mng->logic_cq_dfx[logic_cqid].logic_cq_dispatch_tick = tsdrv_get_cur_cpu_tick();
}

static u32 tsdrv_sqcq_dfx_get_cq_dispatch_head(struct tsdrv_sqcq_dfx *sqcq_dfx_mng, u32 logic_cqid)
{
    return sqcq_dfx_mng->logic_cq_dfx[logic_cqid].logic_cq_dispatch_head;
}

static inline u64 tsdrv_sqcq_dfx_get_cq_dispatch_tick(struct tsdrv_sqcq_dfx *sqcq_dfx_mng, u32 logic_cqid)
{
    return sqcq_dfx_mng->logic_cq_dfx[logic_cqid].logic_cq_dispatch_tick;
}

void tsdrv_sqcq_dfx_set_wakeup_tick(u32 devid, u32 fid, u32 tsid, u32 logic_cqid)
{
#ifndef TSDRV_UT
    struct tsdrv_sqcq_dfx *sqcq_dfx_mng = NULL;

    sqcq_dfx_mng = tsdrv_get_sqcq_dfx_mng(devid, fid, tsid);
    if (sqcq_dfx_mng == NULL) {
        return;
    }

    if (logic_cqid >= TSDRV_MAX_LOGIC_CQ_NUM) {
        TSDRV_PRINT_ERR("Invalid logic cqid. (devid=%u; fid=%u; tsid=%u; logic_cqid=%u)\n",
            devid, fid, tsid, logic_cqid);
        return;
    }

    sqcq_dfx_mng->logic_cq_dfx[logic_cqid].logic_cq_wakeup_tick = tsdrv_get_cur_cpu_tick();
#endif
}

static inline u64 tsdrv_sqcq_dfx_get_wakeup_tick(struct tsdrv_sqcq_dfx *sqcq_dfx_mng, u32 logic_cqid)
{
    return sqcq_dfx_mng->logic_cq_dfx[logic_cqid].logic_cq_wakeup_tick;
}

static void tsdrv_sqcq_dfx_show_cq_tick(u32 devid, u32 fid, u32 tsid, u32 phy_cq_id, u32 logic_cq_id)
{
    struct tsdrv_sqcq_dfx *sqcq_dfx_mng = NULL;

    sqcq_dfx_mng = tsdrv_get_sqcq_dfx_mng(devid, fid, tsid);
    if (sqcq_dfx_mng == NULL) {
        return;
    }

    TSDRV_PRINT_INFO_LIMIT("Show cq tick. (devid=%u; fid=%u; tsid=%u; phy_cq_id=%u; logic_cq_id=%u; irq_tick=%llu; "
        "tasklet_tick=%llu; phy_cq_tick=%llu; dispatch_head=%u; cq_dispatch_tick=%llu; cq_wakeup_tick=%llu)\n",
        devid, fid, tsid, phy_cq_id, logic_cq_id,
        tsdrv_sqcq_dfx_get_cq_irq_tick(tsdrv_get_sqcq_dfx_mng(devid, TSDRV_PM_FID, tsid)),
        tsdrv_sqcq_dfx_get_cq_tasklet_tick(tsdrv_get_sqcq_dfx_mng(devid, TSDRV_PM_FID, tsid)),
        tsdrv_sqcq_dfx_get_phy_cq_tick(sqcq_dfx_mng, phy_cq_id),
        tsdrv_sqcq_dfx_get_cq_dispatch_head(sqcq_dfx_mng, logic_cq_id),
        tsdrv_sqcq_dfx_get_cq_dispatch_tick(sqcq_dfx_mng, logic_cq_id),
        tsdrv_sqcq_dfx_get_wakeup_tick(sqcq_dfx_mng, logic_cq_id));
}

static void tsdrv_sqcq_dfx_report_info_pack(struct tsdrv_cqe_report_info_dfx *report_info,
    struct tag_ts_logic_cq_report_msg *report)
{
#ifndef TSDRV_UT
    report_info->logic_cq_id = (u32)report->logic_cq_id;
    report_info->stream_id = (u32)report->stream_id;
    report_info->task_id = (u32)report->task_id;
    report_info->error_type = (u32)report->error_type;
    report_info->match_flag = (u32)report->match_flag;
#endif
}

static void tsdrv_sqcq_dfx_report_info_show(struct tsdrv_cqe_report_info_dfx *report_info, u32 cq_info_head, u32 cqe_idx)
{
    TSDRV_PRINT_INFO("Cq report show. (logic_cq_id=%u; stream_id=%u; task_id=%u; error_type=%u; match_flag=%u; "
        "cqe_idx=%u; cq_info_head=%u)\n", report_info->logic_cq_id, report_info->stream_id, report_info->task_id,
        report_info->error_type, report_info->match_flag, cq_info_head, cqe_idx);
}

static void tsdrv_sqcq_dfx_show_phy_cq_report(u32 devid, u32 fid, u32 tsid, u32 phy_cq_id, u32 logic_cq_id)
{
    struct tsdrv_ts_resource *ts_res;
    struct devdrv_ts_cq_info *cq_info = NULL;
    struct devdrv_cq_sub_info *cq_sub = NULL;
    struct tag_ts_logic_cq_report_msg *report = NULL;
    struct tsdrv_cqe_report_info_dfx report_info;
    u32 cqe_idx, phase;
    size_t offset;

    ts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
    cq_info = devdrv_calc_cq_info(ts_res->mem_info[DEVDRV_INFO_MEM].virt_addr, phy_cq_id);
    if ((cq_info == NULL) || (cq_info->cq_sub == NULL)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_WARN("Phy cq maynot exit. (devid=%u; fid=%u; tsid=%u; phy_cq_id=%u)\n",
            devid, fid, tsid, phy_cq_id);
        return;
#endif
    }

    cq_sub = (struct devdrv_cq_sub_info *)cq_info->cq_sub;
    ka_task_spin_lock_irq(&cq_sub->spinlock);
    if (cq_sub->virt_addr == 0) {
#ifndef TSDRV_UT
        ka_task_spin_unlock_irq(&cq_sub->spinlock);
        TSDRV_PRINT_WARN("Phy cq sub virt addr is null. (devid=%u; tsid=%u; phy_cq_id=%u)\n", devid, tsid, phy_cq_id);
        return;
#endif
    }

    cqe_idx = cq_info->head;
    phase = cq_info->phase;
    offset = cq_sub->slot_size * cqe_idx;
    report = (struct tag_ts_logic_cq_report_msg *)(uintptr_t)(cq_sub->virt_addr + offset);

    while ((u32)report->phase == phase) {
#ifndef TSDRV_UT
        if (report->logic_cq_id == logic_cq_id) {
            tsdrv_sqcq_dfx_report_info_pack(&report_info, report);
            ka_task_spin_unlock_irq(&cq_sub->spinlock);
            tsdrv_sqcq_dfx_report_info_show(&report_info, cq_info->head, cqe_idx);
            return;
        }
        cqe_idx++;
        if (cqe_idx == cq_sub->depth) {
            cqe_idx = 0;
            phase = !phase;
        }
        offset = cq_sub->slot_size * cqe_idx;
        report = (struct tag_ts_logic_cq_report_msg *)(uintptr_t)(cq_sub->virt_addr + offset);
#endif
    }
    ka_task_spin_unlock_irq(&cq_sub->spinlock);

    TSDRV_PRINT_INFO_LIMIT("Without logic cqe. (logic_cq_id=%u; cq_info_head=%u; cqe_idx=%u)\n",
        logic_cq_id, cq_info->head, cqe_idx);
}

void tsdrv_sqcq_dfx_show(struct tsdrv_ctx *ctx, u32 tsid, u32 logic_cq_id)
{
    struct logic_sqcq_ts_ctx *logic_ctx = NULL;
    struct tsdrv_ts_resource *ts_res = NULL;
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    u32 fid = tsdrv_get_fid_by_ctx(ctx);
    u32 phy_logic_cq;

#ifdef CFG_TSDRV_VM_ENV
    return;
#endif

    logic_ctx = (struct logic_sqcq_ts_ctx *)ctx->ts_ctx[tsid].logic_sqcq_ctx;
    if (logic_ctx == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Logic ctx is null. (devid=%u; fid=%u; tsid=%u)\n", devid, fid, tsid);
        return;
#endif
    }

    if (tsdrv_is_in_container(devid)) {
        ts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
        phy_logic_cq = ts_res->logic_phy_cq;
    } else {
        phy_logic_cq = logic_ctx->phy_cq.index;
    }

    tsdrv_sqcq_dfx_show_phy_cq_report(devid, fid, tsid, phy_logic_cq, logic_cq_id);
    tsdrv_sqcq_dfx_show_cq_tick(devid, fid, tsid, phy_logic_cq, logic_cq_id);
}