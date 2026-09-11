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
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/rculist.h>
#include <linux/rcupdate.h>
#include <linux/nsproxy.h>
#include <linux/pid.h>
#include <linux/version.h>
#include "ka_task_pub.h"
#include "ka_list_pub.h"
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/task.h>
#endif
#ifdef AOS_LLVM_BUILD
#include <linux/slab.h>
#include <asm/pgtable-prot.h>
#endif
#include <linux/kallsyms.h>
#ifndef CFG_TSDRV_VM_ENV
#include "ascend_hal_define.h"
#endif
#include <securec.h>
#include "tsdrv_cb_pm.h"
#include "devdrv_cqsq.h"
#include "devdrv_interface.h"
#include "kernel_version_adapt.h"
#include "event_sched_inner.h"
#ifndef CFG_SOC_PLATFORM_MDC_V51
#include "hvtsdrv_cb.h"
#endif

STATIC struct callback_info *g_cb_info[TSDRV_MAX_DAVINCI_NUM][TSDRV_MAX_FID_NUM][DEVDRV_MAX_TS_NUM];

struct callback_info *callback_cb_info_get(u32 devid, u32 fid, u32 tsid)
{
    if (g_cb_info[devid][fid][tsid] == NULL) {
        return NULL;
    }
    return g_cb_info[devid][fid][tsid];
}

struct callback_logic_cq *callback_logic_cq_get(u32 devid, u32 fid, u32 tsid)
{
    if (g_cb_info[devid][fid][tsid] == NULL) {
        return NULL;
    }
    return &g_cb_info[devid][fid][tsid]->logic_cq;
}

struct callback_phy_sqcq *callback_physic_sqcq_get(u32 devid, u32 fid, u32 tsid)
{
    if (g_cb_info[devid][fid][tsid] == NULL) {
        return NULL;
    }
    return &g_cb_info[devid][fid][tsid]->phy_sqcq;
}

void *callback_kvmalloc(size_t size)
{
    void *addr = NULL;

    if (size == 0) {
        return NULL;
    }
#ifndef AOS_LLVM_BUILD
    addr = tsdrv_kmalloc(size, KA_GFP_NOWAIT | __KA_GFP_NOWARN | __KA_GFP_ACCOUNT);
    if (addr == NULL) {
#ifndef TSDRV_UT
        addr = tsdrv_vmalloc(size, KA_GFP_KERNEL | __KA_GFP_ACCOUNT, KA_PAGE_KERNEL);
        if (addr == NULL) {
            return NULL;
        }

#endif
    }
#else
    addr = tsdrv_kmalloc(size, __KA_GFP_ACCOUNT);
    if (addr == NULL) {
#ifndef TSDRV_UT
        addr = tsdrv_vmalloc(size, KA_GFP_KERNEL | __KA_GFP_ACCOUNT, KA_PAGE_KERNEL);
        if (addr == NULL) {
            return NULL;
        }
    }
#endif
#endif
    return addr;
}

void callback_kvfree(const void *addr)
{
#ifndef AOS_LLVM_BUILD
    if (ka_mm_is_vmalloc_addr(addr)) {
        tsdrv_vfree(addr);
    } else {
        tsdrv_kfree(addr);
    }
#else
    tsdrv_kvfree(addr);
#endif
}

bool callback_queue_full(u32 head, u32 tail, u32 depth)
{
    if (likely(depth != 0)) {
        return (head == ((tail + 1) % depth));
    } else {
        return true;
    }
}

STATIC void callback_wakeup_ctx(struct tsdrv_ctx *ctx, u32 tsid, u32 grpid)
{
    ctx->ts_ctx[tsid].cbcq_wait_flag[grpid] = 1;
    ka_mb();
    if (ka_task_waitqueue_active(&ctx->ts_ctx[tsid].cbcq_wait[grpid]) != 0) {
        ka_task_wake_up(&ctx->ts_ctx[tsid].cbcq_wait[grpid]);
    }
}

tsdrv_cpu_task_t *tsdrv_get_cpu_task(struct callback_phy_sqcq_info *phy_cq)
{
    tsdrv_cpu_task_t *report = (tsdrv_cpu_task_t *)(phy_cq->vaddr + ((unsigned long)phy_cq->head * phy_cq->size));
    TSDRV_PRINT_DEBUG("(cb_cqid=%u; slot_size=%u; head=%u; phase=%u; task_type=%u; sqid=%u; "
        "sq_head=%u; stream_id=%u; task_id=%u; cqid=%u)\n",
        phy_cq->index, phy_cq->size, phy_cq->head, report->phase, report->task_type, report->sqid,
        report->sq_head, report->stream_id, report->task_id, report->task.cb_task.cqid);
    return report;
}

STATIC void callback_abandon_report(struct callback_phy_sqcq *phy_sqcq, u32 cqid)
{
    struct callback_phy_sqcq_info *phy_cq = NULL;
    tsdrv_cpu_task_t *info = NULL;
    u32 tmp_head;
    u32 tmp_phase;

    phy_cq = &phy_sqcq->cb_cq;
    ka_task_spin_lock_bh(&phy_cq->lock);
    if (phy_cq->vaddr == NULL) {
#ifndef TSDRV_UT
        goto out;
#endif
    }

    tmp_head = phy_cq->head;
    tmp_phase = phy_cq->phase;
    info = tsdrv_get_cpu_task(phy_cq);
    while (info->phase == tmp_phase) {
        /**
         * In the ARM environment:
         * 1.Out-of-order may cause the TS driver to read other members of the report structure (read instruction 1)
         *   to be executed before the phase comparison instruction (read instruction 2);
         * 2.The time when TS writes report is between [read instruction 1] and [read instruction 2];
         * This error timing may cause [Read instruction 1] to read the old value.
         */
        ka_rmb();
        tmp_head = tmp_head + 1;
        if (tmp_head == DEVDRV_CB_CQ_MAX_NUM) {
            tmp_head = 0;
            tmp_phase = !tmp_phase;
        }
        if (info->task.cb_task.cqid == cqid) {
            info->task.cb_task.cqid = DEVDRV_CB_CQ_MAX_NUM;
        }

        info = (tsdrv_cpu_task_t *)(phy_cq->vaddr + ((unsigned long)tmp_head * phy_cq->size));
    }

out:
    ka_task_spin_unlock_bh(&phy_cq->lock);
    return;
}

STATIC s32 callback_report_check(struct callback_logic_cq *logic_cq, tsdrv_cpu_task_t *info)
{
    if (info->task.cb_task.cqid >= DEVDRV_CB_CQ_MAX_NUM) {
        /* illegal id, it will also enter this branch when cq report is abandoned by the driver,
         so use debug prs32 here */
#ifndef TSDRV_UT
        TSDRV_PRINT_DEBUG("invalid cq id.(cqid:%u)\n", (u32)info->task.cb_task.cqid);
#endif
        return -EINVAL;
    }

    if (logic_cq->sub_cq[info->task.cb_task.cqid].useflag == CALLBACK_CQ_IS_FREE) {
        return -EINVAL;
    }

    return 0;
}

static void tsdrv_cb_task_to_cqe(tsdrv_cpu_task_t *info, struct cbcq_report *cqe)
{
    cqe->cq_id = info->task.cb_task.cqid;
    cqe->stream_id = info->stream_id;
    cqe->task_id = info->task_id;
    cqe->is_block = info->task.cb_task.is_block;
    cqe->sq_id = info->sqid;
    cqe->host_func_cb_ptr = info->task.cb_task.host_func_cb_ptr;
    cqe->fn_data_ptr = info->task.cb_task.fn_data_ptr;
}

STATIC int callback_cq_dispach(struct callback_phy_sqcq_info *phy_cq, struct callback_logic_cq *logic_cq,
    tsdrv_cpu_task_t *info)
{
    struct callback_logic_cq_sub *sub_cq = NULL;
    struct cbcq_report cqe;
    u32 offset;
    int ret;

    if (callback_report_check(logic_cq, info) != 0) {
        /* Ignore illegal report */
        phy_cq->drop_cnt += 1;
        return 0;
    }

    sub_cq = &logic_cq->sub_cq[info->task.cb_task.cqid];
    if (callback_queue_full(sub_cq->head, sub_cq->tail, sub_cq->depth)) {
        /* cq queue full */
        TSDRV_PRINT_DEBUG("logic cq-%u is full.(groupid:%u)\n", sub_cq->cqid, sub_cq->grpid);
        return -EBUSY;
    }

    tsdrv_cb_task_to_cqe(info, &cqe);
    offset = sub_cq->tail * sub_cq->size;
    ret = memcpy_s(sub_cq->vaddr + offset, sub_cq->size, &cqe, sizeof(struct cbcq_report));
    if (ret != 0) {
        phy_cq->drop_cnt += 1;
        return 0;
    }

    sub_cq->tail = (sub_cq->tail + 1) % sub_cq->depth;
    callback_wakeup_ctx(sub_cq->ctx, logic_cq->tsid, sub_cq->grpid);
    phy_cq->out_cnt += 1;
    sub_cq->in_cnt += 1;

    return 0;
}

STATIC void callback_set_sq_head(struct callback_phy_sqcq *cb_sqcq, u32 head)
{
    struct tsdrv_device *tsdrv_dev = tsdrv_get_dev(cb_sqcq->devid);
    struct callback_phy_sqcq_info *phy_sq = &cb_sqcq->cb_sq;
    struct devdrv_ts_sq_info *sq_info = NULL;
    struct tsdrv_ts_resource *ts_res = NULL;
    phys_addr_t info_mem_addr;

    ts_res = &tsdrv_dev->dev_res[cb_sqcq->fid].ts_resource[cb_sqcq->tsid];
    info_mem_addr = ts_res->mem_info[DEVDRV_INFO_MEM].virt_addr;

    ka_task_spin_lock_bh(&phy_sq->lock);
    if (phy_sq->head != head) {
        if (head > phy_sq->head) {
            phy_sq->out_cnt += (head - phy_sq->head);
        } else {
            phy_sq->out_cnt += (phy_sq->depth - phy_sq->head);
            phy_sq->out_cnt += head;
        }
        phy_sq->head = head;

        if (tsdrv_is_in_container(cb_sqcq->devid)) {
            sq_info = devdrv_calc_sq_info(info_mem_addr, cb_sqcq->cb_sq.index);
            sq_info->head = head;
        }
    }
    ka_task_spin_unlock_bh(&phy_sq->lock);

    return;
}

static int callback_cpu_task_handler(tsdrv_cpu_task_t *info, struct callback_phy_sqcq *phy_sqcq,
    struct callback_logic_cq *logic_cq)
{
    struct callback_phy_sqcq_info *phy_cq = &phy_sqcq->cb_cq;
    int ret;

    if ((info->sqid == phy_sqcq->cb_sq.index) && (info->sq_head < phy_sqcq->cb_sq.depth)) {
        callback_set_sq_head(phy_sqcq, info->sq_head);
        ret = callback_cq_dispach(phy_cq, logic_cq, info);
        if (ret != 0) {
            return ret;
        }
    } else {
        phy_cq->drop_cnt += 1;
    }
    return 0;
}

#ifndef CFG_TSDRV_VM_ENV
extern int sched_submit_event_to_thread(u32 chip_id, struct sched_published_event *event);
#endif
static void normal_cpu_task_handler(tsdrv_cpu_task_t *info)
{
#ifndef CFG_TSDRV_VM_ENV
    struct sched_published_event event;
    u32 devid = info->task.normal_task.phy_devid;

    event.event_info.event_id = info->task.normal_task.event_id;
    event.event_info.subevent_id = info->task.normal_task.subevent_id;
    event.event_info.pid = (int)info->task.normal_task.pid;
    event.event_info.gid = info->task.normal_task.grp_id;
    event.event_info.tid = info->task.normal_task.tid;
    event.event_info.msg_len = info->task.normal_task.msg_len;
    event.event_info.msg = NULL;
    event.event_info.dst_engine = CCPU_HOST;
    if (info->task.normal_task.msg_len != 0) {
        event.event_info.msg = info->task.normal_task.msg;
    }

    event.event_func.event_ack_func = NULL;
    event.event_func.event_finish_func = NULL;

#ifndef TSDRV_UT
    (void)sched_submit_event_to_thread(devid, &event);
#endif
#endif
}

#ifndef TSDRV_UT
static ka_task_struct_t *tsdrv_get_task(ka_pid_t nr, struct pid **pid_inst)
{
    ka_task_struct_t *task = NULL;
    ka_hlist_node_t *first;
    ka_struct_pid_t *pid = NULL;

    ka_task_rcu_read_lock_bh();
    /* get pid inst by global nr and pid_ns */
    pid = ka_task_find_pid_ns(nr, &init_pid_ns);
    if (pid == NULL) {
        ka_task_rcu_read_unlock_bh();
        return NULL;
    }

    ka_task_get_pid(pid);

    /* get task_struct */
    first = ka_task_rcu_dereference_bh_check(ka_hlist_first_rcu(ka_task_get_pid_tasks(pid, KA_PIDTYPE_PID)),
        ka_task_lockdep_tasklist_lock_is_held());
    if (first) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)
        task = ka_hlist_entry(first, ka_task_struct_t, pid_links[(KA_PIDTYPE_PID)]);
#else
        task = ka_hlist_entry(first, ka_task_struct_t, pids[(KA_PIDTYPE_PID)].node);
#endif 
        ka_task_get_task_struct(task); 
    } else {
        ka_task_put_pid(pid);
        ka_task_rcu_read_unlock_bh();
        return NULL;
    }

    ka_task_rcu_read_unlock_bh();

    *pid_inst = pid;

    return task;
}

static void tsdrv_put_task(ka_task_struct_t *task, struct pid *pid_inst)
{
    ka_task_put_task_struct(task);
    ka_task_put_pid(pid_inst);
}
#endif

static bool tsdrv_get_nxproxy_mnt(ka_task_struct_t *task, struct mnt_namespace **mnt_ns, int pid)
{
#ifndef TSDRV_UT
    task_lock(task);
    if (task->nsproxy == NULL) {
        task_unlock(task);
        TSDRV_PRINT_ERR("Nsproxy of task is NULL. (pid=%d)\n", pid);
        return false;
    }
    *mnt_ns = task->nsproxy->mnt_ns;
    task_unlock(task);
#endif
    return true;
}

static bool normal_cpu_task_dest_pid_is_valid(u32 devid, u32 fid, u32 tsid, tsdrv_cpu_task_t *info)
{
#ifndef TSDRV_UT
    struct tsdrv_ts_resource *ts_res = NULL;
    struct devdrv_ts_sq_info *sq_info = NULL;
    struct tsdrv_stream_info *stream_info = NULL;
    struct mnt_namespace *src_mnt_ns = NULL;
    struct mnt_namespace *dest_mnt_ns = NULL;
    struct pid *src_pid = NULL;
    struct pid *dest_pid = NULL;
    ka_task_struct_t *src_task = NULL;
    ka_task_struct_t *dest_task = NULL;
    bool result = false;

    ts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
    if (ts_res == NULL) {
        return false;
    }

    if (info->stream_id >= DEVDRV_MAX_STREAM_ID) {
        return false;
    }

    stream_info = &ts_res->stream_info[info->stream_id];
    sq_info = devdrv_calc_sq_info(ts_res->mem_info[DEVDRV_INFO_MEM].virt_addr, stream_info->sq);

    src_task = tsdrv_get_task(sq_info->tgid, &src_pid);
    if (src_task == NULL) {
        TSDRV_PRINT_ERR("Get src task failed. (src_pid=%d)\n", sq_info->tgid);
        return false;
    }

    dest_task = tsdrv_get_task((ka_pid_t)info->task.normal_task.pid, &dest_pid);
    if (dest_task == NULL) {
        tsdrv_put_task(src_task, src_pid);
        TSDRV_PRINT_ERR("Get dest task failed. (src_pid=%d)\n", info->task.normal_task.pid);
        return false;
    }
    if ((tsdrv_get_nxproxy_mnt(src_task, &src_mnt_ns, sq_info->tgid)) && (tsdrv_get_nxproxy_mnt(dest_task, &dest_mnt_ns, info->task.normal_task.pid))) {
        if (src_mnt_ns == dest_mnt_ns) {
            result = true;
        }
    }
    tsdrv_put_task(dest_task, dest_pid);
    tsdrv_put_task(src_task, src_pid);
    return result;
#else
    return true;
#endif
}

void task_dispatch_handler_ex(u32 devid, u32 fid, u32 tsid, u32 tail_valid, u32 vcq_tail)
{
    struct callback_phy_sqcq_info *phy_cq = NULL;
    struct callback_logic_cq *logic_cq = NULL;
    struct callback_phy_sqcq *phy_sqcq = NULL;
    tsdrv_cpu_task_t *info = NULL;
    u32 report_cnt = 0;
    int ret;

    phy_sqcq = &g_cb_info[devid][fid][tsid]->phy_sqcq;
    logic_cq = &g_cb_info[devid][fid][tsid]->logic_cq;
    phy_cq = &phy_sqcq->cb_cq;
    ka_task_spin_lock_bh(&logic_cq->lock);
    ka_task_spin_lock_bh(&phy_cq->lock);
    if ((phy_cq->vaddr == NULL) || (logic_cq->init == CALLBACK_UNINIT)) {
        ka_task_spin_unlock_bh(&phy_cq->lock);
        ka_task_spin_unlock_bh(&logic_cq->lock);
        return;
    }
    info = tsdrv_get_cpu_task(phy_cq);
    tsdrv_dma_sync_cpu(devid, phy_cq->paddr, phy_cq->size * phy_cq->depth, KA_DMA_FROM_DEVICE);
    while (info->phase == phy_cq->phase) {
        if ((tail_valid == 1) && (phy_cq->head == vcq_tail)) {
            break;
        }

        /**
         * In the ARM environment:
         * 1.Out-of-order may cause the TS driver to read other members of the report structure (read instruction 1)
         *   to be executed before the phase comparison instruction (read instruction 2);
         * 2.The time when TS writes report is between [read instruction 1] and [read instruction 2];
         * This error timing may cause [Read instruction 1] to read the old value.
         */
        ka_rmb();
        if (info->task_type == 0) {
            ret = callback_cpu_task_handler(info, phy_sqcq, logic_cq);
            if (ret != 0) {
                break;
            }
        } else {
            if (normal_cpu_task_dest_pid_is_valid(devid, fid, tsid, info)) {
                normal_cpu_task_handler(info);
            }
        }

        phy_cq->head = phy_cq->head + 1;
        if (phy_cq->head == DEVDRV_CB_CQ_MAX_NUM) {
            phy_cq->head = 0;
            phy_cq->phase = phy_cq->phase > 0 ? 0 : 1;
        }

        info = tsdrv_get_cpu_task(phy_cq);
        report_cnt++;
    }
    ka_task_spin_unlock_bh(&phy_cq->lock);
    ka_task_spin_unlock_bh(&logic_cq->lock);

    if (report_cnt != 0) {
        phy_cq->in_cnt += report_cnt;
        ret = callback_set_doorbell(phy_sqcq, phy_cq->index + DEVDRV_TS_DOORBELL_SQ_NUM, phy_cq->head);
        if (ret != 0) {
            TSDRV_PRINT_ERR("devid:%u, set doorbell failed, cb_cqid:%u cq_head:%u)\n", phy_sqcq->devid,
                phy_cq->index, phy_cq->head);
        }
        TSDRV_PRINT_DEBUG("dev-%u (%u-%u) recv %u cq report, set cq head %u; sq head is %u\n",
            devid, fid, tsid, report_cnt, phy_cq->head, phy_sqcq->cb_sq.head);
    }

    return ;
}

void task_dispatch_handler(u32 devid, u32 fid, u32 tsid)
{
    if ((devid >= TSDRV_MAX_DAVINCI_NUM) || (fid >= TSDRV_MAX_FID_NUM) || (tsid >= DEVDRV_MAX_TS_NUM) ||
        (g_cb_info[devid][fid][tsid] == NULL)) {
        return;
    }
    task_dispatch_handler_ex(devid, fid, tsid, 0, 0);
}

STATIC void callback_wake_wait_event(u32 tsid, struct callback_logic_cq *logic_cq,
    struct callback_logic_cq_sub *sub_cq)
{
    struct callback_logic_cq_sub *cq = NULL;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;

    if (ka_list_empty_careful(&logic_cq->cq_alloc_list) != 0) {
        callback_wakeup_ctx(sub_cq->ctx, tsid, sub_cq->grpid);
        return ;
    }
#ifndef TSDRV_UT
    /* find the logic cq with legal data */
    ka_list_for_each_safe(pos, n, &logic_cq->cq_alloc_list) {
        cq = ka_list_entry(pos, struct callback_logic_cq_sub, cq_list);
        if ((cq->grpid == sub_cq->grpid) && (cq->vpid == sub_cq->vpid)) {
            return;
        }
    }
    /* can not find, wake up ioctl cbcq wait */
    callback_wakeup_ctx(sub_cq->ctx, tsid, sub_cq->grpid);
#endif
}

s32 callback_cq_match(u32 fid, u32 cqid)
{
    if ((cqid != GET_CALLBACK_CQ_ID(fid)) && (cqid != CALLBACK_DEVICECQ_INDEX)) {
        /* cqid that is not corresponding to callback returns a match failure */
        return -ENXIO;
    }
    return 0;
}

void callback_cq_proc(u32 devid, u32 fid, u32 tsid, u32 cqid)
{
    if (!tsdrv_is_in_vm(devid)) {
        if (g_cb_info[devid][fid][tsid] == NULL) {
            TSDRV_PRINT_DEBUG("invalid para(devid:%u  fid:%u  tsid:%u)\n", devid, fid, tsid);
            return;
        }

        if (cqid == g_cb_info[devid][fid][tsid]->phy_sqcq.cb_cq.index) {
            task_dispatch_handler(devid, fid, tsid);
        }
    } else {
#ifndef CFG_SOC_PLATFORM_MDC_V51
        if ((cqid != CALLBACK_HOSTCQ_INDEX) && (cqid != CALLBACK_DEVICECQ_INDEX)) {
            hvtsdrv_cb_cq_handler(devid, fid, tsid, cqid);
        }
#else
        TSDRV_PRINT_DEBUG("environment is not support. para(devid:%u  fid:%u  tsid:%u)\n", devid, fid, tsid);
#endif
    }

    return;
}

void callback_free_logic_cq(struct callback_logic_cq_sub *sub_cq)
{
    if (sub_cq->vaddr != NULL) {
        callback_kvfree(sub_cq->vaddr);
        sub_cq->vaddr = NULL;
    }

    sub_cq->head = 0;
    sub_cq->tail = 0;
    sub_cq->grpid = 0;
    sub_cq->vpid = 0;
    sub_cq->ctx = NULL;
    return;
}

void callback_info_print(u32 devid, u32 fid, u32 tsid)
{
    struct callback_phy_sqcq_info *phy_sq = NULL;
    struct callback_phy_sqcq_info *phy_cq = NULL;
    struct callback_phy_sqcq *phy_sqcq = NULL;

    phy_sqcq = callback_physic_sqcq_get(devid, fid, tsid);
    if (phy_sqcq == NULL) {
        return;
    }
    phy_sq = &phy_sqcq->cb_sq;
    phy_cq = &phy_sqcq->cb_cq;
    /* read-only static space, no lock is required */
    TSDRV_PRINT_DEBUG("---------dev-%u fid-%u ts-%u info---------\n", devid, fid, tsid);
    TSDRV_PRINT_DEBUG("physical sqid:%u   cqid:%u\n", phy_sq->index, phy_cq->index);
    TSDRV_PRINT_DEBUG("phy-sq  head:%u   tail:%u\n", phy_sq->head, phy_sq->tail);
    TSDRV_PRINT_DEBUG("phy-sq  inCnt:%u  outCnt:%u\n", phy_sq->in_cnt, phy_sq->out_cnt);
    TSDRV_PRINT_DEBUG("phy-cq  head:%u   phase:%u\n", phy_cq->head, (u32)phy_cq->phase);
    TSDRV_PRINT_DEBUG("phy-cq  inCnt:%u  outCnt:%u  dropCnt:%u\n", phy_cq->in_cnt, phy_cq->out_cnt, phy_cq->drop_cnt);
}

s32 callback_alloc_logic_cq_mbox(struct callback_phy_sqcq *phy_sqcq,
    struct callback_logic_cq_sub *sub_cq)
{
    s32 ret;

    ret = callback_mbox_logic_cq(DEVDRV_MAILBOX_CREATE_LOGIC_CBCQ, phy_sqcq, sub_cq);
    if (ret != 0) {
        TSDRV_PRINT_ERR("notify ts logic cq-%u alloc fail.(vpid:%u grpid:%u)\n",
            sub_cq->cqid, sub_cq->vpid, sub_cq->grpid);
    }
    return ret;
}

STATIC s32 callback_generate_sq(struct callback_phy_sqcq *cb_sqcq)
{
    int ret;

    ret = callback_get_physic_sq(cb_sqcq);
    if (ret != 0) {
        TSDRV_PRINT_WARN("get physical sq fail ret=%d.\n", ret);
        return -EPERM;
    }

    ka_task_spin_lock_bh(&cb_sqcq->cb_sq.lock);
    cb_sqcq->cb_sq.head = 0;
    cb_sqcq->cb_sq.tail = 0;
    cb_sqcq->cb_sq.size = DEVDRV_SQ_SLOT_SIZE;
    cb_sqcq->cb_sq.depth = DEVDRV_MAX_SQ_DEPTH;
    ka_task_spin_unlock_bh(&cb_sqcq->cb_sq.lock);

    return 0;
}

STATIC void callback_destory_sq(struct callback_phy_sqcq *cb_sqcq)
{
    void *cb_sq_vaddr = NULL;

    ka_task_spin_lock_bh(&cb_sqcq->cb_sq.lock);
    cb_sq_vaddr = cb_sqcq->cb_sq.vaddr;
    cb_sqcq->cb_sq.vaddr = NULL;
    cb_sqcq->cb_sq.paddr = 0;
    ka_task_spin_unlock_bh(&cb_sqcq->cb_sq.lock);

    if (cb_sq_vaddr != NULL) {
        ka_mm_iounmap(cb_sq_vaddr);
    }
    callback_free_physic_sq(cb_sqcq);
}

STATIC s32 callback_generate_cq(struct callback_phy_sqcq *cb_sqcq)
{
    s32 ret;

    ka_task_spin_lock_bh(&cb_sqcq->cb_cq.lock);
    cb_sqcq->cb_cq.tail = 0;
    cb_sqcq->cb_cq.head = 0;
    cb_sqcq->cb_cq.size = DEVDRV_MAX_CBCQ_SIZE;
    cb_sqcq->cb_cq.depth = DEVDRV_MAX_CBCQ_DEPTH;
    cb_sqcq->cb_cq.phase = 1;
    ka_task_spin_unlock_bh(&cb_sqcq->cb_cq.lock);

    ret = callback_get_physic_cq(cb_sqcq);
    if (ret != 0) {
        return -EPERM;
    }

    return ret;
}

int tsdrv_get_cb_cq_mem_phy_addr(u32 devId, u32 tsId, u32 cqId, struct tsdrv_phy_addr_get *info)
{
    return -EINVAL;
}

STATIC s32 callback_logic_cq_init(struct tsdrv_device *tsdrv_dev, u32 fid, u32 tsid)
{
    struct callback_logic_cq *logic_cq = NULL;
    u32 i = 0;

    logic_cq = callback_logic_cq_get(tsdrv_dev->devid, fid, tsid);
    if (logic_cq == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("callback info is null.(devid:%u fid:%u tsid:%u)\n", tsdrv_dev->devid, fid, tsid);
        return -EINVAL;
#endif
    }
    if (logic_cq->init == CALLBACK_INIT) {
#ifndef TSDRV_UT
        TSDRV_PRINT_WARN("logic cq init repeate.(devid:%u fid:%u tsid:%u)\n", tsdrv_dev->devid, fid, tsid);
        return 0;
#endif
    }

    ka_task_spin_lock_init(&logic_cq->lock);
    KA_INIT_LIST_HEAD(&logic_cq->cq_free_list);
    KA_INIT_LIST_HEAD(&logic_cq->cq_alloc_list);
    logic_cq->plat_type = (u32)tsdrv_get_env_type();

    ka_task_spin_lock_bh(&logic_cq->lock);
    logic_cq->devid = tsdrv_dev->devid;
    logic_cq->fid = fid;
    logic_cq->tsid = tsid;
    /* logic_cq init */
    for (i = 0; i < DEVDRV_CB_CQ_MAX_NUM; i++) {
        logic_cq->sub_cq[i].useflag = CALLBACK_CQ_IS_FREE;
        logic_cq->sub_cq[i].size = DEVDRV_MAX_CBCQ_SIZE;
        logic_cq->sub_cq[i].depth = DEVDRV_MAX_CBCQ_DEPTH;
        logic_cq->sub_cq[i].cqid = i;
        logic_cq->sub_cq[i].vaddr = NULL;
        ka_list_add_tail(&logic_cq->sub_cq[i].cq_list, &logic_cq->cq_free_list);
    }
    logic_cq->init = CALLBACK_INIT;
    ka_task_spin_unlock_bh(&logic_cq->lock);

    return 0;
}

STATIC void callback_logic_cq_exit(struct tsdrv_device *tsdrv_dev, u32 fid, u32 tsid)
{
    struct callback_logic_cq *logic_cq = NULL;
    u32 i;

    logic_cq = callback_logic_cq_get(tsdrv_dev->devid, fid, tsid);
    if (logic_cq == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("callback info is null.(devid:%u fid:%u tsid:%u)\n", tsdrv_dev->devid, fid, tsid);
        return;
#endif
    }

    ka_task_spin_lock_bh(&logic_cq->lock);
    if (logic_cq->init == CALLBACK_UNINIT) {
#ifndef TSDRV_UT
        ka_task_spin_unlock_bh(&logic_cq->lock);
        return;
#endif
    }

    logic_cq->init = CALLBACK_UNINIT;
    for (i = 0; i < DEVDRV_CB_CQ_MAX_NUM; i++) {
        if (logic_cq->sub_cq[i].vaddr != NULL) {
#ifndef TSDRV_UT
            callback_kvfree(logic_cq->sub_cq[i].vaddr);
#endif
            logic_cq->sub_cq[i].vaddr = NULL;
        }
        ka_list_del(&logic_cq->sub_cq[i].cq_list);
    }

    ka_task_spin_unlock_bh(&logic_cq->lock);
}

void callback_physic_sqcq_pre_init(struct tsdrv_device *tsdrv_dev, u32 fid, u32 tsid)
{
    struct callback_phy_sqcq *phy_sqcq = callback_physic_sqcq_get(tsdrv_dev->devid, fid, tsid);

    if (phy_sqcq != NULL) {
#ifndef CFG_TSDRV_VM_ENV
        /* In vm, using vpc to send task not db addr. */
        phy_sqcq->doorbell_vaddr = tsdrv_get_db_hwinfo_t(tsdrv_dev->devid, tsid, TSDRV_SQ_DB)->dbs;
#endif
        phy_sqcq->devid = tsdrv_dev->devid;
        phy_sqcq->fid = fid;
        phy_sqcq->tsid = tsid;
        phy_sqcq->plat_type = (u32)tsdrv_get_env_type();
#ifdef CFG_SOC_PLATFORM_MINIV2
        phy_sqcq->board_type = SOC_PLATFORM_MINIV2;
#else
        phy_sqcq->board_type = SOC_NORMAL_BOARD;
#endif
        phy_sqcq->send_flag = 0;
        ka_task_spin_lock_init(&phy_sqcq->cb_sq.lock);
        ka_task_spin_lock_init(&phy_sqcq->cb_cq.lock);
    }

    return;
}

STATIC int callback_alloc_phy_sqcq_mbox(struct callback_phy_sqcq *phy_sqcq)
{
    int ret;

    if (phy_sqcq->send_flag != CALLBACK_MBOX_SENDED) {
        if ((phy_sqcq->cb_sq.paddr == 0) || (phy_sqcq->cb_cq.paddr == 0)) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("Callback paddr invalid. (devid=%u; fid=%u; tsid=%u)\n",
                phy_sqcq->devid, phy_sqcq->fid, phy_sqcq->tsid);
            return -EINVAL;
#endif
        }
        ret = callback_mbox_send(CALLBACK_MBOX_SQCQ_ALLOC, phy_sqcq);
        if (ret != 0) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("Notify ts phy sqcq fail. (devid=%u; fid=%u; tsid=%u)\n",
                phy_sqcq->devid, phy_sqcq->fid, phy_sqcq->tsid);
            return ret;
#endif
        }
        phy_sqcq->send_flag = CALLBACK_MBOX_SENDED;
    }

    return 0;
}

s32 callback_physic_sqcq_init(struct tsdrv_device *tsdrv_dev, u32 fid, u32 tsid)
{
    struct callback_phy_sqcq *phy_sqcq = NULL;
    s32 ret;

    phy_sqcq = callback_physic_sqcq_get(tsdrv_dev->devid, fid, tsid);
    if (phy_sqcq == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("callback info is null.(devid:%u fid:%u tsid:%u)\n", tsdrv_dev->devid, fid, tsid);
        return -EINVAL;
#endif
    }

    ka_task_mutex_lock(&g_cb_info[tsdrv_dev->devid][fid][tsid]->mutex_t);
    if (phy_sqcq->init == CALLBACK_INIT) {
        ka_task_mutex_unlock(&g_cb_info[tsdrv_dev->devid][fid][tsid]->mutex_t);
        return 0;
    }

    /* gernerate sq */
    ret = callback_generate_sq(phy_sqcq);
    if (ret != 0) {
#ifndef TSDRV_UT
        ka_task_mutex_unlock(&g_cb_info[tsdrv_dev->devid][fid][tsid]->mutex_t);
        TSDRV_PRINT_WARN("gernerate sq fail.(devid:%u fid:%u tsid:%u)\n", tsdrv_dev->devid, fid, tsid);
        return ret;
#endif
    }

    /* alloc cq */
    ret = callback_generate_cq(phy_sqcq);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("gernerate cq fail.(devid:%u fid:%u tsid:%u)\n", tsdrv_dev->devid, fid, tsid);
        goto err_free_sq;
#endif
    }

    ret = callback_alloc_phy_sqcq_mbox(phy_sqcq);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Failed to send mbox. (devid=%u; fid=%u; tsid=%u)\n", tsdrv_dev->devid, fid, tsid);
        goto err_send_mbox;
#endif
    }

    phy_sqcq->init = CALLBACK_INIT;
    ka_task_mutex_unlock(&g_cb_info[tsdrv_dev->devid][fid][tsid]->mutex_t);

    TSDRV_PRINT_INFO("devid:%u fid:%u tsid:%u cqid:%u physic sqcq init success.\n", tsdrv_dev->devid, fid,
        tsid, phy_sqcq->cb_cq.index);
    return 0;

#ifndef TSDRV_UT
err_send_mbox:
    callback_destory_cq(phy_sqcq);
err_free_sq:
    callback_destory_sq(phy_sqcq);
    ka_task_mutex_unlock(&g_cb_info[tsdrv_dev->devid][fid][tsid]->mutex_t);
    return ret;
#endif
}

void callback_physic_sqcq_exit(struct tsdrv_device *tsdrv_dev, u32 fid, u32 tsid)
{
    struct callback_phy_sqcq *phy_sqcq = NULL;
    struct callback_logic_cq *logic_cq = NULL;
    int timeout_cnt = 0;

    logic_cq = callback_logic_cq_get(tsdrv_dev->devid, fid, tsid);
    if (logic_cq != NULL) {
#ifndef TSDRV_UT
        while (1) {
            if (timeout_cnt > 300) { /* wait 300 times */
                TSDRV_PRINT_ERR("Wait callback logic cq free timeout. (devid=%u; fid=%u; tsid=%u)\n",
                    tsdrv_dev->devid, fid, tsid);
                return;
            }
            ka_task_spin_lock_bh(&logic_cq->lock);
            if (ka_list_empty_careful(&logic_cq->cq_alloc_list)) {
                ka_task_spin_unlock_bh(&logic_cq->lock);
                break;
            }
            ka_task_spin_unlock_bh(&logic_cq->lock);
            ka_system_msleep(10);    /* 10 ms */
            timeout_cnt++;
        }
#endif
    }

    phy_sqcq = callback_physic_sqcq_get(tsdrv_dev->devid, fid, tsid);
    if (phy_sqcq == NULL) {
        TSDRV_PRINT_ERR("callback info is null.(devid:%u fid:%u tsid:%u)\n", tsdrv_dev->devid, fid, tsid);
        return;
    }

    ka_task_mutex_lock(&g_cb_info[tsdrv_dev->devid][fid][tsid]->mutex_t);
    if (phy_sqcq->init == CALLBACK_UNINIT) {
        ka_task_mutex_unlock(&g_cb_info[tsdrv_dev->devid][fid][tsid]->mutex_t);
        return;
    }
    phy_sqcq->init = CALLBACK_UNINIT;
    if (phy_sqcq->send_flag == CALLBACK_MBOX_SENDED) {
        (void)callback_mbox_send(CALLBACK_MBOX_SQCQ_FREE, phy_sqcq);
        phy_sqcq->send_flag = 0;
    }
    callback_destory_cq(phy_sqcq);
    callback_destory_sq(phy_sqcq);

    ka_task_mutex_unlock(&g_cb_info[tsdrv_dev->devid][fid][tsid]->mutex_t);

    TSDRV_PRINT_INFO("devid:%u fid:%u tsid:%u cqid:%u\n", tsdrv_dev->devid, fid, tsid, phy_sqcq->cb_cq.index);
    return;
}

int callback_ctx_init(struct tsdrv_ctx *ctx, u32 tsnum)
{
    u32 tsid;
    u32 gid;

    for (tsid = 0; tsid < tsnum; tsid++) {
        for (gid = 0; gid < DEVDRV_CBCQ_MAX_GID; gid++) {
            ctx->ts_ctx[tsid].cbcq_wait_flag[gid] = 0;
            ka_task_init_waitqueue_head(&ctx->ts_ctx[tsid].cbcq_wait[gid]);
        }
    }
    return 0;
}

void callback_ctx_exit(struct tsdrv_ctx *ctx, u32 tsnum)
{
}

void callback_free_one_cq(struct callback_logic_cq *logic_cq,
    struct callback_phy_sqcq *phy_sqcq, struct callback_logic_cq_sub *sub_cq)
{
    s32 ret;

    /* Check if the cbcq is blocking the physical queue */
    if (callback_queue_full(sub_cq->head, sub_cq->tail, sub_cq->depth)) {
        task_dispatch_handler(logic_cq->devid, logic_cq->fid, logic_cq->tsid);
    }

    ret = callback_mbox_logic_cq(DEVDRV_MAILBOX_RELEASE_LOGIC_CBCQ, phy_sqcq, sub_cq);
    if (ret != 0) {
        TSDRV_PRINT_ERR("notify ts logic cq(%u) release fail.(devid:%u fid:%u tsid:%u)\n", sub_cq->cqid,
            logic_cq->devid, logic_cq->fid, logic_cq->tsid);
    }

    ka_task_spin_lock_bh(&logic_cq->lock);
    callback_abandon_report(phy_sqcq, sub_cq->cqid);
    sub_cq->useflag = CALLBACK_CQ_IS_FREE;
    ka_list_del(&sub_cq->cq_list);
    callback_wake_wait_event(logic_cq->tsid, logic_cq, sub_cq);
    ka_task_spin_unlock_bh(&logic_cq->lock);

    callback_free_logic_cq(sub_cq);

    ka_task_spin_lock_bh(&logic_cq->lock);
    ka_list_add_tail(&sub_cq->cq_list, &logic_cq->cq_free_list);
    ka_task_spin_unlock_bh(&logic_cq->lock);
}

void callback_recycle_logic_cq(u32 devid, u32 fid, u32 tsid, struct tsdrv_ctx *ctx)
{
    struct callback_logic_cq_sub *sub_cq = NULL;
    struct callback_logic_cq *logic_cq = NULL;
    struct callback_phy_sqcq *phy_sqcq = NULL;
    u32 i;

    if ((devid >= TSDRV_MAX_DAVINCI_NUM) || (ctx == NULL) || (fid >= TSDRV_MAX_FID_NUM)
        || (tsid >= DEVDRV_MAX_TS_NUM)) {
        TSDRV_PRINT_ERR("invalid parameter.(devid:%u fid:%u tsid:%u)\n", devid, fid, tsid);
        return ;
    }

    callback_info_print(devid, fid, tsid);
    logic_cq = callback_logic_cq_get(devid, fid, tsid);
    phy_sqcq = callback_physic_sqcq_get(devid, fid, tsid);
    if ((logic_cq == NULL) || (phy_sqcq == NULL)) {
        TSDRV_PRINT_ERR("callback info is null.(devid:%u fid:%u tsid:%u)\n", devid, fid, tsid);
        return;
    }
    for (i = 0; i < DEVDRV_CB_CQ_MAX_NUM; i++) {
        ka_task_spin_lock_bh(&logic_cq->lock);
        sub_cq = &logic_cq->sub_cq[i];
        if ((sub_cq->useflag == CALLBACK_CQ_IS_FREE) || (sub_cq->vpid != ctx->pid)) {
            ka_task_spin_unlock_bh(&logic_cq->lock);
            continue;
        }
        sub_cq->useflag = CALLBACK_CQ_IS_FREE;
        ka_task_spin_unlock_bh(&logic_cq->lock);
        callback_free_one_cq(logic_cq, phy_sqcq, sub_cq);
    }
}

STATIC int callback_g_info_init(u32 devid, u32 fid, u32 tsid)
{
    if (g_cb_info[devid][fid][tsid] == NULL) {
        g_cb_info[devid][fid][tsid] = tsdrv_kzalloc(sizeof(struct callback_info), KA_GFP_KERNEL);
        if (g_cb_info[devid][fid][tsid] == NULL) {
            TSDRV_PRINT_ERR("g_cb_info kmalloc failed.(devid:%u fid:%u tsid:%u)\n", devid, fid, tsid);
            return -ENOMEM;
        }
    }

    ka_task_mutex_init(&g_cb_info[devid][fid][tsid]->mutex_t);
    TSDRV_PRINT_DEBUG("g_cb_info init success.(devid:%u fid:%u tsid:%u)\n", devid, fid, tsid);

    return 0;
}

STATIC void callback_g_info_exit(u32 devid, u32 fid, u32 tsid)
{
    if (g_cb_info[devid][fid][tsid] != NULL) {
        ka_task_mutex_destroy(&g_cb_info[devid][fid][tsid]->mutex_t);
        tsdrv_kfree(g_cb_info[devid][fid][tsid]);
        g_cb_info[devid][fid][tsid] = NULL;
        TSDRV_PRINT_DEBUG("g_cb_info exit success.(devid:%u fid:%u tsid:%u)\n", devid, fid, tsid);
    }

    return;
}

void callback_g_info_exit_all_fids(u32 devid, u32 tsnum)
{
    u32 fid, tsid;

    for (fid = 0; fid < TSDRV_MAX_FID_NUM; fid++) {
        for (tsid = 0; tsid < tsnum; tsid++) {
            callback_g_info_exit(devid, fid, tsid);
        }
    }
    return;
}

s32 tsdrv_callback_init(struct tsdrv_device *tsdrv_dev, u32 fid, u32 tsid)
{
    s32 ret;

    if (tsdrv_dev->devid >= TSDRV_MAX_DAVINCI_NUM) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid parameter.\n");
        return -EINVAL;
#endif
    }
    if ((tsid >= DEVDRV_MAX_TS_NUM) || (fid >= TSDRV_MAX_FID_NUM)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid tsid(%u) fid(%u)\n", tsid, fid);
        return -EINVAL;
#endif
    }
    ret = callback_g_info_init(tsdrv_dev->devid, fid, tsid);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("g_cb_info init fail.(devid:%u fid:%u tsid:%u)\n", tsdrv_dev->devid, fid, tsid);
        return -EINVAL;
#endif
    }

    ret = callback_logic_cq_init(tsdrv_dev, fid, tsid);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("logic cq init fail.(devid:%u fid:%u tsid:%u)\n", tsdrv_dev->devid, fid, tsid);
        goto err_return;
#endif
    }

    callback_physic_sqcq_pre_init(tsdrv_dev, fid, tsid);
    ret = callback_physic_sqcq_init(tsdrv_dev, fid, tsid);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Failed to init cpu task chan. (devid=%u; tsid=%u)\n", tsdrv_dev->devid, tsid);
        goto err_physic_sqcq_init;
#endif
    }
    TSDRV_PRINT_INFO("init successful.(devid:%u fid:%u tsid:%u)\n", tsdrv_dev->devid, fid, tsid);
    return 0;
#ifndef TSDRV_UT
err_physic_sqcq_init:
    callback_logic_cq_exit(tsdrv_dev, fid, tsid);
#endif
err_return:
    callback_g_info_exit(tsdrv_dev->devid, fid, tsid);
    return ret;
}

void callback_exit(struct tsdrv_device *tsdrv_dev, u32 fid, u32 tsid)
{
    if ((tsdrv_dev->devid >= TSDRV_MAX_DAVINCI_NUM) || (fid >= TSDRV_MAX_FID_NUM) ||
        (tsid >= DEVDRV_MAX_TS_NUM)) {
        TSDRV_PRINT_ERR("invalid parameter.(devid:%u fid:%u tsid:%u)\n", tsdrv_dev->devid, fid, tsid);
        return;
    }

    callback_logic_cq_exit(tsdrv_dev, fid, tsid);
    callback_physic_sqcq_exit(tsdrv_dev, fid, tsid);
    callback_g_info_exit(tsdrv_dev->devid, fid, tsid);

    TSDRV_PRINT_DEBUG("callback exit success.(devid:%u fid:%u tsid:%u)\n", tsdrv_dev->devid, fid, tsid);
    return;
}

int callback_dev_init(u32 devid, u32 fid, u32 tsnum)
{
    struct tsdrv_device *tsdrv_dev = tsdrv_get_dev(devid);
    u32 tsid;
    int err;
    u32 i;

    for (tsid = 0; tsid < tsnum; tsid++) {
        err = tsdrv_callback_init(tsdrv_dev, fid, tsid);
        if (err != 0) {
            TSDRV_PRINT_ERR("callback init fail, devid(%u) tsid(%u)\n", devid, tsid);
            goto err_callback_init;
        }
    }
    return 0;
err_callback_init:
    for (i = 0; i < tsid; i++) {
        callback_exit(tsdrv_dev, fid, i);
    }
    return -ENODEV;
}

void callback_dev_exit_for_container(u32 devid, u32 fid, u32 tsnum)
{
    struct tsdrv_device *tsdrv_dev = tsdrv_get_dev(devid);
    u32 tsid;

    for (tsid = 0; tsid < tsnum; tsid++) {
        callback_logic_cq_exit(tsdrv_dev, fid, tsid);
        callback_physic_sqcq_exit(tsdrv_dev, fid, tsid);
        /* g_cb_info cannot be freed here, because devdrv_find_cq_index may still be used
         * g_cb_info will be freed in devdrv_drv_unregister.
         */
    }
}

void callback_dev_exit(u32 devid, u32 fid, u32 tsnum)
{
    struct tsdrv_device *tsdrv_dev = tsdrv_get_dev(devid);
    u32 tsid;

    for (tsid = 0; tsid < tsnum; tsid++) {
        callback_exit(tsdrv_dev, fid, tsid);
    }
}
