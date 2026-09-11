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
#ifndef QUEUE_STATUS_RECORD_H
#define QUEUE_STATUS_RECORD_H
#include "ka_list_pub.h"
#include "ka_base_pub.h"

#include "queue_context.h"

#define DEV_QUEUE_STATUS_RECORE_START 10
enum queue_status_time_type {
    HOST_START_MAKE_DMA_LIST = 0,
    HOST_END_MAKE_DMA_LIST,
    HOST_END_HDC_SNED,
    HOST_END_WAIT_REPLY,
    HOST_HDC_RECV,
    HOST_WAKE_UP,
    HOST_FINISH_QUEUE_MSG,

    DEV_START_SUBMIT_EVENT = DEV_QUEUE_STATUS_RECORE_START,
    DEV_END_SUBMIT_EVENT,
    DEV_START_MAKE_DMA_LIST,
    DEV_END_MAKE_DMA_LIST,
    DEV_END_DMA_COPY,
    DEV_END_REPLY,
    DEV_FINISH_QUEUE_MSG,
    TIME_RECORD_TYPE_MAX,
};
#define QID_DIR_NO_EXIT 0
#define QID_DIR_IS_EXIT 1

typedef enum {
    RECORD_EXCEPT = 0,
    RECORD_PERF,
    RECORD_MAX,
} STATUS_RECORD_TYPE;

struct queue_qid_status {
    u32 qid;
    ka_pid_t pid;
    u64 serial_num;
    u32 subevent_id;
    bool is_finish;
    ka_atomic_t qid_dir_exit;
    ka_list_head_t list;
    long long int time_record[TIME_RECORD_TYPE_MAX];
    u64 mem_size;
    u64 node_num;
};

void queue_status_record_mng_init(void);

struct queue_qid_status *queue_create_or_get_exit_qid_status(struct queue_context *ctx, u32 qid);
struct queue_qid_status *queue_get_qid_status(struct queue_context *ctx, u32 qid);

void queue_set_qid_status_timestamp(struct queue_qid_status *status, enum queue_status_time_type type);
void queue_set_qid_status_dma_node_num(struct queue_qid_status *status, u64 node_num);
void queue_set_qid_status_subevent_id(struct queue_qid_status *status, u32 subevent_id);
void queue_set_qid_status_serial_num(struct queue_qid_status *status, u64 serial_num);
void queue_set_qid_status_mem_size(struct queue_qid_status *status, u64 mem_size);
void queue_set_perf_switch(bool set_value, u32 time_threshold);

void queue_free_ctx_all_qid_status(struct queue_context *ctx);
void queue_free_one_type_qid_status(STATUS_RECORD_TYPE type);
void queue_free_all_type_qid_status(void);
void queue_show_all_qid_status(ka_seq_file_t *seq, STATUS_RECORD_TYPE type);
void queue_show_one_qid_status(ka_seq_file_t *seq, struct queue_qid_status *per_status);
void queue_show_perf_switch(ka_seq_file_t *seq);

#endif

