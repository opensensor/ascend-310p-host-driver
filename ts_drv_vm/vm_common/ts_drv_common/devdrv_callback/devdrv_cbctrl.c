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
#include <securec.h>
#include <linux/slab.h>

#include "ka_list_pub.h"
#include "devdrv_cbctrl.h"
#include "tsdrv_cb_pm.h"

#ifndef CFG_SOC_PLATFORM_MDC_V51
#include "hvtsdrv_cb.h"
#endif

STATIC s32 callback_send_task(struct callback_send_sq_para *sq_para, struct callback_phy_sqcq *phy_sqcq)
{
    enum tsdrv_env_type env_type;
    u8* addr = NULL;
    int ret;

    if (phy_sqcq->cb_sq.vaddr == NULL) {
        return -ENOMEM;
    }

    if (tsdrv_is_in_container(phy_sqcq->devid)) {
        addr = (u8*)phy_sqcq->cb_sq.vvaddr + ((unsigned long)phy_sqcq->cb_sq.tail * phy_sqcq->cb_sq.size);
    } else {
        addr = (u8*)phy_sqcq->cb_sq.vaddr + ((unsigned long)phy_sqcq->cb_sq.tail * phy_sqcq->cb_sq.size);
    }

    env_type = tsdrv_get_env_type();
    if (env_type == TSDRV_ENV_OFFLINE) {
        TSDRV_PRINT_DEBUG("sqid=%u sq_tail=%u\n", phy_sqcq->cb_sq.index, phy_sqcq->cb_sq.tail);
        ret = memcpy_s(addr, phy_sqcq->cb_sq.size, sq_para->sq_task, sizeof(sq_para->sq_task));
        if (ret != 0) {
            return ret;
        }
    } else {
        TSDRV_PRINT_DEBUG("sqid=%u sq_tail=%u\n", phy_sqcq->cb_sq.index, phy_sqcq->cb_sq.tail);
        ka_mm_memcpy_toio(addr, sq_para->sq_task, sizeof(sq_para->sq_task));
    }
    phy_sqcq->cb_sq.in_cnt += 1;
    phy_sqcq->cb_sq.tail = (phy_sqcq->cb_sq.tail + 1) % phy_sqcq->cb_sq.depth;

#ifndef CFG_SOC_PLATFORM_MDC_V51
    if (tsdrv_is_in_container(phy_sqcq->devid)) {
        return hvtsdrv_cb_inform_tsagent_vsq_tail(phy_sqcq->devid, phy_sqcq->fid, phy_sqcq->cb_sq.virtid,
            phy_sqcq->cb_sq.tail);
    }
#endif

    ret = callback_set_doorbell(phy_sqcq, phy_sqcq->cb_sq.index, phy_sqcq->cb_sq.tail);
    return ret;
}

static void callback_set_cq_bitmap(struct callback_wait_cq_para *cq, u32 cq_id)
{
    u32 offset;
    u32 idx;

    idx = cq_id >> 6;    /* right 6 bits mean divided by 64 */
    offset = cq_id & (BITS_PER_LONG_LONG_INT - 1);
    cq->bitmap_buf[idx] |= (0x01ULL << offset);
    return;
}

STATIC s32 callback_copy_report_to_user(void __user * arg, struct callback_wait_cq_para *cq_para,
    struct callback_logic_cq *logic_cq, struct callback_logic_cq_sub *sub_cq)
{
    u32 full_flag = 0;
    u32 report_cnt;
    u32 tmp_head;
    u32 offset;
    s32 ret;

    offset = sub_cq->cqid * cq_para->report_cnt * sub_cq->size;
    ka_task_spin_lock_bh(&logic_cq->lock);
    if (sub_cq->useflag == CALLBACK_CQ_IS_FREE) {
        ka_task_spin_unlock_bh(&logic_cq->lock);
        TSDRV_PRINT_ERR("[cq_id=%u]:useflag is invalid.\n", sub_cq->cqid);
        return -EINVAL;
    }
    sub_cq->useflag = CALLBACK_CQ_COPY_REPORT;
    if (sub_cq->tail > sub_cq->head) {
        report_cnt = sub_cq->tail - sub_cq->head;
        cq_para->report_cnt = (cq_para->report_cnt > report_cnt) ? report_cnt : cq_para->report_cnt;
        tmp_head = sub_cq->head + cq_para->report_cnt;
    } else {
        report_cnt = sub_cq->depth - sub_cq->head;
        cq_para->report_cnt = (cq_para->report_cnt > report_cnt) ? report_cnt : cq_para->report_cnt;
        tmp_head = (sub_cq->head + cq_para->report_cnt) % sub_cq->depth;
    }
    ka_task_spin_unlock_bh(&logic_cq->lock);

    ret = copy_to_user_safe((void __user *)(uintptr_t)(cq_para->report_buf + offset),
        sub_cq->vaddr + ((unsigned long)sub_cq->head * sub_cq->size),
        (unsigned long)cq_para->report_cnt * sub_cq->size);
    ka_task_spin_lock_bh(&logic_cq->lock);
    sub_cq->useflag = CALLBACK_CQ_IN_USE;
    ka_task_spin_unlock_bh(&logic_cq->lock);
    if (ret != 0) {
        TSDRV_PRINT_ERR("[cq_id=%u]:copy_to_user_safe fail.\n", sub_cq->cqid);
        return ret;
    }

    callback_set_cq_bitmap(cq_para, sub_cq->cqid);
    ret = copy_to_user_safe(arg, cq_para, sizeof(struct callback_wait_cq_para));
    if (ret != 0) {
        TSDRV_PRINT_ERR("[cq_id=%u]:copy_to_user_safe fail.\n", sub_cq->cqid);
        return ret;
    }
    sub_cq->out_cnt += cq_para->report_cnt;
    ka_task_spin_lock_bh(&logic_cq->lock);
    if (callback_queue_full(sub_cq->head, sub_cq->tail, sub_cq->depth)) {
        full_flag = LOGIC_CQ_QUEUE_FULL;
    }
    sub_cq->head = tmp_head;
    ka_task_spin_unlock_bh(&logic_cq->lock);

    if (full_flag == LOGIC_CQ_QUEUE_FULL) {
        task_dispatch_handler(logic_cq->devid, logic_cq->fid, logic_cq->tsid);
    }
    return 0;
}

STATIC s32 callback_get_cq_data(u32 devid, struct tsdrv_ctx *ctx,
    struct callback_wait_cq_para *cq_para, void __user * arg)
{
    struct callback_logic_cq_sub *sub_cq = NULL;
    struct callback_logic_cq *logic_cq = NULL;
    u32 fid = tsdrv_get_fid_by_ctx(ctx);
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;
    u32 tsnum;

    tsnum = tsdrv_get_dev_tsnum(devid);
    if (cq_para->tsid >= tsnum) {
        TSDRV_PRINT_ERR("parameter cq_para is invalid, cq_para->tsid = %u, tsnum = %u\n", cq_para->tsid, tsnum);
        return -EINVAL;
    }
    logic_cq = callback_logic_cq_get(devid, fid, cq_para->tsid);
    if (logic_cq == NULL) {
        TSDRV_PRINT_ERR("callback info is null.(devid:%u fid:%u tsid:%u)\n", devid, fid, cq_para->tsid);
        return -EINVAL;
    }
    ka_task_spin_lock_bh(&logic_cq->lock);
    if (ka_list_empty_careful(&logic_cq->cq_alloc_list) != 0) {
        ka_task_spin_unlock_bh(&logic_cq->lock);
        TSDRV_PRINT_INFO("alloc list empty, devid(%u) tsid(%u)\n", devid, cq_para->tsid);
        return -ENOMSG;
    }

    /* find the logic cq with legal data */
    ka_list_for_each_safe(pos, n, &logic_cq->cq_alloc_list) {
        sub_cq = ka_list_entry(pos, struct callback_logic_cq_sub, cq_list);
        if ((sub_cq->vpid != ctx->pid) || (sub_cq->grpid != cq_para->gid) || (sub_cq->head == sub_cq->tail)) {
            continue;
        }

        if (cq_para->report_len != (cq_para->report_cnt * DEVDRV_CB_CQ_MAX_NUM * sub_cq->size)) {
            ka_task_spin_unlock_bh(&logic_cq->lock);
            TSDRV_PRINT_ERR("invalid report buf len(%u %u)\n", cq_para->report_len, cq_para->report_cnt);
            return -EINVAL;
        }
        /* Add the processed node to the end of the cq queue */
        ka_list_del(&sub_cq->cq_list);
        ka_list_add_tail(&sub_cq->cq_list, &logic_cq->cq_alloc_list);
        ka_task_spin_unlock_bh(&logic_cq->lock);
        return callback_copy_report_to_user(arg, cq_para, logic_cq, sub_cq);
    }
    ctx->ts_ctx[cq_para->tsid].cbcq_wait_flag[cq_para->gid] = 0;
    ka_task_spin_unlock_bh(&logic_cq->lock);
    return -EAGAIN;
}

STATIC s32 callback_init_alloced_cq(struct callback_logic_cq_sub *sub_cq, struct tsdrv_ctx *ctx,
    struct callback_alloc_sqcq_para *cq_para)
{
    sub_cq->vaddr = callback_kvmalloc((unsigned long)cq_para->cq_size * cq_para->cq_depth);
    if (sub_cq->vaddr == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Callback logic cq malloc fail.\n");
        return -ENOMEM;
#endif
    }

    sub_cq->head = 0;
    sub_cq->tail = 0;
    sub_cq->vpid = ctx->pid;
    sub_cq->grpid = cq_para->grp_id;
    sub_cq->size = cq_para->cq_size;
    sub_cq->depth = cq_para->cq_depth;
    sub_cq->ctx = ctx;
    return 0;
}

STATIC s32 callback_free_cq(struct tsdrv_ts_resource *ts_res, struct tsdrv_ctx *ctx,
    struct callback_free_sqcq_para *cq_para)
{
    struct tsdrv_device *tsdrv_dev = tsdrv_res_to_dev(ts_res);
    struct callback_logic_cq_sub *sub_cq = NULL;
    struct callback_phy_sqcq *phy_sqcq = NULL;
    struct callback_logic_cq *logic_cq = NULL;
    u32 fid = tsdrv_get_fid_by_ctx(ctx);

    logic_cq = callback_logic_cq_get(tsdrv_dev->devid, fid, ts_res->tsid);
    phy_sqcq = callback_physic_sqcq_get(tsdrv_dev->devid, fid, ts_res->tsid);
    if ((logic_cq == NULL) || (phy_sqcq == NULL)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("callback info is null.(devid:%u fid:%u tsid:%u)\n", tsdrv_dev->devid, fid, ts_res->tsid);
        return -EINVAL;
#endif
    }

    ka_task_spin_lock_bh(&logic_cq->lock);
    sub_cq = &logic_cq->sub_cq[cq_para->cq_id];
    if ((sub_cq->useflag != CALLBACK_CQ_IN_USE) || (sub_cq->vpid != ctx->pid)) {
#ifndef TSDRV_UT
        ka_task_spin_unlock_bh(&logic_cq->lock);
        TSDRV_PRINT_ERR("free cq fail, cq(%u) useflag(%u).(devid:%u fid:%u tsid:%u)\n",
            cq_para->cq_id, (u32)sub_cq->useflag, tsdrv_dev->devid, fid, ts_res->tsid);
        return -EINVAL;
#endif
    }
    sub_cq->useflag = CALLBACK_CQ_IS_FREE;
    ka_task_spin_unlock_bh(&logic_cq->lock);

    TSDRV_PRINT_DEBUG("logic-cq(%u)  vpid:%u grpid:%u\n", sub_cq->cqid, sub_cq->vpid, sub_cq->grpid);
    TSDRV_PRINT_DEBUG("logic-cq(%u)  size:%u   depth:%u\n", sub_cq->cqid, sub_cq->size, sub_cq->depth);
    TSDRV_PRINT_DEBUG("logic-cq(%u)  head:%u   tail:%u\n", sub_cq->cqid, sub_cq->head, sub_cq->tail);
    TSDRV_PRINT_DEBUG("logic-cq(%u)  inCnt:%u  outCnt:%u\n", sub_cq->cqid, sub_cq->in_cnt, sub_cq->out_cnt);
    callback_free_one_cq(logic_cq, phy_sqcq, sub_cq);

    return 0;
}

STATIC s32 callback_alloc_logic_cq(struct tsdrv_ts_resource *ts_res, struct tsdrv_ctx *ctx,
    struct callback_alloc_sqcq_para *cq_para)
{
    struct tsdrv_device *tsdrv_dev = tsdrv_res_to_dev(ts_res);
    struct callback_logic_cq_sub *sub_cq = NULL;
    struct callback_phy_sqcq *phy_sqcq = NULL;
    struct callback_logic_cq *logic_cq = NULL;
    u32 fid = tsdrv_get_fid_by_ctx(ctx);
    s32 ret;
    u32 tsid;

    tsid = ts_res->tsid;
    logic_cq = callback_logic_cq_get(tsdrv_dev->devid, fid, tsid);
    phy_sqcq = callback_physic_sqcq_get(tsdrv_dev->devid, fid, tsid);
    if ((phy_sqcq == NULL) || (phy_sqcq->init == CALLBACK_UNINIT) ||
        (logic_cq == NULL) || (logic_cq->init == CALLBACK_UNINIT)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("callback info is null.(devid:%u fid:%u tsid:%u init:%u)\n",
            tsdrv_dev->devid, fid, tsid, logic_cq == NULL ? 0 : (logic_cq->init));
        return -EINVAL;
#endif
    }

    ka_task_spin_lock_bh(&logic_cq->lock);
    if (ka_list_empty_careful(&logic_cq->cq_free_list) == 0) {
        sub_cq = ka_list_first_entry(&logic_cq->cq_free_list, struct callback_logic_cq_sub, cq_list);
        ka_list_del(&sub_cq->cq_list);
    } else {
        ka_task_spin_unlock_bh(&logic_cq->lock);
        TSDRV_PRINT_ERR("no free logic cq.(devid:%u fid:%u tsid:%u)\n", logic_cq->devid, logic_cq->fid, tsid);
        return -EBUSY;
    }
    ka_task_spin_unlock_bh(&logic_cq->lock);

    ret = callback_init_alloced_cq(sub_cq, ctx, cq_para);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("init alloced cq fail, ret(%d).(devid:%u fid:%u tsid:%u)\n",
            ret, logic_cq->devid, logic_cq->fid, tsid);
        goto err_ret;
#endif
    }

    cq_para->cq_id = sub_cq->cqid;
    ret = callback_alloc_logic_cq_mbox(phy_sqcq, sub_cq);
    if (ret != 0) {
#ifndef TSDRV_UT
        callback_free_logic_cq(sub_cq);
        TSDRV_PRINT_ERR("logic cq send mbox fail, ret(%d).(devid:%u fid:%u tsid:%u)\n",
            ret, logic_cq->devid, logic_cq->fid, tsid);
        goto err_ret;
#endif
    }

    ka_task_spin_lock_bh(&logic_cq->lock);
    sub_cq->useflag = CALLBACK_CQ_IN_USE;
    ka_list_add(&sub_cq->cq_list, &logic_cq->cq_alloc_list);
    ka_task_spin_unlock_bh(&logic_cq->lock);
    return 0;
#ifndef TSDRV_UT
err_ret:
    ka_task_spin_lock_bh(&logic_cq->lock);
    ka_list_add(&sub_cq->cq_list, &logic_cq->cq_free_list);
    ka_task_spin_unlock_bh(&logic_cq->lock);
    return ret;
#endif
}

STATIC s32 callback_wait_cq_para_check(struct callback_wait_cq_para *cq_para)
{
    if (cq_para->tsid >= DEVDRV_MAX_TS_NUM) {
        TSDRV_PRINT_ERR("invalid tsid(%u)\n", cq_para->tsid);
        return -EINVAL;
    }

    if (cq_para->gid >= DEVDRV_CBCQ_MAX_GID) {
        TSDRV_PRINT_ERR("invalid gid(%u)\n", cq_para->gid);
        return -EINVAL;
    }

    if ((cq_para->timeout <= 0) && (cq_para->timeout != -1)) {
        devdrv_drv_err("invalid timeout(%d)\n", cq_para->timeout);
        return -EINVAL;
    }

    if (cq_para->bitmap_size != (DEVDRV_CB_CQ_MAX_NUM / BITS_PER_LONG_LONG_INT)) {
        TSDRV_PRINT_ERR("invalid bitmap_size(%u)\n", cq_para->bitmap_size);
        return -EINVAL;
    }

    if ((cq_para->report_cnt > MAX_WAIT_CQ_SIZE) || (cq_para->report_cnt == 0)) {
        TSDRV_PRINT_ERR("invalid report_num(%u)\n", cq_para->report_cnt);
        return -EINVAL;
    }

    if (cq_para->report_buf == NULL) {
        TSDRV_PRINT_ERR("report_buf is null\n");
        return -EINVAL;
    }

    return 0;
}

STATIC s32 callback_wait_event(struct tsdrv_ts_ctx *tsctx, u32 gid, s32 timeout)
{
#ifndef TSDRV_UT
    unsigned long wait_time;
    long ret;

    /* wait event without timeout */
    if (timeout == -1) {
        ret = (long)ka_task_wait_event_interruptible(tsctx->cbcq_wait[gid], (tsctx->cbcq_wait_flag[gid] == 1));
        if (ret == -ERESTARTSYS) {
            TSDRV_PRINT_INFO("wait event interrupted\n");
        }
        return ret;
    }

    /* wait event with timeout */
    wait_time = ka_system_msecs_to_jiffies((u32)timeout);
    ret = ka_task_wait_event_interruptible_timeout(tsctx->cbcq_wait[gid], (tsctx->cbcq_wait_flag[gid] == 1), wait_time);
    if (ret == 0) {
        /* wait event timeout */
        return -ETIMEDOUT;
    } else if (ret > 0) {
        /* wait event is awakened */
        return 0;
    } else {
        if (ret == -ERESTARTSYS) {
            TSDRV_PRINT_INFO("wait event interrupted\n");
        }
        return ret;
    }
#endif
}

bool devdrv_cbsqcq_need_recycle(struct tsdrv_ts_ctx *ts_ctx)
{
    return false;
}

void devdrv_cbcqsq_to_recyclelist(u32 devid, u32 fid, u32 tsnum, struct tsdrv_ctx *ctx)
{
    u32 tsid;

    for (tsid = 0; tsid < tsnum; tsid++) {
        callback_recycle_logic_cq(devid, fid, tsid, ctx);
    }
}

int devdrv_recycle_cbcqsq_id(struct tsdrv_ctx *ctx)
{
    return 0;
}

s32 devdrv_ioctl_cbsqcq_alloc(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    struct callback_alloc_sqcq_para *cq_para = NULL;
    struct tsdrv_ts_resource *ts_res = NULL;
    u32 tsid;

    cq_para = &arg->cq_alloc_para;
    tsid = arg->tsid;
    ts_res = tsdrv_ctx_to_ts_res(ctx, tsid);
    if ((cq_para->grp_id >= DEVDRV_CBCQ_MAX_GID)
        || (cq_para->cq_size > DEVDRV_MAX_CBCQ_SIZE) || (cq_para->cq_size < DEVDRV_MIN_CBCQ_SIZE)
        || (cq_para->cq_depth > DEVDRV_MAX_CBCQ_DEPTH) || (cq_para->cq_depth < MAX_WAIT_CQ_SIZE)) {
        TSDRV_PRINT_ERR("cbcq parameter invalid.(grpid:%u size:%u depth:%u)\n",
            cq_para->grp_id, cq_para->cq_size, cq_para->cq_depth);
        return -EINVAL;
    }

    return callback_alloc_logic_cq(ts_res, ctx, cq_para);
}

s32 devdrv_ioctl_cbsqcq_free(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    struct callback_free_sqcq_para *cq_para = NULL;
    struct tsdrv_ts_resource *ts_res = NULL;
    u32 tsid;

    cq_para = &arg->cq_free_para;
    tsid = arg->tsid;
    ts_res = tsdrv_ctx_to_ts_res(ctx, tsid);

    if (cq_para->cq_id >= DEVDRV_CB_CQ_MAX_NUM) {
        TSDRV_PRINT_ERR("cbcq parameter invalid.(%u)\n", cq_para->cq_id);
        return -EINVAL;
    }

    return callback_free_cq(ts_res, ctx, cq_para);
}

s32 devdrv_ioctl_cbcq_wait(struct tsdrv_ctx *ctx, void __user * arg)
{
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    struct callback_wait_cq_para cq_para = {0};
    u32 tsid;
    u32 tsnum;
    s32 ret;

    ret = copy_from_user_safe(&cq_para, arg, sizeof(cq_para));
    if (ret != 0) {
        TSDRV_PRINT_ERR("cbcq wait copy from suer fail.\n");
        return ret;
    }

    ret = callback_wait_cq_para_check(&cq_para);
    if (ret != 0) {
        TSDRV_PRINT_ERR("cbcq wait para invalid.\n");
        return ret;
    }
    tsid = cq_para.tsid;

    tsnum = tsdrv_get_dev_tsnum(devid);
    if (tsid >= tsnum) {
        TSDRV_PRINT_ERR("parameter cq_para is invalid, cq_para->tsid = %u, tsnum = %u\n", tsid, tsnum);
        return -EINVAL;
    }

    ret = callback_get_cq_data(devid, ctx, &cq_para, arg);
    if (ret != 0) {
        if (ret != -EAGAIN) {
            TSDRV_PRINT_ERR("get cbcq data err before wait, ret(%d).(devid:%u tsid:%u gid:%u)\n",
                ret, devid, tsid, cq_para.gid);
            return ret;
        }

        ret = callback_wait_event(&ctx->ts_ctx[tsid], cq_para.gid, cq_para.timeout);
        if (ret != 0) {
            TSDRV_PRINT_DEBUG("wait event fail, ret(%d).(devid:%u tsid:%u gid:%u)\n", ret, devid, tsid, cq_para.gid);
            return ret;
        }

        ret = callback_get_cq_data(devid, ctx, &cq_para, arg);
        if (ret != 0) {
#ifndef TSDRV_UT
            TSDRV_PRINT_WARN("get cbcq data after wait, ret(%d).(devid:%u tsid:%u gid:%u)\n",
                ret, devid, tsid, cq_para.gid);
#endif
            return -ETIMEDOUT;
        }
    }

    return ret;
}

s32 devdrv_ioctl_cbsq_send(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
#ifndef TSDRV_UT
    struct callback_send_sq_para *sq_para = &arg->cb_sq_para;
    struct callback_phy_sqcq_info *phy_sq = NULL;
    struct callback_phy_sqcq *phy_sqcq = NULL;
    struct callback_info *cb_info = NULL;
    u32 fid = tsdrv_get_fid_by_ctx(ctx);
    u32 tsid = arg->tsid;
    u32 devid;
    s32 ret;

    devid = tsdrv_get_devid_by_ctx(ctx);
    if (devid >= TSDRV_MAX_DAVINCI_NUM) {
        TSDRV_PRINT_ERR("invalid devid(%u)\n", devid);
        return -ENODEV;
    }

    cb_info = callback_cb_info_get(devid, fid, tsid);
    phy_sqcq = callback_physic_sqcq_get(devid, fid, tsid);
    if ((cb_info == NULL) || (phy_sqcq == NULL) || (phy_sqcq->init == CALLBACK_UNINIT)) {
        TSDRV_PRINT_ERR("callback info is null.(devid:%u fid:%u tsid:%u)\n", devid, fid, tsid);
        return -EINVAL;
    }

    phy_sq = &phy_sqcq->cb_sq;
    callback_func_lock(cb_info);
    callback_irq_lock(phy_sq);
    if (callback_queue_full(phy_sq->head, phy_sq->tail, phy_sq->depth)) {
        callback_irq_unlock(phy_sq);
        callback_func_unlock(cb_info);
        TSDRV_PRINT_ERR("cb physical sq queue is full.(devid:%u fid:%u tsid:%u)\n", devid, fid, tsid);
        return -EBUSY;
    }
    callback_irq_unlock(phy_sq);

    /* In the VM scenario, the doorbell is sent using the VPC, which may cause sleep */
    ret = callback_send_task(sq_para, phy_sqcq);
    if (ret != 0) {
        callback_func_unlock(cb_info);
        TSDRV_PRINT_ERR("cb send task fail.(devid:%u fid:%u tsid:%u), ret(%d)\n", devid, fid, tsid, ret);
        return ret;
    }
    callback_func_unlock(cb_info);
#endif /* TSDRV_UT */
    return 0;
}

int devdrv_cbsqcq_setup(u32 devid)
{
    return 0;
}

void devdrv_cbsqcq_cleanup(u32 devid)
{
}

