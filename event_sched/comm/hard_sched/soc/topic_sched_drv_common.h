/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
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
 * Create: 2024-03-01
 */

#ifndef TOPIC_SCHED_DRV_COMMON_H
#define TOPIC_SCHED_DRV_COMMON_H

#include "ascend_hal_define.h"
#include "esched.h"
#include "topic_sched_drv.h"

void esched_init_topic_types(void);
u8 esched_get_topic_type(unsigned int policy, unsigned int dst_engine);
void esched_init_usrdata_flag(struct topic_sched_sqe *sqe);
int esched_update_sqe_user_data(struct topic_sched_sqe *sqe, struct sched_published_event_info *event_info);
int esched_restore_mb_user_data(struct topic_sched_mailbox *mb, u32 *dst_devid, u32 *tid, u32 *pid);
int esched_drv_map_host_dev_pid(struct sched_proc_ctx *proc_ctx, u32 identity);
void esched_drv_unmap_host_dev_pid(struct sched_proc_ctx *proc_ctx, u32 identity);
bool esched_drv_is_used_host_pid(u32 event_id, u32 topic_type);
int esched_get_real_pid(struct topic_sched_mailbox *mb, u32 devid, u32 pid);
#endif
