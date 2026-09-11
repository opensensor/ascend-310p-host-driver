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

#include <linux/slab.h>
#include <linux/io.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#ifndef AOS_LLVM_BUILD
#include <linux/sched/signal.h>
#endif
#endif
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/notifier.h>

#include "ascend_hal_define.h"

#include "tsdrv_sync.h"
#include "devdrv_cqsq.h"
#include "devdrv_devinit.h"
#include "tsdrv_logic_cq.h"
#include "tsdrv_ctx.h"
#include "tsdrv_kernel_common.h"
#include "devdrv_common.h"
#ifndef CFG_SOC_PLATFORM_MDC_V51
#include "hvtsdrv_tsagent.h"
#include "hvtsdrv_logic_cq.h"
#endif
#include "logic_cq.h"
#include "event_sched_inner.h"
#include "tsdrv_sqcq_timestamp.h"
#include "event_sched_inner.h"
#include "ka_task_pub.h"
#include "ka_list_pub.h"
#include "ka_memory_pub.h"

#define TSDRV_MAX_THREAD_NUM_IN_GRP SCHED_MAX_THREAD_NUM_IN_GRP * 2
#define LOGIC_CQ_QUEUE_FULL 1

STATIC struct logic_sqcq *g_logic_sqcq[TSDRV_MAX_DAVINCI_NUM][TSDRV_MAX_FID_NUM][DEVDRV_MAX_TS_NUM];

static int tsdrv_is_id_belong_to_proc(struct tsdrv_id_inst *id_inst, ka_pid_t tgid,
    u32 id, enum tsdrv_id_type id_type)
{
    struct tsdrv_ts_resource *ts_res = NULL;
    struct tsdrv_id_info *id_info = NULL;
    struct tsdrv_id_info *n = NULL;
    struct tsdrv_ctx *ctx = NULL;

    if ((id_inst == NULL) || (id_inst->devid >= TSDRV_MAX_DAVINCI_NUM)) {
        return -EINVAL;
    }

    if ((id_inst->fid >= TSDRV_MAX_FID_NUM) || (id_inst->tsid >= tsdrv_get_dev_tsnum(id_inst->devid))) {
        return -EINVAL;
    }

    ctx = tsdrv_dev_proc_ctx_get(id_inst->devid, id_inst->fid, tgid);
    if (ctx == NULL) {
        return -ENODEV;
    }

    ts_res = tsdrv_get_ts_resoruce(id_inst->devid, id_inst->fid, id_inst->tsid);
    ka_task_spin_lock(&ts_res->id_res[id_type].spinlock);
    if (ka_list_empty_careful(&ctx->ts_ctx[id_inst->tsid].id_ctx[id_type].id_list) == 0) {
        ka_list_for_each_entry_safe(id_info, n, &ctx->ts_ctx[id_inst->tsid].id_ctx[id_type].id_list, list) {
            if (((u32)id_info->id == id) && (id_info->tgid == tgid)) {
                ka_task_spin_unlock(&ts_res->id_res[id_type].spinlock);
                tsdrv_dev_proc_ctx_put(ctx);
                return 0;
            }
        }
    }

    ka_task_spin_unlock(&ts_res->id_res[id_type].spinlock);
    tsdrv_dev_proc_ctx_put(ctx);

    return -EFAULT;
}

struct logic_cq_info *logic_cq_info_get(u32 devid, u32 fid, u32 tsid)
{
    if (g_logic_sqcq[devid][fid][tsid] == NULL) {
        TSDRV_PRINT_ERR("get logic_cq fail, devid=%u fid=%u tsid=%u\n", devid, fid, tsid);
        return NULL;
    }
    return &g_logic_sqcq[devid][fid][tsid]->logic_cq;
}

STATIC struct logic_cq_sub *logic_cq_sub_get(u32 devid, u32 fid, u32 tsid, u32 cqid)
{
    if (g_logic_sqcq[devid][fid][tsid] == NULL) {
        TSDRV_PRINT_ERR("get logic_cq fail, devid=%u fid=%u tsid=%u\n", devid, fid, tsid);
        return NULL;
    }
    return &g_logic_sqcq[devid][fid][tsid]->logic_cq.sub_cq[cqid];
}

#ifdef CFG_SOC_PLATFORM_MDC_V51
STATIC u32 g_logic_cq_id_max_num = TSDRV_MAX_LOGIC_CQ_NUM;
void tsdrv_set_logic_cq_id_max_num(struct devdrv_info *dev_info)
{
    switch (dev_info->pdata->ts_spec) {
        case HWTS_SPEC_BASE:
            g_logic_cq_id_max_num = TS_BASE_SPEC_LOGIC_CQ_ID_MAX_NUM;
            break;
        case HWTS_SPEC_PREMIUM:
            g_logic_cq_id_max_num = TS_PREMIUM_SPEC_LOGIC_CQ_ID_MAX_NUM;
            break;
        case HWTS_SPEC_ULTIMATE:
            g_logic_cq_id_max_num = TS_ULTIMATE_SPEC_LOGIC_CQ_ID_MAX_NUM;
            break;
        default:
            break;
    }
    return;
}
u32 tsdrv_get_logic_cq_id_max_num(void)
{
    return g_logic_cq_id_max_num;
}
#endif

STATIC bool logic_cq_queue_full(u32 head, u32 tail, u32 depth)
{
    if (depth == 0) {
        return true;
    }

    return (head == ((tail + 1) % depth));
}

STATIC void logic_cq_exit(u32 devid, u32 fid, u32 tsid)
{
    struct logic_cq_info *logic_cq = NULL;
    u32 i;

    logic_cq = logic_cq_info_get(devid, fid, tsid);
    if (logic_cq == NULL) {
        TSDRV_PRINT_ERR("logic cq get fail, devid=%u fid=%u tsid=%u\n", devid, fid, tsid);
        return;
    }
    ka_task_spin_lock_bh(&logic_cq->lock);
    if (logic_cq->init == LOGIC_SQCQ_UNINIT) {
        ka_task_spin_unlock_bh(&logic_cq->lock);
#ifndef TSDRV_UT
        TSDRV_PRINT_WARN("logic cq info is already uninit, devid=%u fid=%u tsid=%u\n", devid, fid, tsid);
#endif
        return;
    }
    logic_cq->init = LOGIC_SQCQ_UNINIT;
    for (i = 0; i < TSDRV_MAX_LOGIC_CQ_NUM; i++) {
        if (logic_cq->sub_cq[i].vaddr != NULL) {
            tsdrv_kfree(logic_cq->sub_cq[i].vaddr);
            logic_cq->sub_cq[i].vaddr = NULL;
        }
        if (logic_cq->sub_cq[i].useflag == LOGIC_CQ_ABANDONED) {
            continue;
        }
        ka_list_del(&logic_cq->sub_cq[i].cq_list);
        logic_cq->logic_cq_num--;
    }
    ka_task_spin_unlock_bh(&logic_cq->lock);
}

STATIC int logic_cq_init(u32 devid, u32 fid, u32 tsid)
{
    struct logic_cq_info *logic_cq = NULL;
    u32 cqid = 0;

    logic_cq = logic_cq_info_get(devid, fid, tsid);
#ifndef TSDRV_UT
    if (logic_cq == NULL) {
        TSDRV_PRINT_ERR("callback info is null.(devid:%u fid:%u tsid:%u)\n", devid, fid, tsid);
        return -EINVAL;
    }
#endif
    ka_task_spin_lock_init(&logic_cq->lock);
    ka_task_spin_lock_bh(&logic_cq->lock);
#ifndef TSDRV_UT
    if (logic_cq->init == LOGIC_SQCQ_INIT) {
        ka_task_spin_unlock_bh(&logic_cq->lock);
        TSDRV_PRINT_WARN("logic cq init repeate.(devid:%u fid:%u tsid:%u)\n", devid, fid, tsid);
        return 0;
    }
#endif
    logic_cq->logic_cq_num = 0;
    KA_INIT_LIST_HEAD(&logic_cq->cq_free_list);
    /* logic_cq init */
    for (cqid = 0; cqid < TSDRV_MAX_LOGIC_CQ_NUM; cqid++) {
        logic_cq->sub_cq[cqid].useflag = LOGIC_CQ_IS_FREE;
        logic_cq->sub_cq[cqid].size = TSDRV_MAX_LOGIC_CQE_SIZE;
        logic_cq->sub_cq[cqid].depth = TSDRV_MAX_LOGIC_CQE_DEPTH;
        logic_cq->sub_cq[cqid].cqid = cqid;
        logic_cq->sub_cq[cqid].vaddr = NULL;
        ka_task_mutex_init(&logic_cq->sub_cq[cqid].mutex);
        logic_cq->logic_cq_num++;
        ka_list_add_tail(&logic_cq->sub_cq[cqid].cq_list, &logic_cq->cq_free_list);
    }
    logic_cq->init = LOGIC_SQCQ_INIT;
    ka_task_spin_unlock_bh(&logic_cq->lock);
    TSDRV_PRINT_DEBUG("Logic cq init success. (devid=%u; fid=%u; tsid=%u)\n", devid, fid, tsid);
    return 0;
}

STATIC int logic_ioctl_cq_alloc_param_chk(struct logic_sqcq_alloc_para *arg)
{
    if ((arg->cq_size == 0) || (arg->cq_size > LOGIC_CQE_MAX_SIZE)) {
        TSDRV_PRINT_ERR("Invalid cqe size. (cqeSize=%u)\n", arg->cq_size);
        return -ENODEV;
    }
    if ((arg->cq_depth < TSDRV_MIN_LOGIC_CQE_DEPTH) || (arg->cq_depth > TSDRV_MAX_LOGIC_CQE_DEPTH)) {
        TSDRV_PRINT_ERR("invalid cqeDepth=%u cqeDepthRange:[%u %u]\n", arg->cq_depth, TSDRV_MIN_LOGIC_CQE_DEPTH,
            TSDRV_MAX_LOGIC_CQE_DEPTH);
        return -EINVAL;
    }
    return 0;
}

STATIC int logic_cq_mem_init(struct logic_cq_sub *sub_cq, struct logic_cq_alloc_para *para)
{
    sub_cq->vaddr = tsdrv_kzalloc((size_t)para->cq_size * para->cq_depth, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
#ifndef TSDRV_UT
    if (sub_cq->vaddr == NULL) {
        TSDRV_PRINT_ERR("alloc cq mem fail, cq_size=%u cq_depth=%u\n", para->cq_size, para->cq_depth);
        return -ENOMEM;
    }
#endif
    sub_cq->size = para->cq_size;
    sub_cq->depth = para->cq_depth;
    return 0;
}

STATIC void logic_cq_mem_exit(struct logic_cq_sub *sub_cq)
{
    if (sub_cq->vaddr != NULL) {
        tsdrv_kfree(sub_cq->vaddr);
        sub_cq->vaddr = NULL;
    }
    sub_cq->size = 0;
    sub_cq->depth = 0;
}

STATIC struct logic_cq_sub *logic_cqid_get(struct logic_cq_info *logic_cq)
{
    struct logic_cq_sub *sub_cq = NULL;
    u32 useflag;
    u32 cqid;

    ka_task_spin_lock_bh(&logic_cq->lock);
    if (!ka_list_empty_careful(&logic_cq->cq_free_list)) {
        sub_cq = ka_list_first_entry(&logic_cq->cq_free_list, struct logic_cq_sub, cq_list);
        if (sub_cq->useflag != LOGIC_CQ_IS_FREE) {
#ifndef TSDRV_UT
            cqid = sub_cq->cqid;
            useflag = sub_cq->useflag;
            ka_task_spin_unlock_bh(&logic_cq->lock);
            TSDRV_PRINT_ERR("invalid cq useflag=%u, cqid=%u\n", useflag, cqid);
            return NULL;
#endif
        }
        logic_cq->logic_cq_num--;
        ka_list_del(&sub_cq->cq_list);
        ka_task_spin_unlock_bh(&logic_cq->lock);
        return sub_cq;
    }
    ka_task_spin_unlock_bh(&logic_cq->lock);
    return sub_cq;
}

u32 logic_cq_num_get(u32 devid, u32 fid, u32 tsid)
{
    struct logic_cq_info *logic_cq = NULL;
    u32 logic_cq_num;

    logic_cq = logic_cq_info_get(devid, fid, tsid);
    if (logic_cq == NULL) {
        return 0;
    }
    ka_task_spin_lock_bh(&logic_cq->lock);
#ifdef CFG_SOC_PLATFORM_MDC_V51
    logic_cq_num = tsdrv_get_logic_cq_id_max_num() - (TSDRV_MAX_LOGIC_CQ_NUM - logic_cq->logic_cq_num);
#else
    logic_cq_num = logic_cq->logic_cq_num;
#endif
    ka_task_spin_unlock_bh(&logic_cq->lock);
    return logic_cq_num;
}

STATIC void logic_cqid_put(struct logic_cq_info *logic_cq, struct logic_cq_sub *sub_cq)
{
    ka_task_spin_lock_bh(&logic_cq->lock);
    logic_cq->logic_cq_num++;
    ka_list_add(&sub_cq->cq_list, &logic_cq->cq_free_list);
    ka_task_spin_unlock_bh(&logic_cq->lock);
}

STATIC int logic_cqid_init(struct logic_cq_info *logic_cq, struct logic_sqcq_ts_ctx *logic_ctx,
    struct logic_cq_sub *sub_cq, u32 type)
{
    ka_task_spin_lock_bh(&logic_cq->lock);
    sub_cq->head = 0;
    sub_cq->tail = 0;
    sub_cq->vpid = logic_ctx->tgid;
    sub_cq->ctx = logic_ctx;
    sub_cq->type = type;
    if (type == NORMAL_SQCQ_TYPE) {
        logic_ctx->logic_cq_num++;
    } else {
        logic_ctx->ctrl_logic_cq_num++;
    }
    sub_cq->useflag = LOGIC_CQ_IN_USE;
    ka_list_add(&sub_cq->cq_list, &logic_ctx->logic_cq_list);
    ka_task_spin_unlock_bh(&logic_cq->lock);
    return 0;
}

STATIC struct logic_cq_sub *logic_cqid_exit(struct logic_cq_info *logic_cq, struct logic_sqcq_ts_ctx *logic_ctx,
    u32 logic_cqid)
{
    struct logic_cq_sub *sub_cq = NULL;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;

    ka_task_spin_lock_bh(&logic_cq->lock);
    if (!ka_list_empty_careful(&logic_ctx->logic_cq_list)) {
        ka_list_for_each_safe(pos, n, &logic_ctx->logic_cq_list) {
            sub_cq = ka_list_entry(pos, struct logic_cq_sub, cq_list);
            if (sub_cq->cqid == logic_cqid) {
                ka_list_del(&sub_cq->cq_list);
                if (sub_cq->type == CTRL_SQCQ_TYPE) {
                    logic_ctx->ctrl_logic_cq_num--;
                } else {
                    logic_ctx->logic_cq_num--;
                }
                sub_cq->useflag = LOGIC_CQ_IS_FREE;
                sub_cq->ctx = NULL;
                ka_task_spin_unlock_bh(&logic_cq->lock);
                return sub_cq;
            }
        }
    }
    ka_task_spin_unlock_bh(&logic_cq->lock);
    return NULL;
}

STATIC int logic_cq_alloc(struct logic_cq_info *logic_cq, struct logic_sqcq_ts_ctx *logic_ctx,
    struct logic_cq_alloc_para *alloc)
{
    struct logic_cq_sub *sub_cq = NULL;
    int err;

    sub_cq = logic_cqid_get(logic_cq);
#ifndef TSDRV_UT
    if (sub_cq == NULL) {
        TSDRV_PRINT_ERR("get logic cqid fail\n");
        return -ENOMEM;
    }
#endif
    err = logic_cq_mem_init(sub_cq, alloc);
#ifndef TSDRV_UT
    if (err != 0) {
        logic_cqid_put(logic_cq, sub_cq);
        TSDRV_PRINT_ERR("logic cq mem init fail, err=%d\n", err);
        return -EINVAL;
    }
#endif
    err = logic_cqid_init(logic_cq, logic_ctx, sub_cq, alloc->type);
#ifndef TSDRV_UT
    if (err != 0) {
        logic_cq_mem_exit(sub_cq);
        logic_cqid_put(logic_cq, sub_cq);
        return -EFAULT;
    }
#endif
    TSDRV_PRINT_DEBUG("Logic cq info. (logic_cqid=%u; type=%u)\n", sub_cq->cqid, sub_cq->type);
    alloc->cq_id = sub_cq->cqid;
    return 0;
}

STATIC void logic_cq_wakeup_ctx(u32 devid, u32 fid, u32 tsid, struct logic_sqcq_ts_ctx *logic_ctx, u32 logic_cqid)
{
#ifndef TSDRV_UT
    if (logic_ctx != NULL) {
        int wait_thread_num = ka_base_atomic_read(&logic_ctx->wait_thread_num[logic_cqid]);
        int wakeup_num = (wait_thread_num > 0) ? wait_thread_num : 1;

        ka_base_atomic_set(&logic_ctx->wakeup_num[logic_cqid], wakeup_num);
        tsdrv_sqcq_dfx_set_wakeup_tick(devid, fid, tsid, logic_cqid);

        TSDRV_PRINT_DEBUG("Wakeup all. (logic_cqid=%u; wait_thread_num=%d)\n", logic_cqid, wait_thread_num);
        if (ka_task_waitqueue_active(&logic_ctx->logic_cq_wait[logic_cqid]) != 0) {
#ifndef AOS_LLVM_BUILD
            ka_task_wake_up_nr(&logic_ctx->logic_cq_wait[logic_cqid], wakeup_num);
#else
            ka_task_wake_up(&logic_ctx->logic_cq_wait[logic_cqid]);
#endif
        }
    }
#endif
}

STATIC int logic_cq_free(struct tsdrv_ctx *ctx, u32 tsid, u32 logic_cqid)
{
    struct logic_sqcq_ts_ctx *logic_ctx = NULL;
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    struct logic_cq_info *logic_cq = NULL;
    u32 fid = tsdrv_get_fid_by_ctx(ctx);
    struct logic_cq_sub *sub_cq = NULL;

    logic_cq = logic_cq_info_get(devid, fid, tsid);
#ifndef TSDRV_UT
    if (logic_cq == NULL) {
        return -ENODEV;
    }
#endif
    logic_ctx = (struct logic_sqcq_ts_ctx *)ctx->ts_ctx[tsid].logic_sqcq_ctx;
#ifndef TSDRV_UT
    if (logic_ctx == NULL) {
        TSDRV_PRINT_ERR("logic ctx is NULL\n");
        return -ENODEV;
    }
#endif
    sub_cq = logic_cqid_exit(logic_cq, logic_ctx, logic_cqid);
#ifndef TSDRV_UT
    if (sub_cq == NULL) {
        TSDRV_PRINT_ERR("cqid=%u isn't allocated by tgid=%d, devid=%u fid=%u tsid=%u\n", logic_cqid, logic_ctx->tgid,
            devid, fid, tsid);
        return -EINVAL;
    }
#endif
    logic_cq_mem_exit(sub_cq);
    logic_cqid_put(logic_cq, sub_cq);
    logic_cq_wakeup_ctx(devid, fid, tsid, logic_ctx, logic_cqid);
    TSDRV_PRINT_DEBUG("free logic. (cqid=%u; devid=%u; tsid=%u)\n", sub_cq->cqid, devid, tsid);
    return 0;
}

STATIC int logic_sqcq_phy_cq_alloc(struct tsdrv_ctx *ctx, u32 tsid, struct logic_sqcq_ts_ctx *logic_ctx,
    struct logic_sqcq_alloc_para *para)
{
#ifdef CFG_FEATURE_STARS
    return 0;
#else
    struct phy_cq_alloc_para alloc_phy_cq;
    int err;

    if (para->info[2] == U16_MAX) { /* info[2] means alloc logic for ctrl sqcq. */
        return 0;
    }

    if (tsdrv_is_in_container(tsdrv_get_devid_by_ctx(ctx))) {
        TSDRV_PRINT_DEBUG("Container alloc cq in mbox_refresh.\n");
        return 0;
    }

    if (logic_ctx->logic_cq_num == 0) {
        alloc_phy_cq.type = LOGIC_SQCQ_TYPE;
        alloc_phy_cq.cq_size = para->cq_size;
        alloc_phy_cq.cq_depth = para->cq_depth;
        err = tsdrv_phy_cq_alloc(ctx, tsid, &alloc_phy_cq);
        if (err != 0) {
            return err;
        }
        logic_ctx->phy_cq.inited = LOGIC_CQ_PHY_CQ_INIT;
        logic_ctx->phy_cq.phase = 1;
        logic_ctx->phy_cq.index = alloc_phy_cq.cq_id;
        logic_ctx->phy_cq.head = 0;
        logic_ctx->phy_cq.tail = 0;
        logic_ctx->phy_cq.in_cnt = 0;
        logic_ctx->phy_cq.out_cnt = 0;
        logic_ctx->phy_cq.drop_cnt = 0;
        logic_ctx->phy_cq.size = alloc_phy_cq.cq_size;
        logic_ctx->phy_cq.depth = alloc_phy_cq.cq_depth;
        logic_ctx->phy_cq.paddr = alloc_phy_cq.cq_paddr;
        logic_ctx->phy_cq.vaddr = alloc_phy_cq.cq_vaddr;
        logic_ctx->phy_cq.cq_irq = alloc_phy_cq.cq_irq;
        logic_ctx->phy_cq_addr_set_once_flag = 0;
        TSDRV_PRINT_DEBUG("Alloc phy_cq. (phy_cqid=%u)\n", logic_ctx->phy_cq.index);
    }
    return 0;
#endif
}

STATIC int logic_sqcq_phy_cq_free(struct tsdrv_ctx *ctx, u32 tsid, struct logic_sqcq_ts_ctx *logic_ctx, u32 type)
{
#ifdef CFG_FEATURE_STARS
    return 0;
#else
    struct phy_cq_free_para phy_cq_free;
    int err;

    if (type == CTRL_SQCQ_TYPE) {
        return 0;
    }

    if (tsdrv_is_in_container(tsdrv_get_devid_by_ctx(ctx))) {
        TSDRV_PRINT_DEBUG("Container does not free cq in process.\n");
        return 0;
    }

    if (logic_ctx->logic_cq_num == 0) {
        phy_cq_free.cq_id = logic_ctx->phy_cq.index;

        logic_ctx->phy_cq.inited = LOGIC_CQ_PHY_CQ_UNINIT;
        logic_ctx->phy_cq.phase = 0;
        logic_ctx->phy_cq.index = 0;
        logic_ctx->phy_cq.head = 0;
        logic_ctx->phy_cq.tail = 0;
        logic_ctx->phy_cq.size = 0;
        logic_ctx->phy_cq.depth = 0;
        logic_ctx->phy_cq.in_cnt = 0;
        logic_ctx->phy_cq.out_cnt = 0;
        logic_ctx->phy_cq.drop_cnt = 0;
        logic_ctx->phy_cq.paddr = 0;
        logic_ctx->phy_cq.vaddr = NULL;
        err = tsdrv_phy_cq_free(ctx, tsid, &phy_cq_free);
#ifndef TSDRV_UT
        if (err != 0) {
            TSDRV_PRINT_ERR("phy cq free fail\n");
            return err;
        }
#endif
        TSDRV_PRINT_DEBUG("phy cq free succeed, tsid=%u phy_cqid=%u\n", tsid, phy_cq_free.cq_id);
    }
    return 0;
#endif
}

STATIC int logic_sqcq_mbox_alloc_msg_pack(struct logic_cqsq_mbox_msg *mbox_msg,
    struct logic_sqcq_ts_ctx *logic_ctx, struct logic_cq_alloc_para *alloc_logic_cq,
    struct logic_sqcq_alloc_para *para, u32 fid)
{
#ifdef CFG_FEATURE_STARS
    return 0;
#else
    size_t size;
    int err;

    if (alloc_logic_cq->type == CTRL_SQCQ_TYPE) {
        return 0;
    }

    mbox_msg->valid = DEVDRV_MAILBOX_MESSAGE_VALID;
    mbox_msg->cmd_type = TSDRV_MBOX_LOGIC_CQ_ALLOC;
    mbox_msg->result = 0;

    if (alloc_logic_cq->thread_bind_irq >= 0) {
        mbox_msg->alloc.thread_bind_irq_flag = 1;
        mbox_msg->alloc.cq_irq = alloc_logic_cq->thread_bind_irq;
        mbox_msg->alloc.phy_cq_addr = 0;
    } else {
        mbox_msg->alloc.thread_bind_irq_flag = 0;
        mbox_msg->alloc.cq_irq = logic_ctx->phy_cq.cq_irq;
        if (logic_ctx->phy_cq_addr_set_once_flag == 0) {
            logic_ctx->phy_cq_addr_set_once_flag = 1;
            mbox_msg->alloc.phy_cq_addr = logic_ctx->phy_cq.paddr;
        } else {
            mbox_msg->alloc.phy_cq_addr = 0;
        }
    }

    mbox_msg->alloc.cqe_size = logic_ctx->phy_cq.size;
    mbox_msg->alloc.cq_depth = logic_ctx->phy_cq.depth;
    mbox_msg->alloc.vpid = logic_ctx->tgid;
    mbox_msg->alloc.vfid = fid;
    mbox_msg->alloc.logic_cqid = alloc_logic_cq->cq_id;
    mbox_msg->alloc.phy_cqid = logic_ctx->phy_cq.index;
    mbox_msg->alloc.app_flag = tsdrv_get_env_type();

    TSDRV_PRINT_DEBUG("Send message to TS. (tgid=%d; phy_cqid=%u; logic_cq=%u; cqe_size=%u; "
        "cqe_depth=%u; cq_irq=%u)\n", logic_ctx->tgid, (u32)mbox_msg->alloc.phy_cqid,
        (u32)mbox_msg->alloc.logic_cqid, (u32)mbox_msg->alloc.cqe_size,
        (u32)mbox_msg->alloc.cq_depth, (u32)mbox_msg->alloc.cq_irq);

    size = sizeof(u32) * SQCQ_RTS_INFO_LENGTH;
    err = memcpy_s(mbox_msg->alloc.info, size, para->info, size);
    if (err != EOK) {
        TSDRV_PRINT_ERR("memcpy fail, err=%d\n", err);
        return err;
    }
    return 0;
#endif
}

STATIC void logic_sqcq_mbox_free_msg_pack(struct logic_cqsq_mbox_msg *mbox_msg,
    struct logic_sqcq_ts_ctx *logic_ctx, u32 fid, u32 logic_cqid, u32 type)
{
#ifdef CFG_FEATURE_STARS
    return;
#endif

    if (type == CTRL_SQCQ_TYPE) {
        return;
    }

    mbox_msg->valid = DEVDRV_MAILBOX_MESSAGE_VALID;
    mbox_msg->cmd_type = TSDRV_MBOX_LOGIC_CQ_FREE;
    mbox_msg->result = 0;

    mbox_msg->free.logic_cqid = logic_cqid;
    mbox_msg->free.vpid = logic_ctx->tgid;
    mbox_msg->free.vfid = fid;

    if (logic_ctx->logic_cq_num == 1) {
        mbox_msg->free.phy_cqid = logic_ctx->phy_cq.index;
    } else {
        mbox_msg->free.phy_cqid = 0xFFFFU;
    }
}

static u32 logic_cq_calc_report_cnt(struct logic_cq_sub *sub_cq, u32 max_cnt)
{
    u32 report_cnt;

    if (sub_cq->tail > sub_cq->head) {
        report_cnt = sub_cq->tail - sub_cq->head;
        report_cnt = ka_base_min(report_cnt, max_cnt);
    } else {
        report_cnt = sub_cq->depth - sub_cq->head;
        report_cnt = ka_base_min(report_cnt, max_cnt);
    }
    return report_cnt;
}

static int logic_cq_non_match_copy(struct logic_cq_sub *sub_cq, struct logic_cq_wait_cq_para *wait)
{
    u32 report_cnt;
    int ret;

    report_cnt = logic_cq_calc_report_cnt(sub_cq, wait->cqe_num);
    if (report_cnt == 0) {
        return -EAGAIN;
    }

    ret = ka_base_copy_to_user((void __user *)wait->report_buf, sub_cq->vaddr + ((unsigned long)sub_cq->head * sub_cq->size),
        (unsigned long)report_cnt * sub_cq->size);
    if (ret != 0) {
        TSDRV_PRINT_ERR("copy to user fail, cqid=%u report_cnt=%u\n", sub_cq->cqid, report_cnt);
        return -ENODEV;
    }

    wait->report_cqe_num = report_cnt;

    return 0;
}

static u32 logic_cq_get_match_copy_range(struct logic_cq_sub *sub_cq, struct logic_cq_wait_cq_para *wait,
    u32 cqe_start, u32 cqe_end, u32 *num)
{
    u32 cqe_cnt, report_cnt, i, pos, search_start, search_num, stream_id, task_id;
    u8 match_flag;

    cqe_cnt = cqe_end - cqe_start;
    report_cnt = ka_base_min(cqe_cnt, wait->cqe_num);

    search_start = 0;
    search_num = 0;

    /* Principle: Only one hole can be formed after the copy is complete.
       several situations:
       1. taskid invalid, cqe: normal, normal, ..., task1. normal cqe is more than recv cqe_num, copy all normal cqes;
       2. taskid invalid, cqe: normal, task1, normal, normal. continuous normal cqe is less than recv cqe_num, copy all
          continuous cqes before the match task;
       3. taskid invalid, cqe: task1, task2, normal, task3. normal cqe is after than match cqes, ignore the match cqes;
       4. taskid=1, cqe: normal, normal, ..., task1. normal cqe is more than recv cqe_num, copy all
          normal cqes, copy the match cqe when user recv next time;
       5. taskid=1, cqe: normal, task1, normal. normal cqes in front of match cqe is less than(include
          match cqe) recv cqe_num, copy all the cqes in front of match cqe and the match cqe;
       6. taskid=1, cqe: normal, task2, normal, task1. If there are normal cqes and other cqes need to
          be matched before the match cqe, only the first consecutive segment of common cqes is copied.
       7. taskid=1, cqe: task2, normal, task1. copy normal and task1.
    */
    for (i = 0; i < cqe_cnt; i++) {
        pos = cqe_start + i;
        if (sub_cq->type == NORMAL_SQCQ_TYPE) {
            struct tag_ts_logic_cq_report_msg *report =
                (struct tag_ts_logic_cq_report_msg *)(sub_cq->vaddr + ((unsigned long)pos * sub_cq->size));
            match_flag = report->match_flag;
            stream_id = report->stream_id;
            task_id = report->task_id;
        } else {
            struct tag_logic_cq_report_msg *report =
                (struct tag_logic_cq_report_msg *)(sub_cq->vaddr + ((unsigned long)pos * sub_cq->size));
            match_flag = (u8)report->match_flag;
            stream_id = report->stream_id;
            task_id = report->task_id;
        }

        TSDRV_PRINT_DEBUG("Cmp."
            "(cqid=%u; pos=%u; match_flag=%u; cqe_stream=%u; task=%u; wait_stream=%u; wait_task=%u)\n",
            sub_cq->cqid, pos, (u32)match_flag, stream_id, task_id, wait->stream_id, wait->task_id);

        if (match_flag == 1) {
            /* matched, not copy the following cqes. */
            if ((stream_id == wait->stream_id) && (task_id == wait->task_id)) {
                if (search_num == 0) {
                    search_start = pos;
                }
                search_num++;
                break;
            }

            /* there are normal cqes and other cqes need to be matched before the match cqe, copy the normal cqes */
            if (search_num != 0) {
                break;
            }
        } else {
            /* set consecutive normal cqes first pos and update num */
            if (search_num == 0) {
                search_start = pos;
            }
            search_num++;
        }

        if (search_num >= report_cnt) {
            break;
        }
    }

    TSDRV_PRINT_DEBUG("Cmp. (cqid=%u; tid=%d; report_cnt=%u; cqe_num=%u; cqe_cnt=%u, search_start=%u; search_num=%u)\n",
        sub_cq->cqid, current->pid, report_cnt, wait->cqe_num, cqe_cnt, search_start, search_num);

    *num = search_num;
    return search_start;
}

#ifndef TSDRV_UT
static void logic_cq_move_ts_cqes(struct logic_cq_sub *sub_cq, u32 src_pos, u32 dst_pos, u32 num)
{
    struct tag_ts_logic_cq_report_msg *report_s = NULL, *report_d = NULL;
    u32 i, src_last, dst_last;

    TSDRV_PRINT_DEBUG("Move cqe. (cqid=%u; num=%u; src_pos=%u; dst_pos=%u)\n",
        sub_cq->cqid, num, src_pos, dst_pos);

    src_last = src_pos + num - 1;
    dst_last = dst_pos + num - 1;
    report_s = (struct tag_ts_logic_cq_report_msg *)(sub_cq->vaddr + ((unsigned long)src_last * sub_cq->size));
    report_d = (struct tag_ts_logic_cq_report_msg *)(sub_cq->vaddr + ((unsigned long)dst_last * sub_cq->size));

    /* Copy cqes from the back to the front to avoid overwriting. */
    for (i = 0; i < num; i++) {
        *report_d = *report_s;
        report_s--;
        report_d--;
    }
}

static void logic_cq_move_cqes(struct logic_cq_sub *sub_cq, u32 src_pos, u32 dst_pos, u32 num)
{
    struct tag_logic_cq_report_msg *report_s = NULL, *report_d = NULL;
    u32 i, src_last, dst_last;

    TSDRV_PRINT_DEBUG("Move cqe. (cqid=%u; num=%u; src_pos=%u; dst_pos=%u)\n",
        sub_cq->cqid, num, src_pos, dst_pos);

    src_last = src_pos + num - 1;
    dst_last = dst_pos + num - 1;
    report_s = (struct tag_logic_cq_report_msg *)(sub_cq->vaddr + ((unsigned long)src_last * sub_cq->size));
    report_d = (struct tag_logic_cq_report_msg *)(sub_cq->vaddr + ((unsigned long)dst_last * sub_cq->size));

    /* Copy cqes from the back to the front to avoid overwriting. */
    for (i = 0; i < num; i++) {
        *report_d = *report_s;
        report_s--;
        report_d--;
    }
}
#endif

static void logic_cq_eliminate_holes(struct logic_cq_sub *sub_cq, u32 start, u32 report_cnt, u32 rollback)
{
#ifndef TSDRV_UT
    u32 num, src_pos, dst_pos;

    TSDRV_PRINT_DEBUG("Eliminate holes. (cqid=%u; start=%u; report_cnt=%u; rollback=%u; head=%u; tail=%u)\n",
        sub_cq->cqid, start, report_cnt, rollback, sub_cq->head, sub_cq->tail);

    if (rollback == 0) {
        num = start - sub_cq->head;
        src_pos = sub_cq->head;
    } else {
        num = start;
        src_pos = 0;
    }
    dst_pos = src_pos + report_cnt;
    if (sub_cq->type == NORMAL_SQCQ_TYPE) {
        logic_cq_move_ts_cqes(sub_cq, src_pos, dst_pos, num);
    } else {
        logic_cq_move_cqes(sub_cq, src_pos, dst_pos, num);
    }

    if (rollback == 0) {
        return;
    }

    /* move the cqes in que tail to head */
    num = ka_base_min(report_cnt, sub_cq->depth - sub_cq->head);
    src_pos = sub_cq->depth - num;
    dst_pos = 0;
    if (sub_cq->type == NORMAL_SQCQ_TYPE) {
        logic_cq_move_ts_cqes(sub_cq, src_pos, dst_pos, num);
    } else {
        logic_cq_move_cqes(sub_cq, src_pos, dst_pos, num);
    }

    /* move the cqes to tail */
    num = sub_cq->depth - sub_cq->head - num;
    if (num == 0) {
        return;
    }
    src_pos = sub_cq->head;
    dst_pos = sub_cq->depth - num;
    if (sub_cq->type == NORMAL_SQCQ_TYPE) {
        logic_cq_move_ts_cqes(sub_cq, src_pos, dst_pos, num);
    } else {
        logic_cq_move_cqes(sub_cq, src_pos, dst_pos, num);
    }
#endif
}

static int logic_cq_match_copy(struct logic_cq_sub *sub_cq, struct logic_cq_wait_cq_para *wait)
{
    u32 start, report_cnt, tail, rollback = 0;
    int ret;

    tail = sub_cq->tail; /* tail is update by tasklet, should save local */
    if (tail > sub_cq->head) {
        start = logic_cq_get_match_copy_range(sub_cq, wait, sub_cq->head, tail, &report_cnt);
    } else {
        start = logic_cq_get_match_copy_range(sub_cq, wait, sub_cq->head, sub_cq->depth, &report_cnt);
        if (report_cnt == 0) {
            rollback = 1;
            start = logic_cq_get_match_copy_range(sub_cq, wait, 0, tail, &report_cnt);
        }
    }

    if (report_cnt == 0) {
        return -EAGAIN;
    }

    ret = ka_base_copy_to_user((void __user *)wait->report_buf, sub_cq->vaddr + ((unsigned long)start * sub_cq->size),
        (unsigned long)report_cnt * sub_cq->size);
    if (ret != 0) {
        TSDRV_PRINT_ERR("copy to user fail, cqid=%u report_cnt=%u\n", sub_cq->cqid, report_cnt);
        return -ENODEV;
    }

    wait->report_cqe_num = report_cnt;

    if (sub_cq->head != start) {
        logic_cq_eliminate_holes(sub_cq, start, report_cnt, rollback);
    }

    return 0;
}

STATIC int logic_cq_copy_report(u32 devid, u32 fid, u32 tsid, struct logic_cq_wait_cq_para *wait,
    struct logic_sqcq_ts_ctx *logic_ctx)
{
    struct logic_cq_info *logic_cq = NULL;
    struct logic_cq_sub *sub_cq = NULL;
    bool is_logic_cq_full = false;
    int ret;

    logic_cq = logic_cq_info_get(devid, fid, tsid);
    sub_cq = logic_cq_sub_get(devid, fid, tsid, wait->cqid);
    if (sub_cq->useflag == LOGIC_CQ_IS_FREE) {
        TSDRV_PRINT_ERR("[cq_id=%u]:useflag is invalid.\n", sub_cq->cqid);
        return -EINVAL;
    }
    if (logic_cq_queue_full(sub_cq->head, sub_cq->tail, sub_cq->depth)) {
        is_logic_cq_full = true;
    }

    if (wait->version == 0) {
        ret = logic_cq_non_match_copy(sub_cq, wait);
    } else {
        ret = logic_cq_match_copy(sub_cq, wait);
    }

    if (ret != 0) {
        return ret;
    }

    ka_wmb();

    sub_cq->out_cnt += wait->report_cqe_num;
    sub_cq->head = (sub_cq->head + wait->report_cqe_num) % sub_cq->depth;
    TSDRV_PRINT_DEBUG("logic cq report copy to user, wait->cnt=%u cqid=%u tail=%u head=%u\n",
        wait->report_cqe_num, sub_cq->cqid, sub_cq->tail, sub_cq->head);

    if (is_logic_cq_full) {
        tsdrv_trigger_phy_cq_scan(devid, tsid, logic_ctx->phy_cq.index);
    }
    return 0;
}

STATIC int logic_cq_get_data(u32 devid, u32 fid, u32 tsid, struct logic_sqcq_ts_ctx *logic_ctx,
    struct logic_cq_wait_cq_para *wait)
{
    struct logic_cq_info *logic_cq = NULL;
    struct logic_cq_sub *sub_cq = NULL;
    int ret;

    logic_cq = logic_cq_info_get(devid, fid, tsid);
    if (logic_cq == NULL) {
        return -ENOMEM;
    }
    ka_task_spin_lock_bh(&logic_cq->lock);
    sub_cq = logic_cq_sub_get(devid, fid, tsid, wait->cqid);
    if (sub_cq == NULL) {
        ka_task_spin_unlock_bh(&logic_cq->lock);
        TSDRV_PRINT_ERR("get sub cq fail, devid=%u fid=%u tsid=%u\n", devid, fid, tsid);
        return -EINVAL;
    }
    if (sub_cq->useflag != LOGIC_CQ_IN_USE) {
        ka_task_spin_unlock_bh(&logic_cq->lock);
        TSDRV_PRINT_ERR("invalid userflag=%d, devid=%u fid=%u tsid=%u logic_cqid=%u\n", sub_cq->useflag,
            devid, fid, tsid, sub_cq->cqid);
        return -EFAULT;
    }

    /* thread bind irq, ts not send cqe */
    if (sub_cq->thread_bind_irq >= 0) {
        if (ka_base_atomic_read(&logic_ctx->wakeup_num[wait->cqid]) > 0) {
            ka_base_atomic_set(&logic_ctx->wakeup_num[wait->cqid], 0);
            ka_task_spin_unlock_bh(&logic_cq->lock);
            TSDRV_PRINT_DEBUG("Wake thread_bind_irq. (logic_cqid=%u; current_pid=%d)\n", wait->cqid, current->pid);
            return 0;
        }
        ka_task_spin_unlock_bh(&logic_cq->lock);
        return -EAGAIN;
    }

    if (unlikely(sub_cq->vpid != logic_ctx->tgid)) {
        ka_task_spin_unlock_bh(&logic_cq->lock);
        TSDRV_PRINT_ERR("logic_cq=%u is not belong to this process, sub_cq->vpid=%u, devid=%u fid=%u tsid=%u\n",
            sub_cq->cqid, sub_cq->vpid, devid, fid, tsid);
        return -EFAULT;
    }
    if (unlikely((wait->cqe_num == 0) || (wait->cqe_num > sub_cq->depth))) {
        ka_task_spin_unlock_bh(&logic_cq->lock);
        TSDRV_PRINT_ERR("invalid report buf len(%u %u), devid=%u fid=%u tsid=%u\n", wait->cqe_num, sub_cq->size,
            devid, fid, tsid);
        return -EINVAL;
    }
    ka_task_spin_unlock_bh(&logic_cq->lock);

    ka_task_mutex_lock(&sub_cq->mutex);
    if (sub_cq->head == sub_cq->tail) {
#ifdef AOS_LLVM_BUILD
        ka_base_atomic_set(&logic_ctx->wakeup_num[wait->cqid], 0);
#endif
        ka_task_mutex_unlock(&sub_cq->mutex);
        TSDRV_PRINT_DEBUG("logic_cq=%u has no report prepare to wait, tail=%u head=%u, devid=%u fid=%u tsid=%u\n",
            sub_cq->cqid, sub_cq->tail, sub_cq->head, devid, fid, tsid);
        return -EAGAIN;
    }

    ret = logic_cq_copy_report(devid, fid, tsid, wait, logic_ctx);
    ka_task_mutex_unlock(&sub_cq->mutex);

    return ret;
}

STATIC int logic_ioctl_wait_param_chk(u32 devid, u32 tsid, struct logic_cq_wait_cq_para *wait)
{
    if (wait->cqid >= TSDRV_MAX_LOGIC_CQ_NUM) {
        TSDRV_PRINT_ERR("invlid logic cqid=%u, devid=%u tsid=%u\n", wait->cqid, devid, tsid);
        return -EINVAL;
    }
    if ((wait->timeout < 0)  && (wait->timeout != -1)) {
        TSDRV_PRINT_ERR("invalid timeout=%d(s), devid=%u tsid=%u\n", wait->timeout, devid, tsid);
        return -ENODEV;
    }
    if (wait->report_buf == NULL) {
        TSDRV_PRINT_ERR("wait->report_buf is NULL, devid=%u tsid=%u\n", devid, tsid);
        return -ENOMEM;
    }

    if ((wait->version != 0) && (wait->version != 1)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_WARN("version not support, devid=%u tsid=%u version=%u\n", devid, tsid, wait->version);
#endif
        return -EINVAL;
    }

    return 0;
}

STATIC int logic_cq_wait_event(struct logic_sqcq_ts_ctx *logic_ctx, struct logic_cq_sub *sub_cq, u32 cqid, int timeout)
{
#ifndef TSDRV_UT
#ifndef AOS_LLVM_BUILD
    KA_TASK_DEFINE_WAIT(wq_entry);
    long ret, tm;

    if (timeout == 0) {
        TSDRV_PRINT_DEBUG("Get report without wait irq.\n");
        return 0;
    }

    ka_base_atomic_inc(&logic_ctx->wait_thread_num[cqid]);
    TSDRV_PRINT_DEBUG("Wake wait start. (logic_cqid=%u; timeout=%dms; wait_thread_num=%d)\n",
        cqid, timeout, ka_base_atomic_read(&logic_ctx->wait_thread_num[cqid]));

    tm = (timeout == -1) ? KA_TASK_MAX_SCHEDULE_TIMEOUT : ka_system_msecs_to_jiffies((u32)timeout);
    ka_task_prepare_to_wait_exclusive(&logic_ctx->logic_cq_wait[cqid], &wq_entry, KA_TASK_INTERRUPTIBLE);
    if (ka_base_atomic_read(&logic_ctx->wakeup_num[cqid]) > 0) {
        if (sub_cq->thread_bind_irq < 0) {
            ka_base_atomic_dec(&logic_ctx->wakeup_num[cqid]);
        }
        ret = tm;
    } else {
        ret = ka_system_schedule_timeout(tm);
    }
    if (ka_task_signal_pending_state(KA_TASK_INTERRUPTIBLE, current)) {
        ret = -ERESTARTSYS;
    }
    ka_task_finish_wait(&logic_ctx->logic_cq_wait[cqid], &wq_entry);

    ka_base_atomic_dec(&logic_ctx->wait_thread_num[cqid]);
    TSDRV_PRINT_DEBUG("Wake wait finish. (logic_cqid=%u; timeout=%dms; wait_thread_num=%d; ret=%ld)\n",
        cqid, timeout, ka_base_atomic_read(&logic_ctx->wait_thread_num[cqid]), ret);
#else
    unsigned long wait_time;
    long ret;

    if (timeout == -1) {
        ret = (long)ka_task_wait_event_interruptible(logic_ctx->logic_cq_wait[cqid],
                                             ka_base_atomic_read(&logic_ctx->wakeup_num[cqid]) > 0);
        if (ret == -ERESTARTSYS) {
            TSDRV_PRINT_WARN("wait event interrupted, cqid=%u\n", cqid);
        }
        return ret;
    }
    wait_time = ka_system_msecs_to_jiffies((u32)timeout);
    ret = ka_task_wait_event_interruptible_timeout(logic_ctx->logic_cq_wait[cqid],
                                           ka_base_atomic_read(&logic_ctx->wakeup_num[cqid]) > 0,
                                           wait_time);
#endif
    if (ret == 0) {
        /* wait event timeout */
        return -ETIMEDOUT;
    } else if (ret > 0) {
        /* wait event is awakened */
        return 0;
    }
    if (ret == -ERESTARTSYS) {
        TSDRV_PRINT_WARN("wait event interrupted, cqid=%u\n", cqid);
    }
    return ret;
#else
    if (timeout == 0) {
        TSDRV_PRINT_DEBUG("Get report without wait irq.\n");
        return 0;
    }
#endif
}

static struct tsdrv_wait_tid_info *tsdrv_stream_ctx_has_set_tid(struct tsdrv_ts_resource *ts_res, u32 stream_id)
{
    struct tsdrv_wait_tid_info *tid_info = NULL;
    struct tsdrv_wait_tid_info *tmp = NULL;

    ka_task_spin_lock_bh(&ts_res->stream_info[stream_id].lock);
    ka_list_for_each_entry_safe(tid_info, tmp, &ts_res->stream_info[stream_id].head, node) {
        if (tid_info->os_tid == current->pid) {
            ka_task_spin_unlock_bh(&ts_res->stream_info[stream_id].lock);
            return tid_info;
        }
    }
    ka_task_spin_unlock_bh(&ts_res->stream_info[stream_id].lock);
    return NULL;
}

typedef int (*tsdrv_event_submit)(u32 chip_id, struct sched_published_event *event);
typedef int (*tsdrv_query_esched_tid)(u32 chip_id, int pid, u32 gid, u32 os_tid, u32 *tid);

#ifndef TSDRV_UT
#ifndef CFG_TSDRV_VM_ENV
static void logic_cq_event_pack(ka_pid_t pid, u32 grp_id, ka_pid_t tid, u32 event_id, struct sched_published_event *event)
{
    event->event_info.dst_engine = ACPU_DEVICE;
    event->event_info.policy = ONLY;
    event->event_info.pid = pid;
    event->event_info.gid = grp_id;
    event->event_info.tid = (u32)tid;
    event->event_info.event_id = event_id;
    event->event_info.subevent_id = 0;

    event->event_func.event_ack_func = NULL;
    event->event_func.event_finish_func = NULL;

    event->event_info.msg = NULL;
    event->event_info.msg_len = 0;
}

extern int sched_query_tid_in_grp(u32 chip_id, int pid, u32 gid, u32 os_tid, u32 *tid);
static int logic_cq_query_esched_tid(u32 devid, int pid, u32 gid, u32 os_tid, u32 *tid)
{
    return sched_query_tid_in_grp(devid, pid, gid, os_tid, tid);
}

extern int32_t sched_submit_event_to_thread(uint32_t chip_id, struct sched_published_event *event);
static int logic_cq_event_submit(u32 devid, struct sched_published_event *event)
{
    return sched_submit_event_to_thread(devid, event);
}
#endif
#endif

#ifndef TSDRV_UT
static bool tsdrv_tid_is_not_exist(u32 devid, u32 grp_id, struct tsdrv_wait_tid_info *tid_info)
{
#ifndef CFG_TSDRV_VM_ENV
    int ret;
    u32 tid;

    ret = logic_cq_query_esched_tid(devid, tid_info->pid, grp_id, (u32)tid_info->os_tid, &tid);
    if (ret != 0) {
        return true;
    }
#endif
    return false;
}

static int tsdrv_find_and_release_unuse_tid(struct tsdrv_ts_resource *ts_res, u32 devid, u32 idx)
{
    struct tsdrv_wait_tid_info *tid_info = NULL;
    struct tsdrv_wait_tid_info *tmp = NULL;
    u32 grp_id = ts_res->stream_info[idx].grp_id;

    spin_lock_bh(&ts_res->stream_info[idx].lock);
    ka_list_for_each_entry_safe(tid_info, tmp, &ts_res->stream_info[idx].head, node) {
        if (tsdrv_tid_is_not_exist(devid, grp_id, tid_info)) {
            list_del(&tid_info->node);
            tsdrv_kfree(tid_info);
            ts_res->stream_info[idx].list_num--;
        }
    }
    spin_unlock_bh(&ts_res->stream_info[idx].lock);
    if (ts_res->stream_info[idx].list_num >= TSDRV_MAX_THREAD_NUM_IN_GRP) {
        return -ENOSPC;
    }
    return 0;
}
#endif

static int tsdrv_set_stream_ctx_tid(struct tsdrv_ts_resource *ts_res, u32 stream_id)
{
    struct tsdrv_wait_tid_info *tid_info = NULL;
    struct tsdrv_id_inst id_inst;
    u32 devid = tsdrv_get_devid_by_res(ts_res);
    u32 fid = tsdrv_get_fid_by_res(ts_res);
    int ret;

    tsdrv_pack_id_inst(devid, fid, ts_res->tsid, &id_inst);
    ret = tsdrv_is_id_belong_to_proc(&id_inst, current->tgid, stream_id, TSDRV_STREAM_ID);
    if (ret != 0) {
        return -EINVAL;
    }

    if (ts_res->stream_info[stream_id].event_enable == 0) {
        TSDRV_PRINT_DEBUG("No stream enable event. (devid=%u; tsid=%u; streamId=%u)\n", devid, ts_res->tsid, stream_id);
        return -EPERM;
    } else {
        tid_info = tsdrv_stream_ctx_has_set_tid(ts_res, stream_id);
        if (tid_info != NULL) {
            return 0;
        }
#ifndef TSDRV_UT
        if (ts_res->stream_info[stream_id].list_num >= TSDRV_MAX_THREAD_NUM_IN_GRP) {
            ret = tsdrv_find_and_release_unuse_tid(ts_res, devid, stream_id);
            if (ret != 0) {
                TSDRV_PRINT_ERR("Free tid node failed. (devid=%u; tsid=%u; streamId=%u)\n",
                    devid, ts_res->tsid, stream_id);
                return ret;
            }
        }
#endif
        tid_info = tsdrv_kzalloc(sizeof(struct tsdrv_wait_tid_info), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
        if (tid_info == NULL) {
            TSDRV_PRINT_ERR("Malloc tid node failed. (devid=%u; tsid=%u; streamId=%u)\n",
                devid, ts_res->tsid, stream_id);
            return -ENOMEM;
        }
    }

    ka_task_spin_lock_bh(&ts_res->stream_info[stream_id].lock);
    if (ts_res->stream_info[stream_id].event_enable == 0) {
        ka_task_spin_unlock_bh(&ts_res->stream_info[stream_id].lock);
        tsdrv_kfree(tid_info);
        TSDRV_PRINT_WARN("The stream has been deleted. (devid=%u; tsid=%u; streamId=%u)\n",
            devid, ts_res->tsid, stream_id);
        return -EACCES;
    }

    tid_info->os_tid = current->pid;
    tid_info->pid = current->tgid;
    ka_list_add(&tid_info->node, &ts_res->stream_info[stream_id].head);
    ts_res->stream_info[stream_id].list_num++;
    ka_task_spin_unlock_bh(&ts_res->stream_info[stream_id].lock);

    return 0;
}

void logic_cq_send_event(struct tsdrv_ts_resource *ts_res, u32 stream_id)
{
    struct tsdrv_wait_tid_info *tid_info = NULL;
    struct tsdrv_wait_tid_info *tmp = NULL;
    u32 grp_id, event_id;
#ifndef CFG_TSDRV_VM_ENV
    struct sched_published_event event;
    u32 devid = tsdrv_get_devid_by_res(ts_res);
    u32 tid;
    int ret;
#endif

    if (stream_id >= DEVDRV_MAX_STREAM_ID) {
        return;
    }

    if (ts_res->stream_info[stream_id].event_enable == 0) {
        return;
    }

    ka_task_spin_lock_bh(&ts_res->stream_info[stream_id].lock);
    if (ts_res->stream_info[stream_id].event_enable == 0) {
        ka_task_spin_unlock_bh(&ts_res->stream_info[stream_id].lock);
        return;
    }

    event_id = ts_res->stream_info[stream_id].event_id;
    grp_id = ts_res->stream_info[stream_id].grp_id;
#ifndef TSDRV_UT
#ifndef CFG_TSDRV_VM_ENV
    ka_list_for_each_entry(tid_info, &ts_res->stream_info[stream_id].head, node) {
        ret = logic_cq_query_esched_tid(devid, tid_info->pid, grp_id, (u32)tid_info->os_tid, &tid);
        if (ret != 0) {
            continue;
        }

        logic_cq_event_pack(tid_info->pid, grp_id, (ka_pid_t)tid, event_id, &event);
        (void)logic_cq_event_submit(devid, &event);
    }
#endif
#endif
    ka_list_for_each_entry_safe(tid_info, tmp, &ts_res->stream_info[stream_id].head, node) {
        ka_list_del(&tid_info->node);
        tsdrv_kfree(tid_info);
        ts_res->stream_info[stream_id].list_num--;
    }

    ka_task_spin_unlock_bh(&ts_res->stream_info[stream_id].lock);
}

#define TSDRV_DFX_MAX_LOGIC_CQE 5U
static void tsdrv_check_logic_cq_report(u32 head, u32 tail, u32 *cqe_count, struct logic_cq_sub *sub_cq,
    struct logic_cq_wait_cq_para *wait)
{
#ifndef TSDRV_UT
    struct tag_ts_logic_cq_report_msg *report = NULL;
    u32 i, offset;

    *cqe_count = 0;

    for (i = head; i < tail; i++) {
        offset = i * sub_cq->size;
        report = (struct tag_ts_logic_cq_report_msg *)(uintptr_t)(sub_cq->vaddr + offset);
        if ((report->match_flag == 1) &&
            (report->stream_id == wait->stream_id) &&
            (report->task_id == wait->task_id)) {
            TSDRV_PRINT_INFO("Find logic cqe. (cqe_idx=%u; logic_cq_id=%u; stream_id=%u; task_id=%u; match_flag=%u)\n",
                i, report->logic_cq_id, report->stream_id, report->task_id, report->match_flag);
            continue;
        }

        if ((*cqe_count) < TSDRV_DFX_MAX_LOGIC_CQE) {
            TSDRV_PRINT_INFO("Report show. (cqe_idx=%u; logic_cq_id=%u; stream_id=%u; task_id=%u; match_flag=%u)\n",
                i, report->logic_cq_id, report->stream_id, report->task_id, report->match_flag);
            (*cqe_count)++;
        }
    }
#endif
}

static void tsdrv_show_logic_cq_info(u32 devid, u32 fid, u32 tsid, u32 logic_cqid, struct logic_cq_wait_cq_para *wait)
{
    struct logic_cq_info *logic_cq = NULL;
    struct logic_cq_sub *sub_cq = NULL;
    u32 tail, cqe_count;

    logic_cq = logic_cq_info_get(devid, fid, tsid);
    if (logic_cq == NULL) {
        return;
    }

    ka_task_spin_lock_bh(&logic_cq->lock);
    sub_cq = logic_cq_sub_get(devid, fid, tsid, logic_cqid);
    if (sub_cq == NULL) {
#ifndef TSDRV_UT
        ka_task_spin_unlock_bh(&logic_cq->lock);
        TSDRV_PRINT_WARN("sub_cq is null. (devid=%u; fid=%u; tsid=%u; logic_cq_id=%u)\n",
            devid, fid, tsid, logic_cqid);
        return;
#endif
    }

    if (sub_cq->useflag != LOGIC_CQ_IN_USE) {
#ifndef TSDRV_UT
        ka_task_spin_unlock_bh(&logic_cq->lock);
        TSDRV_PRINT_WARN("Invalid flag. (devid=%u; fid=%u; tsid=%u; logic_cq_id=%u; userflag=%d)\n",
            devid, fid, tsid, sub_cq->cqid, sub_cq->useflag);
        return;
#endif
    }
    tail = sub_cq->tail;
    ka_task_spin_unlock_bh(&logic_cq->lock);

    ka_task_mutex_lock(&sub_cq->mutex);
    if (sub_cq->head == tail) {
        ka_task_mutex_unlock(&sub_cq->mutex);
        TSDRV_PRINT_INFO_LIMIT("Logic cq is empty. (logic_cq_id=%u; head=%u)\n", sub_cq->cqid, sub_cq->head);
        return;
    }

#ifndef TSDRV_UT
    if (sub_cq->head < tail) {
        tsdrv_check_logic_cq_report(sub_cq->head, tail, &cqe_count, sub_cq, wait);
    } else {
        tsdrv_check_logic_cq_report(sub_cq->head, sub_cq->depth, &cqe_count, sub_cq, wait);
        tsdrv_check_logic_cq_report(0, tail, &cqe_count, sub_cq, wait);
    }

    ka_task_mutex_unlock(&sub_cq->mutex);
#endif
}

static void tsdrv_logic_cq_wait_dfx_proc(struct tsdrv_ctx *ctx, u32 tsid, struct logic_cq_wait_cq_para *wait, int err,
    u64 logic_wait_start_tick)
{
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    u32 fid = tsdrv_get_fid_by_ctx(ctx);
    u32 logic_cq_id = wait->cqid;
    u64 wait_end_tick = tsdrv_get_cur_cpu_tick();

    /* version 0 means irqwait and runtime monitor will use and would timeout, so not show dfx info. */
    if (((wait->version == 1) && (!tsdrv_sqcq_dfx_ratelimited())) || (tsdrv_sqcq_dfx_is_enabled(devid, fid))) {
        TSDRV_PRINT_WARN_LIMIT("Wait logic cq result show. (devid=%u; fid=%u; tsid=%u; logic_cq_id=%u; "
            "version=%u; cqe_num=%u; timeout=%d; streamid=%u; taskid=%u; err=%d; "
            "logic_wait_start_tick=%llu; wait_end_tick=%llu)\n",
            devid, fid, tsid, logic_cq_id, wait->version, wait->cqe_num, wait->timeout, wait->stream_id, wait->task_id,
            err, logic_wait_start_tick, wait_end_tick);
            tsdrv_show_logic_cq_info(devid, fid, tsid, logic_cq_id, wait);
            tsdrv_sqcq_dfx_show(ctx, tsid, logic_cq_id);
        return;
    }

    if (err == -ERESTARTSYS) {
        TSDRV_PRINT_DEBUG("Wait event interrupted. (tsid=%u; logic_cq_id=%u; version=%u; cqe_num=%u; timeout=%dms; "
            "err=%d; logic_wait_start_tick=%llu; wait_end_tick=%llu)\n",
            tsid, logic_cq_id, wait->version, wait->cqe_num, wait->timeout, err, logic_wait_start_tick, wait_end_tick);
    }
}

STATIC int logic_wait_cq_report(u32 devid, u32 fid, u32 tsid, struct tsdrv_ctx *ctx, struct logic_cq_wait_cq_para *wait)
{
    struct logic_sqcq_ts_ctx *logic_ctx = NULL;
    struct logic_cq_sub *sub_cq = NULL;
    u64 logic_wait_start_tick = tsdrv_get_cur_cpu_tick();
    int err;

    logic_ctx = (struct logic_sqcq_ts_ctx *)ctx->ts_ctx[tsid].logic_sqcq_ctx;
    if (logic_ctx == NULL) {
        TSDRV_PRINT_ERR("logic ctx is NULL, (devid=%u, tsid=%u)\n", devid, tsid);
        return -EFAULT;
    }

    sub_cq = logic_cq_sub_get(devid, fid, tsid, wait->cqid);
    if (sub_cq == NULL) {
        TSDRV_PRINT_ERR("Invalid para. (devid=%u; fid=%u; tsid=%u; cqid=%u)\n", devid, fid, tsid, wait->cqid);
        return -EINVAL;
    }

    TSDRV_PRINT_DEBUG("wait logic_cqid=%u, devid=%u fid=%u tsid=%u\n", wait->cqid, devid, fid, tsid);
    err = logic_cq_get_data(devid, fid, tsid, logic_ctx, wait);
    if (err != 0) {
        if ((err != -EAGAIN) && (err != 0)) {
            TSDRV_PRINT_ERR("get logic cq data fail before wait, devid=%u fid=%u tsid=%u cqid=%u\n",
                devid, fid, tsid, wait->cqid);
            return err;
        }
        TSDRV_PRINT_DEBUG("start to wait, timeout=%d(s) devid=%u fid=%u tsid=%u cqid=%u\n", wait->timeout, devid,
            fid, tsid, wait->cqid);

retry:
        err = logic_cq_wait_event(logic_ctx, sub_cq, wait->cqid, wait->timeout);
        if ((err != 0) || tsdrv_sqcq_dfx_is_enabled(devid, fid)) {
            TSDRV_PRINT_DEBUG("wait event time out, err=%d devid=%u fid=%u tsid=%u cqid=%u\n", err, devid,
                fid, tsid, wait->cqid);
            tsdrv_logic_cq_wait_dfx_proc(ctx, tsid, wait, err, logic_wait_start_tick);
            return err;
        }

        if (wait->timeout == 0) {
            struct tsdrv_ts_resource *ts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
            wait->report_cqe_num = 0;
            err = tsdrv_set_stream_ctx_tid(ts_res, wait->stream_id);
            if (err != 0) {
                return 0;
            }
            /* set tid maybe behind submit event */
            ka_task_mutex_lock(&sub_cq->mutex);
            if (sub_cq->head != sub_cq->tail) {
                logic_cq_send_event(ts_res, wait->stream_id);
            }
            ka_task_mutex_unlock(&sub_cq->mutex);
            return 0;
        }

        TSDRV_PRINT_DEBUG("wait succeed, copy data again, cqid=%u devid=%u fid=%u tsid=%u\n",
            wait->cqid, devid, fid, tsid);
        err = logic_cq_get_data(devid, fid, tsid, logic_ctx, wait);
        if (err != 0) {
            if (err == -EAGAIN) {
                goto retry;
            }

            TSDRV_PRINT_ERR("get logic cq data fail after wait, err=%d devid=%u fid=%u tsid=%u cqid=%u\n", err,
                devid, fid, tsid, wait->cqid);
            return -ETIMEDOUT;
        }
    }
    return err;
}

int logic_ioctl_cq_wait(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    struct logic_cq_wait_cq_para *wait = &arg->logic_cq_wait;
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    u32 fid = tsdrv_get_fid_by_ctx(ctx);
    u32 tsid = arg->tsid;
    int err;

    TSDRV_PRINT_DEBUG("Recv logic cq. (devid=%u; tsid=%u; tid=%d, logic_cqid=%u; timeout=%dms; version=%u; "
        "stream_id=%u; task_id=%u; cqe_num=%u\n", devid, tsid, current->pid, wait->cqid, wait->timeout,
        wait->version, wait->stream_id, wait->task_id, wait->cqe_num);

    err = logic_ioctl_wait_param_chk(devid, tsid, wait);
    if (err != 0) {
        return err;
    }

    err = logic_wait_cq_report(devid, fid, tsid, ctx, wait);
    if (tsdrv_mirror_ctx_abnormal(ctx)) {
        TSDRV_PRINT_ERR("mirror process hang, devid=%u fid=%u tgid=%d pid=%d\n", devid, fid, ctx->tgid, ctx->pid);
        err = (int)DEDVRV_DEV_PROCESS_HANG;
    } else if (tsdrv_get_mirror_ctx_status(ctx) == DEVDRV_STATUS_HDC_CLOSE_FLAG) {
        TSDRV_PRINT_ERR("hdc connect down, devid(%u) fid(%u) tsid(%u)\n", devid, fid, tsid);
        err = DEVDRV_HDC_CONNECT_DOWN;
    } else if (tsdrv_get_ts_status(devid, tsid) != TS_WORK) {
        TSDRV_PRINT_ERR("ts is down, devid(%u) fid(%u) tsid(%u)\n", devid, fid, tsid);
        err = DEVDRV_BUS_DOWN;
    }
    arg->logic_cq_wait.timeout = err;

    return 0;
}

void logic_thread_bind_irq_init(struct tsdrv_ctx *ctx, u32 tsid,
    struct logic_sqcq_alloc_para *para, struct logic_cq_alloc_para *alloc)
{
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    u32 fid = tsdrv_get_fid_by_ctx(ctx);
    struct logic_cq_sub *sub_cq = logic_cq_sub_get(devid, fid, tsid, alloc->cq_id);

    alloc->thread_bind_irq = -1;
    sub_cq->thread_bind_irq = -1;

    if ((para->flag & TSDRV_FLAG_THREAD_BIND_IRQ) != 0) {
        if (!tsdrv_is_in_pm(devid)) {
            para->flag &= ~TSDRV_FLAG_THREAD_BIND_IRQ;
            TSDRV_PRINT_INFO("Computing group not support fast cq. (devid=%u)\n", devid);
            return;
        }
        /* thread bind irq num in proc needn't be precise, so not use lock  */
        if (ctx->thread_bind_irq_num < TSDRV_PROC_MAX_THREAD_BIND_IRQ_NUM) {
            alloc->thread_bind_irq = tsdrv_thread_bind_irq_alloc(devid, tsid);
        }

        if (alloc->thread_bind_irq < 0) {
            para->flag &= ~TSDRV_FLAG_THREAD_BIND_IRQ;
        } else {
#ifndef TSDRV_UT
            struct logic_sqcq_ts_ctx *logic_ctx = (struct logic_sqcq_ts_ctx *)ctx->ts_ctx[tsid].logic_sqcq_ctx;
            ka_base_atomic_set(&logic_ctx->wakeup_num[alloc->cq_id], 0);
            tsdrv_thread_bind_irq_set_wait_para(devid, tsid, alloc->thread_bind_irq,
                &logic_ctx->logic_cq_wait[alloc->cq_id], &logic_ctx->wakeup_num[alloc->cq_id]);
            sub_cq->thread_bind_irq = alloc->thread_bind_irq;
            ctx->thread_bind_irq_num++;
#endif
        }
    }
}

void logic_thread_bind_irq_uninit(struct tsdrv_ctx *ctx, u32 tsid, u32 cq_id)
{
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    u32 fid = tsdrv_get_fid_by_ctx(ctx);
    struct logic_cq_sub *sub_cq = logic_cq_sub_get(devid, fid, tsid, cq_id);

    if (sub_cq->thread_bind_irq >= 0) {
        ctx->thread_bind_irq_num--;
        tsdrv_thread_bind_irq_free(devid, tsid, sub_cq->thread_bind_irq);
    }
}

int logic_ioctl_cq_alloc(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    struct logic_sqcq_alloc_para *para = &arg->logic_sqcq_alloc_para;
    struct logic_cq_alloc_para alloc_logic_cq;
    struct logic_sqcq_ts_ctx *logic_ctx = NULL;
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    struct logic_cq_info *logic_cq = NULL;
    u32 fid = tsdrv_get_fid_by_ctx(ctx);
    struct logic_cqsq_mbox_msg mbox_msg;
    u32 tsid = arg->tsid;
#if !defined(CFG_SOC_PLATFORM_MDC_V51) && !defined(CFG_TSDRV_VM_ENV)
    struct hvtsdrv_trans_mailbox_ctx trans_mbox;
    u32 disable_thread;
#endif
    int err;

    err = logic_ioctl_cq_alloc_param_chk(para);
    if (err != 0) {
        return err;
    }
    logic_ctx = (struct logic_sqcq_ts_ctx *)ctx->ts_ctx[tsid].logic_sqcq_ctx;
#ifndef TSDRV_UT
    if (logic_ctx == NULL) {
        TSDRV_PRINT_ERR("logic ctx is NULL, devid=%u fid=%u tsid=%u tgid=%d\n", devid, fid, tsid, ctx->tgid);
        return -ENODEV;
    }
#endif
#ifdef CFG_SOC_PLATFORM_MDC_V51
    if (logic_ctx->logic_cq_num >= tsdrv_get_logic_cq_id_max_num()) {
        TSDRV_PRINT_ERR("alloc logic_cq fail, tsid(%d) current num(%d) max num(%d)\n",
            tsid, logic_ctx->logic_cq_num, tsdrv_get_logic_cq_id_max_num());
        return -ENOMEM;
    }
#endif
    logic_cq = logic_cq_info_get(devid, fid, tsid);
    if (logic_cq == NULL) {
        return -ENOMEM;
    }
    ka_task_mutex_lock(&logic_ctx->lock);
    err = logic_sqcq_phy_cq_alloc(ctx, tsid, logic_ctx, para);
    if (err != 0) {
        ka_task_mutex_unlock(&logic_ctx->lock);
        return err;
    }
    alloc_logic_cq.cq_size = para->cq_size;
    alloc_logic_cq.cq_depth = para->cq_depth;
    alloc_logic_cq.type = NORMAL_SQCQ_TYPE;
    if (para->info[2] == U16_MAX) { /* info[2] means alloc logic cq for ctrl sqcqs. */
        alloc_logic_cq.type = CTRL_SQCQ_TYPE;
    }
    err = logic_cq_alloc(logic_cq, logic_ctx, &alloc_logic_cq);
    if (err != 0) {
#ifndef TSDRV_UT
        goto err_logic_cq_alloc;
#endif
    }
    logic_thread_bind_irq_init(ctx, tsid, para, &alloc_logic_cq);

    if (alloc_logic_cq.type == NORMAL_SQCQ_TYPE) {
        err = logic_sqcq_mbox_alloc_msg_pack(&mbox_msg, logic_ctx, &alloc_logic_cq, para, fid);
        if (err != 0) {
#ifndef TSDRV_UT
            goto err_mbox_send;
#endif
        }

#if !defined(CFG_SOC_PLATFORM_MDC_V51) && !defined(CFG_TSDRV_VM_ENV)
        if (tsdrv_is_in_container(devid)) {
            err = hvtsdrv_logic_refresh_alloc_msg(devid, tsid, fid, &mbox_msg);
            if (err != 0) {
                TSDRV_PRINT_ERR("Failed to refresh logic alloc message. (devid=%u; fid=%u; tsid=%u)", devid, fid, tsid);
#ifndef TSDRV_UT
                goto err_mbox_send;
#endif
            }
        }

        disable_thread = (tsdrv_ctx_is_nothread_type(ctx) == true) ? 1 : 0;
        hvtsdrv_fill_trans_info_mbox(devid, fid, tsid, disable_thread, &trans_mbox);
        err = hvtsdrv_trans_info_msg(&trans_mbox, (void *)(&mbox_msg), sizeof(struct logic_cqsq_mbox_msg));
        if (err != 0) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("Failed to translate logic cq info. (ret=%d; devid=%u; fid=%u; tsid=%u)\n",
                err, devid, fid, tsid);
            goto err_mbox_send;
#endif
        }
#endif

        err = logic_sqcq_mbox_send(devid, tsid, (unsigned char *)&mbox_msg);
        if (err != 0) {
#ifndef TSDRV_UT
            goto err_mbox_send;
#endif
        }
    }

    ka_task_mutex_unlock(&logic_ctx->lock);
    para->cq_id = alloc_logic_cq.cq_id;
    TSDRV_PRINT_DEBUG("Alloc logic cq. (logic_cqid=%u; logic_cq_type= %u; devid=%u; tsid=%u; tgid=(%d, %d)\n",
        alloc_logic_cq.cq_id, alloc_logic_cq.type, devid, tsid, logic_ctx->tgid, ctx->tgid);
    return 0;
#ifndef TSDRV_UT
err_mbox_send:
    if (mbox_msg.alloc.phy_cq_addr != 0) {
        logic_ctx->phy_cq_addr_set_once_flag = 0;
    }
    logic_thread_bind_irq_uninit(ctx, tsid, alloc_logic_cq.cq_id);
    err = logic_cq_free(ctx, tsid, alloc_logic_cq.cq_id);
    if (err != 0) {
        TSDRV_PRINT_INFO("logic_cq_free unable to do, err=%d\n", err);
    }
err_logic_cq_alloc:
    (void)logic_sqcq_phy_cq_free(ctx, tsid, logic_ctx, alloc_logic_cq.type);
    ka_task_mutex_unlock(&logic_ctx->lock);
    return -ENODEV;
#endif
}

STATIC void logic_cq_abandon(struct tsdrv_ctx *ctx, u32 tsid, struct logic_sqcq_ts_ctx *logic_ctx,
    u32 logic_cqid, u32 type)
{
#ifndef TSDRV_UT
    struct logic_cq_info *logic_cq = NULL;
    struct logic_cq_sub *sub_cq = NULL;
    struct tsdrv_id_inst id_inst;
    u32 devid, fid, phy_cqid;
    int err;

    devid = tsdrv_get_devid_by_ctx(ctx);
    fid = tsdrv_get_fid_by_ctx(ctx);
    logic_cq = logic_cq_info_get(devid, fid, tsid);
    if (logic_cq == NULL) {
        TSDRV_PRINT_ERR("logic cq isn't initiated. devid=%u fid=%u tsid=%u\n", devid, fid, tsid);
        return;
    }

    if (tsdrv_is_in_container(devid)) {
        struct tsdrv_ts_resource *ts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
        phy_cqid = ts_res->logic_phy_cq;
    } else {
        phy_cqid = logic_ctx->phy_cq.index;
    }

    tsdrv_pack_id_inst(devid, fid, tsid, &id_inst);
    err = logic_cq_phy_cq_clean(&id_inst, phy_cqid, logic_cqid, type);
    if (err != 0) {
        TSDRV_PRINT_ERR("Failed to clean phy cq. (logic_cq=%u; phy_cq=%u; devid=%u; fid=%u; tsid=%u)\n",
            logic_cqid, phy_cqid, devid, fid, tsid);
    }

    sub_cq = logic_cqid_exit(logic_cq, logic_ctx, logic_cqid);
    if (sub_cq == NULL) {
        TSDRV_PRINT_ERR("cqid=%u isn't allocated by tgid=%d devid=%u fid=%u tsid=%u\n",
            logic_cqid, logic_ctx->tgid, devid, fid, tsid);
    } else {
        sub_cq->useflag = LOGIC_CQ_ABANDONED;
    }
    TSDRV_PRINT_DEBUG("abandon logic cqid=%u, devid=%u tsid=%u\n", logic_cqid, devid, tsid);
#endif
}

STATIC int logic_cq_free_one_logic_cq(struct tsdrv_ctx *ctx, u32 tsid, struct logic_sqcq_ts_ctx *logic_ctx,
    u32 logic_cqid)
{
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    struct logic_cqsq_mbox_msg mbox_msg;
    u32 fid = tsdrv_get_fid_by_ctx(ctx);
    struct logic_cq_info *logic_cq = logic_cq_info_get(devid, fid, tsid);
    struct logic_cq_sub *sub_cq = NULL;
    struct tsdrv_id_inst id_inst;
    u32 phy_cqid;
    int err;

    sub_cq = &logic_cq->sub_cq[logic_cqid];
    if (sub_cq->type == NORMAL_SQCQ_TYPE) {
        logic_sqcq_mbox_free_msg_pack(&mbox_msg, logic_ctx, fid, logic_cqid, sub_cq->type);
#if !defined(CFG_SOC_PLATFORM_MDC_V51) && !defined(CFG_TSDRV_VM_ENV)
        if (tsdrv_is_in_container(devid)) {
            hvtsdrv_logic_refresh_free_msg(devid, tsid, fid, &mbox_msg);
        }
#endif
        err = logic_sqcq_mbox_send(devid, tsid, (unsigned char *)&mbox_msg);
        if (err != 0) {
            logic_cq_abandon(ctx, tsid, logic_ctx, logic_cqid, sub_cq->type);
            return -EFAULT;
        }
    }

    if (tsdrv_is_in_container(devid)) {
        struct tsdrv_ts_resource *ts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
        phy_cqid = ts_res->logic_phy_cq;
    } else {
        phy_cqid = logic_ctx->phy_cq.index;
    }

    tsdrv_pack_id_inst(devid, fid, tsid, &id_inst);
    err = logic_cq_phy_cq_clean(&id_inst, phy_cqid, logic_cqid, sub_cq->type);
    if (err != 0) {
        return err;
    }
    err = logic_cq_free(ctx, tsid, logic_cqid);
    if (err != 0) {
        return err;
    }
    err = logic_sqcq_phy_cq_free(ctx, tsid, logic_ctx, sub_cq->type);
    if (err != 0) {
        return err;
    }

    return 0;
}

int logic_cq_is_belong_to_proc(u32 devid, u32 fid, u32 tsid, u32 logic_cqid,
    struct logic_sqcq_ts_ctx *logic_ctx)
{
    struct logic_cq_info *logic_cq = NULL;
    struct logic_cq_sub *sub_cq = NULL;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;
    u32 useflag;
    if (logic_cqid >= TSDRV_MAX_LOGIC_CQ_NUM) {
        TSDRV_PRINT_ERR("Invalid logic cq. (logic_cqid=%u)\n", logic_cqid);
        return -EFAULT;
    }
    logic_cq = logic_cq_info_get(devid, fid, tsid);
    if (logic_cq == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("logic_cq is NULL, devid=%u fid=%u tsid=%u\n", devid, fid, tsid);
        return -ENOMEM;
#endif
    }

    if (ka_list_empty_careful(&logic_ctx->logic_cq_list) != 0) {
        TSDRV_PRINT_ERR("logic_cq_list is empty, devid=%u fid=%u tsid=%u\n", devid, fid, tsid);
        return -ENODEV;
    }

    ka_list_for_each_safe(pos, n, &logic_ctx->logic_cq_list) {
        sub_cq = ka_list_entry(pos, struct logic_cq_sub, cq_list);
        if (sub_cq->cqid == logic_cqid) {
            ka_task_spin_lock_bh(&logic_cq->lock);
            if (sub_cq->useflag != LOGIC_CQ_IN_USE) {
#ifndef TSDRV_UT
                useflag = sub_cq->useflag;
                ka_task_spin_unlock_bh(&logic_cq->lock);
                TSDRV_PRINT_ERR("invalid cqid(%u) useflag(%u), devid=%u fid=%u tsid=%u\n",
                    logic_cqid, useflag, devid, fid, tsid);
                return -ENODEV;
#endif
            }
            ka_task_spin_unlock_bh(&logic_cq->lock);
            return 0;
        }
    }

    TSDRV_PRINT_ERR("Logic_cq doesn't exist in this process. (logic_cqid=%u)\n", logic_cqid);
    return -ENODEV;
}

int logic_ioctl_cq_free(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    struct logic_sqcq_free_para *para = &arg->logic_sqcq_free_para;
    struct logic_sqcq_ts_ctx *logic_ctx = NULL;
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    u32 fid = tsdrv_get_fid_by_ctx(ctx);
    u32 tsid = arg->tsid;
    int err;

    logic_ctx = (struct logic_sqcq_ts_ctx *)ctx->ts_ctx[tsid].logic_sqcq_ctx;
    if (logic_ctx == NULL) {
        TSDRV_PRINT_ERR("logic ctx is NULL, devid=%u fid=%u tsid=%u tgid=%d\n", devid, fid, tsid, ctx->tgid);
        return -ENODEV;
    }

    ka_task_mutex_lock(&logic_ctx->lock);
    err = logic_cq_is_belong_to_proc(devid, fid, tsid, para->cq_id, logic_ctx);
    if (err != 0) {
        ka_task_mutex_unlock(&logic_ctx->lock);
        TSDRV_PRINT_ERR("parameter chk fail, devid=%u fid=%u tsid=%u\n", devid, fid, tsid);
        return err;
    }

    logic_thread_bind_irq_uninit(ctx, tsid, para->cq_id);

    err = logic_cq_free_one_logic_cq(ctx, tsid, logic_ctx, para->cq_id);
    if (err != 0) {
        ka_task_mutex_unlock(&logic_ctx->lock);
        return err;
    }
    ka_task_mutex_unlock(&logic_ctx->lock);
    TSDRV_PRINT_DEBUG("Free logic cq. (logic_cqid=%u)\n", para->cq_id);
    return 0;
}

STATIC int logic_cq_report_chk(u32 phy_cqid, struct logic_cq_info *logic_cq,
    struct tag_ts_logic_cq_report_msg *report)
{
#ifndef TSDRV_UT
    if (report->logic_cq_id == LOGIC_CQ_CLEANED_CQID) {
        TSDRV_PRINT_DEBUG("Clean cqid. (phy_cqid=%u; logic_cq_id=%u)\n", phy_cqid, (u32)report->logic_cq_id);
        return -ENODEV;
    }
    if (report->logic_cq_id >= TSDRV_MAX_LOGIC_CQ_NUM) {
        TSDRV_PRINT_ERR("invalid cq id, phy_cqid=%u logic_cq_id=%u\n", phy_cqid, (u32)report->logic_cq_id);
        return -ENODEV;
    }
    if (logic_cq->sub_cq[report->logic_cq_id].useflag == LOGIC_CQ_IS_FREE) {
        TSDRV_PRINT_DEBUG("invalid logic cq=%u status, possibly because the process has exited.\n",
            (u32)report->logic_cq_id);
        return -EINVAL;
    }
    if (logic_cq->sub_cq[report->logic_cq_id].ctx == NULL) {
        TSDRV_PRINT_ERR("logic cq ctx is NULL, logic_cqid=%u\n", (u32)report->logic_cq_id);
        return -EFAULT;
    }
    return 0;
#endif
}

int tsdrv_fill_logic_cqe(u32 devid, u32 fid, u32 tsid, u32 logic_cqid, struct logic_cq_info *logic_cq, logic_cq_report_msg_t *logic_cqe)
{
    struct logic_cq_sub *sub_cq = NULL;
    u32 offset;
    int err;

    TSDRV_PRINT_DEBUG("Dispatch logic_cq. (logic_cqid=%u).\n", logic_cqid);

    sub_cq = &logic_cq->sub_cq[logic_cqid];
    if (sub_cq->depth == 0) {
        TSDRV_PRINT_ERR("(logic_cqid:%u), depth is 0.\n", logic_cqid);
        return 0;
    }

    err = logic_cq_queue_full(sub_cq->head, sub_cq->tail, sub_cq->depth);
    if (err != 0) {
        TSDRV_PRINT_WARN("logic_cq=%u que is full, tail=%u head=%u\n", sub_cq->cqid, sub_cq->tail, sub_cq->head);
        return -EBUSY;
    }
    offset = sub_cq->tail * sub_cq->size;
    err = memcpy_s(sub_cq->vaddr + offset, sub_cq->size, logic_cqe, sizeof(struct tag_logic_cq_report_msg));
    if (err != EOK) {
        TSDRV_PRINT_ERR("cq dispach memcpy fail. (logic_cqid=%u; cqe_size=%u; err=%d)\n", logic_cqid, sub_cq->size,
            err);
        return err;
    }
    TSDRV_PRINT_DEBUG("Dispatch succeed. (logic_cqid=%u; cq_size=%u; tail=%u; depth=%u)\n",
        sub_cq->cqid, sub_cq->size, sub_cq->tail, sub_cq->depth);
    sub_cq->tail = (sub_cq->tail + 1) % sub_cq->depth;
    ka_wmb();
    logic_cq_wakeup_ctx(devid, fid, tsid, (struct logic_sqcq_ts_ctx *)sub_cq->ctx, sub_cq->cqid);
    return 0;
}

int logic_cq_dispatch(u32 devid, u32 fid, u32 tsid, u32 phy_cqid,
    struct logic_cq_info *logic_cq, struct tag_ts_logic_cq_report_msg *report)
{
#ifndef TSDRV_UT
    struct logic_cq_sub *sub_cq = NULL;
    u32 offset;
    int err;

    TSDRV_PRINT_DEBUG("Dispatch logic_cq. (phy_cqid=%u).\n", phy_cqid);
    err = logic_cq_report_chk(phy_cqid, logic_cq, report);
    if (err != 0) {
        /* abandon illegal report */
        return 0;
    }

    sub_cq = &logic_cq->sub_cq[report->logic_cq_id];
    if (sub_cq->depth == 0) {
        TSDRV_PRINT_ERR("(cqid:%u), depth is 0.\n", (u32)report->logic_cq_id);
        return 0;
    }

    err = logic_cq_queue_full(sub_cq->head, sub_cq->tail, sub_cq->depth);
    if (err != 0) {
        TSDRV_PRINT_DEBUG("logic_cq=%u que is full, tail=%u head=%u\n", sub_cq->cqid, sub_cq->tail, sub_cq->head);
        return -EBUSY;
    }
    offset = sub_cq->tail * sub_cq->size;
    err = memcpy_s(sub_cq->vaddr + offset, sub_cq->size, report, sizeof(struct tag_ts_logic_cq_report_msg));
    if (err != EOK) {
        TSDRV_PRINT_ERR("cq dispach memcpy fail.(cqid:%u)\n", (u32)report->logic_cq_id);
        return 0;
    }
    ka_wmb();
    TSDRV_PRINT_DEBUG("Dispatch succeed. (logic_cqid=%u; cq_size=%u; tail=%u; depth=%u)\n",
        sub_cq->cqid, sub_cq->size, sub_cq->tail, sub_cq->depth);
    sub_cq->tail = (sub_cq->tail + 1) % sub_cq->depth;
    ka_wmb();
    logic_cq_wakeup_ctx(devid, fid, tsid, (struct logic_sqcq_ts_ctx *)sub_cq->ctx, sub_cq->cqid);
    return 0;
#endif
}

#ifdef CFG_FEATURE_RUNTIME_NO_THREAD
STATIC void logic_sqcq_recycle(struct tsdrv_ctx *ctx, u32 tsid)
{
    struct logic_sqcq_ts_ctx *logic_ctx = NULL;
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    u32 fid = tsdrv_get_fid_by_ctx(ctx);
    struct logic_cq_sub *sub_cq = NULL;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;
    int err;

    logic_ctx = (struct logic_sqcq_ts_ctx *)ctx->ts_ctx[tsid].logic_sqcq_ctx;
    if (logic_ctx == NULL) {
        TSDRV_PRINT_ERR("logic_ctx is NULL, devid=%u fid=%u tsid=%u\n", devid, fid, tsid);
        return;
    }
    ka_task_mutex_lock(&logic_ctx->lock);
    if (logic_ctx->logic_cq_num != 0 || logic_ctx->ctrl_logic_cq_num != 0) {
        TSDRV_PRINT_INFO("recycle start logic_cq_num=%u devid=%u fid=%u tsid=%u\n", logic_ctx->logic_cq_num,
            devid, fid, tsid);
    }
    if (!ka_list_empty_careful(&logic_ctx->logic_cq_list)) {
        ka_list_for_each_safe(pos, n, &logic_ctx->logic_cq_list) {
            sub_cq = ka_list_entry(pos, struct logic_cq_sub, cq_list);
            logic_thread_bind_irq_uninit(ctx, tsid, sub_cq->cqid);
            err = logic_cq_free_one_logic_cq(ctx, tsid, logic_ctx, sub_cq->cqid);
            if (err != 0) {
                TSDRV_PRINT_ERR("recycle logic cq fail. (devid=%u; fid=%u; tsid=%u; logic_cqid=%u; err=%d\n", devid,
                    fid, tsid, sub_cq->cqid, err);
                continue;
            }
            TSDRV_PRINT_DEBUG("recycle succeed. (logic_cqid=%u; devid=%u; fid=%u; tsid=%u)\n", sub_cq->cqid, devid,
                fid, tsid);
        }
    }
    if (logic_ctx->logic_cq_num == 0 && logic_ctx->ctrl_logic_cq_num == 0) {
        TSDRV_PRINT_DEBUG("recycle succeed devid=%u fid=%u tsid=%u\n", devid, fid, tsid);
    } else {
        TSDRV_PRINT_INFO("unable to recycle, logic_cq_num=%u remained, devid=%u fid=%u tsid=%u\n",
            logic_ctx->logic_cq_num, devid, fid, tsid);
    }
    ka_task_mutex_unlock(&logic_ctx->lock);
}

int logic_sqcq_ctx_init(struct tsdrv_ctx *ctx, u32 tsnum)
{
    struct logic_sqcq_ts_ctx *logic_ctx = NULL;
    u32 logic_cqid;
    u32 tsid;

    for (tsid = 0; tsid < tsnum; tsid++) {
        ctx->ts_ctx[tsid].logic_sqcq_ctx = tsdrv_vmalloc(sizeof(struct logic_sqcq_ts_ctx),
            KA_GFP_KERNEL | __KA_GFP_ACCOUNT, KA_PAGE_KERNEL);
        if (ctx->ts_ctx[tsid].logic_sqcq_ctx == NULL) {
#ifndef TSDRV_UT
            goto err_kmalloc_logic_ts_ctx;
#endif
        }
        (void)memset_s(ctx->ts_ctx[tsid].logic_sqcq_ctx, sizeof(struct logic_sqcq_ts_ctx), 0,
            sizeof(struct logic_sqcq_ts_ctx));
        logic_ctx = (struct logic_sqcq_ts_ctx *)ctx->ts_ctx[tsid].logic_sqcq_ctx;
        logic_ctx->logic_cq_num = 0;
        logic_ctx->ctrl_logic_cq_num = 0;
        logic_ctx->pid = ctx->pid;
        logic_ctx->tgid = ctx->tgid;
        ka_task_mutex_init(&logic_ctx->lock);

        logic_ctx->phy_cq.inited = LOGIC_CQ_PHY_CQ_UNINIT;
        KA_INIT_LIST_HEAD(&logic_ctx->logic_cq_list);
        for (logic_cqid = 0; logic_cqid < TSDRV_MAX_LOGIC_CQ_NUM; logic_cqid++) {
            ka_base_atomic_set(&logic_ctx->wakeup_num[logic_cqid], 0);
            ka_base_atomic_set(&logic_ctx->wait_thread_num[logic_cqid], 0);
            ka_task_init_waitqueue_head(&logic_ctx->logic_cq_wait[logic_cqid]); // ka_task
        }
        ka_wmb();
    }
    return 0;
#ifndef TSDRV_UT
err_kmalloc_logic_ts_ctx:
    for (tsid = 0; tsid < tsnum; tsid++) {
        if (ctx->ts_ctx[tsid].logic_sqcq_ctx != NULL) {
            tsdrv_vfree(ctx->ts_ctx[tsid].logic_sqcq_ctx);
            ctx->ts_ctx[tsid].logic_sqcq_ctx = NULL;
        }
    }
    return -ENODEV;
#endif
}

void logic_sqcq_ctx_exit(struct tsdrv_ctx *ctx, u32 tsnum)
{
    u32 tsid;

    for (tsid = 0; tsid < tsnum; tsid++) {
        if (ctx->ts_ctx[tsid].logic_sqcq_ctx != NULL) {
            logic_sqcq_recycle(ctx, tsid);
            tsdrv_vfree(ctx->ts_ctx[tsid].logic_sqcq_ctx);
            ctx->ts_ctx[tsid].logic_sqcq_ctx = NULL;
        }
    }
}

#else

int logic_sqcq_ctx_init(struct tsdrv_ctx *ctx, u32 tsnum)
{
    return 0;
}

void logic_sqcq_ctx_exit(struct tsdrv_ctx *ctx, u32 tsnum)
{
}

#endif

STATIC int logic_sqcq_init(u32 devid, u32 fid, u32 tsid)
{
    int err;

    if (g_logic_sqcq[devid][fid][tsid] == NULL) {
        g_logic_sqcq[devid][fid][tsid] = tsdrv_kzalloc(sizeof(struct logic_sqcq), KA_GFP_KERNEL);
        if (g_logic_sqcq[devid][fid][tsid] == NULL) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("logic cq kmalloc fail, devid=%u fid=%u tsid=%u\n", devid, fid, tsid);
            return -ENOMEM;
#endif
        }
    }
    err = logic_cq_init(devid, fid, tsid);
    if (err != 0) {
#ifndef TSDRV_UT
        goto err_logic_cq_init;
#endif
    }

    ka_task_mutex_init(&g_logic_sqcq[devid][fid][tsid]->mutex_t);
    return 0;
#ifndef TSDRV_UT
err_logic_cq_init:
    if (g_logic_sqcq[devid][fid][tsid] != NULL) {
        tsdrv_kfree(g_logic_sqcq[devid][fid][tsid]);
        g_logic_sqcq[devid][fid][tsid] = NULL;
    }
    return -ENODEV;
#endif
}

STATIC void logic_sqcq_exit(u32 devid, u32 fid, u32 tsid)
{
    logic_cq_exit(devid, fid, tsid);
    if (g_logic_sqcq[devid][fid][tsid] != NULL) {
        ka_task_mutex_destroy(&g_logic_sqcq[devid][fid][tsid]->mutex_t);
        tsdrv_kfree(g_logic_sqcq[devid][fid][tsid]);
        g_logic_sqcq[devid][fid][tsid] = NULL;
    }
}

int logic_sqcq_dev_init(u32 devid, u32 fid, u32 tsnum)
{
    u32 tsid;
    int err;
    u32 i;

    for (tsid = 0; tsid < tsnum; tsid++) {
        err = logic_sqcq_init(devid, fid, tsid);
        if (err != 0) {
            goto err_logic_cq_init;
        }
    }
    return 0;
err_logic_cq_init:
#ifndef TSDRV_UT
    for (i = 0; i < tsid; i++) {
        logic_sqcq_exit(devid, fid, i);
    }
#endif
    return -EINVAL;
}

void logic_sqcq_dev_exit(u32 devid, u32 fid, u32 tsnum)
{
    u32 tsid;

    for (tsid = 0; tsid < tsnum; tsid++) {
        logic_sqcq_exit(devid, fid, tsid);
    }
}
