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

#include <linux/ktime.h>
#include <linux/notifier.h>

#include "uda.h"
#include "devdrv_manager_common.h"
#include "dbl_mem_alloc_interface.h"
#include "heart_beat/heart_beat.h"
#include "heart_beat/hb_read.h"
#include "soft_fault_define.h"
#include "devdrv_interface.h"

unsigned int heart_beat_get_max_lost_count(unsigned int dev_id)
{
    return HEART_BEAT_HCCS_DEATH_COUNT;
}

int check_and_update_link_abnormal_status(u32 dev_id, u64 count)
{
    return 0;
}

int hb_set_heart_beat_count(unsigned int dev_id, unsigned long long count)
{
    int ret = 0;
    ret = agentdrv_set_heartbeat_count(dev_id, count);
    return ret;
}

int hb_get_heart_beat_count(unsigned int dev_id, unsigned long long* count)
{
    int ret;
    ret = agentdrv_get_heartbeat_count(dev_id, count);
    return ret;
}

int hb_read_item_work_start(unsigned int dev_id, struct hb_read_block *hb_read_item)
{
    hb_read_item->dev_id = dev_id;
    hb_read_item->hb_stutas = HEART_BEAT_READY;
    hb_read_item->old_count = 0;
    hb_read_item->lost_count = 0;
    hb_read_item->total_lost_count = 0;
    hb_read_item->miss_read_count = 0;
    hb_read_item->last_read_time = ktime_get_raw_ns();
    return 0;
}

int hb_report_heart_beat_lost_event(unsigned int dev_id)
{
    int ret;

    ret = uda_dev_ctrl(dev_id, UDA_CTRL_COMMUNICATION_LOST);
    return ret;
}
