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
*/

#ifndef DMS_PG_INFO_H
#define DMS_PG_INFO_H

#ifdef CFG_FEATURE_PARTIAL_GOOD
#include "hiss/hsm_status.h" // firmware\hiss\inc\hiss
#include "dms_define.h"
#include <linux/aos/cpu_domain_info.h>

typedef struct {
    u32 valid;     // 0:full good,1:partial good
    u64 totalNum;  // physical core total num£¨full good£©
    u64 bitMapIdx; // buf idx
    u64 freq;      // (1 Mhz) core working frequency
} common_pg_info;

typedef struct {
    u32 magic;          // valid magic is 0x5A5A5A5AU
    u64 start_time;     // thread start time
    common_pg_info com_pg_info[PG_MODULE_TYPE_ALL];
} dev_pg_info;

typedef struct {
    dev_pg_info pg_info[1]; // DEVICE_NUM_MAX
} all_pg_info;

typedef struct {
    u16 serv_id;
    u16 msg_type;
    u16 length;
    u16 check_sum;
    all_pg_info a_pg_info;
} xpc_msg;

#define PG_INFO_MAGIC  0x5A5A5A5AU

int dms_get_pg_info(u32 dev_id, pg_cmd_data cmd, u64 *out_arg);

/**
 * init_pg_info_thread - create sync pg info to aoscore thread
 */
int init_pg_info_thread(void);
void uninit_pg_info_thread(void);
#endif

#endif
