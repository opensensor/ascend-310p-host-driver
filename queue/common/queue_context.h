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
#ifndef QUEUE_CONTEXT_H
#define QUEUE_CONTEXT_H

#include "ka_list_pub.h"
#include "ka_base_pub.h"
#include "ka_task_pub.h"

#include "queue_module.h"

struct queue_context {
    int pid;
    ka_hlist_node_t link; /* hash task link */
    ka_atomic_t refcnt;
    ka_proc_dir_entry_t *entry;
    void *private_data;
    void *qid_status[MAX_SURPORT_QUEUE_NUM];
    ka_spinlock_t qid_status_lock;
};

struct queue_context *queue_context_get(ka_pid_t pid);
void queue_context_put(struct queue_context *ctx);
struct queue_context *queue_context_init(ka_pid_t pid);
void queue_context_uninit(struct queue_context *ctx);
void *queue_context_private_data_create(void);
void queue_context_private_data_destroy(void *private_data);
#endif