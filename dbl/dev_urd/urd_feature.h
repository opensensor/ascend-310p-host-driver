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

#ifndef URD_FEATURE_H
#define URD_FEATURE_H
#include <linux/ktime.h>

typedef struct tag_dms_feature_arg {
    unsigned int msg_source;
    char *key;
    char *input;
    u32 input_len;
    char *output;
    u32 output_len;
} DMS_FEATURE_ARG_S;

typedef s32 (*feature_handler)(void *feature, char *in, u32 in_len, char *out, u32 out_len);

typedef struct tag_dms_feature {
    const char *owner_name;
    u32 main_cmd;
    u32 sub_cmd;
    const char *filter;
    const char *proc_ctrl_str;
    u32 privilege;
    feature_handler handler;
} DMS_FEATURE_S;

typedef struct tag_feature_statistic {
    u64 used;
    u64 failed;
    u64 time_max;
    u64 time_min;
    u64 time_last;
    s32 last_ret;
}FEATURE_STATISTIC_S;

typedef struct tag_dms_feature_node {
    struct tag_dms_feature *feature;
    char **proc_ctrl;
    char *proc_buf;
    atomic_t count;
    u32 proc_num;
    u32 state;
    FEATURE_STATISTIC_S s;
} DMS_FEATURE_NODE_S;


/* max time 60s */
#define FEATURE_WAIT_MAX_TIME    (6000)

#define FEATURE_WAIT_EACH_TIME    (10)

#define FEATURE_CONFIRM_WARN_MASK (1000)
#ifdef STATIC_SKIP
#define DMS_GET_CUR_SYSTEM_COUNTER(cnt)
#else
#define DMS_GET_CUR_SYSTEM_COUNTER(cnt) asm volatile("mrs %0, CNTVCT_EL0" : "=r"(cnt) :)
#endif

#define US_EACH_S 1000000
#define NS_EACH_US 1000

static inline unsigned long long dms_get_cur_cpu_tick(void)
{
#ifndef CFG_HOST_ENV
    u64 cnt = 0;
    DMS_GET_CUR_SYSTEM_COUNTER(cnt);
    return cnt;
#else
    u64 ts = 0;
    ts = ktime_get_raw_ns();
    return ts / NSEC_PER_USEC;
#endif
}

int dms_feature_register(DMS_FEATURE_S *feature);
int dms_feature_unregister(DMS_FEATURE_S *feature);
int dms_feature_process(DMS_FEATURE_ARG_S *arg);
int dms_feature_make_key(u32 main_cmd, u32 sub_cmd, const char *filter, char *key, u32 len);
ssize_t dms_feature_print_feature_list(char *buf);
int feature_inc_work(DMS_FEATURE_NODE_S* node);

#endif
