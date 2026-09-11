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
*/

#ifndef DMS_TS_SCAN_H
#define DMS_TS_SCAN_H

#include <linux/types.h>
#include <linux/time.h>
#include <linux/delay.h>

#include "ka_system_pub.h"

#include "ascend_hal_error.h"
#include "dms_define.h"
#include "drv_type.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#define SCAN_VALUE_INVALID 0xFFFFFFFF

#define TS_MAX_INTERNEL_TIME 6000
#define DATA_TO_PERCENTAGE 100
#define DATA_TO_PERCENTAGE_TEN 10
#define DATA_TO_PERCENTAGE_FIVE 5

#define TS_SCAN_INFO_CYCLE 1000
#define TS_SCAN_DELAY  10

#define TS_DEVICE_NUM_MAX 4


typedef int (*ts_get_info_handle)(u32 dev_id, u32 vfid, u32 core_id, u32 *value);

struct ts_retry_cycle {
    unsigned int interval_cnt;
    unsigned int remain_cnt;
    ka_ktime_t prev_fail_time;
};

struct ts_scan_list {
    ts_get_info_handle get_value_handle;
    u32 type_id;
    u32 core_id;
    u32 pf_value[TS_DEVICE_NUM_MAX];
    struct ts_retry_cycle period_ctrl[TS_DEVICE_NUM_MAX];
    u32 scan_flag;
};

struct ts_scan_list *find_operate_ts_info_str(u32 type_id, u32 core_id);
int ts_scan_init(void);
int ts_scan_uninit(void);

#endif