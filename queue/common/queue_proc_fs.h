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
 * Create: 2022-06-01
 */
#ifndef _QUEUE_PROC_FS_H
#define _QUEUE_PROC_FS_H

#include "queue_context.h"
#include "queue_status_record.h"
#include "ka_base_pub.h"

void queue_proc_fs_add_process(struct queue_context *ctx);
void queue_proc_fs_del_process(struct queue_context *ctx);
void queue_proc_fs_add_qid(struct queue_qid_status *status, ka_proc_dir_entry_t *parent);
void queue_proc_fs_del_qid(struct queue_qid_status *status, ka_proc_dir_entry_t *parent);
void queue_proc_fs_init(void);
void queue_proc_fs_uninit(void);
#endif