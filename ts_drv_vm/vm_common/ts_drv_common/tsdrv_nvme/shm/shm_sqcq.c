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
#include <linux/types.h>
#include <linux/slab.h>

#include "shm_sqcq.h"
#include "devdrv_recycle.h"
#include "devdrv_cqsq.h"
#include "devdrv_id.h"
#include "tsdrv_ctx.h"
#ifndef CFG_SOC_PLATFORM_MDC_V51
#include "hvtsdrv_tsagent.h"
#include "hvtsdrv_shm_sqcq.h"
#endif
#include "hvtsdrv_id.h"

enum shm_cq_report_type {
    SHM_CQ_REPORT_TYPE_TASK = 0, /**< task command report */
    SHM_CQ_REPORT_TYPE_PROFILE_TIMELINE = 1, /**< timeline profile data */
    SHM_CQ_REPORT_TYPE_PROFILE_TASKBASED = 2, /**< task based profile data */
    SHM_CQ_REPORT_TYPE_RECYCLE_SQ_FINISHED = 3, /* recycle sq report */
    SHM_CQ_REPORT_TYPE_RECYCLE_STREAM_FINISHED = 4, /* recycle streamid report */
    SHM_CQ_REPORT_TYPE_RECYCLE_NOTIFY_FINISHED = 5, /* recycle notifyid report */
    SHM_CQ_REPORT_TYPE_RECYCLE_EVENT_FINISHED = 6, /* recycle eventid report */
    SHM_CQ_REPORT_TYPE_RECYCLE_CQ_FINISHED = 7, /* recycle eventid report */
    SHM_CQ_REPORT_TYPE_UPDATE_SQ_HEAD = 8
};

struct tag_ts_driver_msg {
    volatile uint16_t phase : 1;
    volatile uint16_t report_type : 15;
    volatile uint16_t sq_id;
    volatile uint16_t sq_head;
    volatile uint16_t recycle_id;
    volatile uint64_t reserved;
};

STATIC void shm_sqcq_ts_ctx_exit(struct tsdrv_ctx *ctx, u32 tsid)
{
    struct shm_sqcq_ts_ctx *shm_ctx = NULL;

    if (ctx->ts_ctx[tsid].shm_sqcq_ctx != NULL) {
        shm_ctx = (struct shm_sqcq_ts_ctx *)ctx->ts_ctx[tsid].shm_sqcq_ctx;
        ka_task_mutex_destroy(&shm_ctx->lock);
        tsdrv_kfree(ctx->ts_ctx[tsid].shm_sqcq_ctx);
        ctx->ts_ctx[tsid].shm_sqcq_ctx = NULL;
    }
}

#ifdef CFG_FEATURE_RUNTIME_NO_THREAD
int shm_sqcq_ctx_init(struct tsdrv_ctx *ctx, u32 tsnum)
{
    struct shm_sqcq_ts_ctx *shm_ctx = NULL;
    u32 tsid;

    for (tsid = 0; tsid < tsnum; tsid++) {
        ctx->ts_ctx[tsid].shm_sqcq_ctx = tsdrv_kzalloc(sizeof(struct shm_sqcq_ts_ctx), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
#ifndef TSDRV_UT
        if (ctx->ts_ctx[tsid].shm_sqcq_ctx == NULL) {
            TSDRV_PRINT_ERR("kzalloc shm sqcq ctx fail, tsid=%u\n", tsid);
            goto err_kmalloc_shm_ctx;
        }
#endif
        KA_TASK_INIT_WORK(&ctx->ts_ctx[tsid].recycle_work, shm_sqcq_rec_work);
        shm_ctx = (struct shm_sqcq_ts_ctx *)ctx->ts_ctx[tsid].shm_sqcq_ctx;
        shm_ctx->sqid = DEVDRV_MAX_SQ_NUM;
        shm_ctx->cqid = DEVDRV_MAX_CQ_NUM;
        shm_ctx->shm_sqcq_num = 0;
        shm_ctx->pid = ctx->pid;
        shm_ctx->tgid = ctx->tgid;
        ka_base_atomic_set(&shm_ctx->rec_ready_flag, 0);
        ka_task_mutex_init(&shm_ctx->lock);
        ka_task_init_waitqueue_head(&shm_ctx->rec_ready);
    }
    return 0;
#ifndef TSDRV_UT
err_kmalloc_shm_ctx:
    for (tsid = 0; tsid < tsnum; tsid++) {
        shm_sqcq_ts_ctx_exit(ctx, tsid);
    }
    return -ENOMEM;
#endif
}

void shm_sqcq_ctx_exit(struct tsdrv_ctx *ctx, u32 tsnum)
{
    u32 tsid;

    for (tsid = 0; tsid < tsnum; tsid++) {
        shm_sqcq_ts_ctx_exit(ctx, tsid);
    }
}
#else
int shm_sqcq_ctx_init(struct tsdrv_ctx *ctx, u32 tsnum)
{
    return 0;
}

void shm_sqcq_ctx_exit(struct tsdrv_ctx *ctx, u32 tsnum)
{
}
#endif

#ifndef TSDRV_UT
STATIC int shm_sqcq_phy_cq_handle_chk(u32 devid, u32 tsid, struct devdrv_ts_cq_info *cq_info)
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
    if (cq_sub->ctx == NULL) {
        TSDRV_PRINT_ERR("cq_sub->ctx is NULL, cqid=%u devid=%u tsid=%u\n", cq_info->index, devid, tsid);
        return -ENOMEM;
    }
    return 0;
}

STATIC int shm_sqcq_res_rec(u32 devid, u32 tsid, struct tsdrv_ctx *ctx, struct tag_ts_driver_msg *shm_cq_report,
    struct devdrv_recycle_message *recycle_msg)
{
    struct tsdrv_ts_resource *ts_res = NULL;
    struct shm_sqcq_ts_ctx *shm_ctx = NULL;
    struct phy_cq_free_para para;
    u32 stream_id;

    shm_ctx = (struct shm_sqcq_ts_ctx *)ctx->ts_ctx[tsid].shm_sqcq_ctx;
    if (shm_ctx == NULL) {
        TSDRV_PRINT_ERR("shm_ctx is NULL, devid=%u tsid=%u\n", devid, tsid);
        return -ENOMEM;
    }

    ts_res = tsdrv_ctx_to_ts_res(ctx, tsid);
    switch (shm_cq_report->report_type) {
        case SHM_CQ_REPORT_TYPE_RECYCLE_SQ_FINISHED:
            recycle_msg->recycle_sq[recycle_msg->recycle_sq_num % DEVDRV_MAX_SQ_NUM] = shm_cq_report->sq_id;
            recycle_msg->recycle_sq_num++;
            tsdrv_recycle_sq(ts_res, ctx, shm_cq_report->sq_id);
            TSDRV_PRINT_DEBUG("recycle sqid=%u, devid=%u tsid=%u\n", (u32)shm_cq_report->sq_id, devid, tsid);
            break;
        case SHM_CQ_REPORT_TYPE_RECYCLE_STREAM_FINISHED:
            stream_id = shm_cq_report->recycle_id;
            recycle_msg->recycle_stream[recycle_msg->recycle_stream_num % DEVDRV_MAX_STREAM_ID] = stream_id;
            recycle_msg->recycle_stream_num++;
            tsdrv_recycle_stream(ts_res, ctx, stream_id);
            TSDRV_PRINT_DEBUG("recycle stream_id=%u, devid=%u tsid=%u\n", stream_id, devid, tsid);
            break;
        case SHM_CQ_REPORT_TYPE_RECYCLE_NOTIFY_FINISHED:
            recycle_msg->recycle_notify_num++;
            tsdrv_recycle_notify_id(ts_res, ctx);
            TSDRV_PRINT_DEBUG("recycle notify_id devid=%u tsid=%u\n", devid, tsid);
            break;
        case SHM_CQ_REPORT_TYPE_RECYCLE_EVENT_FINISHED:
            recycle_msg->recycle_event_num++;
            tsdrv_recycle_event_id(ts_res, ctx);
            tsdrv_recycle_ipc_event_id(ts_res, ctx);
            TSDRV_PRINT_DEBUG("recycle event_id devid=%u tsid=%u\n", devid, tsid);
            break;
        case SHM_CQ_REPORT_TYPE_RECYCLE_CQ_FINISHED:
            recycle_msg->recycle_cq[recycle_msg->recycle_cq_num % DEVDRV_MAX_CQ_NUM] = shm_cq_report->recycle_id;
            recycle_msg->recycle_cq_num++;
            if (shm_ctx->cqid != shm_cq_report->recycle_id) {
#ifndef TSDRV_UT
                TSDRV_PRINT_ERR("cqid=%u is not allocated by tgid=%d, devid=%u tsid=%u, shm_ctx_cqid=%u\n",
                    (u32)shm_cq_report->recycle_id, shm_ctx->tgid, devid, tsid, shm_ctx->cqid);
#endif
                return -EFAULT;
            }
            para.cq_id = shm_cq_report->recycle_id;
            (void)tsdrv_phy_cq_recycle(ctx, tsid, &para);
            TSDRV_PRINT_DEBUG("recycle cqid=%u devid=%u tsid=%u\n", (u32)para.cq_id, devid, tsid);
            return -EACCES; // cq recycle finished, return -EACCESS to recycle all resources remained
        default:
            recycle_msg->invalid_type_num++;
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("invalid report type=%u\n", (u32)shm_cq_report->report_type);
#endif
            return -ENODEV;
    }
    return 0;
}

STATIC int shm_sqcq_rec_handler(u32 devid, struct tsdrv_ts_resource *ts_res, struct tsdrv_ctx *ctx,
    struct devdrv_recycle_message *recycle_msg)
{
    struct tag_ts_driver_msg *shm_cq_report = NULL;
    struct devdrv_cq_sub_info *cq_sub = NULL;
    struct devdrv_ts_cq_info *cq_info = NULL;
    struct shm_sqcq_ts_ctx *shm_ctx = NULL;
    u32 report_cnt = 0;
    size_t offset;
    int err = 0;

    shm_ctx = (struct shm_sqcq_ts_ctx *)ctx->ts_ctx[ts_res->tsid].shm_sqcq_ctx;
    if (shm_ctx == NULL) {
        TSDRV_PRINT_ERR("shm_ctx is NULL, devid=%u tsid=%u\n", devid, ts_res->tsid);
        return -ENOMEM;
    }
    cq_info = devdrv_calc_cq_info(ts_res->mem_info[DEVDRV_INFO_MEM].virt_addr, shm_ctx->cqid);
    cq_sub = (struct devdrv_cq_sub_info *)cq_info->cq_sub;

    offset = cq_sub->slot_size * cq_info->head;
    shm_cq_report = (struct tag_ts_driver_msg *)((uintptr_t)cq_sub->virt_addr + offset);
    tsdrv_dma_sync_cpu(devid, cq_sub->phy_addr, cq_sub->size, KA_DMA_FROM_DEVICE);
    while (shm_cq_report->phase == cq_info->phase) {
        /**
         * In the ARM environment:
         * 1.Out-of-order may cause the TS driver to read other members of the report structure (read instruction 1)
         *   to be executed before the phase comparison instruction (read instruction 2);
         * 2.The time when TS writes report is between [read instruction 1] and [read instruction 2];
         * This error timing may cause [Read instruction 1] to read the old value.
         */
        ka_rmb();
        switch (shm_cq_report->report_type) {
            case SHM_CQ_REPORT_TYPE_RECYCLE_SQ_FINISHED:
            case SHM_CQ_REPORT_TYPE_RECYCLE_STREAM_FINISHED:
            case SHM_CQ_REPORT_TYPE_RECYCLE_NOTIFY_FINISHED:
            case SHM_CQ_REPORT_TYPE_RECYCLE_EVENT_FINISHED:
            case SHM_CQ_REPORT_TYPE_RECYCLE_CQ_FINISHED:
                err = shm_sqcq_res_rec(devid, ts_res->tsid, ctx, shm_cq_report, recycle_msg);
                break;
            default:
                recycle_msg->invalid_type_num++;
                err = 0;
                break;
        }
        if (err == -EACCES) {
            TSDRV_PRINT_INFO("Recycle finish, don't handle any cq report anymore. "
                "(devid=%u; tgid=%d; phy_cqid=%u)\n", devid, ctx->tgid, cq_info->index);
            return err;
        }
        cq_info->head = cq_info->head + 1;
        if (cq_info->head == cq_sub->depth) {
            cq_info->head = 0;
            cq_info->phase = !cq_info->phase;
        }
        offset = cq_sub->slot_size * cq_info->head;
        shm_cq_report = (struct tag_ts_driver_msg *)(uintptr_t)(cq_sub->virt_addr + offset);
        report_cnt++;
    }

    if (report_cnt != 0) {
        recycle_msg->send_doorbell_num += report_cnt;
        (void)tsdrv_ring_phy_cq_doorbell(devid, ts_res->tsid, cq_info->index, cq_info->head);
    }
    return 0;
}

STATIC bool shm_rec_wait_flag_chk(struct shm_sqcq_ts_ctx *shm_ctx)
{
    int val = ka_base_atomic_cmpxchg(&shm_ctx->rec_ready_flag, 1, 0);
    return val == 1;
}
#endif /* TSDRV_UT */

STATIC void shm_sqcq_rec_wait(u32 devid, struct tsdrv_ts_resource *ts_res, struct tsdrv_ctx *ctx,
    struct devdrv_recycle_message *recycle_msg)
{
#ifndef TSDRV_UT
    unsigned long timeout = ka_system_msecs_to_jiffies(2000); // check every 2 sec
    struct shm_sqcq_ts_ctx *shm_ctx = NULL;
    struct tsdrv_ts_ctx *ts_ctx = NULL;
    enum tsdrv_dev_status dev_status;
    int loop = 100; // loop 100 times maximum
    long err;
    u32 tsid;
    int ret;

    tsid = ts_res->tsid;
    ts_ctx = &ctx->ts_ctx[tsid];
    shm_ctx = (struct shm_sqcq_ts_ctx *)ts_ctx->shm_sqcq_ctx;
    DRV_CHECK_PTR(shm_ctx, return, "shm_sqcq_ctx is NULL, devid=%u tsid=%u\n", devid, tsid);
    DRV_CHECK_EXP_ACT((devid >= TSDRV_MAX_DAVINCI_NUM) || (tsid >= tsdrv_get_dev_tsnum(devid)), return,
        "invalid devid=%u tsid=%u\n", devid, tsid);

    ka_task_mutex_lock(&shm_ctx->lock);
    if (shm_ctx->shm_sqcq_num == 0) {
        ka_task_mutex_unlock(&shm_ctx->lock);
        TSDRV_PRINT_WARN("no shm sqcq, release all resources, devid=%u tsid=%u\n", devid, tsid);
        goto out_recycle_finish;
    }
    ka_task_mutex_unlock(&shm_ctx->lock);

    TSDRV_PRINT_DEBUG("start wait, shm_sqcq_num=%u devid=%u tsid=%u\n", shm_ctx->shm_sqcq_num, devid, tsid);
    while (loop > 0) {
        dev_status = tsdrv_get_dev_status(devid, TSDRV_PM_FID);
        if (dev_status != TSDRV_DEV_ACTIVE) {
            TSDRV_PRINT_WARN("invalid device status=%d don't wait for cq recycle, devid=%u\n",
                (int)dev_status, devid);
            return;
        }
        err = ka_task_wait_event_interruptible_timeout(shm_ctx->rec_ready, shm_rec_wait_flag_chk(shm_ctx), timeout);
        if (err > 0) {
            ret = shm_sqcq_rec_handler(devid, ts_res, ctx, recycle_msg);
            if (ret == -EACCES) {
                TSDRV_PRINT_DEBUG("rec finished, recycle all others resources, devid=%u tsid=%u\n", devid, tsid);
                goto out_recycle_finish;
            }
            if (ret != 0) {
                TSDRV_PRINT_ERR("shm sqcq rec handler fail, devid=%u tsid=%u ret=%d\n", devid, tsid, ret);
                return;
            }
            continue;
        }
        if (err == -ERESTARTSYS) {
            TSDRV_PRINT_INFO("recycle wait interrupted, devid=%u tsid=%u loop=%d\n", devid, tsid, loop);
            continue;
        }
        loop--;
        TSDRV_PRINT_DEBUG("loop=%d, devid=%u tsid=%u\n", loop, devid, tsid);
    }
    TSDRV_PRINT_ERR("recycle fail, loop=%d devid=%u tsid=%u\n", loop, devid, tsid);
    return;
out_recycle_finish:
    tsdrv_recycle_streams(ts_res, ctx);
    tsdrv_recycle_notify_id(ts_res, ctx);
    tsdrv_recycle_event_id(ts_res, ctx);
    tsdrv_recycle_ipc_event_id(ts_res, ctx);
    tsdrv_recycle_all_sq(ts_res, ctx);
    return;
#endif
}

STATIC int shm_sqcq_rec_process(struct tsdrv_ts_resource *ts_res, struct tsdrv_ctx *ctx,
    struct tsdrv_ts_ctx *ts_ctx)
{
#ifndef TSDRV_UT
    struct devdrv_recycle_message *recycle_msg = NULL;
    u32 devid, fid, tsid;
    ka_pid_t tgid;
    int err;

    tgid = ctx->tgid;
    devid = tsdrv_get_devid_by_ctx(ctx);
    fid = tsdrv_get_fid_by_ctx(ctx);
    tsid = ts_ctx->tsid;

    recycle_msg = tsdrv_kzalloc(sizeof(struct devdrv_recycle_message), KA_GFP_KERNEL);
    if (recycle_msg == NULL) {
        TSDRV_PRINT_ERR("kzalloc recycle_message fail, devid=%u tsid=%u fid=%u.\n", devid, tsid, fid);
        return -ENOMEM;
    }

    shm_sqcq_rec_wait(devid, ts_res, ctx, recycle_msg);
    tsdrv_recycle_model_id(ts_res, ctx);
    if (tsdrv_ts_ctx_id_res_chk(ts_ctx, ctx->tgid, ctx->pid, LEAK_CHK_PRINT) == true) {
        tsdrv_resource_recycled_print(recycle_msg);
        tsdrv_resource_no_recycle_print(ts_res, ctx);
        ka_base_atomic_set(&ts_ctx->status, TSDRV_TS_CTX_RECYCLE_ERR);
        tsdrv_dev_set_ctx_recycle_status(ctx, TSDRV_CTX_RECYCLE_ERR);
        TSDRV_PRINT_DEBUG("shm_sqcq_ts_ctx_exit, devid=%u tsid=%u fid=%u.\n", devid, tsid, fid);
        shm_sqcq_ts_ctx_exit(ctx, ts_ctx->tsid);
        err = tsdrv_set_runtime_available_conflict_check(devid);
        if (err != 0) {
            TSDRV_PRINT_ERR("set runtime status fail err %d, devid=%u tsid=%u fid=%u.\n", err, devid, tsid, fid);
        }
        tsdrv_kfree(recycle_msg);
        return -EBUSY;
    }
    ka_base_atomic_set(&ts_ctx->status, TSDRV_TS_CTX_IDLE);

    TSDRV_PRINT_DEBUG("shm_sqcq_ts_ctx_exit tgid=%d, devid=%u tsid=%u\n", tgid, devid, tsid);
    shm_sqcq_ts_ctx_exit(ctx, ts_ctx->tsid);
    TSDRV_PRINT_DEBUG("exit shm sqcq rec work tgid=%d, devid=%u tsid=%u\n", tgid, devid, tsid);

    tsdrv_kfree(recycle_msg);
#endif /* TSDRV_UT */
    return 0;
}

void shm_sqcq_rec_work(ka_work_struct_t *work)
{
#ifndef TSDRV_UT
    struct tsdrv_ts_resource *ts_res = NULL;
    struct tsdrv_ts_ctx *ts_ctx = NULL;
    struct tsdrv_ctx *ctx = NULL;
    u32 devid, fid, tsnum, i;
    int ret;

    ts_ctx = ka_container_of(work, struct tsdrv_ts_ctx, recycle_work);
    ctx = tsdrv_tsctx_to_ctx(ts_ctx);
    ts_res = tsdrv_ctx_to_ts_res(ctx, ts_ctx->tsid);

    devid = tsdrv_get_devid_by_ctx(ctx);
    fid = tsdrv_get_fid_by_ctx(ctx);
    tsnum = tsdrv_get_dev_tsnum(devid);
    TSDRV_PRINT_DEBUG("rec work start. devid[%u], fid[%u], tsid[%u], tgid[%d], tsnum[%u]\n",
        devid, fid, ts_ctx->tsid, ctx->tgid, tsnum);

    ret = shm_sqcq_rec_process(ts_res, ctx, ts_ctx);
#ifndef CFG_SOC_PLATFORM_MDC_V51
    // no thread. send recycle vpc msg to vm.
    hvtsdrv_recycle(devid, fid, ctx->tgid, ctx->unique_id, ret);
#endif
    if (ret != 0) {
        return;
    }

    for (i = 0; i < tsnum; i++) {
        if (ka_base_atomic_read(&ctx->ts_ctx[i].status) != TSDRV_TS_CTX_IDLE) {
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

STATIC void shm_sqcq_send_rec_msg(u32 dev_id, u32 tsid, int pid, u8 fid)
{
#ifndef TSDRV_UT
    struct recycle_pid_info msg;
    struct tsdrv_mbox_data data;
    int err;

    msg.header.valid = DEVDRV_MAILBOX_MESSAGE_VALID;
    msg.header.cmd_type = DEVDRV_MAILBOX_RECYCLE_PID_ID;
    msg.header.result = 0;
    msg.pid_info.app_cnt = 1;
    msg.pid_info.pid[0] = pid;
    msg.pid_info.plat_type = tsdrv_get_env_type();
    msg.pid_info.fid = fid;
    data.msg = &msg;
    data.msg_len = sizeof(struct recycle_pid_info);
    data.out_data = NULL;
    data.out_len = 0;

    err = tsdrv_mailbox_send_sync(dev_id, tsid, &data);
    if (err != 0) {
        TSDRV_PRINT_WARN("send rec msg fail err=%d, devid=%u tsid=%u\n", err, dev_id, tsid);
        return;
    }
    TSDRV_PRINT_DEBUG("Send rec msg success. (devid=%u; fid=%u; tsid=%u; pid=%d)\n",
        dev_id, (u32)fid, tsid, pid);
#endif
}

int shm_sqcq_dev_res_recycle(u32 devid, struct tsdrv_dev_resource *dev_res, struct tsdrv_ctx *ctx)
{
    u32 tsnum = tsdrv_get_dev_tsnum(devid);
    struct tsdrv_dev_ctx *dev_ctx = NULL;
    int recycle_err = 0;
    u32 tsid;
    int err;

    if (unlikely(ctx->ctx_index >= TSDRV_MAX_CTX_NUM)) {
        TSDRV_PRINT_ERR("invalid ctx_index(%u)\n", ctx->ctx_index);
        return -EINVAL;
    }
    dev_ctx = ka_container_of(ctx, struct tsdrv_dev_ctx, ctx[ctx->ctx_index]);
    if (unlikely(dev_ctx->fid >= TSDRV_MAX_FID_NUM)) {
        TSDRV_PRINT_ERR("invalid fid(%u)\n", dev_ctx->fid);
        return -EINVAL;
    }

    TSDRV_PRINT_DEBUG("Start to recycle. (tsnum=%u)\n", tsnum);
    for (tsid = 0; tsid < tsnum; tsid++) {
        struct tsdrv_ts_resource *ts_res = &dev_res->ts_resource[tsid];
        struct tsdrv_ts_ctx *ts_ctx = &ctx->ts_ctx[tsid];
        if (tsdrv_ts_ctx_id_res_chk(ts_ctx, ctx->tgid, ctx->pid, LEAK_CHK_NO_PRINT) == false) {
            TSDRV_PRINT_DEBUG("shm_sqcq_ts_ctx_exit, devid=%u tsid=%u fid=%u.\n", devid, tsid, dev_ctx->fid);
            shm_sqcq_ts_ctx_exit(ctx, tsid);
            continue;
        }
        ka_base_atomic_set(&ts_ctx->status, TSDRV_TS_CTX_START_RECYCLE);

        shm_sqcq_send_rec_msg(devid, tsid, ctx->tgid, dev_ctx->fid);

        if (tsdrv_is_in_vm(devid) == false) {
            err = shm_sqcq_rec_process(ts_res, ctx, ts_ctx);
            if (err != 0) {
                recycle_err++;
            }
        } else {
#ifndef TSDRV_UT
            ka_task_mutex_lock(&dev_res->dev_res_lock);
            if (ts_res->wq != NULL) {
                TSDRV_PRINT_DEBUG("Start recycle work. (devid=%u; tsid=%u)\n", devid, tsid);
                (void)ka_task_queue_work(ts_res->wq, &ts_ctx->recycle_work);
            }
            ka_task_mutex_unlock(&dev_res->dev_res_lock);
#endif
        }
    }

    return (recycle_err > 0) ? -EBUSY : 0;
}

#ifndef TSDRV_UT
STATIC int shm_sqcq_update_sq_head(u32 devid, u32 fid, u32 tsid, struct tag_ts_driver_msg *shm_cq_report)
{
    u32 sq_head;
    u32 sq_id;

    sq_id = shm_cq_report->sq_id;
    sq_head = shm_cq_report->sq_head;
    if (sq_id >= DEVDRV_MAX_SQ_NUM) {
        TSDRV_PRINT_ERR("invalid sq_id=%u, devid=%u tsid=%u\n", sq_id, devid, tsid);
        return -ENODEV;
    }
    if (sq_head >= DEVDRV_MAX_SQ_DEPTH) {
        TSDRV_PRINT_ERR("invalid sq_head=%u, devid=%u tsid=%u\n", sq_head, devid, tsid);
        return -EFAULT;
    }

    TSDRV_PRINT_DEBUG("sq_id=%u sq_head=%u, devid=%u tsid=%u\n", sq_id, sq_head, devid, tsid);
    return tsdrv_phy_sq_head_update(devid, fid, tsid, sq_id, sq_head);
}

STATIC void shm_sqcq_ctx_wakeup(struct shm_sqcq_ts_ctx *shm_ctx)
{
    ka_base_atomic_set(&shm_ctx->rec_ready_flag, 1);
    if (ka_task_waitqueue_active(&shm_ctx->rec_ready) != 0) {
        ka_task_wake_up(&shm_ctx->rec_ready);
    }
}
#endif /* TSDRV_UT */

void shm_sqcq_phy_cq_handler(u32 devid, u32 tsid, struct devdrv_ts_cq_info *cq_info)
{
#ifndef TSDRV_UT
    struct tag_ts_driver_msg *shm_cq_report = NULL;
    struct devdrv_cq_sub_info *cq_sub = NULL;
    struct shm_sqcq_ts_ctx *shm_ctx = NULL;
    struct tsdrv_ctx *ctx = NULL;
    u32 report_cnt = 0;
    size_t offset;
    u32 fid;
    int err;

    TSDRV_PRINT_DEBUG("Handle shm sqcq. (devid=%u; tsid=%u)\n", devid, tsid);
    err = shm_sqcq_phy_cq_handle_chk(devid, tsid, cq_info);
    if (err != 0) {
        return;
    }
    cq_sub = (struct devdrv_cq_sub_info *)cq_info->cq_sub;
    ctx = cq_sub->ctx;

    fid = tsdrv_get_fid_by_ctx(ctx);
    offset = cq_sub->slot_size * cq_info->head;
    shm_cq_report = (struct tag_ts_driver_msg *)((uintptr_t)cq_sub->virt_addr + offset);
    tsdrv_dma_sync_cpu(devid, cq_sub->phy_addr, cq_sub->size, KA_DMA_FROM_DEVICE);

    TSDRV_PRINT_DEBUG("1. cq_head=%u, size=%lu depth=%u report_phase=%u; cq_phase=%u;phy_cqid=%u\n", cq_info->head,
        cq_sub->slot_size, cq_sub->depth, (u32)shm_cq_report->phase, cq_info->phase, cq_sub->index);
    while (shm_cq_report->phase == cq_info->phase) {
        /**
         * In the ARM environment:
         * 1.Out-of-order may cause the TS driver to read other members of the report structure (read instruction 1)
         *   to be executed before the phase comparison instruction (read instruction 2);
         * 2.The time when TS writes report is between [read instruction 1] and [read instruction 2];
         * This error timing may cause [Read instruction 1] to read the old value.
         */
        ka_rmb();
        switch (shm_cq_report->report_type) {
            case SHM_CQ_REPORT_TYPE_UPDATE_SQ_HEAD:
                TSDRV_PRINT_DEBUG("update sq head, repot_type=%u, recycle_id=%u phase=%u sq_id=%u sq_head=%u "
                    "phy_cqid=%u\n", (u32)shm_cq_report->report_type, (u32)shm_cq_report->recycle_id,
                    (u32)shm_cq_report->phase, (u32)shm_cq_report->sq_id, (u32)shm_cq_report->sq_head, cq_sub->index);
                err = shm_sqcq_update_sq_head(devid, fid, tsid, shm_cq_report);
                break;
            case SHM_CQ_REPORT_TYPE_RECYCLE_SQ_FINISHED:
            case SHM_CQ_REPORT_TYPE_RECYCLE_STREAM_FINISHED:
            case SHM_CQ_REPORT_TYPE_RECYCLE_NOTIFY_FINISHED:
            case SHM_CQ_REPORT_TYPE_RECYCLE_EVENT_FINISHED:
            case SHM_CQ_REPORT_TYPE_RECYCLE_CQ_FINISHED:
                TSDRV_PRINT_DEBUG("resouce recycle, repot_type=%u, recycle_id=%u phase=%u sq_id=%u sq_head=%u "
                    "phy_cqid=%u\n", (u32)shm_cq_report->report_type, (u32)shm_cq_report->recycle_id,
                    (u32)shm_cq_report->phase, (u32)shm_cq_report->sq_id, (u32)shm_cq_report->sq_head, cq_sub->index);
                shm_ctx = (struct shm_sqcq_ts_ctx *)ctx->ts_ctx[tsid].shm_sqcq_ctx;
                if (shm_ctx == NULL) {
                    return;
                }
                shm_sqcq_ctx_wakeup(shm_ctx);
                return;
            default:
#ifndef TSDRV_UT
                TSDRV_PRINT_ERR("invalid report type=%u, devid=%u tsid=%u\n", (u32)shm_cq_report->report_type,
                    devid, tsid);
#endif
                return;
        }
        if (err != 0) {
            TSDRV_PRINT_ERR("handle report type fail, err=%d\n", err);
            break;
        }
        cq_info->head = cq_info->head + 1;
        if (cq_info->head == cq_sub->depth) {
            cq_info->head = 0;
            cq_info->phase = !cq_info->phase;
        }
        offset = cq_sub->slot_size * cq_info->head;
        shm_cq_report = (struct tag_ts_driver_msg *)(uintptr_t)(cq_sub->virt_addr + offset);
        report_cnt++;
        TSDRV_PRINT_DEBUG("2. cq_head=%u, phase=%u report_cnt=%u phy_cqid=%u\n", cq_info->head,
            (u32)shm_cq_report->phase, report_cnt, cq_sub->index);
    }
    TSDRV_PRINT_DEBUG("3. cq_head=%u, phase=%u report_cnt=%u phy_cqid=%u\n",
        cq_info->head, (u32)shm_cq_report->phase, report_cnt, cq_sub->index);
    if (report_cnt != 0) {
        (void)tsdrv_ring_phy_cq_doorbell(devid, tsid, cq_info->index, cq_info->head);
    }
#endif /* TSDRV_UT */
}

STATIC int shm_sqcq_mbox_alloc_msg_pack(struct tsdrv_ctx *ctx, struct shm_sqcq_mbox_msg *shm_sqcq_mbox,
    struct phy_sq_alloc_para *sq_alloc_para, struct phy_cq_alloc_para *cq_alloc_para,
    struct shm_sqcq_alloc_para *shm_sqcq_alloc_para)
{
    size_t size;
    int err;

    shm_sqcq_mbox->valid = DEVDRV_MAILBOX_MESSAGE_VALID;
    shm_sqcq_mbox->cmd = TSDRV_MBOX_SHM_SQCQ_ALLOC;
    shm_sqcq_mbox->result = 0;

    shm_sqcq_mbox->sq_id = sq_alloc_para->sq_id;
    shm_sqcq_mbox->sq_addr = sq_alloc_para->sq_paddr;
    shm_sqcq_mbox->sqesize = sq_alloc_para->sq_size;
    shm_sqcq_mbox->sqdepth = sq_alloc_para->sq_depth;

    shm_sqcq_mbox->cq_id = cq_alloc_para->cq_id;
    shm_sqcq_mbox->cq_addr = cq_alloc_para->cq_paddr;
    shm_sqcq_mbox->cqesize = cq_alloc_para->cq_size;
    shm_sqcq_mbox->cqdepth = cq_alloc_para->cq_depth;
    shm_sqcq_mbox->cq_irq = cq_alloc_para->cq_irq;

    shm_sqcq_mbox->app_type = tsdrv_get_env_type();
    shm_sqcq_mbox->sq_cq_side = 0;
    if (shm_sqcq_mbox->app_type == TSDRV_ENV_ONLINE) {
        shm_sqcq_mbox->sq_cq_side = (0x0 << 0) | (0x1 << 1);
    }
    shm_sqcq_mbox->fid = tsdrv_get_fid_by_ctx(ctx);
    shm_sqcq_mbox->pid = ctx->tgid;

    TSDRV_PRINT_DEBUG("Send Msg to Ts. (tgid=%d; phy_sqid=%u; phy_cqid=%u)\n", ctx->tgid,
        sq_alloc_para->sq_id, cq_alloc_para->cq_id);

    size = sizeof(u32) * SQCQ_RTS_INFO_LENGTH;
    err = memcpy_s(shm_sqcq_mbox->info, size, shm_sqcq_alloc_para->info, size);
    if (err != EOK) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("copy mbox info fail, err=%d\n", err);
        return err;
#endif
    }

    return 0;
}

STATIC void shm_sqcq_mbox_free_msg_pack(u32 devid, struct tsdrv_ctx *ctx, struct shm_sqcq_mbox_msg *shm_sqcq_mbox,
    struct phy_sq_free_para *sq_free_para, struct phy_cq_free_para *cq_free_para)
{
    shm_sqcq_mbox->valid = DEVDRV_MAILBOX_MESSAGE_VALID;
    shm_sqcq_mbox->cmd = TSDRV_MBOX_SHM_SQCQ_FREE;
    shm_sqcq_mbox->result = 0;

    if (tsdrv_is_in_pm(devid) == true) {
        shm_sqcq_mbox->sq_id = sq_free_para->sq_id;
    } else {
        shm_sqcq_mbox->sq_id = (u16)-1;
    }
    shm_sqcq_mbox->cq_id = cq_free_para->cq_id;
    shm_sqcq_mbox->app_type = tsdrv_get_env_type();
    shm_sqcq_mbox->fid = tsdrv_get_fid_by_ctx(ctx);
    shm_sqcq_mbox->pid = ctx->tgid;
}

int shm_sqcq_mbox_send(u32 devid, u32 tsid, struct shm_sqcq_mbox_msg *shm_sqcq_mbox)
{
    struct tsdrv_ts_resource *pm_ts_res = NULL;
    int mbox_ret = 0;
    int err;

    pm_ts_res = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, tsid);
    if (pm_ts_res == NULL) {
        return -EINVAL;
    }
    err = devdrv_mailbox_kernel_sync_no_feedback(&pm_ts_res->mailbox, (u8 *)shm_sqcq_mbox,
        sizeof(struct shm_sqcq_mbox_msg), &mbox_ret);
    if ((err != 0) || (mbox_ret != 0)) {
        TSDRV_PRINT_ERR("shm sqcq mbox send fail, err=%d mbox_ret=%d devid=%u tsid=%u\n", err, mbox_ret, devid, tsid);
        return -ENODEV;
    }
    return 0;
}

STATIC_INLINE void shm_sq_alloc_arg_pack(struct phy_sq_alloc_para *sq_alloc_para,
    struct shm_sqcq_alloc_para *alloc_arg)
{
    sq_alloc_para->type = SHM_SQCQ_TYPE;
    sq_alloc_para->sq_size = alloc_arg->sq_size;
    sq_alloc_para->sq_depth = alloc_arg->sq_depth;
}

STATIC_INLINE void shm_sq_free_arg_pack(struct phy_sq_free_para *sq_free_para,
    struct shm_sqcq_free_para *free_arg)
{
    sq_free_para->sq_id = free_arg->sq_id;
}

STATIC_INLINE void shm_cq_alloc_arg_pack(struct phy_cq_alloc_para *cq_alloc_para,
    struct shm_sqcq_alloc_para *shm_sqcq_alloc_para)
{
    cq_alloc_para->type = SHM_SQCQ_TYPE;
    cq_alloc_para->cq_size = shm_sqcq_alloc_para->cq_size;
    cq_alloc_para->cq_depth = shm_sqcq_alloc_para->cq_depth;
}

STATIC_INLINE void shm_cq_free_arg_pack(struct phy_cq_free_para *cq_alloc_para,
    struct shm_sqcq_free_para *shm_sqcq_free_para)
{
    cq_alloc_para->cq_id = shm_sqcq_free_para->cq_id;
}

STATIC int shm_sqcq_alloc_param_chk(struct shm_sqcq_alloc_para *arg)
{
    if ((arg->sq_size <  TSDRV_MIN_SHM_SQE_SIZE) || (arg->sq_size >  TSDRV_MAX_SHM_SQE_SIZE) ||
        (arg->cq_size != sizeof(struct tag_ts_driver_msg))) {
        TSDRV_PRINT_ERR("invalid SqCq size, sqeSize=%u sqeSizeRange:[%d %d] cqeSize=%u cqeSizeRange:[%lu %lu]\n",
            arg->sq_size, TSDRV_MIN_SHM_SQE_SIZE, TSDRV_MAX_SHM_SQE_SIZE, arg->cq_size,
            sizeof(struct tag_ts_driver_msg), sizeof(struct tag_ts_driver_msg));
        return -ENODEV;
    }
    if ((arg->sq_depth < TSDRV_MIN_SHM_SQE_DEPTH) || (arg->sq_depth > TSDRV_MAX_SHM_SQE_DEPTH)) {
        TSDRV_PRINT_ERR("invalid sqeDepth=%u sqeDepthRange:[%u %u]\n", arg->sq_depth,
            TSDRV_MIN_SHM_SQE_DEPTH, TSDRV_MAX_SHM_SQE_DEPTH);
        return -EFAULT;
    }
    if ((arg->cq_depth < TSDRV_MIN_SHM_CQE_DEPTH) || (arg->cq_depth > TSDRV_MAX_SHM_CQE_DEPTH)) {
        TSDRV_PRINT_ERR("invalid cqeDepth=%u cqeDepthRange:[%u %u]\n", arg->cq_depth, TSDRV_MIN_SHM_CQE_DEPTH,
            TSDRV_MAX_SHM_CQE_DEPTH);
        return -EINVAL;
    }
#ifndef AOS_LLVM_BUILD
    if (arg->sqMapVaddr == 0) {
        TSDRV_PRINT_ERR("sqMapVaddr is NULL\n");
        return -ENOMEM;
    }
#endif
    return 0;
}
#ifdef AOS_LLVM_BUILD
static int shm_get_sq_va(struct tsdrv_ctx *ctx, struct shm_sqcq_alloc_para *arg, u32 tsid, u32 sqid)
{
    struct devdrv_sq_sub_info *sq_sub = NULL;
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    u32 fid = tsdrv_get_fid_by_ctx(ctx);
    int err;
    struct tsdrv_ts_resource *ts_res = tsdrv_ctx_to_ts_res(ctx, tsid);

    if (sqid >= DEVDRV_MAX_SQ_NUM) {
        TSDRV_PRINT_ERR("Input pararmeter is error. (sqid=%u)\n", sqid);
        return -EINVAL;
    }

    if (!tsdrv_is_sq_belong_to_proc(ctx, tsid, sqid)) {
        TSDRV_PRINT_ERR("Not self sq. (sqid=%u; tsid=%u)\n", sqid, tsid);
        return -EINVAL;
    }

    sq_sub = tsdrv_get_sq_sub_info(ts_res, sqid);
    if (sq_sub == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("devid = %u, tsid = %u, fid = %u.\n", devid, tsid, fid);
        return -ENODEV;
#endif
    }

    err = tsdrv_get_user_va(sq_sub->vaddr, arg->sq_size * arg->sq_depth, &arg->sqMapVaddr);
    if (err != 0) {
        TSDRV_PRINT_ERR("tsdrv_get_user_va fail. (devid=%u; tsid=%u; err=%d)\n", devid, tsid, err);
        sq_sub->vaddr = 0;
        return err;
    }
    return 0;
}
#endif
static int shm_sq_mmap(struct tsdrv_ctx *ctx, u32 tsid, struct shm_sqcq_alloc_para *arg, u32 sqid)
{
    struct shm_sqcq_ts_ctx *shm_ctx = (struct shm_sqcq_ts_ctx *)ctx->ts_ctx[tsid].shm_sqcq_ctx;
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    struct tsdrv_mem_map_para map_para;
    int err;
#ifndef AOS_LLVM_BUILD
    map_para.id = sqid;
    map_para.id_type = TSDRV_SQ_ID;
    map_para.sub_type = MEM_MAP_SUBTYPE_SHM;
    map_para.va = arg->sqMapVaddr;
    map_para.len = arg->sq_size * arg->sq_depth;
    err = tsdrv_remap_va(ctx, tsid, &map_para);
    if (err != 0) {
        TSDRV_PRINT_ERR("Shm sq map fail. (devid=%u; tsid=%u; sqid=%u)\n", devid, tsid, sqid);
        return err;
    }
    shm_ctx->sq_map_va = map_para.va;
    shm_ctx->sq_map_len = map_para.len;
#else
    err = shm_get_sq_va(ctx, arg, tsid, sqid);
    if (err != 0) {
        TSDRV_PRINT_ERR("Shm get user va fail. (devid=%u; tsid=%u; sqid=%u)\n", devid, tsid, sqid);
        return err;
    }
    shm_ctx->sq_map_va = arg->sqMapVaddr;
    shm_ctx->sq_map_len = arg->sq_size * arg->sq_depth;
#endif
    TSDRV_PRINT_DEBUG("Mmap shm sq success. (sqid=%u; va=0x%pK; len=0x%lx; tgid=%d)\n", sqid,
        (void *)(uintptr_t)map_para.va, map_para.len, ctx->tgid);
    return 0;
}
#ifndef AOS_LLVM_BUILD
static int shm_sq_unmap(struct tsdrv_ctx *ctx, u32 tsid)
{
    struct shm_sqcq_ts_ctx *shm_ctx = (struct shm_sqcq_ts_ctx *)ctx->ts_ctx[tsid].shm_sqcq_ctx;
    struct tsdrv_mem_unmap_para unmap_para;

    unmap_para.va = shm_ctx->sq_map_va;
    unmap_para.len = shm_ctx->sq_map_len;
    TSDRV_PRINT_DEBUG("Unmap shm sq. (sqid=%u; va=0x%pK; len=0x%lx; tgid=%d)\n", shm_ctx->sqid,
        (void *)(uintptr_t)unmap_para.va, unmap_para.len, ctx->tgid);
    return tsdrv_unmap_va(ctx, tsid, &unmap_para);
}
#else
static int shm_sq_unmap(struct tsdrv_ctx *ctx, u32 tsid)
{
    struct shm_sqcq_ts_ctx *shm_ctx = (struct shm_sqcq_ts_ctx *)ctx->ts_ctx[tsid].shm_sqcq_ctx;
    struct devdrv_sq_sub_info *sq_sub = NULL;
    struct tsdrv_ts_resource *ts_res = NULL;
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    u32 fid = tsdrv_get_fid_by_ctx(ctx);
    int err = 0;

    TSDRV_PRINT_DEBUG("enter devid = %u, tsid = %u, fid = %u.\n", devid, tsid, fid);

    ts_res = tsdrv_ctx_to_ts_res(ctx, tsid);
    sq_sub = tsdrv_get_sq_sub_info(ts_res, shm_ctx->sqid);
    if (sq_sub == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("sqId=%u is not allocated by tgid=%d, devid=%u fid=%u tsid=%u\n", shm_ctx->sqid, ctx->tgid,
            devid, fid, tsid);
        return -EINVAL;
#endif
    }
    err = mg_sp_unshare(shm_ctx->sq_map_va, shm_ctx->sq_map_len, 0);
    if (err != 0) {
        TSDRV_PRINT_ERR("Unshare sq fail. (va=0x%pK; len=0x%lx)\n",
            (void *)(uintptr_t)shm_ctx->sq_map_va, shm_ctx->sq_map_len);
        return -ENODEV;
    }
    return 0;
}
#endif
int shm_ioctl_cqsq_alloc(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    struct phy_sq_alloc_para sq_alloc_para = {0};
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    struct shm_sqcq_ts_ctx *shm_ctx = NULL;
    struct shm_sqcq_mbox_msg shm_sqcq_mbox;
    struct phy_cq_alloc_para cq_alloc_para;
    struct phy_cq_free_para cq_free_para;

    u32 tsid = arg->tsid;
    u32 fid = tsdrv_get_fid_by_ctx(ctx);
    struct tsdrv_id_info *sq_id_info = NULL;
    struct tsdrv_ts_resource *vts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
    int err;

    err = shm_sqcq_alloc_param_chk(&arg->shm_sqcq_alloc_para);
    if (err != 0) {
        TSDRV_PRINT_ERR("shm sqcq alloc param chk fail, devid=%u tsid=%u\n", devid, tsid);
        return err;
    }
    shm_ctx = (struct shm_sqcq_ts_ctx *)ctx->ts_ctx[tsid].shm_sqcq_ctx;
#ifndef TSDRV_UT
    if (shm_ctx == NULL) {
        TSDRV_PRINT_ERR("shm_sqcq_ctx is NULL, devid=%u tsid=%u\n", devid, tsid);
        return -ENOMEM;
    }
#endif
    ka_task_mutex_lock(&shm_ctx->lock);
    if (shm_ctx->shm_sqcq_num != 0) {
        ka_task_mutex_unlock(&shm_ctx->lock);
        TSDRV_PRINT_ERR("invalid shm_sqcq_num=%u, devid=%u tsid=%u\n", shm_ctx->shm_sqcq_num, devid, tsid);
        return -EFAULT;
    }
    shm_sq_alloc_arg_pack(&sq_alloc_para, &arg->shm_sqcq_alloc_para);
    if (tsdrv_is_in_pm(devid) == true) {
        ka_task_mutex_lock(&vts_res->id_res[TSDRV_SQ_ID].id_mutex_t);
        err = tsdrv_phy_sq_alloc(ctx, tsid, &sq_alloc_para);
        ka_task_mutex_unlock(&vts_res->id_res[TSDRV_SQ_ID].id_mutex_t);
        if (err != 0) {
            ka_task_mutex_unlock(&shm_ctx->lock);
            return err;
        }
    } else {
#ifndef CFG_SOC_PLATFORM_MDC_V51
        err = hvtsdrv_get_shm_phy_sq(devid, tsdrv_get_fid_by_ctx(ctx), tsid, &sq_alloc_para);
        if (err != 0) {
            ka_task_mutex_unlock(&shm_ctx->lock);
            TSDRV_PRINT_ERR("Failed to get shm physics sq. (devid=%u; fid=%u; tsid=%u)\n",
                devid, tsdrv_get_fid_by_ctx(ctx), tsid);
            return err;
        }
#endif
    }
    shm_cq_alloc_arg_pack(&cq_alloc_para, &arg->shm_sqcq_alloc_para);
    err = tsdrv_phy_cq_alloc(ctx, tsid, &cq_alloc_para);
    if (err != 0) {
#ifndef TSDRV_UT
        goto err_phy_cq_alloc;
#endif
    }
    sq_id_info = devdrv_find_one_id(&vts_res->id_res[TSDRV_SQ_ID], (int)sq_alloc_para.sq_id);
    if (sq_id_info == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Get id_info fail. (devid=%u; fid=%u; phy_sqid=%u)\n", devid, fid, sq_alloc_para.sq_id);
        goto err_find_sq_id_info;
#endif
    }
    if (!tsdrv_is_in_vm(devid)) {
        err = shm_sq_mmap(ctx, tsid, &arg->shm_sqcq_alloc_para, sq_alloc_para.sq_id);
        if (err != 0) {
#ifndef TSDRV_UT
            goto err_shm_sq_map;
#endif
        }
    }
    err = shm_sqcq_mbox_alloc_msg_pack(ctx, &shm_sqcq_mbox, &sq_alloc_para, &cq_alloc_para, &arg->shm_sqcq_alloc_para);
    if (err != 0) {
#ifndef TSDRV_UT
        goto err_alloc_mbox_msg_pack;
#endif
    }
    err = shm_sqcq_mbox_send(devid, tsid, &shm_sqcq_mbox);
    if (err != 0) {
#ifndef TSDRV_UT
        goto err_shm_sqcq_mbox_send;
#endif
    }
    shm_ctx->sqid = sq_alloc_para.sq_id;
    shm_ctx->cqid = cq_alloc_para.cq_id;
    arg->shm_sqcq_alloc_para.cq_id = cq_alloc_para.cq_id;
    shm_ctx->shm_sqcq_num++;
    arg->shm_sqcq_alloc_para.sq_id = sq_id_info->id;
    TSDRV_PRINT_DEBUG("Alloc shm: devid=%u, fid=%u, tsid=%u, tgid=%d, phy_sqid=%u, virt_sqid=%u, cq_id=%u\n",
        devid, tsdrv_get_fid_by_ctx(ctx), tsid, ctx->tgid, sq_alloc_para.sq_id,
        arg->shm_sqcq_alloc_para.sq_id, arg->shm_sqcq_alloc_para.cq_id);
    ka_task_mutex_unlock(&shm_ctx->lock);
    tsdrv_set_ctx_nothread_type(ctx);
    arg->shm_sqcq_alloc_para.rdonly = 0;
    return 0;
#ifndef TSDRV_UT
err_shm_sqcq_mbox_send:
err_alloc_mbox_msg_pack:
    if (!tsdrv_is_in_vm(devid)) {
        (void)shm_sq_unmap(ctx, tsid);
    }
err_shm_sq_map:
err_find_sq_id_info:
    cq_free_para.cq_id = cq_alloc_para.cq_id;
    (void)tsdrv_phy_cq_free(ctx, tsid, &cq_free_para);
err_phy_cq_alloc:
    tsdrv_phy_sq_alloc_restore(ctx, tsid, sq_alloc_para.sq_id);
    ka_task_mutex_unlock(&shm_ctx->lock);
    return -EINVAL;
#endif
}

#ifndef CFG_SOC_PLATFORM_MDC_V51
static int shm_phy_sq_check(u32 devid, u32 fid, u32 tsid, u32 sqid)
{
    struct tsdrv_ts_resource *ts_resource = tsdrv_get_ts_resoruce(devid, fid, tsid);
    struct phy_sq_alloc_para sq_alloc_para;
    u32 phy_sqid;
    int ret;

    if (sqid >= ts_resource->id_res[TSDRV_SQ_ID].id_capacity) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("The shm sqid is invalid. (sqid=%u; id_capacity=%u)\n", sqid,
            ts_resource->id_res[TSDRV_SQ_ID].id_capacity);
        return -EINVAL;
#endif
    }

    ret = hvtsdrv_get_shm_phy_sq(devid, fid, tsid, &sq_alloc_para);
    if (ret != 0) {
        TSDRV_PRINT_ERR("Failed to get shm physical sqid. (devid=%u; fid=%u; tsid=%u)\n", devid, fid, tsid);
        return ret;
    }
    phy_sqid = tsdrv_vrit_to_physic_id(&ts_resource->id_res[TSDRV_SQ_ID], sqid);
    if (phy_sqid != sq_alloc_para.sq_id) {
        TSDRV_PRINT_ERR("Invalid sqid. (devid=%u; fid=%u; tsid=%u; sqid=%u; expected_sqid=%u)\n",
            devid, fid, tsid, phy_sqid, sq_alloc_para.sq_id);
        return -EINVAL;
    }
    ret = tsdrv_sq_type_check(devid, fid, tsid, phy_sqid, SHM_SQCQ_TYPE);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Failed to check shm sq type. (sqid=%u; ret=%d)\n", phy_sqid, ret);
        return ret;
#endif
    }

    return 0;
}
#endif

int shm_ioctl_cqsq_free(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    struct shm_sqcq_mbox_msg shm_sqcq_mbox = {0};
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    struct shm_sqcq_ts_ctx *shm_ctx = NULL;
    u32 fid = tsdrv_get_fid_by_ctx(ctx);
    struct phy_sq_free_para sq_free_para;
    struct phy_cq_free_para cq_free_para;
    u32 tsid = arg->tsid;
    int err;

    shm_ctx = (struct shm_sqcq_ts_ctx *)ctx->ts_ctx[tsid].shm_sqcq_ctx;
    if (shm_ctx == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("shm_sqcq_ctx is NULL, devid=%u tsid=%u\n", devid, tsid);
        return -ENOMEM;
#endif
    }
    shm_sq_free_arg_pack(&sq_free_para, &arg->shm_sqcq_free_para);
    shm_cq_free_arg_pack(&cq_free_para, &arg->shm_sqcq_free_para);

    ka_task_mutex_lock(&shm_ctx->lock);
    if (tsdrv_is_in_pm(devid) == true) {
        err = tsdrv_phy_sqid_exist_chk(ctx, tsid, sq_free_para.sq_id, SHM_SQCQ_TYPE);
        if ((err != 0) || (sq_free_para.sq_id != shm_ctx->sqid)) {
            ka_task_mutex_unlock(&shm_ctx->lock);
            TSDRV_PRINT_ERR("sqid=%u doesn't exist, shm_sq=%u tgid=%d, devid=%u fid=%u tsid=%u\n",
                sq_free_para.sq_id, shm_ctx->sqid, ctx->tgid, devid, fid, tsid);
            return -ENODEV;
        }
    } else {
#ifndef CFG_SOC_PLATFORM_MDC_V51
        err = shm_phy_sq_check(devid, fid, tsid, sq_free_para.sq_id);
        if (err != 0) {
            ka_task_mutex_unlock(&shm_ctx->lock);
            TSDRV_PRINT_ERR("Failed to check shm sq. (devid=%u; fid=%u; tsid=%u; sqid=%u)\n", devid, fid, tsid,
                sq_free_para.sq_id);
            return err;
        }
#endif
    }
    err = tsdrv_phy_cqid_exist_chk(ctx, tsid, cq_free_para.cq_id, SHM_SQCQ_TYPE);
    if (err != 0) {
        ka_task_mutex_unlock(&shm_ctx->lock);
        TSDRV_PRINT_ERR("cqid=%u doesn't exist for tgid=%d, devid=%u fid=%u tsid=%u\n", cq_free_para.cq_id,
            ctx->tgid, devid, fid, tsid);
        return -EINVAL;
    }
    shm_sqcq_mbox_free_msg_pack(devid, ctx, &shm_sqcq_mbox, &sq_free_para, &cq_free_para);

    if (!tsdrv_is_in_vm(devid)) {
        err = shm_sq_unmap(ctx, tsid);
        if (err != 0) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("Shm sq unmap fail. (tgid=%d; sqid=%u)\n", ctx->tgid, shm_ctx->sqid);
            ka_task_mutex_unlock(&shm_ctx->lock);
            return -ENODEV;
#endif
        }
    }
    err = shm_sqcq_mbox_send(devid, tsid, &shm_sqcq_mbox);
    if (err != 0) {
#ifndef TSDRV_UT
        ka_task_mutex_unlock(&shm_ctx->lock);
        return -EINVAL;
#endif
    }

    if (tsdrv_is_in_pm(devid) == true) {
        err = tsdrv_phy_sq_free(ctx, tsid, &sq_free_para);
        if (err != 0) {
#ifndef TSDRV_UT
            ka_task_mutex_unlock(&shm_ctx->lock);
            return -EINVAL;
#endif
        }
    }
    err = tsdrv_phy_cq_free(ctx, tsid, &cq_free_para);
    if (err != 0) {
#ifndef TSDRV_UT
        ka_task_mutex_unlock(&shm_ctx->lock);
        return -EINVAL;
#endif
    }
    shm_ctx->sqid = DEVDRV_MAX_SQ_NUM;
    shm_ctx->cqid = DEVDRV_MAX_CQ_NUM;
    shm_ctx->shm_sqcq_num--;
    ka_task_mutex_unlock(&shm_ctx->lock);

    TSDRV_PRINT_DEBUG("Free shm. (devid=%u; fid=%u; tsid=%u; sq_id=%u; cq_id=%u; tgid=%d)\n", devid, fid,
        tsid, sq_free_para.sq_id, cq_free_para.cq_id, ctx->tgid);
    return 0;
}

STATIC int shm_sqcq_init(u32 devid, u32 fid, u32 tsid)
{
    TSDRV_PRINT_INFO("shm_sqcq_init, devid=%u, fid=%u, tsid=%u\n", devid, fid, tsid);
    return 0;
}

STATIC void shm_sqcq_exit(u32 devid, u32 fid, u32 tsid)
{
    TSDRV_PRINT_INFO("shm_sqcq_exit, devid=%u, fid=%u, tsid=%u\n", devid, fid, tsid);
}

int shm_sqcq_dev_init(u32 devid, u32 fid, u32 tsnum)
{
    u32 tsid;
    int err;
    u32 i;

    for (tsid = 0; tsid < tsnum; tsid++) {
        err = shm_sqcq_init(devid, fid, tsid);
        if (err != 0) {
            goto err_shm_sqcq_init;
        }
    }
    return 0;
err_shm_sqcq_init:
    for (i = 0; i < tsid; i++) {
        shm_sqcq_exit(devid, fid, i);
    }
    return -EINVAL;
}

void shm_sqcq_dev_exit(u32 devid, u32 fid, u32 tsnum)
{
    u32 tsid;
    for (tsid = 0; tsid < tsnum; tsid++) {
        shm_sqcq_exit(devid, fid, tsid);
    }
}

