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
#ifndef DMS_TS_NPU_UTILIZATION_H
#define DMS_TS_NPU_UTILIZATION_H

#define SQID_VALID_MASK   (0x1)
#define ACSQ_MAX_NUM 128

#define NPU_UTILIZATION_SAMPLE_NUM 29
#define NPU_UTILIZATION_RATE_PERCENT 100
#define NPU_UTILIZATION_CYCLE_MS 9
#define DMS_TIMER_INVALID 0xffff

#define NPU_NUM_PER_DEV 1

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

int copy_to_user_safe(void __user *to, const void *from, unsigned long n);

struct npu_util_ctrl_block {
    void __iomem *remap_addr[ACSQ_MAX_NUM];
    unsigned char buf[NPU_UTILIZATION_SAMPLE_NUM];
    u32 head;
};

struct npu_util_hrtimer {
    struct hrtimer timer;
    ktime_t period;
};

int dms_ts_npu_utilization_init(void);
void dms_ts_npu_utilization_exit(void);
int dms_get_npu_utilization(core_utilization_rate_t *core_status, int copy_flag);
int rb_read_all(u32 devid, u32 *occupy_count);
#endif
