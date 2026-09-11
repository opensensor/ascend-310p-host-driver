/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
* Create: 2025-08-06
*/

#ifndef __HB_READ_H
#define __HB_READ_H

#include <linux/workqueue.h>

struct hb_read_block {
    unsigned int dev_id;
    struct work_struct hb_lost_work; /* work for deal with the tasks when heartbeat is lost */
    struct workqueue_struct *hb_lost_wq;
    unsigned int hb_stutas;
    volatile u32 lost_count;
    volatile u64 old_count;
    volatile u64 total_lost_count;
    volatile u64 miss_read_count;
    u64 last_read_time;
    u32 urgent_task_id; /* ub 紧急心跳注册给dms的timer的id，用于ko卸载时去注册回调 */
};

int hb_read_item_work_start(unsigned int dev_id, struct hb_read_block *hb_read_item);
void hb_read_item_work_stop(unsigned int dev_id, struct hb_read_block* hb_read_item);
struct hb_read_block* get_heart_beat_read_item(unsigned int dev_id);
#endif