/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
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
 * Create: 2022-06-06
 */
#include "ka_fs_pub.h"
#include "securec.h"

#include "queue_module.h"
#include "queue_dma.h"
#include "queue_status_record.h"

#define PER_STATUS_SIZE        sizeof(struct queue_qid_status)
#define TIME_RECODE_SIZE       (sizeof(long long int) * TIME_RECORD_TYPE_MAX)
#define MAX_EXCEPT_STATUS_CNT  1000
#define MAX_PERF_STATUS_CNT    1000

struct que_status_record_mng {
    ka_spinlock_t lock;
    ka_list_head_t list;
    u32 cur_record_cnt;
};

static struct que_status_record_mng status_record_mng[RECORD_MAX];
static const u32 max_record_cnt[RECORD_MAX] = {
    [RECORD_EXCEPT] = MAX_EXCEPT_STATUS_CNT,
    [RECORD_PERF] = MAX_PERF_STATUS_CNT,
};
#ifndef EMU_ST
static bool perf_switch = false;
static u32 g_perf_time_threshold = 0; /* If the time exceeds the threshold, will record except */
#else
static bool perf_switch = true;
static u32 g_perf_time_threshold = 1;
#endif

void queue_status_record_mng_init(void)
{
    u32 type;

    for (type = RECORD_EXCEPT; type < RECORD_MAX; type++) {
        ka_task_spin_lock_init(&status_record_mng[type].lock);
        status_record_mng[type].cur_record_cnt = 0;
        KA_INIT_LIST_HEAD(&status_record_mng[type].list);
    }
}

static void queue_collect_qid_status(struct queue_qid_status *except_status, STATUS_RECORD_TYPE type);
static void queue_reinit_qid_status(struct queue_qid_status *status)
{
    if ((perf_switch == false) && (status->is_finish == false)) {
        queue_collect_qid_status(status, RECORD_EXCEPT);
    } else if (perf_switch == true) {
        long long int *time_record = status->time_record;
#ifndef EMU_ST
        if (((time_record[HOST_FINISH_QUEUE_MSG] - time_record[HOST_START_MAKE_DMA_LIST] > g_perf_time_threshold) ||
            (time_record[DEV_FINISH_QUEUE_MSG] - time_record[DEV_START_SUBMIT_EVENT] > g_perf_time_threshold)) &&
            (g_perf_time_threshold != 0)) {
            queue_collect_qid_status(status, RECORD_EXCEPT);
        }
#endif
        queue_collect_qid_status(status, RECORD_PERF);
    }
    (void)memset_s(status->time_record, TIME_RECODE_SIZE, 0, TIME_RECODE_SIZE);
    status->is_finish = false;
}

static struct queue_qid_status *queue_create_qid_status(struct queue_context *ctx, u32 qid)
{
    struct queue_qid_status *status = NULL;

    status = (struct queue_qid_status *)queue_drv_kzalloc(sizeof(struct queue_qid_status), KA_GFP_ATOMIC | __KA_GFP_ACCOUNT);
    if (status == NULL) {
        return NULL;
    }
    ctx->qid_status[qid] = (void *)status;
    status->pid = ctx->pid;
    status->qid = qid;
    ka_base_atomic_set(&status->qid_dir_exit, QID_DIR_NO_EXIT);
    status->is_finish = false;

    return status;
}

struct queue_qid_status *queue_create_or_get_exit_qid_status(struct queue_context *ctx, u32 qid)
{
    struct queue_qid_status *status = NULL;

    if (qid >= MAX_SURPORT_QUEUE_NUM) {
        return NULL;
    }

    ka_task_spin_lock(&ctx->qid_status_lock);
    status = (struct queue_qid_status *)ctx->qid_status[qid];
    if (status != NULL) {
        queue_reinit_qid_status(status);
        ka_task_spin_unlock(&ctx->qid_status_lock);
        return status;
    }

    status = queue_create_qid_status(ctx, qid);
    ka_task_spin_unlock(&ctx->qid_status_lock);

    return status;
}

struct queue_qid_status *queue_get_qid_status(struct queue_context *ctx, u32 qid)
{
    if (qid >= MAX_SURPORT_QUEUE_NUM) {
        return NULL;
    }

    return ctx->qid_status[qid];
}

void queue_free_ctx_all_qid_status(struct queue_context *ctx)
{
    struct queue_qid_status *status = NULL;
    u32 qid;

    ka_task_spin_lock(&ctx->qid_status_lock);
    for (qid = 0; qid < MAX_SURPORT_QUEUE_NUM; qid++) {
        status = ctx->qid_status[qid];
        if (status == NULL) {
            continue;
        }

        if (status->is_finish == false) {
            queue_collect_qid_status(status, RECORD_EXCEPT);
        }

        ctx->qid_status[qid] = NULL;
        queue_drv_kfree(status);
    }
    ka_task_spin_unlock(&ctx->qid_status_lock);

    return;
}

void queue_set_qid_status_timestamp(struct queue_qid_status *status, enum queue_status_time_type type)
{
    if (status != NULL) {
        status->time_record[type] = queue_get_ktime_us();
        if ((type == HOST_FINISH_QUEUE_MSG) || (type == DEV_FINISH_QUEUE_MSG)) {
            status->is_finish = true;
        }
    }
}

void queue_set_qid_status_dma_node_num(struct queue_qid_status *status, u64 node_num)
{
    if (status != NULL) {
        status->node_num = node_num;
    }
}

void queue_set_qid_status_subevent_id(struct queue_qid_status *status, u32 subevent_id)
{
    if (status != NULL) {
        status->subevent_id = subevent_id;
    }
}

void queue_set_qid_status_serial_num(struct queue_qid_status *status, u64 serial_num)
{
    if (status != NULL) {
        status->serial_num = serial_num;
    }
}

void queue_set_qid_status_mem_size(struct queue_qid_status *status, u64 mem_size)
{
    if (status != NULL) {
        status->mem_size = mem_size;
    }
}

STATIC void queue_collect_qid_status(struct queue_qid_status *except_status, STATUS_RECORD_TYPE type)
{
    struct queue_qid_status *old_status = NULL;
    struct queue_qid_status *new_status = NULL;

    if (type >= RECORD_MAX) {
        return;
    }

    ka_task_spin_lock_bh(&status_record_mng[type].lock);
    if (status_record_mng[type].cur_record_cnt >= max_record_cnt[type]) {
        old_status = ka_list_first_entry(&status_record_mng[type].list, struct queue_qid_status, list);
        ka_list_del(&old_status->list);
        queue_drv_kfree(old_status);
        status_record_mng[type].cur_record_cnt--;
    }

    new_status = (struct queue_qid_status *)queue_drv_kzalloc(sizeof(struct queue_qid_status), KA_GFP_ATOMIC | __KA_GFP_ACCOUNT);
    if (new_status == NULL) {
        ka_task_spin_unlock_bh(&status_record_mng[type].lock);
        return;
    }

    (void)memcpy_s(new_status, PER_STATUS_SIZE, except_status, PER_STATUS_SIZE);
    ka_list_add_tail(&new_status->list, &status_record_mng[type].list);
    status_record_mng[type].cur_record_cnt++;

    ka_task_spin_unlock_bh(&status_record_mng[type].lock);
}

#ifndef EMU_ST
void queue_set_perf_switch(bool set_value, u32 time_threshold)
{
    perf_switch = set_value;
    g_perf_time_threshold = time_threshold;
}

void queue_show_perf_switch(ka_seq_file_t *seq)
{
    ka_fs_seq_printf(seq, "perf_switch=%u time_threshold=%u ms\n", perf_switch, g_perf_time_threshold);
}
#endif
void queue_show_one_qid_status(ka_seq_file_t *seq, struct queue_qid_status *per_status)
{
    long long int *time_record = per_status->time_record;
#ifndef EMU_ST
    ka_fs_seq_printf(seq, "serial_num:%llu, pid:%d, qid:%d, is_finish:%d, subevent_id:%u\n"
        "mem_size:%llu, dma_node_num:%llu\n"
        "host_time_show:\n"
        "    make_dma_list: start-%lldus end-%lldus cost-%lldus \n"
        "    hdc_send     : end-%lldus cost-%lldus \n"
        "    dev_reply    : end-%lldus cost-%lldus \n"
        "    hdc_data_in  : start-%lldus end-%lldus cost-%lldus \n"
        "    total_cost   : %lldus \n"
        "dev_time_show:\n"
        "    sched_submit : start-%lldus end-%lldus cost-%lldus \n"
        "    wait_event   : end-%lldus cost-%lldus \n"
        "    make_dma_list: end-%lldus cost-%lldus \n"
        "    dma_copy     : end-%lldus cost-%lldus \n"
        "    hdc_reply    : end-%lldus cost-%lldus \n"
        "    total_cost   : %lldus \n",
        per_status->serial_num, per_status->pid, per_status->qid, per_status->is_finish, per_status->subevent_id,
        per_status->mem_size, per_status->node_num,
        time_record[HOST_START_MAKE_DMA_LIST], time_record[HOST_END_MAKE_DMA_LIST],
        time_record[HOST_END_MAKE_DMA_LIST] - time_record[HOST_START_MAKE_DMA_LIST],
        time_record[HOST_END_HDC_SNED], time_record[HOST_END_HDC_SNED] - time_record[HOST_END_MAKE_DMA_LIST],
        time_record[HOST_END_WAIT_REPLY], time_record[HOST_END_WAIT_REPLY] - time_record[HOST_END_HDC_SNED],
        time_record[HOST_HDC_RECV], time_record[HOST_WAKE_UP], time_record[HOST_WAKE_UP] - time_record[HOST_HDC_RECV],
        time_record[HOST_FINISH_QUEUE_MSG] - time_record[HOST_START_MAKE_DMA_LIST],

        time_record[DEV_START_SUBMIT_EVENT], time_record[DEV_END_SUBMIT_EVENT],
        time_record[DEV_END_SUBMIT_EVENT] - time_record[DEV_START_SUBMIT_EVENT],
        time_record[DEV_START_MAKE_DMA_LIST],
        time_record[DEV_START_MAKE_DMA_LIST] - time_record[DEV_END_SUBMIT_EVENT],
        time_record[DEV_END_MAKE_DMA_LIST],
        time_record[DEV_END_MAKE_DMA_LIST] - time_record[DEV_START_MAKE_DMA_LIST],
        time_record[DEV_END_DMA_COPY], time_record[DEV_END_DMA_COPY] - time_record[DEV_END_MAKE_DMA_LIST],
#ifndef DRV_HOST
        time_record[DEV_END_REPLY], time_record[DEV_END_REPLY] - time_record[DEV_END_DMA_COPY],
        time_record[DEV_FINISH_QUEUE_MSG] - time_record[DEV_START_SUBMIT_EVENT]);
#else
        (long long int)0, (long long int)0, time_record[DEV_END_REPLY - 1] - time_record[DEV_START_SUBMIT_EVENT]);
#endif
#endif
}

void queue_show_all_qid_status(ka_seq_file_t *seq, STATUS_RECORD_TYPE type)
{
    struct queue_qid_status *all_status = NULL;
    struct queue_qid_status *per_status = NULL;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;
    u32 i, j;

    if (type >= RECORD_MAX) {
        return;
    }

    all_status = (struct queue_qid_status *)queue_kvalloc(max_record_cnt[type] * PER_STATUS_SIZE, 0);
    if (all_status == NULL) {
        return;
    }

    j = 0;
    ka_task_spin_lock_bh(&status_record_mng[type].lock);
    if (ka_list_empty_careful(&status_record_mng[type].list) == 0) {
        ka_list_for_each_safe(pos, n, &status_record_mng[type].list) {
            per_status = ka_list_entry(pos, struct queue_qid_status, list);
            (void)memcpy_s(&all_status[j], PER_STATUS_SIZE, per_status, PER_STATUS_SIZE);
            j++;
            if (j >= max_record_cnt[type]) {
                break;
            }
        }
    }
    ka_task_spin_unlock_bh(&status_record_mng[type].lock);

    for (i = 0; i < j; i++) {
        queue_show_one_qid_status(seq, &all_status[i]);
    }

    queue_kvfree(all_status);
}

void queue_free_one_type_qid_status(STATUS_RECORD_TYPE type)
{
    struct queue_qid_status *status = NULL;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;

    ka_task_spin_lock_bh(&status_record_mng[type].lock);
    if (ka_list_empty_careful(&status_record_mng[type].list) == 0) {
        ka_list_for_each_safe(pos, n, &status_record_mng[type].list) {
            status = ka_list_entry(pos, struct queue_qid_status, list);
            ka_list_del(&status->list);
            queue_drv_kfree(status);
        }
    }
    status_record_mng[type].cur_record_cnt = 0;
    ka_task_spin_unlock_bh(&status_record_mng[type].lock);
}

void queue_free_all_type_qid_status(void)
{
    u32 type;

    for (type = RECORD_EXCEPT; type < RECORD_MAX; type++) {
        queue_free_one_type_qid_status((STATUS_RECORD_TYPE)type);
    }
}

