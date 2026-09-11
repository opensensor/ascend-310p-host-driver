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

#ifndef EVENT_SCHED_UT

#include "securec.h"

#include "ka_barrier_pub.h"
#include "ka_memory_pub.h"
#include "esched.h"
#include "topic_sched_drv_common.h"
#include "esched_drv_adapt.h"
#include "esched_fops.h"

#include "tsdrv_interface.h"
#include "trs_chan.h"
#include "devdrv_interface.h"
#include "hwts_task_info.h"

struct sched_hard_res *esched_get_hard_res(u32 chip_id)
{
    struct sched_numa_node *node = sched_get_numa_node(chip_id);
    if (node == NULL) {
        sched_warn("node is null, chip_id (%u).\n", chip_id);
        return NULL;
    }

    return &node->hard_res;
}

struct topic_data_chan *esched_drv_get_topic_chan(u32 dev_id, u32 chan_id)
{
    struct sched_numa_node *node = sched_get_numa_node(dev_id);

    if (chan_id >= TOPIC_SCHED_MAX_CHAN_NUM) {
        sched_err("Invalid topic chan id (dev_id=%u; chan_id=%u).\n", dev_id, chan_id);
        return NULL;
    }

    return node->hard_res.topic_chan[chan_id];
}

STATIC bool esched_can_report_silent_fault(u16 kernel_type, u32 result)
{
    if (((kernel_type == TOPIC_SCHED_CUSTOM_KERNEL_TYPE) || (kernel_type == TOPIC_SCHED_CUSTOM_KFC_KERNEL_TYPE))
        && (result == TOPIC_SCHED_SLIENT_FAULT)) {
#ifndef EMU_ST
        sched_err("Cp2 not allow report silent fault\n");
        return false;
#endif
    }
    return true;
}

STATIC int esched_drv_ack(u32 devid, u32 subevent_id, const char *msg, u32 msg_len, void *priv)
{
    struct hwts_response *resp = (struct hwts_response *)msg;
    struct topic_data_chan *topic_chan = NULL;
    struct topic_sched_mailbox *mb = (struct topic_sched_mailbox *)priv;
    u32 status;

    if (msg_len < sizeof(struct hwts_response)) {
        sched_err("The msg is invalid. (length=%u)\n", msg_len);
        return DRV_ERROR_PARA_ERROR;
    }

    if (!esched_can_report_silent_fault(mb->kernel_type, resp->result)) {
#ifndef EMU_ST
        return DRV_ERROR_PARA_ERROR;
#endif
    }

    if (resp->status >= (unsigned int)TASK_STATUS_MAX) {
        sched_err("The variable status is invalid. (status=%u)\n", resp->status);
        return DRV_ERROR_PARA_ERROR;
    }

    topic_chan = esched_drv_get_topic_chan(devid, mb->mailbox_id);
    if (topic_chan == NULL) {
        sched_err("Get topic_chan failed. (devid=%u)\n", devid);
        return DRV_ERROR_PARA_ERROR;
    }

    if (topic_chan->report_flag == SCHED_DRV_REPORT_GET_EVENT) {
        esched_drv_get_status_report(topic_chan, TOPIC_FINISH_STATUS_NORMAL);
    }

    if (resp->status == TASK_SUCC) {
        status = TOPIC_FINISH_STATUS_NORMAL;
    } else if (resp->status == TASK_OVERFLOW) {
        status = TOPIC_FINISH_STATUS_WARNING;
    } else {
        status = TOPIC_FINISH_STATUS_EXCEPTION;
    }

    sched_debug("Show details. (mb_id=%u; result=%u, status=%u; serial_no=%llu)\n",
        topic_chan->mb_id, resp->result, status, resp->serial_no);

    esched_drv_cpu_report(topic_chan, resp->result, status);
    topic_chan->report_flag = SCHED_DRV_REPORT_ACK;

    return 0;
}

void esched_drv_uninit_non_sched_task_submit_chan(u32 chip_id)
{
    struct trs_id_inst inst = {chip_id, 0};
    struct sched_hard_res *res = esched_get_hard_res(chip_id);

    if (res->rtsq.non_sched_rtsq.chan_id != TRS_INVALID_CHAN_ID) {
        hal_kernel_trs_chan_destroy(&inst, res->rtsq.non_sched_rtsq.chan_id);
        res->rtsq.non_sched_rtsq.chan_id = TRS_INVALID_CHAN_ID;
    }

    sched_info("Destroy non sched task submit chan. (chip_id=%u)\n", chip_id);
}

void esched_drv_uninit_sched_task_submit_chan(u32 chip_id)
{
    struct trs_id_inst inst = {chip_id, 0};
    struct sched_hard_res *res = esched_get_hard_res(chip_id);
    u32 i, j;

    for (i = 0; i < TOPIC_SCHED_RTSQ_CLASS_NUM; i++) {
        for (j = 0; j < res->rtsq.sched_rtsq[i].rtsq_num; j++) {
            if (res->rtsq.sched_rtsq[i].sqe_submit[j].chan_id == TRS_INVALID_CHAN_ID) {
                continue;
            }

            /* In the VF scenario, multiple submit handles may share the same chan entity. */
            if (res->rtsq.sched_rtsq[i].sqe_submit[j].need_destroy) {
                hal_kernel_trs_chan_destroy(&inst, res->rtsq.sched_rtsq[i].sqe_submit[j].chan_id);
            }
            res->rtsq.sched_rtsq[i].sqe_submit[j].chan_id = TRS_INVALID_CHAN_ID;
        }
    }

    sched_info("Destroy sched task submit chan. (chip_id=%u)\n", chip_id);
}

static void esched_finish_thread_cpu(struct sched_thread_ctx *thread_ctx, struct sched_event *event)
{
    struct sched_cpu_ctx *cpu_ctx = NULL;

    ka_task_mutex_lock(&thread_ctx->thread_mutex);
    ka_task_spin_lock_bh(&thread_ctx->thread_finish_lock);
    if ((thread_ctx->event == event) && (event != NULL)) {
        sched_thread_finish(thread_ctx, SCHED_TASK_FINISH_SCENE_TIME_OUT);
    }
    ka_task_spin_unlock_bh(&thread_ctx->thread_finish_lock);
    ka_task_mutex_unlock(&thread_ctx->thread_mutex);

    if (thread_ctx->grp_ctx->sched_mode == SCHED_MODE_SCHED_CPU) {
        cpu_ctx = sched_get_cpu_ctx(thread_ctx->grp_ctx->proc_ctx->node, thread_ctx->bind_cpuid);
        ka_task_spin_lock_bh(&cpu_ctx->sched_lock);
        ka_base_atomic_set(&thread_ctx->status, SCHED_THREAD_STATUS_IDLE);
        esched_cpu_idle(cpu_ctx);
        ka_task_spin_unlock_bh(&cpu_ctx->sched_lock);
    } else {
#ifndef EMU_ST
        ka_base_atomic_set(&thread_ctx->status, SCHED_THREAD_STATUS_IDLE);
#endif
    }
}

static void esched_drv_finish_task_inner(struct topic_data_chan *topic_chan)
{
    struct sched_proc_ctx *proc_ctx = NULL;
    struct sched_grp_ctx *grp_ctx = NULL;
    struct sched_thread_ctx *thread_ctx = NULL;
    struct sched_event_list *event_list = NULL;
    struct sched_event *event = NULL, *match_event = NULL, *tmp = NULL;
    u32 i = 0;

    event = topic_chan->event;
    if (event == NULL) {
        sched_info("Topic_chan got none event.\n");
        return;
    }

    proc_ctx = esched_chip_proc_get(topic_chan->hard_res->dev_id, event->pid);
    if (proc_ctx == NULL) {
        sched_info("None such proc_ctx. (chip_id=%u; pid=%d)\n", topic_chan->hard_res->dev_id, event->pid);
        return;
    }

    grp_ctx = sched_get_grp_ctx(proc_ctx, event->gid);
    if (grp_ctx->sched_mode == SCHED_MODE_SCHED_CPU) {
        event_list = sched_get_sched_event_list(proc_ctx->node, proc_ctx->pri, proc_ctx->event_pri[event->event_id]);
    } else {
        event_list = sched_get_non_sched_event_list(grp_ctx, proc_ctx->event_pri[event->event_id]);
    }

    ka_task_spin_lock_bh(&event_list->lock);
    ka_list_for_each_entry_safe(match_event, tmp, &event_list->head, list) {
        if (match_event == event) { /* event not sched to thread_ctx yet, only need list_del */
            ka_list_del(&event->list);
            event_list->cur_num--;
            event_list->sched_num++;
            ka_task_spin_unlock_bh(&event_list->lock);
            (void)sched_event_enque_lock(event->que, event);
            esched_chip_proc_put(proc_ctx);
            return;
        }
    }
    ka_task_spin_unlock_bh(&event_list->lock);

    for (i = 0; i < grp_ctx->cfg_thread_num; i++) {
        thread_ctx = sched_get_thread_ctx(grp_ctx, i);
        if (thread_ctx->event == event) {
            break;
        }
    }

    if (thread_ctx != NULL) {
        esched_finish_thread_cpu(thread_ctx, event);
    }

    esched_chip_proc_put(proc_ctx);
    esched_drv_mb_intr_enable(topic_chan);
}

static void esched_drv_finish_sched_task(struct topic_data_chan *topic_chan)
{
    struct sched_cpu_ctx *cpu_ctx = topic_chan->cpu_ctx;
    struct sched_thread_ctx *thread_ctx = NULL;

#ifdef CFG_FEATURE_HARD_SOFT_SCHED
    if (!esched_drv_cpu_is_hw_sched_mode(topic_chan)) {
        esched_drv_finish_task_inner(topic_chan);
        return;
    }
#endif

    thread_ctx = esched_cpu_cur_thread_get(cpu_ctx);
    if (thread_ctx == NULL) {
        sched_info("None thread_ctx founded.\n");
        return;
    }

    esched_finish_thread_cpu(thread_ctx, thread_ctx->event);
    esched_cpu_cur_thread_put(thread_ctx);
    esched_drv_mb_intr_enable(topic_chan);
}

static void esched_drv_finish_non_sched_task(struct topic_data_chan *topic_chan)
{
    esched_drv_finish_task_inner(topic_chan);
}

int esched_drv_abnormal_task_handle(struct trs_id_inst *inst, u32 sqid, void *sqe, void *info)
{
    struct aicpu_task_info *task_info = (struct aicpu_task_info *)info;
    struct sched_numa_node *node = NULL;
    struct topic_data_chan *topic_chan = NULL;
    struct sched_cpu_ctx *cpu_ctx = NULL;
    u32 i, chan_id;

    sched_info("Handle abnormal task. (devid=%u; sqid=%u; mb_bitmap=0x%llx)\n",
        inst->devid, sqid, task_info->mb_bitmap);

    (void)sqe;
    node = esched_dev_get(inst->devid);
    if (node == NULL) {
        sched_err("Get dev failed. (devid=%u)\n", inst->devid);
        return DRV_ERROR_NO_DEVICE;
    }

    for (i = 0; i < node->hard_res.aicpu_chan_num; i++) {
        chan_id = node->hard_res.aicpu_chan_start_id + i;
        if (((task_info->mb_bitmap) & (1U << chan_id)) == 0) {
            continue;
        }

        topic_chan = node->hard_res.topic_chan[chan_id];
        if ((topic_chan == NULL) || esched_drv_is_mb_valid(topic_chan)) { /* mailbox not read by driver yet */
            continue;
        }

        cpu_ctx = topic_chan->cpu_ctx;
        if (cpu_ctx != NULL) { /* aicpu num != 0 */
            esched_drv_finish_sched_task(topic_chan);
        } else { /* aicpu num == 0 */
            esched_drv_finish_non_sched_task(topic_chan);
        }
    }

    esched_dev_put(node);
    sched_info("Handle abnormal task success.\n");

    return DRV_ERROR_NONE;
}

STATIC void esched_drv_set_chan_create_para(u32 pool_id, struct trs_chan_para *para)
{
    para->flag = (0x1U << CHAN_FLAG_ALLOC_SQ_BIT) | (0x1U << CHAN_FLAG_ALLOC_CQ_BIT) |
        (0x1U << CHAN_FLAG_NOTICE_TS_BIT) | (0x1U << CHAN_FLAG_AUTO_UPDATE_SQ_HEAD_BIT);
    para->msg[0] = 0xffff;   /* 0 : streamid */
    para->msg[1] = TOPIC_SCHED_RTSQ_PRI;   /* 1 : rtsq pri */
    para->msg[2] = 0;   /* 2 : ssid */
    para->msg[3] = pool_id;   /* 3 : pool_id */

    para->types.type = CHAN_TYPE_HW;
    para->types.sub_type = CHAN_SUB_TYPE_HW_TOPIC_SCHED;

    para->sq_para.sq_depth = TOPIC_SCHED_TASK_SUBMIT_SQ_DEPTH;
    para->sq_para.sqe_size = TOPIC_SCHED_TASK_SQE_SIZE;

    para->cq_para.cq_depth = TOPIC_SCHED_TASK_SUBMIT_CQ_DEPTH;
    para->cq_para.cqe_size = TOPIC_SCHED_TASK_CQE_SIZE;
}

static u32 esched_drv_non_sched_submit_pool_id(u32 chip_id, u32 pool_id)
{
    return (uda_get_chip_type(chip_id) == HISI_MINI_V3) ? 1U : pool_id;
}

int esched_drv_init_non_sched_task_submit_chan(u32 chip_id, u32 pool_id)
{
    int ret;
    struct trs_chan_para para = {0};
    struct trs_id_inst id_inst = {.devid = chip_id, .tsid = 0};
    struct sched_hard_res *res = esched_get_hard_res(chip_id);
    u32 submit_pool_id = esched_drv_non_sched_submit_pool_id(chip_id, pool_id);
    res->rtsq.non_sched_rtsq.chan_id = TRS_INVALID_CHAN_ID;

    /* alloc rtsq submit task chan for non sched mode, reserve 1 rtsq channel with pri 0 */
    esched_drv_set_chan_create_para(submit_pool_id, &para);
    ret = hal_kernel_trs_chan_create(&id_inst, &para, &res->rtsq.non_sched_rtsq.chan_id);
    if (ret != 0) {
#ifndef EMU_ST
        /* Host may retry. */
        sched_debug("Create task submit chan for non sched mode not success. "
            "(chip_id=%u; pool_id=%u; submit_pool_id=%u; ret=%d)\n", chip_id, pool_id, submit_pool_id, ret);
#endif
        return DRV_ERROR_INNER_ERR;
    }

    sched_debug("Init non sched task submit chan. (chip_id=%u; pool_id=%u; submit_pool_id=%u; submit_chan_id=%d)\n",
        chip_id, pool_id, submit_pool_id, res->rtsq.non_sched_rtsq.chan_id);

    return ret;
}

#ifndef ESCHED_HOST
static int esched_drv_init_sched_task_submit_chan_irq(u32 chip_id, u32 pool_id)
{
    struct trs_chan_para para = {0};
    struct trs_id_inst id_inst = {.devid = chip_id, .tsid = 0};
    struct sched_hard_res *res = esched_get_hard_res(chip_id);
    int ret, chan_id;
    u32 i;

    if (res == NULL) {
        sched_err("Hard res is null. (chip_id=%u; pool_id=%u)\n", chip_id, pool_id);
        return DRV_ERROR_NO_RESOURCES;
    }
    res->rtsq.sched_rtsq[TOPIC_SCHED_RTSQ_FOR_IRQ].rtsq_num = TOPIC_SCHED_RTSQ_NUM_FOR_IRQ;
    res->rtsq.sched_rtsq[TOPIC_SCHED_RTSQ_FOR_IRQ].init_rtsq_index = 0;
    ka_base_atomic_set(&res->rtsq.sched_rtsq[TOPIC_SCHED_RTSQ_FOR_IRQ].cur_rtsq_index, 0);

    for (i = 0; i < TOPIC_SCHED_MAX_RTSQ_NUM_PER_CLASS; i++) {
        res->rtsq.sched_rtsq[TOPIC_SCHED_RTSQ_FOR_IRQ].sqe_submit[i].chan_id = TRS_INVALID_CHAN_ID;
    }
    esched_drv_set_chan_create_para(pool_id, &para);
    ret = hal_kernel_trs_chan_create(&id_inst, &para, &chan_id);
    if (ret != 0) {
        sched_err("Failed to create task submit chan for irq. (chip_id=%u; pool_id=%u; ret=%d)\n",
            chip_id, pool_id, ret);
        return DRV_ERROR_NO_RESOURCES;
    }
    res->rtsq.sched_rtsq[TOPIC_SCHED_RTSQ_FOR_IRQ].sqe_submit[0].chan_id = chan_id;
    res->rtsq.sched_rtsq[TOPIC_SCHED_RTSQ_FOR_IRQ].sqe_submit[0].need_destroy = true;

    sched_info("Init sched task submit chan irq success. (chip_id=%u; pool_id=%u; submit_chan_id=%d)\n",
        chip_id, pool_id, chan_id);

    return 0;
}
#endif

static u32 esched_drv_get_submit_chan_num(u32 aicpu_chan_num, u32 resv_rtsq_num)
{
    return ka_base_min((TOPIC_SCHED_MAX_PRIORITY * aicpu_chan_num), resv_rtsq_num);
}

static int esched_drv_init_sched_task_submit_chan_normal(u32 chip_id, u32 pool_id, u32 chan_num, u32 aicpu_chan_num)
{
    struct trs_chan_para para = {0};
    struct trs_id_inst id_inst = {.devid = chip_id, .tsid = 0};
    struct sched_hard_res *res = esched_get_hard_res(chip_id);
    int chan_id, ret;
    u32 adapt_chan_num = esched_drv_get_submit_chan_num(aicpu_chan_num, chan_num);
    u32 compressed_chan_start = adapt_chan_num - (adapt_chan_num % TOPIC_SCHED_MAX_PRIORITY);
    u32 qos_per_chan, i, j, loops;
    u32 qos = 0;

    qos_per_chan = ((adapt_chan_num % TOPIC_SCHED_MAX_PRIORITY) == 0) ?
        1U : (u32)(KA_BASE_DIV_ROUND_UP(TOPIC_SCHED_MAX_PRIORITY, (adapt_chan_num % TOPIC_SCHED_MAX_PRIORITY)));

    for (i = 0; i < TOPIC_SCHED_MAX_PRIORITY; i++) {
        res->rtsq.sched_rtsq[i].rtsq_num = KA_BASE_DIV_ROUND_UP(adapt_chan_num, TOPIC_SCHED_MAX_PRIORITY);
        res->rtsq.sched_rtsq[i].init_rtsq_index = 0;
        ka_base_atomic_set(&res->rtsq.sched_rtsq[i].cur_rtsq_index, 0);
        for (j = 0; j < TOPIC_SCHED_MAX_RTSQ_NUM_PER_CLASS; j++) {
            res->rtsq.sched_rtsq[i].sqe_submit[j].chan_id = TRS_INVALID_CHAN_ID;
        }
    }

    for (i = 0; i < adapt_chan_num; i++) {
        esched_drv_set_chan_create_para(pool_id, &para);
        ret = hal_kernel_trs_chan_create(&id_inst, &para, &chan_id);
        if (ret != 0) {
            sched_err("Failed to create task submit chan for sched mode. (chip_id=%u; pool_id=%u; i=%u; ret=%d)\n",
                chip_id, pool_id, i, ret);
            return DRV_ERROR_NO_RESOURCES;
        }

        loops = (i < compressed_chan_start) ? 1 : qos_per_chan;
        for (j = 0; j < loops; j++) {
            res->rtsq.sched_rtsq[qos].sqe_submit[res->rtsq.sched_rtsq[qos].init_rtsq_index].need_destroy =
                (j == 0) ? true : false;
            res->rtsq.sched_rtsq[qos].sqe_submit[res->rtsq.sched_rtsq[qos].init_rtsq_index].chan_id = chan_id;
            res->rtsq.sched_rtsq[qos].init_rtsq_index++;
            qos++;
            qos = (qos >= TOPIC_SCHED_MAX_PRIORITY) ? 0 : qos;
        }
    }

    sched_debug("Init sched task submit chan success. (chip_id=%u; pool_id=%u; chan_num=%u; aicpu_chan_num=%u)\n",
        chip_id, pool_id, adapt_chan_num, aicpu_chan_num);

    return 0;
}

int esched_drv_init_sched_task_submit_chan(u32 chip_id, u32 pool_id, u32 available_chan_num, u32 aicpu_chan_num)
{
    int ret;

#ifndef ESCHED_HOST
    ret = esched_drv_init_sched_task_submit_chan_irq(chip_id, pool_id);
    if (ret != 0) {
        sched_err("Failed to init task submit chan for irq. (chip_id=%u; pool_id=%u; ret=%d)\n",
            chip_id, pool_id, ret);
        return ret;
    }
#endif

    ret = esched_drv_init_sched_task_submit_chan_normal(chip_id, pool_id, available_chan_num, aicpu_chan_num);
    if (ret != 0) {
        sched_err("Failed to init task submit chan for normal. "
            "(chip_id=%u; pool_id=%u; available_chan_num=%u; aicpu_chan_num=%u)\n",
            chip_id, pool_id, available_chan_num, aicpu_chan_num);
        esched_drv_uninit_sched_task_submit_chan(chip_id);
        return ret;
    }

    return 0;
}

STATIC u32 esched_get_sched_mode(u32 dst_engine)
{
    if ((dst_engine == (u32)ACPU_DEVICE) || (dst_engine == (u32)ACPU_HOST) || (dst_engine == (u32)ACPU_LOCAL)) {
        return SCHED_MODE_SCHED_CPU;
    } else {
        return SCHED_MODE_NON_SCHED_CPU;
    }
}

STATIC int esched_drv_get_submit_chan(struct sched_rtsq_res *rtsq, u32 dst_engine, u32 qos, int *chan_id)
{
    int rtsq_index;

    if (esched_get_sched_mode(dst_engine) == SCHED_MODE_SCHED_CPU) {
#ifndef ESCHED_HOST
        if (in_softirq()) {
#ifndef EMU_ST
            *chan_id = rtsq->sched_rtsq[TOPIC_SCHED_RTSQ_FOR_IRQ].sqe_submit[0].chan_id;
            return 0;
#endif
        }
#endif
        if (rtsq->sched_rtsq[qos].rtsq_num == 0) {
            return DRV_ERROR_NO_RESOURCES;
        }

        rtsq_index = ka_base_atomic_inc_return(&rtsq->sched_rtsq[qos].cur_rtsq_index) % rtsq->sched_rtsq[qos].rtsq_num;
        *chan_id = rtsq->sched_rtsq[qos].sqe_submit[rtsq_index].chan_id;
    } else {
        *chan_id = rtsq->non_sched_rtsq.chan_id;
    }

    return 0;
}

STATIC int esched_drv_submit_task(u32 devid, int chan_id, u8 *sqe, u32 timeout)
{
    struct trs_id_inst id_inst = {.devid = devid, .tsid = 0};
    struct trs_chan_send_para para;

    para.sqe = sqe;
    para.sqe_num = 1;
    para.timeout = (int)timeout;
    return hal_kernel_trs_chan_send(&id_inst, chan_id, &para);
}

STATIC int esched_drv_submit_normal_event(struct sched_numa_node *node, u32 dst_engine,
    struct topic_sched_sqe *sqe, u32 timeout, int *submit_chan_id)
{
    struct sched_rtsq_res *rtsq = &node->hard_res.rtsq;
    int ret;

    ret = esched_drv_get_submit_chan(rtsq, dst_engine, sqe->qos, submit_chan_id);
    if (ret != 0) {
        return ret;
    }

    ret = esched_drv_submit_task(node->node_id, *submit_chan_id, (u8 *)sqe, timeout);
    if (ret != 0) {
        if (ret == -ENOSPC) {
            ret = DRV_ERROR_QUEUE_FULL;
        } else {
            ret = DRV_ERROR_INNER_ERR;
        }
    }

    return ret;
}

STATIC void esched_drv_try_to_report_get_status(struct topic_data_chan *topic_chan)
{
    if (topic_chan->report_flag == SCHED_DRV_REPORT_GET_EVENT) {
        esched_drv_get_status_report(topic_chan, TOPIC_FINISH_STATUS_NORMAL);
        topic_chan->report_flag = SCHED_DRV_REPORT_NONE;
    }
}

STATIC void esched_drv_try_to_report_wait_status(struct topic_data_chan *topic_chan, u32 status)
{
    if (topic_chan->report_flag == SCHED_DRV_REPORT_NONE) {
        esched_drv_cpu_report(topic_chan, 0, status);
        sched_debug("Report wait mb. (mb_id=%u; status=%u)\n", topic_chan->mb_id, status);
    } else if (topic_chan->report_flag == SCHED_DRV_REPORT_ACK) {
        sched_debug("Wait mb has been acked. (mb_id=%u; status=%u)\n", topic_chan->mb_id, status);
        topic_chan->report_flag = SCHED_DRV_REPORT_NONE;
    } else {
        return;
    }
}

STATIC void esched_drv_check_next_event(struct topic_data_chan *topic_chan)
{
    if (esched_drv_is_mb_valid(topic_chan)) {
        ka_system_tasklet_schedule(&topic_chan->sched_task);
    } else {
        esched_drv_mb_intr_enable(topic_chan);

        if (esched_drv_is_mb_valid(topic_chan)) {
            ka_system_tasklet_schedule(&topic_chan->sched_task);
        }
    }
}

STATIC void esched_drv_finish(struct sched_event_func_info *finish_info, unsigned int finish_scene, void *priv)
{
    u32 devid = finish_info->devid;
    struct topic_sched_mailbox *mb = (struct topic_sched_mailbox *)priv;
    struct topic_data_chan *topic_chan = esched_drv_get_topic_chan(devid, mb->mailbox_id);
    u32 topic_finish_flag = (finish_scene == SCHED_TASK_FINISH_SCENE_NORMAL) ?
        TOPIC_FINISH_STATUS_NORMAL : TOPIC_FINISH_STATUS_EXCEPTION;

    if (topic_chan == NULL) {
        return;
    }

    sched_debug("Proc finish. (devid=%u; mb_id=%u; report_flag=%u; cpu_type=%u; task_finish_scene=%u)\n",
        devid, topic_chan->mb_id, topic_chan->report_flag, topic_chan->mb_type, finish_scene);

    if ((topic_chan->mb_type == CCPU_DEVICE) || (topic_chan->mb_type == CCPU_HOST)) {
        return;
    }

    esched_drv_try_to_report_wait_status(topic_chan, topic_finish_flag);

    /* if none get_next-operation perform later, should exec here, e.g. hw-soft, task exit, aicpu zero */
    if ((!esched_drv_cpu_is_hw_sched_mode(topic_chan)) || (sched_get_numa_node(devid)->sched_cpu_num == 0) ||
        (finish_scene == SCHED_TASK_FINISH_SCENE_PROC_EXIT) || (finish_scene == SCHED_TASK_FINISH_SCENE_TIME_OUT)) {
        /* if thread get event, but none ack invoke, should finish get_mb by wait interface */
        if (topic_chan->report_flag == SCHED_DRV_REPORT_GET_EVENT) {
            esched_drv_try_to_report_get_status(topic_chan);
            esched_drv_try_to_report_wait_status(topic_chan, topic_finish_flag);
        }

        esched_drv_check_next_event(topic_chan);
        return;
    }
}

STATIC void esched_drv_fill_rts_task_event(struct sched_event *event, struct topic_data_chan *topic_chan,
    struct topic_sched_mailbox *mb)
{
    struct topic_sched_rts_task_info *rts_task = (struct topic_sched_rts_task_info *)mb->user_data;
    struct hwts_ts_task *task = (struct hwts_ts_task *)event->msg;

    task->mailbox_id = mb->mailbox_id;
    task->kernel_info.pid = mb->pid;
    task->serial_no = (topic_chan->serial_no)++;
    task->kernel_info.kernel_type = mb->kernel_type;
    task->kernel_info.batchMode = mb->batch_mode;
    task->kernel_info.satMode = mb->sat_mode;
    task->kernel_info.rspMode = mb->rsp_mode;
    task->kernel_info.streamID = mb->stream_id;
    task->kernel_info.kernelName = rts_task->task_name_ptr;
    task->kernel_info.kernelSo = rts_task->task_so_name_ptr;
    task->kernel_info.paramBase = rts_task->para_ptr;
    task->kernel_info.l2Ctrl = rts_task->l2_struct_ptr;
    task->kernel_info.l2VaddrBase = 0;
    task->kernel_info.blockId = mb->blk_id;
    task->kernel_info.blockNum = mb->blk_dim;
    task->kernel_info.l2InMain = 0;
    task->kernel_info.taskID = rts_task->extra_field_ptr;

    event->msg_len = sizeof(struct hwts_ts_task);
}

STATIC int esched_drv_sub_fill_event(u32 cpu_type, struct sched_event *event, struct topic_sched_mailbox *mb,
    struct topic_sched_mailbox *mb_addr)
{
    event->publish_pid = -1;
    event->publish_cpuid = 0;
    event->pid = (int32_t)mb->pid;
    event->gid = mb->gid;
    event->event_id = mb->topic_id;

    if ((cpu_type == ACPU_HOST) && (event->event_id != (u32)EVENT_TS_HWTS_KERNEL)) {
        sched_err("Invalid event_id in host side. (mb_type=%u; event_id=%u)\n", cpu_type, event->event_id);
        return DRV_ERROR_EVENT_NOT_MATCH;
    }

    if ((cpu_type == ACPU_DEVICE) || (cpu_type == ACPU_HOST) || (cpu_type == DCPU_DEVICE)) {
        /* Note the life cycle of the variable that 'priv' points to. */
        event->priv = mb_addr;
        event->event_ack_func = esched_drv_ack;
        event->event_finish_func = esched_drv_finish;
    } else {
        event->event_ack_func = NULL;
        event->event_finish_func = NULL;
    }

    event->event_thread_map = NULL;
    return DRV_ERROR_NONE;
}

STATIC struct sched_event *esched_drv_fill_event(struct topic_data_chan *topic_chan, struct topic_sched_mailbox *mb,
    struct topic_sched_mailbox *mb_addr, struct sched_grp_ctx *grp_ctx, u32 tid)
{
    struct sched_event *event = NULL;
    int ret;
    u32 dst_tid = tid;

    event = sched_alloc_event(grp_ctx->proc_ctx->node);
    if (event == NULL) {
        sched_err("Failed to alloc memory for variable event. (mb_id=%u)\n", topic_chan->mb_id);
        return NULL;
    }

    ret = esched_drv_sub_fill_event(topic_chan->mb_type, event, mb, mb_addr);
    if (ret != DRV_ERROR_NONE) {
        (void)sched_event_enque_lock(event->que, event);
        sched_err("Invalid event_id (mb_type=%u; event_id=%u)\n", topic_chan->mb_type, event->event_id);
        return NULL;
    }

    if (event->event_id == EVENT_CDQ_MSG) {
#ifdef CFG_FEATURE_CDQM
        u32 dev_id = topic_chan->hard_res->dev_id;
        ret = tsdrv_cdqm_get_name_by_cdqid(dev_id, 0, mb->stream_id, event->msg, SCHED_MAX_EVENT_MSG_LEN);
        if (ret < 0) {
            (void)sched_event_enque_lock(event->que, event);
            sched_err("Failed to invoke the tsdrv_cdqm_get_name_by_cdqid to get name. "
                "(chip_id=%u; tsid=%d qid=%u; ret=%d)\n", dev_id, 0, (u32)mb->stream_id, ret);
            return NULL;
        }
        event->subevent_id = tsdrv_cdqm_get_instance_by_cdqid(dev_id, 0, mb->stream_id);
        event->msg_len = (u32)ret;
#else
        (void)sched_event_enque_lock(event->que, event);
        return NULL;
#endif
    } else {
        if (event->event_id == EVENT_TS_CALLBACK_MSG) {
            struct callback_event_info *cb_event_info = (struct callback_event_info *)mb->user_data;
            dst_tid = cb_event_info->cb_groupid;
        }

        if (dst_tid != SCHED_INVALID_TID) {
            if (dst_tid >= grp_ctx->cfg_thread_num) {
                (void)sched_event_enque_lock(event->que, event);
                sched_err("The tid out of group thread range. (tid=%u; max=%u)\n", dst_tid, grp_ctx->cfg_thread_num);
                return NULL;
            }

            ret = sched_event_add_thread(event, dst_tid);
            if (ret != 0) {
                (void)sched_event_enque_lock(event->que, event);
                sched_err("Failed to invoke esched_event_thread_map_add_tid. (ret=%d)\n", ret);
                return NULL;
            }
        }

        event->subevent_id = mb->subtopic_id;
        if ((mb->topic_id == (u32)EVENT_TS_HWTS_KERNEL) && (mb->kernel_type != TOPIC_SCHED_DEFAULT_KERNEL_TYPE)) {
            esched_drv_fill_rts_task_event(event, topic_chan, mb);
        } else {
            event->msg_len = (mb->user_data_len > TOPIC_SCHED_USER_DATA_PAYLOAD_LEN) ?
                TOPIC_SCHED_USER_DATA_PAYLOAD_LEN : mb->user_data_len;
            ka_mm_memcpy_fromio(event->msg, &mb->user_data[0], event->msg_len);

            event->msg_len = mb->user_data_len;
        }
        sched_debug("Show details. (pid=%u; gid=%u; topic_id=%u; subtopic_id=%u; kernel_type=%u; msg_len=%u)\n",
            mb->pid, mb->gid, mb->topic_id, mb->subtopic_id, (u32)mb->kernel_type, event->msg_len);
    }

    /* Performance tuning, recourd publish time */
    event->timestamp.publish_user = sched_get_cur_timestamp();
    event->timestamp.publish_in_kernel = sched_get_cur_timestamp();
    event->timestamp.publish_user_of_day = sched_do_gettimeofday();

    return event;
}

STATIC struct sched_grp_ctx *esched_drv_get_grp(struct sched_proc_ctx *proc_ctx, u32 gid, u32 sched_mode)
{
    struct sched_grp_ctx *grp_ctx = NULL;

    if (gid >= SCHED_MAX_GRP_NUM) {
        sched_err("The gid is out of range. (pid=%d; gid=%u; max=%d)\n", proc_ctx->pid, gid, SCHED_MAX_GRP_NUM);
        return NULL;
    }

    grp_ctx = sched_get_grp_ctx(proc_ctx, gid);
    if (grp_ctx->sched_mode != sched_mode) {
        sched_err("Grp sched_mode does not match. (pid=%d; gid=%u; grp_ctx_sched_mode=%u; sched_mode=%u)\n",
                  proc_ctx->pid, gid, grp_ctx->sched_mode, sched_mode);
        return NULL;
    }

    return grp_ctx;
}

STATIC struct sched_thread_ctx *esched_drv_aicpu_get_thread(struct sched_proc_ctx *proc_ctx,
    u32 gid, u32 cpuid, u32 event_id)
{
    struct sched_grp_ctx *grp_ctx = NULL;
    struct sched_thread_ctx *thread_ctx = NULL;

    if (event_id >= (u32)SCHED_MAX_EVENT_TYPE_NUM) {
        sched_err("The event_id is out of range. (cpuid=%u; pid=%d; gid=%u; event_id=%u; max=%d)\n",
                  cpuid, proc_ctx->pid, gid, event_id, SCHED_MAX_EVENT_TYPE_NUM);
        return NULL;
    }

    grp_ctx = esched_drv_get_grp(proc_ctx, gid, SCHED_MODE_SCHED_CPU);
    if (grp_ctx == NULL) {
        sched_err("Failed to invoke the esched_drv_get_grp. (cpuid=%u; pid=%d; gid=%u)\n", cpuid, proc_ctx->pid, gid);
        return NULL;
    }

    if (grp_ctx->cpuid_to_tid[cpuid] == grp_ctx->cfg_thread_num) {
        sched_err("It has no threads. (cpuid=%u; pid=%d; gid=%u; cfg_thread_num=%u)\n",
            cpuid, proc_ctx->pid, gid, grp_ctx->cfg_thread_num);
        return NULL;
    }

    thread_ctx = sched_get_thread_ctx(grp_ctx, grp_ctx->cpuid_to_tid[cpuid]);
    if (thread_ctx->valid == SCHED_INVALID) {
        sched_err("The thread is invalid. (cpuid=%u; pid=%d; gid=%u)\n", cpuid, proc_ctx->pid, gid);
        return NULL;
    }

    if (thread_ctx->timeout_flag == SCHED_VALID) {
        sched_err("The thread is timed out. (cpuid=%u; pid=%d; gid=%u)\n", cpuid, proc_ctx->pid, gid);
        return NULL;
    }

    if ((thread_ctx->subscribe_event_bitmap & (0x1ULL << event_id)) == 0) {
        sched_err("The thread is not subscribed. (cpuid=%u; pid=%d; gid=%u; event_id=%u)\n",
            cpuid, proc_ctx->pid, gid, event_id);
        return NULL;
    }

    return thread_ctx;
}

#ifdef CFG_FEATURE_HARD_SOFT_SCHED
static inline bool esched_drv_event_need_finish(u32 event_id, u32 cur_event_num)
{
    /* caution: some event id is net-model-perf-sensitive, do not finish by esched self */
    if ((event_id != EVENT_QUEUE_FULL_TO_NOT_FULL) && (event_id != EVENT_QUEUE_EMPTY_TO_NOT_EMPTY) &&
        (event_id != EVENT_DRV_MSG) && !((event_id >= EVENT_USR_START) && (event_id <= EVENT_USR_END))) {
        return false;
    }

    if (cur_event_num >= TOPIC_EVENT_QUEUE_LIMIT) {
        return false;
    }

    return true;
}
#endif

/* aicpu channel scenarios:
   1. Device aicpu(aicpu num != 0) : SCHED;     cpu_ctx != NULL.
   2. Device aicpu(aicpu num == 0) : NON-SCHED; cpu_ctx == NULL.
   3. Host   aicpu(aicpu num != 0) : NON-SCHED; cpu_ctx == NULL. */
static int esched_drv_publish_event_from_topic_aicpu_chan(struct topic_data_chan *topic_chan,
    struct sched_event *event, struct sched_proc_ctx *proc_ctx, struct sched_grp_ctx *grp_ctx)
{
    struct sched_cpu_ctx *cpu_ctx = topic_chan->cpu_ctx;
    struct sched_thread_ctx *thread_ctx = NULL;

    sched_debug("Publish event from TOPIC. (event_id=%u; mb_id=%u; report_flag=%u; cpu_type=%u; sched_mode=%u)\n",
        event->event_id, topic_chan->mb_id, topic_chan->report_flag, topic_chan->mb_type, topic_chan->sched_mode);

    topic_chan->event = event;
    if (grp_ctx->sched_mode == SCHED_MODE_NON_SCHED_CPU) {
        /* Prevent the aicpu sched task from publishing events to non-sched groups. */
        if (cpu_ctx != NULL) {
            sched_err("Aicpu sched task publishing event to non-sched group. (chan_id=%u; pid=%d; gid=%u)\n",
                topic_chan->mb_id, proc_ctx->pid, grp_ctx->gid);
            return DRV_ERROR_GROUP_NON_SCHED;
        }
        return sched_publish_event_to_non_sched_grp(event, grp_ctx);
    }

#ifdef CFG_FEATURE_HARD_SOFT_SCHED
    if (!esched_drv_cpu_is_hw_sched_mode(topic_chan)) {
        if (grp_ctx->sched_mode == SCHED_MODE_SCHED_CPU) {
            bool finish_flag = false;
            int ret = 0;
            finish_flag = esched_drv_event_need_finish(event->event_id,
                (u32)ka_base_atomic_read(&proc_ctx->node->cur_event_num));
            if (finish_flag == true) {
                event->event_finish_func = NULL;
                event->event_ack_func = NULL;
            }

            ret = sched_publish_event_to_sched_grp(event, grp_ctx);
            if ((ret == DRV_ERROR_NONE) && (finish_flag == true)) {
                esched_drv_cpu_report(topic_chan, 0, TOPIC_FINISH_STATUS_NORMAL);
                esched_drv_check_next_event(topic_chan);
            }
            return ret;
        } else {
            sched_err("Group uninit. (chan_id=%u; pid=%d; gid=%u)\n", topic_chan->mb_id, proc_ctx->pid, grp_ctx->gid);
            return DRV_ERROR_UNINIT;
        }
    }
#endif
    if (cpu_ctx == NULL) {
        sched_err("Topic_chan has no cpu_ctx. (chan_id=%u)\n", topic_chan->mb_id);
        return DRV_ERROR_INNER_ERR;
    }

    thread_ctx = esched_drv_aicpu_get_thread(proc_ctx, grp_ctx->gid, cpu_ctx->cpuid, event->event_id);
    if (thread_ctx == NULL) {
        sched_err("Failed to get aicpu thread. (cpuid=%u; pid=%d; gid=%u)\n",
            cpu_ctx->cpuid, proc_ctx->pid, grp_ctx->gid);
        return DRV_ERROR_INNER_ERR;
    }
#ifdef CFG_FEATURE_THREAD_SWAPOUT
    if (thread_ctx->swapout_flag == SCHED_VALID) {
        return DRV_ERROR_TRY_AGAIN;
    }
#endif
    thread_ctx->event = event;
    ka_task_spin_lock_bh(&cpu_ctx->sched_lock);
    esched_cpu_cur_thread_set(cpu_ctx, thread_ctx);
    ka_task_spin_unlock_bh(&cpu_ctx->sched_lock);

    if (event->event_id < (u32)SCHED_MAX_EVENT_TYPE_NUM) {
        (void)sched_grp_event_num_update(grp_ctx, event->event_id);
    }
    sched_wake_up_thread(thread_ctx);
    ka_base_atomic_inc(&proc_ctx->publish_event_num);
    return 0;
}

static inline void esched_drv_updata_cpu_sched_mode(struct sched_cpu_ctx *cpu_ctx)
{
    if (!esched_drv_cpu_is_hw_sched_mode(cpu_ctx->topic_chan)) {
        return;
    }

    sched_info("Change to hw soft sched mode. (devid=%u; cpuid=%u)\n", cpu_ctx->node->node_id, cpu_ctx->cpuid);

    cpu_ctx->topic_chan->sched_mode = ESCHED_DRV_SCHED_MODE_HW_SOFT;
    esched_drv_dev_to_hw_soft_sched_mode(cpu_ctx->topic_chan->hard_res);
}

void esched_aicpu_sched_task(unsigned long data)
{
    struct topic_data_chan *topic_chan = (struct topic_data_chan *)((uintptr_t)data);
    struct topic_data_chan *real_topic_chan = NULL;
    struct topic_sched_mailbox mb_tmp;
    struct topic_sched_mailbox *mb = NULL; /* cpy from io to stack, read faster */
    struct sched_numa_node *node = NULL;
    struct sched_proc_ctx *proc_ctx = NULL;
    struct sched_grp_ctx *grp_ctx = NULL;
    struct sched_event *event = NULL;
    u32 err_code = TOPIC_FINISH_REPORT_ABNORMAL;
    u32 status = TOPIC_FINISH_STATUS_EXCEPTION;
    u32 devid, submit_devid, tid, pid;
    int ret;

    ka_mm_memcpy_fromio(&mb_tmp, topic_chan->wait_mb, sizeof(struct topic_sched_mailbox));
    mb = &mb_tmp;

    /* different vf in vfg use same aicpu */
    devid = esched_get_devid_from_hw_vfid(topic_chan->hard_res->dev_id, mb->vfid, topic_chan->hard_res->sub_dev_num);
    submit_devid = devid;
    tid = SCHED_INVALID_TID;
    pid = SCHED_INVALID_PID;
#ifndef ESCHED_HOST
    ret = esched_restore_mb_user_data(mb, &submit_devid, &tid, &pid);
    if (ret != 0) {
#ifndef EMU_ST
        goto response_report;
#endif
    }
#endif

    sched_debug("Tasklet. (devid=%u; submit_devid=%u; tid=%u; pid=%u, "
        "topic_id=%u, mb_id=%u; mailbox_id=%u; vfid=%u; cpu_type=%u)\n",
        devid, submit_devid, tid, pid,
        mb->topic_id, topic_chan->mb_id, mb->mailbox_id, mb->vfid, topic_chan->mb_type);

    node = esched_dev_get(devid);
    if (node == NULL) {
#ifndef EMU_ST
        sched_debug("Get dev not success. (devid=%u; vfid=%u)\n", devid, mb->vfid);
#endif
        goto response_report;
    }

    /* vf/phy topic_chan */
    real_topic_chan = esched_drv_get_topic_chan(devid, mb->mailbox_id);
    if (real_topic_chan == NULL) {
        sched_err("Get topic_chan failed. (devid=%u; mailbox_id=%u)\n", devid, mb->mailbox_id);
        goto dev_put;
    }

    real_topic_chan->sched_record.schedule_sn++;

    /* If send to a specified thread or the device has been set to the hw+soft scheduling mode,
       the CPU is also set to the hw+soft scheduling mode. */
#ifdef CFG_FEATURE_HARD_SOFT_SCHED
    if ((tid != SCHED_INVALID_TID) || esched_drv_dev_is_hw_soft_sched_mode(&node->hard_res)) {
        esched_drv_updata_cpu_sched_mode(real_topic_chan->cpu_ctx);
    }
#endif

    real_topic_chan->wait_mb_status = TOPIC_SCHED_MB_STATUS_BUSY;
    if (esched_drv_is_sched_mode_change_task(mb->topic_id, mb->subtopic_id)) {
        err_code = 0;
        status = TOPIC_FINISH_STATUS_NORMAL;
        goto dev_put;
    }

    ret = esched_get_real_pid(mb, devid, pid);
    if (ret != 0) {
#ifndef EMU_ST
        sched_debug("Get real pid not success. (dev_id=%u)\n", devid);
#endif
        goto dev_put;
    }

    proc_ctx = esched_proc_get(node, (int32_t)mb->pid);
    if (proc_ctx == NULL) {
#ifndef EMU_ST
        if (!esched_log_limited(SCHED_LOG_LIMIT_GET_PROC_CTX)) {
            sched_warn("Get proc_ctx not success. (devid=%u; pid=%u; topic_id=%u; topic_type=%u;"
                "ts_pid_data=%u; tscb_pid_data=%u; esched_pid_data=%u)\n",
                devid, mb->pid, mb->topic_id, mb->topic_type,
                mb->user_data[TOPIC_SCHED_TS_PID_INDEX], mb->user_data[TOPIC_SCHED_TS_CALLBACK_PID_INDEX],
                (u32)mb->stream_id + (((u32)mb->task_id) << TOPIC_SCHED_ESCHED_PID_HIGH_OFFSET));
        }
#endif
        goto dev_put;
    }

    if (mb->gid >= (unsigned char)SCHED_MAX_GRP_NUM) {
        sched_err("Invalid gid. (mb_id=%u; pid=%u; gid=%u)\n",
            real_topic_chan->mb_id, mb->pid, mb->gid);
        goto proc_put;
    }

    grp_ctx = sched_get_grp_ctx(proc_ctx, mb->gid);
    event = esched_drv_fill_event(real_topic_chan, mb, topic_chan->wait_mb, grp_ctx, tid);
    if (event == NULL) {
        sched_err("Failed to fill the event. (mb_id=%u; pid=%u; gid=%u)\n",
                  real_topic_chan->mb_id, mb->pid, mb->gid);
        goto proc_put;
    }

#ifdef CFG_FEATURE_VFIO
    event->vfid = proc_ctx->vfid;
#endif

    ret = esched_drv_publish_event_from_topic_aicpu_chan(real_topic_chan, event, proc_ctx, grp_ctx);
    if (ret != 0) {
        if (ret == (int)DRV_ERROR_TRY_AGAIN) {
            err_code = 0;
            status = TOPIC_FINISH_STATUS_NORMAL;
        } else {
            sched_err("Failed to publish event. (mb_id=%u; pid=%u; gid=%u; ret=%d)\n",
                real_topic_chan->mb_id, mb->pid, mb->gid, ret);
        }
        (void)sched_event_enque_lock(event->que, event);
        goto proc_put;
    }
    esched_publish_trace_update(proc_ctx, event);
    sched_publish_state_update(node, event, ret);
    esched_proc_put(proc_ctx);
    esched_dev_put(node);
    return;

proc_put:
    esched_proc_put(proc_ctx);

dev_put:
    esched_dev_put(node);

response_report:
    esched_drv_cpu_report(topic_chan, err_code, status);

    /* The hardware continues to report interrupts only after the software reads the get event register. */
    esched_drv_check_next_event(topic_chan);
}

void esched_ccpu_sched_task(unsigned long data)
{
    struct topic_data_chan *topic_chan = (struct topic_data_chan *)((uintptr_t)data);
    struct topic_data_chan *real_topic_chan = NULL;
    struct topic_sched_mailbox mb_tmp;
    struct topic_sched_mailbox *mb = NULL; /* mb context may be refill after report to hardware */
    struct sched_numa_node *node = NULL;
    struct sched_event *event = NULL;
    struct sched_proc_ctx *proc_ctx = NULL;
    struct sched_grp_ctx *grp_ctx = NULL;
    u32 devid, submit_devid, tid, pid;
    int ret;

    ka_mm_memcpy_fromio(&mb_tmp, topic_chan->wait_mb, sizeof(struct topic_sched_mailbox));
    mb = &mb_tmp;

    /* No matter what the processing result is, report success to the hardware first.
       report early for hardware prepare next event */
    esched_drv_cpu_report(topic_chan, 0, TOPIC_FINISH_STATUS_NORMAL);

    /* different vf in vfg use same aicpu */
    devid = esched_get_devid_from_hw_vfid(topic_chan->hard_res->dev_id, mb->vfid, topic_chan->hard_res->sub_dev_num);
    submit_devid = devid;
    tid = SCHED_INVALID_TID;
    pid = SCHED_INVALID_PID;
#ifdef ESCHED_HOST
    ret = esched_restore_mb_user_data(mb, &submit_devid, &tid, &pid);
    if (ret != 0) {
#ifndef EMU_ST
        goto next;
#endif
    }
#endif
    sched_debug("Tasklet. (devid=%u; submit_devid=%u; tid=%u; pid=%u, "
        "topic_id=%u, mb_id=%u; mailbox_id=%u; vfid=%u; cpu_type=%u)\n",
        devid, submit_devid, tid, pid,
        mb->topic_id, topic_chan->mb_id, mb->mailbox_id, mb->vfid, topic_chan->mb_type);

    node = esched_dev_get(submit_devid);
    if (node == NULL) {
#ifndef EMU_ST
        sched_debug("Get dev not success. (devid=%u; vfid=%u)\n", submit_devid, mb->vfid);
#endif
        goto next;
    }

    ret = esched_get_real_pid(mb, devid, pid);
    if (ret != 0) {
#ifndef EMU_ST
        sched_debug("Get real pid not success. (dev_id=%u)\n", devid);
#endif
        goto dev_put;
    }

    proc_ctx = esched_proc_get(node, (int32_t)mb->pid);
    if (proc_ctx == NULL) {
        sched_warn("Get proc_ctx not success. (devid=%u; pid=%u; topic_id=%u; topic_type=%u;"
            "ts_pid_data=%u; tscb_pid_data=%u; esched_pid_data=%u)\n",
            submit_devid, mb->pid, mb->topic_id, mb->topic_type,
            mb->user_data[TOPIC_SCHED_TS_PID_INDEX], mb->user_data[TOPIC_SCHED_TS_CALLBACK_PID_INDEX],
            (u32)mb->stream_id + (((u32)mb->task_id) << TOPIC_SCHED_ESCHED_PID_HIGH_OFFSET));
        goto dev_put;
    }

    grp_ctx = esched_drv_get_grp(proc_ctx, mb->gid, SCHED_MODE_NON_SCHED_CPU);
    if (grp_ctx == NULL) {
        sched_err("Get grp_ctx failed. (mb_id=%u; pid=%u; gid=%u; topic_id=%u)\n",
                  topic_chan->mb_id, mb->pid, mb->gid, mb->topic_id);
        goto proc_put;
    }

    /* vf/phy topic_chan */
    real_topic_chan = esched_drv_get_topic_chan(devid, mb->mailbox_id);
    if (real_topic_chan == NULL) {
        sched_err("Get topic_chan failed. (devid=%u; mailbox_id=%u)\n", devid, mb->mailbox_id);
        goto proc_put;
    }

    event = esched_drv_fill_event(real_topic_chan, mb, topic_chan->wait_mb, grp_ctx, tid);
    if (event == NULL) {
        sched_err("Fill event failed. (mb_id=%u; pid=%u; gid=%u; topic_id=%u)\n",
            real_topic_chan->mb_id, mb->pid, mb->gid, mb->topic_id);
        goto proc_put;
    }

#ifdef CFG_FEATURE_VFIO
    event->vfid = proc_ctx->vfid;
#endif

    ret = sched_publish_event_to_non_sched_grp(event, grp_ctx);
    if (ret != 0) {
        (void)sched_event_enque_lock(event->que, event);
    }
    esched_publish_trace_update(proc_ctx, event);
    sched_publish_state_update(node, event, ret);

proc_put:
    esched_proc_put(proc_ctx);

dev_put:
    esched_dev_put(node);

next:
    /* The hardware reports another event and reschedules the event. */
    esched_drv_check_next_event(topic_chan);
}

#ifndef ESCHED_HOST
STATIC void esched_drv_sched_other_node(u32 chip_id, struct topic_data_chan *topic_chan)
{
    struct sched_numa_node *node = NULL;
    struct topic_sched_mailbox *mb = topic_chan->wait_mb;
    u32 devid = esched_get_devid_from_hw_vfid(chip_id, mb->vfid, topic_chan->hard_res->sub_dev_num);

    node = esched_dev_get(devid);
    if (node == NULL) {
        sched_err("Get dev failed. (devid=%u; vfid=%u)\n", devid, mb->vfid);
#ifndef EMU_ST
        esched_drv_cpu_report(topic_chan, TOPIC_FINISH_REPORT_ABNORMAL, TOPIC_FINISH_STATUS_EXCEPTION);
#endif
        return;
    }

    sched_debug("Sched other node success. (devid=%u; mb_id=%u; vfid=%u)\n", devid, topic_chan->mb_id, mb->vfid);

    ka_system_tasklet_schedule(&topic_chan->sched_task);

    esched_dev_put(node);
}

struct sched_thread_ctx *esched_drv_get_cpu_next_thread(u32 chip_id, u32 vfid, struct sched_cpu_ctx *cpu_ctx)
{
    struct topic_data_chan *topic_chan = cpu_ctx->topic_chan;
    struct topic_sched_mailbox *mb = NULL, mb_tmp;
    struct sched_proc_ctx *proc_ctx = NULL;
    struct sched_thread_ctx *thread_ctx = NULL;
    struct sched_event *event = NULL;

    /* report get and wait status to topic if user didn't reponse ack in split usecase */
    if (topic_chan->report_flag == SCHED_DRV_REPORT_GET_EVENT) {
        esched_drv_try_to_report_get_status(topic_chan);
        esched_drv_try_to_report_wait_status(topic_chan, TOPIC_FINISH_STATUS_NORMAL);
    }

    sched_debug("Get next. (chip_id=%u; mb_id=%u; vfid=%u; get_vfid=%u)\n",
        chip_id, topic_chan->mb_id, topic_chan->wait_mb->vfid, vfid);

again:
    if (!esched_drv_is_mb_valid(topic_chan)) {
        return NULL;
    }

    topic_chan->sched_record.schedule_sn++;

    /* cpy from io to stack, read faster */
    ka_mm_memcpy_fromio(&mb_tmp, topic_chan->wait_mb, sizeof(struct topic_sched_mailbox));
    mb = &mb_tmp;

    if (mb->vfid != vfid) { /* different vf in vfg use same aicpu */
        esched_drv_sched_other_node(chip_id, topic_chan); /* mb valid reg read clear, so we should wakeup other node */
        return NULL;
    }

    topic_chan->wait_mb_status = TOPIC_SCHED_MB_STATUS_BUSY;

    /* After the thread is wakeup, then put the proc */

    if (esched_get_real_pid(mb, cpu_ctx->node->node_id, SCHED_INVALID_PID) != 0) {
#ifndef EMU_ST
        sched_debug("Get real pid not success. (dev_id=%u)\n", cpu_ctx->node->node_id);
        esched_drv_cpu_report(topic_chan, TOPIC_FINISH_REPORT_ABNORMAL, TOPIC_FINISH_STATUS_EXCEPTION);
#endif
        goto again;
    }

    proc_ctx = esched_proc_get(cpu_ctx->node, (int32_t)mb->pid);
    if (proc_ctx == NULL) {
        sched_err("Get proc_ctx failed. (devid=%u; pid=%u; topic_id=%u; topic_type=%u;"
            "ts_pid_data=%u; tscb_pid_data=%u; esched_pid_data=%u)\n",
            chip_id, mb->pid, mb->topic_id, mb->topic_type,
            mb->user_data[TOPIC_SCHED_TS_PID_INDEX], mb->user_data[TOPIC_SCHED_TS_CALLBACK_PID_INDEX],
            (u32)mb->stream_id + (((u32)mb->task_id) << TOPIC_SCHED_ESCHED_PID_HIGH_OFFSET));
#ifndef EMU_ST
        esched_drv_cpu_report(topic_chan, TOPIC_FINISH_REPORT_ABNORMAL, TOPIC_FINISH_STATUS_EXCEPTION);
#endif
        goto again;
    }

    thread_ctx = esched_drv_aicpu_get_thread(proc_ctx, mb->gid, cpu_ctx->cpuid, mb->topic_id);
    if (thread_ctx == NULL) {
        sched_err("Failed to get the thread_ctx. (mb_id=%u; pid=%u; gid=%u; cpuid=%u)\n",
            topic_chan->mb_id, mb->pid, mb->gid, cpu_ctx->cpuid);
        esched_proc_put(proc_ctx);
#ifndef EMU_ST
        esched_drv_cpu_report(topic_chan, TOPIC_FINISH_REPORT_ABNORMAL, TOPIC_FINISH_STATUS_EXCEPTION);
#endif
        goto again;
    }
#ifdef CFG_FEATURE_THREAD_SWAPOUT
    if (thread_ctx->swapout_flag == SCHED_VALID) {
        esched_proc_put(proc_ctx);
#ifndef EMU_ST
        esched_drv_cpu_report(topic_chan, 0, TOPIC_FINISH_STATUS_NORMAL);
#endif
        goto again;
    }
#endif
    event = esched_drv_fill_event(topic_chan, mb, topic_chan->wait_mb, thread_ctx->grp_ctx, SCHED_INVALID_TID);
    if (event == NULL) {
        sched_err("Failed to invoke the esched_drv_fill_event to fill the event. "
            "(mb_id=%u; pid=%u; gid=%u; cpuid=%u)\n",
            topic_chan->mb_id, mb->pid, mb->gid, cpu_ctx->cpuid);
        esched_proc_put(proc_ctx);
#ifndef EMU_ST
        esched_drv_cpu_report(topic_chan, TOPIC_FINISH_REPORT_ABNORMAL, TOPIC_FINISH_STATUS_EXCEPTION);
#endif
        goto again;
    }

#ifdef CFG_FEATURE_VFIO
    event->vfid = proc_ctx->vfid;
#endif
    thread_ctx->event = event;
    ka_base_atomic_inc(&proc_ctx->publish_event_num);
    if (event->event_id < (u32)SCHED_MAX_EVENT_TYPE_NUM) {
        (void)sched_grp_event_num_update(thread_ctx->grp_ctx, event->event_id);
    }
    esched_publish_trace_update(proc_ctx, event);

    sched_debug("End of calling esched_drv_get_next_thread. (mb_id=%u; cpu_type=%u; pid=%u; tid=%u)\n",
        topic_chan->mb_id, topic_chan->mb_type, proc_ctx->pid, thread_ctx->kernel_tid);

    return thread_ctx;
}

STATIC struct sched_thread_ctx *esched_drv_get_next_thread(struct sched_cpu_ctx *cpu_ctx)
{
    return esched_drv_get_cpu_next_thread(cpu_ctx->node->node_id, 0, cpu_ctx);
}

STATIC void esched_drv_cpu_to_idle(struct sched_thread_ctx *thread_ctx, struct sched_cpu_ctx *cpu_ctx)
{
    struct topic_data_chan *topic_chan = cpu_ctx->topic_chan;

    ka_task_spin_lock_bh(&cpu_ctx->sched_lock);
    ka_base_atomic_set(&thread_ctx->status, SCHED_THREAD_STATUS_IDLE);
    esched_cpu_idle(cpu_ctx);
    ka_task_spin_unlock_bh(&cpu_ctx->sched_lock);

    esched_drv_mb_intr_enable(topic_chan);
    if (esched_drv_is_mb_valid(topic_chan)) {
        ka_system_tasklet_schedule(&topic_chan->sched_task);
    }
}

#ifdef CFG_FEATURE_HARD_SOFT_SCHED
static struct topic_data_chan *esched_drv_get_thread_topic_chan(u32 dev_id, struct sched_thread_ctx *thread_ctx)
{
    struct topic_data_chan *topic_chan = NULL;
    struct sched_event *event = NULL;

    event = thread_ctx->event;
    if (event == NULL) {
#ifndef EMU_ST
        sched_warn("No event on thread. (tid=%u, kernel_tid=%u)\n", thread_ctx->tid, thread_ctx->kernel_tid);
        return NULL;
#endif
    }

    topic_chan = esched_drv_get_topic_chan(dev_id, ((struct topic_sched_mailbox *)event->priv)->mailbox_id);
    if (topic_chan == NULL) {
#ifndef EMU_ST
        sched_warn("None such topic chan. (devid=%u, chan_id=%u)\n",
            dev_id, ((struct topic_sched_mailbox *)event->priv)->mailbox_id);
        return NULL;
#endif
    }

    return topic_chan;
}
#endif

STATIC struct sched_event *_esched_drv_get_event(struct topic_data_chan *topic_chan,
    struct sched_thread_ctx *thread_ctx, u32 event_id)
{
    struct sched_proc_ctx *proc_ctx = NULL;
    struct sched_event *event = NULL;
    struct topic_sched_mailbox *mb = NULL, mb_tmp;
#ifndef EMU_ST
    esched_drv_try_to_report_get_status(topic_chan);

    if (!esched_drv_is_get_mb_valid(topic_chan)) {
        sched_debug("Invalid mb. (mb_id=%u, event_id=%u)\n", topic_chan->mb_id, event_id);
        return NULL;
    }

    /* cpy from io to stack, read faster */
    ka_mm_memcpy_fromio(&mb_tmp, topic_chan->get_mb, sizeof(struct topic_sched_mailbox));
    mb = &mb_tmp;

    (void)esched_get_real_pid(mb, topic_chan->hard_res->dev_id, SCHED_INVALID_PID);
    if ((mb->pid != (u32)thread_ctx->grp_ctx->pid) || (mb->gid != thread_ctx->grp_ctx->gid) ||
        (mb->topic_id != event_id)) {
        sched_err("The variable pid, gid or event_id is invalid. "
            "(mb_id=%u; target_pid=%u; mb_gid=%u; mb_event_id=%u; topic_type=%u;"
            "thread_pid=%d; thread_gid=%u; event_id=%u)\n",
            topic_chan->mb_id, mb->pid, mb->gid, mb->topic_id, mb->topic_type,
            thread_ctx->grp_ctx->pid, thread_ctx->grp_ctx->gid, event_id);
        return NULL;
    }

    event = esched_drv_fill_event(topic_chan, mb, topic_chan->get_mb, thread_ctx->grp_ctx, SCHED_INVALID_TID);
    if (event == NULL) {
        sched_err("Failed to invoke the esched_drv_fill_event to fill the event. (mb_id=%u; event_id=%u)\n",
            topic_chan->mb_id, event_id);
        return NULL;
    }

    proc_ctx = thread_ctx->grp_ctx->proc_ctx;
#ifdef CFG_FEATURE_VFIO
    event->vfid = proc_ctx->vfid;
#endif
    ka_base_atomic_inc(&proc_ctx->publish_event_num);
    if (event->event_id < (u32)SCHED_MAX_EVENT_TYPE_NUM) {
        (void)sched_grp_event_num_update(thread_ctx->grp_ctx, event->event_id);
    }

    topic_chan->report_flag = SCHED_DRV_REPORT_GET_EVENT;

    sched_debug("End of calling esched_drv_get_event. (mb_id=%u; pid=%u; gid=%u; event_id=%u)\n",
        topic_chan->mb_id, mb->pid, mb->gid, mb->topic_id);
#endif
    return event;
}

STATIC struct sched_event *esched_drv_get_event(struct sched_cpu_ctx *cpu_ctx,
    struct sched_thread_ctx *thread_ctx, u32 event_id)
{
    return _esched_drv_get_event(cpu_ctx->topic_chan, thread_ctx, event_id);
}
#ifdef CFG_FEATURE_HARD_SOFT_SCHED
static struct sched_event *esched_drv_get_event_curr_chan(struct sched_cpu_ctx *cpu_ctx,
    struct sched_thread_ctx *thread_ctx, u32 event_id)
{
    struct topic_data_chan *topic_chan = NULL;
#ifndef EMU_ST
    topic_chan = esched_drv_get_thread_topic_chan(cpu_ctx->node->node_id, thread_ctx);
    if (topic_chan == NULL) {
        sched_err("Failed to get thread topic chan.\n");
        return NULL;
    }
#endif
    return _esched_drv_get_event(topic_chan, thread_ctx, event_id);
}
#endif

STATIC int esched_drv_get_host_pid(u32 chip_id, int pid, u32 *host_pid, enum devdrv_process_type *cp_type)
{
    struct sched_proc_ctx *proc_ctx = NULL;
    u32 dev_id, vfid;
    int ret;

    proc_ctx = esched_chip_proc_get(chip_id, pid);
    if (proc_ctx != NULL) {
        *cp_type = proc_ctx->cp_type;
        *host_pid = (u32)proc_ctx->host_pid;
        esched_chip_proc_put(proc_ctx);
        return 0;
    }

    ret = devdrv_query_process_host_pid(pid, &dev_id, &vfid, host_pid, cp_type);
    if (ret != 0) {
        sched_err("Failed to invoke the devdrv_query_process_host_pid. (pid=%d)\n", pid);
        return ret;
    }

    return 0;
}
#endif

STATIC int esched_drv_fill_sqe_qos(u32 chip_id, struct sched_published_event_info *event_info,
    struct topic_sched_sqe *sqe)
{
    struct sched_proc_ctx *proc_ctx = NULL;
    struct sched_grp_ctx *grp_ctx = NULL;
    struct sched_event event;
    u32 dst_engine = event_info->dst_engine;
#ifndef CFG_FEATURE_MORE_PID_PRIORITY
    u16 bond_pri;
#endif

    if ((dst_engine == (u32)TS_CPU) || (esched_drv_is_sched_mode_change_task(sqe->topic_id, sqe->subtopic_id))) {
        sqe->qos = 0;
        return 0;
    }

#ifdef ESCHED_HOST
    if ((dst_engine != (u32)ACPU_HOST) || (dst_engine != (u32)CCPU_HOST)) {
#else
    if ((dst_engine == (u32)ACPU_HOST) || (dst_engine == (u32)CCPU_HOST)) {
#endif
        /* local os filter */
        sqe->qos = 0;
        return 0;
    }

    proc_ctx = esched_chip_proc_get(chip_id, event_info->pid);
    if (proc_ctx == NULL) {
        sched_err("Failed to get proc_ctx. (chip_id=%u; pid=%d)\n", chip_id, event_info->pid);
        return DRV_ERROR_NO_PROCESS;
    }

    grp_ctx = sched_get_grp_ctx(proc_ctx, event_info->gid);
    event.event_id = event_info->event_id;
    event.event_thread_map = NULL;
    if (!sched_grp_can_handle_event(grp_ctx, &event)) {
        esched_chip_proc_put(proc_ctx);
        sched_err("There is no subscribe thread. (pid=%d; gid=%u; event_id=%u)\n",
            grp_ctx->pid, grp_ctx->gid, event_info->event_id);
        return DRV_ERROR_NO_SUBSCRIBE_THREAD;
    }

#ifdef CFG_FEATURE_MORE_PID_PRIORITY
    sqe->qos = (proc_ctx->pri >= TOPIC_SCHED_MAX_PRIORITY) ? (TOPIC_SCHED_MAX_PRIORITY - 1) : proc_ctx->pri;
    sched_debug("Fill sqe qos. (qos=%u)\n", (u32)sqe->qos);
#else
    bond_pri = (proc_ctx->pri * (u32)SCHED_MAX_EVENT_PRI_NUM) + proc_ctx->event_pri[event_info->event_id];
    sqe->qos = (bond_pri >= TOPIC_SCHED_MAX_PRIORITY) ? (TOPIC_SCHED_MAX_PRIORITY - 1U) : bond_pri;
    sched_debug("Fill sqe qos. (qos=%u; bond_pri=%u)\n", (u32)sqe->qos, (u32)bond_pri);
#endif

    esched_chip_proc_put(proc_ctx);

    return 0;
}

#ifndef ESCHED_HOST
#ifdef CFG_FEATURE_NO_BIND_SCHED
STATIC bool esched_is_proc_mapped(u32 chip_id, int pid, u32 dst_engine)
{
    bool ret = false;
    struct sched_numa_node *node = NULL;
    struct sched_proc_ctx *proc_ctx = NULL;

    if (!esched_dst_engine_is_local(dst_engine)) {
        return true;
    }

    node = esched_dev_get(chip_id);
    if (node == NULL) {
        return false;
    }
    proc_ctx = esched_proc_get(node, pid);
    if (proc_ctx == NULL) {
        ret = false;
        esched_dev_put(node);
        return false;
    }
    ret = ((proc_ctx->host_pid) != 0);
    esched_proc_put(proc_ctx);
    esched_dev_put(node);
    return ret;
}
#else
STATIC bool esched_is_proc_mapped(u32 chip_id, int pid, u32 dst_engine)
{
    return true;
}
#endif

static inline void esched_drv_check_fill_sqe_dst_pid(u32 chip_id, u32 *host_pid,
    enum devdrv_process_type cp_type, struct sched_published_event_info *event_info)
{
    bool is_pid_map = true;

    if (((event_info->dst_engine == (u32)ACPU_DEVICE) || (event_info->dst_engine == (u32)ACPU_LOCAL)) &&
        (cp_type == DEVDRV_PROCESS_USER)) {
        is_pid_map = false;
    }

    is_pid_map &= esched_is_proc_mapped(chip_id, (int)(*host_pid), event_info->dst_engine);
    if (is_pid_map == false) {
        *host_pid = (((u32)event_info->pid) | PID_MAP_MASK);
    }
}
#endif

static inline bool esched_is_pid_hw_mapped(u32 chip_id)
{
    if (esched_is_phy_dev(chip_id) == false) {
#ifndef CFG_FEATURE_MIA_MAP_TOPIC_TABLE
        return false;
#endif
    } else {
#ifndef CFG_FEATURE_SIA_MAP_TOPIC_TABLE
        return false;
#endif
    }

    return true;
}

STATIC int esched_set_sqe_pid_msg(u32 chip_id, u32 event_src, u32 target_pid, struct topic_sched_sqe *sqe,
    struct sched_published_event_info *event_info)
{
    if ((esched_is_pid_hw_mapped(chip_id) == false) || ((target_pid & PID_MAP_MASK) != 0)) {
        sqe->pid = esched_get_hw_vfid_from_devid(chip_id);
        /* pid store in rt_streamid and task_id */
        if (memcpy_s(&sqe->rt_streamid, sizeof(target_pid), &target_pid, sizeof(target_pid)) != 0) {
#ifndef EMU_ST
            sched_err("Failed to memcpy_s pid to rt_streamid. (dev_id=%u; pid=%d)\n", chip_id, target_pid);
            return DRV_ERROR_INNER_ERR;
#endif
        }
    } else {
        sqe->pid = target_pid;
    }

    if (event_info->msg_len > TOPIC_SCHED_USER_DATA_PAYLOAD_LEN) {
        sched_err("Invalid msg_len. (chip_id=%u; msg_len=%u; max=%d)\n",
            chip_id, event_info->msg_len, TOPIC_SCHED_USER_DATA_PAYLOAD_LEN);
        return DRV_ERROR_PARA_ERROR;
    }

    if (event_info->msg_len != 0) {
        ka_mm_memcpy_toio((void __iomem *)&sqe->user_data[0], event_info->msg, event_info->msg_len);
    }

    return esched_update_sqe_user_data(sqe, event_info);
}

STATIC int esched_drv_fill_sqe(u32 chip_id, u32 event_src, struct topic_sched_sqe *sqe,
    struct sched_published_event_info *event_info)
{
    u32 host_pid, target_pid;
    int ret;

    sqe->type = TOPIC_SCHED_SQE_TYPE;
    sqe->wr_cqe = 0;
    sqe->blk_dim = 1;

    if (event_info->event_id == EVENT_SPLIT_KERNEL) {
        sqe->block_id = 1;         /* split task only, no use. */
    }
    sqe->kernel_type = TOPIC_SCHED_DEFAULT_KERNEL_TYPE;
    sqe->batch_mode = 0;           /* no use. */

    sqe->topic_type = esched_get_topic_type(event_info->policy, event_info->dst_engine);
#ifdef CFG_SOC_PLATFORM_MINIV3
    if ((sqe->topic_type == TOPIC_TYPE_AICPU_HOST_ONLY) || (sqe->topic_type == TOPIC_TYPE_AICPU_HOST_FIRST)) {
        sched_err("topic type is invalid. (pid=%d; gid=%u; eventid=%u; policy=%u; topic_type=%u)\n",
            event_info->pid, event_info->gid, event_info->event_id, event_info->policy, event_info->dst_engine);
        return DRV_ERROR_NOT_SUPPORT;
    }
#endif

    sqe->kernel_credit = TOPIC_SCHED_SQE_KERNEL_CREDIT;

    sqe->subtopic_id = event_info->subevent_id;
    sqe->topic_id = event_info->event_id;
    sqe->gid = event_info->gid;
    sqe->user_data_len = event_info->msg_len;

    esched_init_usrdata_flag(sqe);

    ret = esched_drv_fill_sqe_qos(chip_id, event_info, sqe);
    if (ret != 0) {
        sched_err("Failed to fill qos. (pid=%d; gid=%u; eventid=%u; ret=%d)\n",
            event_info->pid, event_info->gid, event_info->event_id, ret);
        return ret;
    }

    if ((esched_drv_is_used_host_pid(event_info->event_id, sqe->topic_type)) &&
        (!esched_drv_is_sched_mode_change_task(sqe->topic_id, sqe->subtopic_id))) {
#ifdef ESCHED_HOST
        host_pid = (u32)SCHED_CURRENT_TGID;
        /* query host app pid by cp pid */
        ret = devdrv_query_master_pid_by_device_slave(chip_id, event_info->pid, &host_pid);
        if (ret == 0) {
            sched_debug("Query master pid success. (pid=%d; host_pid=%u; current_pid=%d)\n",
                event_info->pid, host_pid, SCHED_CURRENT_TGID);
        }
#else
        enum devdrv_process_type cp_type;
        ret = esched_drv_get_host_pid(chip_id, event_info->pid, &host_pid, &cp_type);
        if (ret != 0) {
            sched_err("Failed to get host pid. (pid=%d; ret=%d)\n", event_info->pid, ret);
            return ret;
        }

        esched_drv_check_fill_sqe_dst_pid(chip_id, &host_pid, cp_type, event_info);

        if (cp_type == DEVDRV_PROCESS_CP2) {
            sqe->kernel_type = TOPIC_SCHED_CUSTOM_KERNEL_TYPE;
        }
#endif
        target_pid = host_pid;
    } else {
        target_pid = (u32)event_info->pid;
    }

    return esched_set_sqe_pid_msg(chip_id, event_src, target_pid, sqe, event_info);
}

STATIC int esched_drv_submit_split_event(struct sched_numa_node *node, struct topic_sched_sqe *sqe, u32 timeout)
{
    struct topic_data_chan *topic_chan = NULL;
#ifndef EMU_ST
#ifdef CFG_FEATURE_HARD_SOFT_SCHED
    struct sched_thread_ctx *thread_ctx = NULL;
#endif
    u32 cpuid;
    int ret;

    if (sqe->topic_type != TOPIC_TYPE_AICPU_DEVICE_ONLY) {
        sched_err("The event not allow to submit. (event_id=%u; topic_type=%u)\n",
            sqe->topic_id, (u32)sqe->topic_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = sched_get_sched_cpuid_in_node(node, &cpuid);
    if (ret != 0) {
        sched_err("Not sched cpu. (chip_id=%u; cpu_num=%u; cur_processor_id=%u)\n",
            node->node_id, node->cpu_num, sched_get_cur_processor_id());
        return ret;
    }

#ifdef CFG_FEATURE_HARD_SOFT_SCHED
    if (esched_drv_dev_is_hw_soft_sched_mode(&node->hard_res)) {
        thread_ctx = esched_cpu_cur_thread_get(sched_get_cpu_ctx(node, cpuid));
        if (thread_ctx == NULL) {
            sched_err("No running thread. (cpuid=%u)\n", cpuid);
            return DRV_ERROR_THREAD_NOT_RUNNIG;
        }

        topic_chan = esched_drv_get_thread_topic_chan(node->node_id, thread_ctx);
        if (topic_chan == NULL) {
            esched_cpu_cur_thread_put(thread_ctx);
            sched_err("Failed to get thread topic chan.\n");
            return DRV_ERROR_INNER_ERR;
        }

        esched_cpu_cur_thread_put(thread_ctx);
    }
#endif
#ifndef ESCHED_HOST
    if (topic_chan == NULL) {
        topic_chan = esched_drv_get_topic_chan(node->node_id, esched_get_cpuid_in_node(cpuid));
    }
#endif
#endif
    return esched_cpu_port_submit_task(topic_chan, sqe, timeout);
}

int esched_publish_event_to_topic(u32 chip_id, u32 event_src,
                             struct sched_published_event_info *event_info,
                             struct sched_published_event_func *event_func)
{
    struct sched_numa_node *node = NULL;
    struct topic_sched_sqe sqe;
    int submit_chan_id = -1;
    int ret;

    if ((event_info->dst_engine >= (unsigned int)DST_ENGINE_MAX) || (event_info->policy >= (unsigned int)POLICY_MAX) ||
        (esched_get_topic_type(event_info->policy, event_info->dst_engine) >= TOPIC_TYPE_MAX) ||
        (event_info->subevent_id >= TOPIC_SCHED_MAX_SUBEVENT_ID)) {
        sched_err("The parameters is invalid. (chip_id=%u; dst_engine=%u; policy=%u; subevent_id=%u)\n",
            chip_id, event_info->dst_engine, event_info->policy, event_info->subevent_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    node = sched_get_numa_node(chip_id);
    if (node == NULL) {
        sched_err("Failed to invoke the sched_get_numa_node to get node. (chip_id=%u)\n", chip_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (node->hard_res.init_flag == SCHED_INVALID) {
#ifndef EMU_ST
        sched_err("Hard res not inited. (chip_id=%u)\n", chip_id);
        return DRV_ERROR_UNINIT;
#endif
    }

    ret = memset_s((void *)&sqe, sizeof(sqe), 0, sizeof(sqe));
    if (ret != 0) {
        sched_err("Failed to invoke the memset_s. (chip_id=%u; ret=%d)\n", chip_id, ret);
        return ret;
    }

    ret = esched_drv_fill_sqe(chip_id, event_src, &sqe, event_info);
    if (ret != 0) {
        sched_err("Failed to fill variable sqe. (chip_id=%u; ret=%d)\n", chip_id, ret);
        return ret;
    }

    if (event_info->event_id == EVENT_CDQ_MSG) {
        sched_info("CDQM func is not support.\n");
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (event_info->event_id == EVENT_SPLIT_KERNEL) {
        ret = esched_drv_submit_split_event(node, &sqe, 0);
    } else {
        ret = esched_drv_submit_normal_event(node, event_info->dst_engine, &sqe, 0, &submit_chan_id);
        esched_submit_trace_update(event_src, node, event_info);
    }

    sched_debug("End of calling esched_drv_publish_event. "
        "(chip_id=%u; dst_engine=%u; policy=%u; topic_type=%u; pid=%u; event_pid=%d; "
        "submit_chan_id=%d; subtopic_id=%u; topic_id=%u; gid=%u; tid=%u; msg_len=%u; data[0]=%u; "
        "task_id=%u; stream_id=%u; ret=%d)\n",
        chip_id, event_info->dst_engine, event_info->policy, (u32)sqe.topic_type, sqe.pid, event_info->pid,
        submit_chan_id, sqe.subtopic_id, sqe.topic_id, sqe.gid, event_info->tid, event_info->msg_len, sqe.user_data[0],
        sqe.task_id, sqe.rt_streamid, ret);

    return ret;
}

#ifdef CFG_FEATURE_NO_BIND_SCHED
STATIC int esched_get_grp_type(u32 chip_id, int pid, u32 gid, bool local_flag, int *ccpu_flag)
{
    int ret = 0;
    struct sched_numa_node *node = NULL;
    struct sched_proc_ctx *proc_ctx = NULL;
    struct sched_grp_ctx *grp_ctx = NULL;

    if (!local_flag) {
        *ccpu_flag = SCHED_INVALID;
        return 0;
    }

    node = esched_dev_get(chip_id);
    if (node == NULL) {
        sched_err("Get dev failed. (chip_id=%d)\n", chip_id);
        return DRV_ERROR_UNINIT;
    }
    proc_ctx = esched_proc_get(node, pid);
    if (proc_ctx == NULL) {
        sched_err("Get proc ctx failed. (pid=%d)\n", pid);
        esched_dev_put(node);
        return DRV_ERROR_UNINIT;
    }
    grp_ctx = sched_get_grp_ctx(proc_ctx, gid);
    if (grp_ctx->sched_mode == SCHED_MODE_UNINIT) {
        sched_err("The group is not added. (chip_id=%u; pid=%d; gid=%u)\n",
            proc_ctx->node->node_id, pid, gid);
        ret = DRV_ERROR_UNINIT;
    } else {
        *ccpu_flag = (grp_ctx->sched_mode == SCHED_MODE_SCHED_CPU) ? SCHED_INVALID : SCHED_VALID;
    }
    esched_proc_put(proc_ctx);
    esched_dev_put(node);
    return ret;
}
#endif

STATIC int esched_drv_submit_event_distribute(u32 dev_id, u32 event_src,
    struct sched_published_event_info *event_info, struct sched_published_event_func *event_func)
{
    int ccpu_flag = SCHED_VALID;
    u32 dst_engine = event_info->dst_engine;
    bool local_flag = esched_dst_engine_is_local(dst_engine);
#ifdef CFG_FEATURE_NO_BIND_SCHED
    int ret;
    ret = esched_get_grp_type(dev_id, event_info->pid, event_info->gid, local_flag, &ccpu_flag);
    if (ret != 0) {
        sched_err("Get group type failed. (dev_id=%u; pid=%d; gid=%u; ccpu_flag=%d)\n",
            dev_id, event_info->pid, event_info->gid, ccpu_flag);
        return ret;
    }
#else
    if ((dst_engine != CCPU_DEVICE) && (dst_engine != CCPU_HOST) && (dst_engine != CCPU_LOCAL)) {
        ccpu_flag = SCHED_INVALID;
    }
#endif
    if (local_flag && (ccpu_flag == SCHED_VALID)) {
        return sched_publish_event(dev_id, event_src, event_info, event_func);
    } else {
        return esched_publish_event_to_topic(dev_id, event_src, event_info, event_func);
    }
}

STATIC int esched_drv_map_host_pid(struct sched_proc_ctx *proc_ctx)
{
    return esched_drv_map_host_dev_pid(proc_ctx, 0);
}

STATIC void esched_drv_unmap_host_pid(struct sched_proc_ctx *proc_ctx)
{
    esched_drv_unmap_host_dev_pid(proc_ctx, 0);
}

static bool esched_is_all_node_cpu_idle(u32 cpuid)
{
    u32 i;
    struct sched_numa_node *node = NULL;
    struct sched_cpu_ctx *cpu_ctx = NULL;

    for (i = 0; i < SCHED_MAX_CHIP_NUM; i++) {
        node = esched_dev_get(i);
        if (node == NULL) {
            continue;
        }

        if ((node->sched_set_cpu_flag == SCHED_INVALID) || (node->sched_cpu_num == 0)) {
            esched_dev_put(node);
            continue;
        }

        cpu_ctx = sched_get_cpu_ctx(node, cpuid);
        if ((cpu_ctx != NULL) && (!esched_is_cpu_idle(cpu_ctx))) {
            esched_dev_put(node);
            return false;
        }
        esched_dev_put(node);
    }

    return true;
}

STATIC bool sched_check_topic_sched_idle(struct topic_data_chan_sched_record *sched_record)
{
    if (sched_record->schedule_sn_last != sched_record->schedule_sn) {
        sched_record->schedule_sn_last = sched_record->schedule_sn;
        sched_record->no_schedule_cnt = 0;
        return false;
    }

    sched_record->no_schedule_cnt++;

    if (sched_record->no_schedule_cnt <= TOPIC_SCHED_STATUS_CHECK_TIME) {
        return false;
    }

    sched_record->no_schedule_cnt = 0;
    return true;
}

STATIC void sched_check_wait_topic(struct sched_numa_node *node)
{
    struct sched_cpu_ctx *cpu_ctx = NULL;
    struct topic_data_chan *topic_chan = NULL;
    u32 i;

    if (node->sched_set_cpu_flag == SCHED_INVALID) {
        return;
    }

    for (i = 0; i < node->sched_cpu_num; i++) {
        u32 cnt = 0;
        cpu_ctx = sched_get_cpu_ctx(node, node->sched_cpuid[i]);
        topic_chan = cpu_ctx->topic_chan;
        if ((topic_chan == NULL) || (topic_chan->hard_res == NULL) || (topic_chan->hard_res->io_base == NULL)) {
            break;
        }

        /* different vf in vfg use same aicpu */
        if ((esched_is_all_node_cpu_idle(cpu_ctx->cpuid) == false) ||
            (sched_check_topic_sched_idle(&topic_chan->sched_record) == false)) {
            continue;
        }

        if (esched_drv_is_mb_valid(topic_chan) == false) {
            continue;
        }

        while (cnt < TOPIC_SCHED_WAIT_CPU_IDLE_CNT_MAX) { /* 100ms ~ 200ms */
            if (esched_is_all_node_cpu_idle(cpu_ctx->cpuid)) {
                break;
            }
            cnt++;
            ka_system_usleep_range(TOPIC_SCHED_WAIT_CPU_IDLE_MIN_INTERVAL, TOPIC_SCHED_WAIT_CPU_IDLE_MAX_INTERVAL);
        }

        sched_debug("It's waiting to be checked by the guards. (node_id=%u; mb_id=%u)\n",
            node->node_id, cpu_ctx->cpuid);
        ka_system_tasklet_schedule(&topic_chan->sched_task);
    }
}

STATIC void sched_drv_check_cpu_task(struct sched_numa_node *node, struct sched_proc_ctx *proc_ctx)
{
    struct sched_thread_ctx *thread_ctx = NULL;
    struct sched_cpu_ctx *cpu_ctx = NULL;
    u32 i;

    for (i = 0; i < node->sched_cpu_num; i++) {
        cpu_ctx = sched_get_cpu_ctx(node, node->sched_cpuid[i]);
        thread_ctx = esched_get_proc_thread_on_cpu(proc_ctx, cpu_ctx);
        if (thread_ctx == NULL) {
            continue;
        }

        /* take lock check again */
        ka_task_mutex_lock(&thread_ctx->thread_mutex);
        if (esched_is_cpu_cur_thread(cpu_ctx, thread_ctx)) {
            ka_task_spin_lock_bh(&thread_ctx->thread_finish_lock);
            /* sched_thread_finish maybe sched a next event and cpu cur thread maybe change. */
            sched_thread_finish(thread_ctx, SCHED_TASK_FINISH_SCENE_PROC_EXIT);
            ka_task_spin_unlock_bh(&thread_ctx->thread_finish_lock);

            ka_base_atomic_set(&thread_ctx->status, SCHED_THREAD_STATUS_IDLE);
            ka_wmb();
            /* Set cpu idle only if there is no event refresh cpu cur thread. */
            ka_task_spin_lock_bh(&cpu_ctx->sched_lock);
            if (esched_is_cpu_cur_thread(cpu_ctx, thread_ctx)) {
                esched_cpu_idle(cpu_ctx);
            }
            ka_task_spin_unlock_bh(&cpu_ctx->sched_lock);
        }

        ka_task_mutex_unlock(&thread_ctx->thread_mutex);
    }
}

static void sched_drv_task_exit_check_sched_cpu(struct sched_numa_node *node, struct sched_proc_ctx *proc_ctx)
{
#ifndef CFG_SOC_PLATFORM_MDC_V11
#ifdef CFG_FEATURE_HARD_SOFT_SCHED
    if (esched_drv_dev_is_hw_soft_sched_mode(&node->hard_res)) {
        sched_task_exit_check_sched_cpu(node, proc_ctx);
        return;
    }
#endif
#endif
    sched_drv_check_cpu_task(node, proc_ctx);
    sched_wakeup_process_all_thread(proc_ctx);
}

struct topic_data_chan *esched_drv_create_one_topic_chan(u32 devid, u32 chan_id)
{
    struct sched_numa_node *node = sched_get_numa_node(devid);
    struct topic_data_chan *topic_chan = NULL;

    if (chan_id >= TOPIC_SCHED_MAX_CHAN_NUM) {
        sched_err("Invalid chan_id. (node=%u; chan_id=%u)\n", node->node_id, chan_id);
        return NULL;
    }

    topic_chan = (struct topic_data_chan *)sched_vzalloc(sizeof(struct topic_data_chan));
    if (topic_chan == NULL) {
        sched_err("Failed to vzalloc topic_chan. (node=%u; chan_id=%u)\n", node->node_id, chan_id);
        return NULL;
    }
    node->hard_res.topic_chan[chan_id] = topic_chan;

    sched_debug("Create one topic_chan success. (node=%u; chan_id=%u)\n", node->node_id, chan_id);

    return topic_chan;
}

void esched_drv_destroy_one_topic_chan(u32 devid, u32 chan_id)
{
    struct sched_numa_node *node = sched_get_numa_node(devid);

    if (chan_id >= TOPIC_SCHED_MAX_CHAN_NUM) {
        sched_err("Invalid chan_id. (node=%u; chan_id=%u)\n", node->node_id, chan_id);
        return;
    }

    if (node->hard_res.topic_chan[chan_id] != NULL) {
        if (node->hard_res.topic_chan[chan_id]->cpu_ctx != NULL) {
            node->hard_res.topic_chan[chan_id]->cpu_ctx->topic_chan = NULL;
        }

        sched_vfree(node->hard_res.topic_chan[chan_id]);
        node->hard_res.topic_chan[chan_id] = NULL;
    }
}

void esched_drv_destroy_topic_chans(u32 devid, u32 start_chan_id, u32 chan_num)
{
    u32 chan_id;

    for (chan_id = start_chan_id; chan_id < (start_chan_id + chan_num); chan_id++) {
        esched_drv_destroy_one_topic_chan(devid, chan_id);
    }
}

int esched_drv_create_topic_chans(u32 devid, u32 start_chan_id, u32 chan_num)
{
    struct topic_data_chan *topic_chan = NULL;
    u32 chan_id;

    for (chan_id = start_chan_id; chan_id < (start_chan_id + chan_num); chan_id++) {
        topic_chan = esched_drv_create_one_topic_chan(devid, chan_id);
        if (topic_chan == NULL) {
            esched_drv_destroy_topic_chans(devid, start_chan_id, chan_id - start_chan_id);
            return DRV_ERROR_INNER_ERR;
        }
    }

    sched_debug("Create topic channels success. (devid=%u; chan_id=[%u, %u))\n",
        devid, start_chan_id, (start_chan_id + chan_num));

    return 0;
}

#ifdef CFG_FEATURE_HARD_SOFT_SCHED
STATIC void sched_drv_send_sched_mode_change_event(struct sched_numa_node *node)
{
    struct sched_published_event_info event_info;
    u32 i;

    event_info.dst_engine = ACPU_DEVICE;
    event_info.policy = ONLY;
    event_info.pid = 0;
    event_info.gid = 0;
    event_info.tid = 0;
    event_info.event_id = EVENT_DRV_MSG;
    event_info.subevent_id = DRV_SUBEVENT_ESCHED_SCHED_MODE_CHANGE_MSG;
    event_info.msg_len = 0;
    event_info.msg = NULL;

    for (i = 0; i < node->sched_cpu_num; i++) {
        int ret = esched_publish_event_to_topic(node->node_id, SCHED_PUBLISH_FORM_KERNEL, &event_info, NULL);
        if (ret != 0) {
            sched_warn("Send not success. (devid=%u; ret=%d)\n", node->node_id, ret);
        }
    }
}

STATIC void sched_drv_hw_sw_try_resched_cpu(struct sched_numa_node *node)
{
    struct sched_hard_res *hard_res = &node->hard_res;
    bool hw_flag = false;
    u32 i, start_id, aicpu_chan_num;

    if (!esched_drv_dev_is_hw_soft_sched_mode(&node->hard_res)) {
        return;
    }

    start_id = hard_res->aicpu_start_cpuid;
    aicpu_chan_num = node->sched_cpu_num;

    /* If a CPU is scheduled by hw, the entire device is scheduled by hw. */
    for (i = start_id; i < start_id + aicpu_chan_num; i++) {
        if ((hard_res->topic_chan[i] != NULL) && (hard_res->topic_chan[i]->valid == 1) &&
            (esched_drv_cpu_is_hw_sched_mode(hard_res->topic_chan[i]))) {
            hw_flag = true;
        }
    }

    if (hw_flag) {
        sched_drv_send_sched_mode_change_event(node);
    } else {
        sched_check_sched_task(node);
    }
}
#endif

STATIC void sched_drv_try_resched_cpu(struct sched_numa_node *node)
{
    sched_check_wait_topic(node);
#ifdef CFG_FEATURE_HARD_SOFT_SCHED
    sched_drv_hw_sw_try_resched_cpu(node);
#endif
}

#ifndef ESCHED_HOST
STATIC struct sched_event *esched_drv_sched_cpu_get_event(struct sched_cpu_ctx *cpu_ctx,
    struct sched_thread_ctx *thread_ctx, u32 event_id)
{
#ifdef CFG_FEATURE_HARD_SOFT_SCHED
    if (!esched_drv_cpu_is_hw_sched_mode(cpu_ctx->topic_chan)) {
        return esched_drv_get_event_curr_chan(cpu_ctx, thread_ctx, event_id);
    }
#endif
    return esched_drv_get_event(cpu_ctx, thread_ctx, event_id);
}

STATIC struct sched_thread_ctx *sched_drv_sched_cpu_get_next_thread(struct sched_cpu_ctx *cpu_ctx)
{
#ifdef CFG_FEATURE_HARD_SOFT_SCHED
    if (!esched_drv_cpu_is_hw_sched_mode(cpu_ctx->topic_chan)) {
        return sched_get_next_thread(cpu_ctx);
    } else if (esched_drv_dev_is_hw_soft_sched_mode(cpu_ctx->topic_chan->hard_res)) {
        return NULL;
    }
#endif
    return esched_drv_get_next_thread(cpu_ctx);
}

STATIC void esched_drv_sched_cpu_to_idle(struct sched_thread_ctx *thread_ctx, struct sched_cpu_ctx *cpu_ctx)
{
#ifdef CFG_FEATURE_HARD_SOFT_SCHED
    if (!esched_drv_cpu_is_hw_sched_mode(cpu_ctx->topic_chan)) {
        return sched_cpu_to_idle(thread_ctx, cpu_ctx);
    }
#endif
    return esched_drv_cpu_to_idle(thread_ctx, cpu_ctx);
}
#endif

void esched_setup_dev_hw_ops(struct sched_dev_ops *ops)
{
    ops->sumbit_event = esched_drv_submit_event_distribute;
#ifdef ESCHED_HOST
    ops->sched_cpu_get_next_thread = NULL;
    ops->sched_cpu_to_idle = NULL;
    ops->sched_cpu_get_event = NULL;
    ops->conf_sched_cpu = NULL;
#else
    ops->conf_sched_cpu = esched_drv_conf_sched_cpu;
#endif
    ops->map_host_pid = esched_drv_map_host_pid;
    ops->unmap_host_pid = esched_drv_unmap_host_pid;
    ops->task_exit_check_sched_cpu = sched_drv_task_exit_check_sched_cpu;
    ops->try_resched_cpu = sched_drv_try_resched_cpu;
#ifndef ESCHED_HOST
    ops->sched_cpu_get_event = esched_drv_sched_cpu_get_event;
    ops->sched_cpu_get_next_thread = sched_drv_sched_cpu_get_next_thread;
    ops->sched_cpu_to_idle = esched_drv_sched_cpu_to_idle;
#endif
    ops->node_res_init = esched_hw_dev_init;
    ops->node_res_uninit = esched_hw_dev_uninit;
}

#else
int tmp_sched_check_wait_topic()
{
    return 0;
}
#endif

