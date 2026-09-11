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
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mm.h>

#include "devdrv_sync_cbctrl.h"
#include "devdrv_cbsq.h"
#include "devdrv_cbcq.h"
#include "devdrv_manager_common.h"
#include "tsdrv_log.h"
#include "tsdrv_device.h"
#include "tsdrv_delay_recycle.h"

STATIC int devdrv_cbsqcq_setup(u32 devid);
STATIC void devdrv_cbsqcq_cleanup(u32 devid);

enum devdrv_cbsqcq_alloc_type {
    CBSQCQ_MBOX_ALLOC,
    CBSQCQ_MBOX_FREE
};

struct devdrv_cbsqcq_mbox_free {
    u32 sq_index;
    u32 cq_index;
};

struct devdrv_cbsqcq_mbox_alloc {
    u64 sq_addr;
    u64 cq_addr;
    u32 sq_index;
    u32 cq_index;
    u16 sqe_size;
    u16 cqe_size;
    u16 sq_depth;
    u16 cq_depth;
    u8 plat_type;
    u8 reserved[3]; /* reserved */
    u32 cq_irq;
};

struct devdrv_cbcqsq_mbox {
    u16 valid;    /* validity judgement, 0x5a5a is valid */
    u16 cmd_type; /* command type */
    u32 result;   /* TS's process result succ or fail: no error: 0, error: not 0 */
    union {
        struct devdrv_cbsqcq_mbox_alloc alloc;
        struct devdrv_cbsqcq_mbox_free free;
    };
};

STATIC int devdrv_cbsqcq_mbox_send(u32 devid, u32 tsid, enum devdrv_cbsqcq_alloc_type alloc_type,
    struct devdrv_cbsqcq_para *cbsqcq_alloc)
{
    struct devdrv_cbcqsq_mbox *cbcqsq = NULL;
    struct tsdrv_mbox_data data;
    int ret;

    cbcqsq = tsdrv_kzalloc(sizeof(struct devdrv_cbcqsq_mbox), KA_GFP_KERNEL);
    DRV_CHECK_EXP_ACT(cbcqsq == NULL, return -ENOMEM, "cbsqcq kmalloc failed.\n");

    /* add message header */
    cbcqsq->valid = DEVDRV_MAILBOX_MESSAGE_VALID;
    switch (alloc_type) {
        case CBSQCQ_MBOX_ALLOC:
            cbcqsq->cmd_type = DEVDRV_MAILBOX_CREATE_CBCQSQ;
            cbcqsq->alloc.sq_addr = cbsqcq_alloc->alloc_para.sq_alloc_para.paddr;
            cbcqsq->alloc.cq_addr = cbsqcq_alloc->alloc_para.cq_alloc_para.paddr;
            cbcqsq->alloc.sq_index = cbsqcq_alloc->alloc_para.sq_alloc_para.sqid;
            cbcqsq->alloc.cq_index = cbsqcq_alloc->alloc_para.cq_alloc_para.cqid;
            cbcqsq->alloc.sqe_size = SYNC_CB_MAX_SQE_SIZE;
            cbcqsq->alloc.cqe_size = cbsqcq_alloc->alloc_para.cq_alloc_para.size;
            cbcqsq->alloc.sq_depth = cbsqcq_alloc->alloc_para.sq_alloc_para.depth;
            cbcqsq->alloc.cq_depth = cbsqcq_alloc->alloc_para.cq_alloc_para.depth;
            cbcqsq->alloc.cq_irq = devdrv_cbcq_get_irq(devid, tsid);
            break;
        case CBSQCQ_MBOX_FREE:
            cbcqsq->cmd_type = DEVDRV_MAILBOX_RELEASE_CBCQSQ;
            cbcqsq->free.sq_index = cbsqcq_alloc->alloc_para.sq_alloc_para.sqid;
            cbcqsq->free.cq_index = cbsqcq_alloc->alloc_para.cq_alloc_para.cqid;
            break;
#ifndef TSDRV_UT
        default:
            TSDRV_PRINT_ERR("Unknown alloc_type=%d, devid=%u tsid=%u\n", (int)alloc_type, devid, tsid);
            ret = -EFAULT;
            goto err_invalid_type;
#endif
    }
    cbcqsq->result = 0;
    data.msg = cbcqsq;
    data.msg_len = sizeof(struct devdrv_cbcqsq_mbox);
    data.out_data = NULL;
    data.out_len = 0;
    ret = tsdrv_mailbox_send_sync(devid, tsid, &data);
err_invalid_type:
    tsdrv_kfree(cbcqsq);
    cbcqsq = NULL;
    return ret;
}

STATIC void devdrv_cbsqcq_bind(u32 devid, u32 tsid, struct devdrv_cbsqcq_para *cbsqcq_alloc)
{
    struct devdrv_cbsq_data *cbsq_data = NULL;
    struct devdrv_cbcq_data *cbcq_data = NULL;
    u32 sqid, cqid;

    sqid = cbsqcq_alloc->alloc_para.sq_alloc_para.sqid;
    cqid = cbsqcq_alloc->alloc_para.cq_alloc_para.cqid;
    cbsq_data = devdrv_cbsq_data_get(devid);
    cbcq_data = devdrv_cbcq_data_get(devid);

    ka_task_mutex_lock(&cbsq_data->sq_mutex[tsid]);
    cbsq_data->cbsq[tsid][sqid].pair_cqid = cqid;
    ka_task_mutex_unlock(&cbsq_data->sq_mutex[tsid]);

    ka_task_mutex_lock(&cbcq_data->cq_mutex[tsid]);
    cbcq_data->cbcq[tsid][cqid].pair_sqid = sqid;
    ka_task_mutex_unlock(&cbcq_data->cq_mutex[tsid]);
}

STATIC int devdrv_cbsqcq_alloc(u32 devid, u32 tsid, struct devdrv_cbsqcq_para *cbsqcq_alloc)
{
    int ret;

    ret = devdrv_cbsq_alloc(devid, tsid, &cbsqcq_alloc->alloc_para.sq_alloc_para);
    DRV_CHECK_EXP_ACT(ret != 0, return -ENOMEM, "cbsq alloc fail, ret(%d)\n", ret);

    ret = devdrv_cbcq_alloc(devid, tsid, &cbsqcq_alloc->alloc_para.cq_alloc_para);
    DRV_CHECK_EXP_ACT(ret != 0, goto err_cbcq_alloc, "cbsq alloc fail, ret(%d)\n", ret);

    devdrv_cbsqcq_bind(devid, tsid, cbsqcq_alloc);
    return 0;

err_cbcq_alloc:
    (void)devdrv_cbsq_recycle(devid, tsid, &cbsqcq_alloc->alloc_para.sq_alloc_para, CBSQCQ_UNALLOC);
    return -ENODEV;
}

STATIC void devdrv_cbsqcq_recycle(u32 devid, u32 tsid, struct devdrv_cbsqcq_para *cbsqcq_alloc,
    enum cbsqcq_alloc_flag flag)
{
    (void)devdrv_cbsq_recycle(devid, tsid, &cbsqcq_alloc->alloc_para.sq_alloc_para, flag);
    (void)devdrv_cbcq_recycle(devid, tsid, &cbsqcq_alloc->alloc_para.cq_alloc_para, flag);
}

STATIC int devdrv_cbsqcq_init(u32 devid, u32 tsid, struct devdrv_cbsqcq_para *args)
{
    int ret;

    ret = devdrv_cbsq_init(devid, tsid, &args->init_para.sq_init_para);
    if (ret != 0) {
        goto err_cbsq_init;
    }
    ret = devdrv_cbcq_init(devid, tsid, &args->init_para.cq_init_para);
    if (ret != 0) {
        goto err_cbcq_init;
    }
    return 0;
err_cbcq_init:
    devdrv_cbsq_exit(devid, tsid);
err_cbsq_init:
    TSDRV_PRINT_ERR("cbsqcq init fail, devid(%u)\n", devid);
    return -EINVAL;
}

STATIC void devdrv_cbsqcq_exit(u32 devid, u32 tsid)
{
    devdrv_cbsq_exit(devid, tsid);
    devdrv_cbcq_exit(devid, tsid);
}

STATIC void tsdrv_cbsq_args_pack(u32 devid, u32 tsid, struct devdrv_cbsq_init_para *cbsq_init_para)
{
    enum tsdrv_env_type env_type = tsdrv_get_env_type();
    struct tsdrv_sq_hwinfo *sq_hwinfo = NULL;

    cbsq_init_para->cbsq_num[tsid] = SYNC_CB_MAX_SQID_NUM;
    cbsq_init_para->mem_type[tsid] = RESERVED_MEM;

    sq_hwinfo = tsdrv_get_ts_sq_hwinfo(devid, tsid);
    if (env_type == TSDRV_ENV_OFFLINE) {
        cbsq_init_para->paddr[tsid] = sq_hwinfo->paddr + DEVDRV_MAX_SQ_NUM * DEVDRV_SQ_SLOT_SIZE * DEVDRV_MAX_SQ_DEPTH;
    } else {
        cbsq_init_para->paddr[tsid] = sq_hwinfo->bar_addr +
            DEVDRV_MAX_SQ_NUM * DEVDRV_SQ_SLOT_SIZE * DEVDRV_MAX_SQ_DEPTH;
    }
    cbsq_init_para->p_size[tsid] = SYNC_CB_MAX_SQE_SIZE * SYNC_CB_MAX_SQE_DEPTH * SYNC_CB_MAX_SQID_NUM;
    cbsq_init_para->slot_size[tsid] = SYNC_CB_MAX_SQE_SIZE;
    cbsq_init_para->depth[tsid] = SYNC_CB_MAX_SQE_DEPTH;
}

STATIC void tsdrv_cbcq_args_pack(u32 devid, u32 tsid, struct devdrv_cbcq_init_para *cq_init_para)
{
    cq_init_para->cbcq_num[tsid] = SYNC_CB_MAX_CQID_NUM;

    cq_init_para->slot_size[tsid] = SYNC_CB_MAX_CQE_SIZE;
    cq_init_para->depth[tsid] = SYNC_CB_MAX_CQE_DEPTH;
    cq_init_para->mem_type[tsid] = ALLOCATE_MEM;
}

STATIC void devdrv_cbsqcq_args_pack(u32 devid, u32 tsid, struct devdrv_cbsqcq_para *args)
{
    tsdrv_cbsq_args_pack(devid, tsid, &args->init_para.sq_init_para);
    tsdrv_cbcq_args_pack(devid, tsid, &args->init_para.cq_init_para);
}

static int tsdrv_cbcq_mmap(struct tsdrv_ctx *ctx, u32 tsid, unsigned long va, size_t size, u32 cbcqid)
{
    struct tsdrv_mem_map_para map_para = {0};
    int ret;

    map_para.id_type = TSDRV_CQ_ID;
    map_para.sub_type = MEM_MAP_SUBTYPE_CB;
    map_para.id = cbcqid;
    map_para.va = va;
    map_para.len = KA_MM_PAGE_ALIGN(size);
    map_para.addr_side = TSDRV_MEM_ON_HOST_SIDE;

    ret = tsdrv_remap_va(ctx, tsid, &map_para);
    if (ret != 0) {
        return ret;
    }
    return 0;
}

static int tsdrv_cbcq_unmap(struct tsdrv_ctx *ctx, u32 tsid, u32 cbcqid)
{
    struct tsdrv_mem_unmap_para unmap_para = {0};
    struct devdrv_cbcq_data *cbcq_data = NULL;
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    struct devdrv_cbcq *cbcq = NULL;
    unsigned long size;
    unsigned long va;

    cbcq_data = devdrv_cbcq_data_get(devid);
    ka_task_spin_lock_bh(&cbcq_data->lock[tsid]);
    cbcq = &cbcq_data->cbcq[tsid][cbcqid];
    if (cbcq->allocated != CBSQCQ_ALLOC) {
        ka_task_spin_unlock_bh(&cbcq_data->lock[tsid]);
        return -ENODEV;
    }
    va = cbcq->map_va;
    size = KA_MM_PAGE_ALIGN(cbcq->depth * cbcq->size);
    ka_task_spin_unlock_bh(&cbcq_data->lock[tsid]);
    unmap_para.va = va;
    unmap_para.len = size;

    return tsdrv_unmap_va(ctx, tsid, &unmap_para);
}

int devdrv_ioctl_cbsqcq_alloc(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    struct callback_alloc_sqcq_para *sqcq_alloc = &arg->cq_alloc_para;

    struct devdrv_cbsqcq_para cbsqcq_alloc;
    struct callback_ctx *cb_ctx = NULL;
    u32 tsid = arg->tsid;
    u32 devid;
    int ret;

    devid = tsdrv_get_devid_by_ctx(ctx);
    cb_ctx = ctx->ts_ctx[arg->tsid].sync_cb_sqcq_ctx;
    if (cb_ctx == NULL) {
        TSDRV_PRINT_ERR("cb_ctx is NULL, devid=%u tsid=%u\n", devid, tsid);
        return -ENODEV;
    }
    cbsqcq_alloc.alloc_para.sq_alloc_para.depth = sqcq_alloc->sq_depth;
    cbsqcq_alloc.alloc_para.sq_alloc_para.size = sqcq_alloc->sq_size;
    cbsqcq_alloc.alloc_para.sq_alloc_para.ctx = ctx;
    cbsqcq_alloc.alloc_para.cq_alloc_para.depth = sqcq_alloc->cq_depth;
    cbsqcq_alloc.alloc_para.cq_alloc_para.size = sqcq_alloc->cq_size;
    cbsqcq_alloc.alloc_para.cq_alloc_para.gid = sqcq_alloc->grp_id;
    cbsqcq_alloc.alloc_para.cq_alloc_para.ctx = ctx;
    cbsqcq_alloc.alloc_para.cq_alloc_para.cqMapVaddr = sqcq_alloc->cqMapVaddr;

    ka_task_mutex_lock(&cb_ctx->lock);
    ret = devdrv_cbsqcq_alloc(devid, tsid, &cbsqcq_alloc);
    if (ret != 0) {
#ifndef TSDRV_UT
        ka_task_mutex_unlock(&cb_ctx->lock);
        TSDRV_PRINT_ERR("cbsqcq alloc fail, ret=%d devid=%u tsid=%u\n", ret, devid, tsid);
        return -EFAULT;
#endif
    }
    ret = tsdrv_cbcq_mmap(ctx, tsid, sqcq_alloc->cqMapVaddr, sqcq_alloc->cq_size * sqcq_alloc->cq_depth,
        cbsqcq_alloc.alloc_para.cq_alloc_para.cqid);
    if (ret != 0) {
#ifndef TSDRV_UT
        devdrv_cbsqcq_recycle(devid, tsid, &cbsqcq_alloc, CBSQCQ_UNALLOC);
        ka_task_mutex_unlock(&cb_ctx->lock);
        return ret;
#endif
    }
    ret = devdrv_cbsqcq_mbox_send(devid, tsid, CBSQCQ_MBOX_ALLOC, &cbsqcq_alloc);
    if (ret != 0) {
#ifndef TSDRV_UT
        (void)tsdrv_cbcq_unmap(ctx, tsid, cbsqcq_alloc.alloc_para.cq_alloc_para.cqid);
        devdrv_cbsqcq_recycle(devid, tsid, &cbsqcq_alloc, CBSQCQ_UNALLOC);
        ka_task_mutex_unlock(&cb_ctx->lock);
        TSDRV_PRINT_ERR("cbqscq mbox send fail, ret=%d devid=%u tsid=%u\n", ret, devid, tsid);
        return -ENODEV;
#endif
    }
    ka_task_mutex_unlock(&cb_ctx->lock);
    sqcq_alloc->sq_id = cbsqcq_alloc.alloc_para.sq_alloc_para.sqid;
    sqcq_alloc->cq_id = cbsqcq_alloc.alloc_para.cq_alloc_para.cqid;
    return 0;
}

STATIC int devdrv_cbsqcq_free_param_chk(u32 devid, u32 tsid, struct callback_free_sqcq_para *sqcq_para,
    struct tsdrv_ctx *ctx)
{
    struct devdrv_cbcq_data *cbcq_data = devdrv_cbcq_data_get(devid);
    struct devdrv_cbsq_data *cbsq_data = devdrv_cbsq_data_get(devid);
    struct devdrv_cbcq *cbcq = NULL;
    struct devdrv_cbsq *cbsq = NULL;
    enum cbsqcq_alloc_flag status;

    if (sqcq_para->cq_id >= SYNC_CB_MAX_CQID_NUM || sqcq_para->sq_id >= SYNC_CB_MAX_SQID_NUM) {
        TSDRV_PRINT_ERR("invalid cbsqcqid, cbsqid=%u cbcqid=%u, devid=%u tsid=%u\n", sqcq_para->sq_id, sqcq_para->cq_id,
            devid, tsid);
        return -EFAULT;
    }
    cbcq = &cbcq_data->cbcq[tsid][sqcq_para->cq_id];
    ka_task_spin_lock_bh(&cbcq->lock);
    if (cbcq->allocated != CBSQCQ_ALLOC || cbcq->ctx != ctx) {
#ifndef TSDRV_UT
        ka_task_spin_unlock_bh(&cbcq->lock);
        TSDRV_PRINT_ERR("The cbcq is invalid or ctx is not the same. (cbcq_cqid=%u; status=%u; "
            "cbcq_tgid=%u; ctx_tgid=%d; devid=%u; tsid=%u)\n",
            cbcq->cqid, cbcq->allocated, cbcq->tgid, ctx->tgid, devid, tsid);
        return -ENODEV;
#endif
    }
    ka_task_spin_unlock_bh(&cbcq->lock);

    cbsq_data = devdrv_cbsq_data_get(devid);
    cbsq = &cbsq_data->cbsq[tsid][sqcq_para->sq_id];

    status = (enum cbsqcq_alloc_flag)ka_base_atomic_read(&cbsq->allocated);
    if (status != CBSQCQ_ALLOC || cbsq->ctx != ctx) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid cbsq(%u) status=%u or not same ctx(%u %d), devid=%u tsid=%u\n",
            cbsq->sqid, status, cbsq->tgid, ctx->tgid, devid, tsid);
        return -EINVAL;
#endif
    }

    return 0;
}

int devdrv_ioctl_cbsqcq_free(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    struct callback_free_sqcq_para *cq_para = &arg->cq_free_para;
    struct devdrv_cbsqcq_para cbsqcq_alloc;
    struct callback_ctx *cb_ctx = NULL;
    u32 tsid = arg->tsid;
    u32 devid;
    int ret;

    devid = tsdrv_get_devid_by_ctx(ctx);
    cb_ctx = ctx->ts_ctx[arg->tsid].sync_cb_sqcq_ctx;
    if (cb_ctx == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("cb_ctx is NULL, devid=%u tsid=%u\n", devid, tsid);
        return -ENODEV;
#endif
    }
    ka_task_mutex_lock(&cb_ctx->lock);
    ret = devdrv_cbsqcq_free_param_chk(devid, tsid, cq_para, ctx);
    if (ret != 0) {
#ifndef TSDRV_UT
        ka_task_mutex_unlock(&cb_ctx->lock);
        TSDRV_PRINT_ERR("Failed to check cbsqcq free parameters. (devid=%u; tsid=%u)\n", devid, tsid);
        return ret;
#endif
    }
    ret = tsdrv_cbcq_unmap(ctx, tsid, cq_para->cq_id);
    if (ret != 0) {
#ifndef TSDRV_UT
        ka_task_mutex_unlock(&cb_ctx->lock);
        return ret;
#endif
    }
    cbsqcq_alloc.alloc_para.sq_alloc_para.sqid = cq_para->sq_id;
    cbsqcq_alloc.alloc_para.cq_alloc_para.cqid = cq_para->cq_id;

    ret = devdrv_cbsqcq_mbox_send(devid, arg->tsid, CBSQCQ_MBOX_FREE, &cbsqcq_alloc);
    // we should abandon this cbsqcq after mbox send failed
    if (ret != 0) {
        TSDRV_PRINT_ERR("mbox send fail, devid=%u tsid=%u ret=%d\n", devid, arg->tsid, ret);
    }
    devdrv_cbsqcq_recycle(devid, arg->tsid, &cbsqcq_alloc, CBSQCQ_UNALLOC);
    ka_task_mutex_unlock(&cb_ctx->lock);

    return 0;
}

STATIC int devdrv_ioctl_cbcq_wait_chk(u32 devid, struct tsdrv_ctx *ctx,
    struct callback_wait_cq_para *cbsqcq_wait)
{
    u32 tsnum;

    tsnum = tsdrv_get_dev_tsnum(devid);
    if (cbsqcq_wait->tsid >= tsnum) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid tsid = %u, tsnum = %u\n", cbsqcq_wait->tsid, tsnum);
        return -EINVAL;
#endif
    }
    if (cbsqcq_wait->gid >= SYNC_CB_CQ_MAX_GID) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid gid=%u, devid=%u\n", cbsqcq_wait->gid, devid);
        return -EFAULT;
#endif
    }
    if (cbsqcq_wait->bitmap_size != SYNC_CB_BITMAP_SIZE) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid bitmap_size=%u, devid=%u\n", cbsqcq_wait->bitmap_size, devid);
        return -ENODEV;
#endif
    }
    if ((cbsqcq_wait->timeout == 0) || ((cbsqcq_wait->timeout < 0) && (cbsqcq_wait->timeout != -1))) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid timeout=%d(s), devid=%u\n", cbsqcq_wait->timeout, devid);
        return -EINVAL;
#endif
    }
    return 0;
}

int devdrv_ioctl_cbsq_send(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    struct callback_send_sq_para *sq_para = &arg->cb_sq_para;
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    u32 tsid = arg->tsid;

    return devdrv_cbsq_msg_send(devid, tsid, sq_para);
}

int devdrv_ioctl_cbcq_wait(struct tsdrv_ctx *ctx, void __user * arg)
{
    struct devdrv_report_wait_para wait_para = {0};
    struct callback_wait_cq_para cbsqcq_wait = {0};
    u32 devid;
    int ret;
    u32 i;

    devid = tsdrv_get_devid_by_ctx(ctx);
    ret = copy_from_user_safe(&cbsqcq_wait, arg, sizeof(struct callback_wait_cq_para));
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("cbsqcq_wait fail, devid=%u\n", devid);
        return ret;
#endif
    }
    ret = devdrv_ioctl_cbcq_wait_chk(devid, ctx, &cbsqcq_wait);
    if (ret != 0) {
        return ret;
    }
    wait_para.cbcq_bitmap_size = cbsqcq_wait.bitmap_size;
    wait_para.timeout = cbsqcq_wait.timeout;
    wait_para.gid = cbsqcq_wait.gid;
    ret = devdrv_cbcq_report_wait(devid, cbsqcq_wait.tsid, &wait_para, ctx);
    if (ret != 0) {
        goto err_report_wait;
    }
    for (i = 0; i < cbsqcq_wait.bitmap_size; i++) {
        cbsqcq_wait.bitmap_buf[i] = wait_para.cbcq_bitmap[i];
    }
    ret = copy_to_user_safe(arg, &cbsqcq_wait, sizeof(struct callback_wait_cq_para));

err_report_wait:
    return ret;
}

bool devdrv_cbsqcq_need_recycle(struct tsdrv_ts_ctx *ts_ctx)
{
    if (ts_ctx->recycle_cbsqcq_num != 0) {
        return true;
    }
    return false;
}

void devdrv_cbcqsq_to_recyclelist(u32 devid, u32 fid, u32 tsnum, struct tsdrv_ctx *ctx)
{
    struct devdrv_cbsq_data *cbsq_data = NULL;
    struct devdrv_cbcq_data *cbcq_data = NULL;
    ka_list_head_t *pos = NULL, *n = NULL;
    struct callback_ctx *cb_ctx = NULL;
    struct devdrv_cbsq *cbsq = NULL;
    struct devdrv_cbcq *cbcq = NULL;
    u32 recycle_num = 0;
    u32 tsid;

    cbsq_data = devdrv_cbsq_data_get(devid);
    cbcq_data = devdrv_cbcq_data_get(devid);

    for (tsid = 0; tsid < tsnum; tsid++) {
        cb_ctx = (struct callback_ctx *)ctx->ts_ctx[tsid].sync_cb_sqcq_ctx;
        ka_task_mutex_lock(&cbsq_data->sq_mutex[tsid]);
        ka_task_spin_lock_bh(&cbcq_data->lock[tsid]);
        ka_list_for_each_safe(pos, n, &cbsq_data->sq_alloc_head[tsid]) {
            cbsq = ka_list_entry(pos, struct devdrv_cbsq, sq_list);
            if (cbsq->ctx == ctx) {
                /* add cbsq&cbcq header */
                ka_list_del(&cbsq->sq_list);
                ka_list_add(&cbsq->sq_list, &cb_ctx->cbsq_recycle);
                recycle_num++;
            }
        }
        ka_list_for_each_safe(pos, n, &cbcq_data->cq_alloc_head[tsid]) {
            cbcq = ka_list_entry(pos, struct devdrv_cbcq, cq_list);
            ka_task_spin_lock_bh(&cbcq->lock);
            if (cbcq->ctx == ctx) {
                /* add cbsq&cbcq header */
                ka_list_del(&cbcq->cq_list);
                ka_list_add(&cbcq->cq_list, &cb_ctx->cbcq_recycle);
                recycle_num++;
            }
            ka_task_spin_unlock_bh(&cbcq->lock);
        }
        ka_task_spin_unlock_bh(&cbcq_data->lock[tsid]);
        ka_task_mutex_unlock(&cbsq_data->sq_mutex[tsid]);
        ctx->ts_ctx[tsid].recycle_cbsqcq_num = recycle_num;
    }
}

int devdrv_recycle_cbcqsq_id(struct tsdrv_ctx *ctx)
{
#ifndef TSDRV_UT
    ka_list_head_t *pos = NULL, *n = NULL;
    struct devdrv_cbsqcq_para cbsqcq_alloc;
    struct callback_ctx *cb_ctx = NULL;
    struct devdrv_cbsq *cbsq = NULL;
    u32 tsnum;
    u32 devid;
    u32 tsid;

    devid = tsdrv_get_devid_by_ctx(ctx);
    tsnum = tsdrv_get_dev_tsnum(devid);

    for (tsid = 0; tsid < tsnum; tsid++) {
        cb_ctx = (struct callback_ctx *)ctx->ts_ctx[tsid].sync_cb_sqcq_ctx;
        if (cb_ctx == NULL) {
            TSDRV_PRINT_WARN("cb ctx is NULL, devid=%u tsid=%u\n", devid, tsid);
            return -ENOMEM;
        }

        if (ka_list_empty_careful(&cb_ctx->cbsq_recycle)) {
            continue;
        }
        ka_list_for_each_safe(pos, n, &cb_ctx->cbsq_recycle) {
            cbsq = ka_list_entry(pos, struct devdrv_cbsq, sq_list);
            cbsqcq_alloc.alloc_para.sq_alloc_para.sqid = cbsq->sqid;
            cbsqcq_alloc.alloc_para.cq_alloc_para.cqid = cbsq->pair_cqid;
            devdrv_cbsqcq_recycle(devid, tsid, &cbsqcq_alloc, CBSQCQ_UNALLOC);
        }
        ctx->ts_ctx[tsid].recycle_cbsqcq_num = 0;
    }
    callback_ctx_exit(ctx, tsnum);
#endif
    return 0;
}

void tsdrv_cbsqcq_no_recycle_print(struct tsdrv_ts_ctx *ts_ctx)
{
#ifndef TSDRV_UT
    ka_list_head_t *pos = NULL, *n = NULL;
    struct callback_ctx *cb_ctx = NULL;
    struct devdrv_cbcq *cbcq = NULL;
    u32 devid;

    devid = tsdrv_get_devid_by_ts_ctx(ts_ctx);
    cb_ctx = (struct callback_ctx *)ts_ctx->sync_cb_sqcq_ctx;
    if (cb_ctx != NULL) {
        if (!ka_list_empty_careful(&cb_ctx->cbcq_recycle)) {
            TSDRV_PRINT_ERR("The cbsqcq ids are not completely recycled. (tsid=%u; recycle_cbsqcq_num=%u)\n",
                ts_ctx->tsid, ts_ctx->recycle_cbsqcq_num);
            ka_list_for_each_safe(pos, n, &cb_ctx->cbcq_recycle) {
                cbcq = ka_list_entry(pos, struct devdrv_cbcq, cq_list);
                TSDRV_PRINT_ERR("(cbsq_id=%u, cbcq_id=%u)\n", cbcq->pair_sqid, cbcq->cqid);
                tsdrv_add_mem_delay_free(devid, (void *)cbcq->vaddr, cbcq->size * cbcq->depth);
                ka_task_spin_lock_bh(&cbcq->lock);
                cbcq->ctx = NULL;
                ka_task_spin_unlock_bh(&cbcq->lock);
            }
        }
        tsdrv_add_mem_delay_free(devid, (void *)cb_ctx, 0);
    }
#endif
}

int callback_ctx_init(struct tsdrv_ctx *ctx, u32 tsnum)
{
    struct callback_ctx *cb_ctx = NULL;
    u32 tsid;
    u32 i;

    for (tsid = 0; tsid < tsnum; tsid++) {
        ctx->ts_ctx[tsid].sync_cb_sqcq_ctx = tsdrv_kzalloc(sizeof(struct callback_ctx), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
        if (ctx->ts_ctx[tsid].sync_cb_sqcq_ctx == NULL) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("alloc sync cb sqcq ctx fail\n");
            goto err_sync_cb_ctx_kmalloc;
#endif
        }
        cb_ctx = (struct callback_ctx *)ctx->ts_ctx[tsid].sync_cb_sqcq_ctx;
        ka_task_mutex_init(&cb_ctx->lock);
        KA_INIT_LIST_HEAD(&cb_ctx->cbsq_recycle);
        KA_INIT_LIST_HEAD(&cb_ctx->cbcq_recycle);
        callback_cq_ctx_init(cb_ctx);
    }
    return 0;
#ifndef TSDRV_UT
err_sync_cb_ctx_kmalloc:
    for (i = 0; i < tsid; i++) {
        cb_ctx = ctx->ts_ctx[i].sync_cb_sqcq_ctx;
        ka_task_mutex_destroy(&cb_ctx->lock);
        tsdrv_kfree(ctx->ts_ctx[i].sync_cb_sqcq_ctx);
        ctx->ts_ctx[i].sync_cb_sqcq_ctx = NULL;
    }
    return -ENOMEM;
#endif
}

void callback_ctx_exit(struct tsdrv_ctx *ctx, u32 tsnum)
{
    struct callback_ctx *cb_ctx = NULL;
    u32 tsid;

    for (tsid = 0; tsid < tsnum; tsid++) {
        if (ctx->ts_ctx[tsid].sync_cb_sqcq_ctx != NULL) {
            cb_ctx = (struct callback_ctx *)ctx->ts_ctx[tsid].sync_cb_sqcq_ctx;
            ka_task_mutex_destroy(&cb_ctx->lock);
            tsdrv_kfree(ctx->ts_ctx[tsid].sync_cb_sqcq_ctx);
            ctx->ts_ctx[tsid].sync_cb_sqcq_ctx = NULL;
        }
    }
}

int callback_dev_init(u32 devid, u32 fid, u32 tsnum)
{
    struct devdrv_cbsqcq_para args;
    u32 tsid;
    int ret;
    u32 i;

    ret = devdrv_cbsqcq_setup(devid);
    if (ret != 0) {
        return ret;
    }

    for (tsid = 0; tsid < tsnum; tsid++) {
        devdrv_cbsqcq_args_pack(devid, tsid, &args);
        ret = devdrv_cbsqcq_init(devid, tsid, &args);
        if (ret != 0) {
#ifndef TSDRV_UT
            goto err_cbsqcq_init;
#endif
        }
    }
    return 0;
#ifndef TSDRV_UT
err_cbsqcq_init:
    for (i = 0; i < tsid; i++) {
        devdrv_cbsqcq_exit(devid, i);
    }
    devdrv_cbsqcq_cleanup(devid);
    return -ENODEV;
#endif
}

void callback_dev_exit(u32 devid, u32 fid, u32 tsnum)
{
    u32 tsid;

    for (tsid = 0; tsid < tsnum; tsid++) {
        devdrv_cbsqcq_exit(devid, tsid);
    }

    devdrv_cbsqcq_cleanup(devid);
}

STATIC int devdrv_cbsqcq_setup(u32 devid)
{
    int ret;

    TSDRV_PRINT_INFO("Devdrv callback sqcq starts to setup. (devid=%u)\n", devid);

    ret = devdrv_cbsq_setup(devid);
    if (ret != 0) {
        return ret;
    }
    ret = devdrv_cbcq_setup(devid);
    if (ret != 0) {
        devdrv_cbsq_cleanup(devid);
        return ret;
    }
    return 0;
}

STATIC void devdrv_cbsqcq_cleanup(u32 devid)
{
    TSDRV_PRINT_INFO("Devdrv callback sqcq starts to cleanup. (devid=%u)\n", devid);

    devdrv_cbsq_cleanup(devid);
    devdrv_cbcq_cleanup(devid);
}
void callback_g_info_exit_all_fids(u32 devid, u32 tsnum)
{
    return;
}

