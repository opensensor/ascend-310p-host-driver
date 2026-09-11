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

#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>

#include "ka_task_pub.h"
#include "ka_kernel_def_pub.h"

#include "dms_define.h"
#include "dms_device_time_zone.h"
#include "devdrv_user_common.h"
#include "drv_whitelist.h"

#define MINUTES 60

STATIC KA_TASK_DEFINE_SPINLOCK(time_sync_lock);
STATIC struct dms_time_sync time_interval = {0};

int dms_process_time_sync(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
#ifndef AOS_LLVM_BUILD
    int ret;
    const char *process_name[WHITE_LIST_PROCESS_NUM] = {PROCESS_NAME_DMP};
#endif
    int new_time = 0;
    unsigned long flags;
    long kernel_interval = time_interval.interval_seconds;
    struct dms_time_sync *interval = (struct dms_time_sync *)in;
    if ((interval == NULL) || (in_len != sizeof(struct dms_time_sync))) {
        dms_err("Input arg is NULL, or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }

#ifndef AOS_LLVM_BUILD
    /* check process whitelist */
    ret = whitelist_process_handler(process_name, WHITE_LIST_PROCESS_NUM);
    if (ret != 0) {
        dms_err("whitelist_process_handler error. (ret=%d)\n", ret);
        return ret;
    }
#endif

    spin_lock_irqsave(&time_sync_lock, flags);
    if (interval->interval_seconds != time_interval.interval_seconds) {
        time_interval.interval_seconds = interval->interval_seconds;
        #ifndef AOS_LLVM_BUILD
        sys_tz.tz_minuteswest = -time_interval.interval_seconds / MINUTES;
        #endif
        new_time = 1;
    }

    spin_unlock_irqrestore(&time_sync_lock, flags);
    if (new_time != 0) {
        dms_event("new interval seconds from user space: %ld, global interval seconds: %ld.\n",
                  interval->interval_seconds, kernel_interval);
    }
    return 0;
}

void dms_get_time_interval(struct timespec64 *time_sync)
{
    unsigned long flags;

    if (time_sync == NULL) {
        dms_err("input handle is null.\n");
        return;
    }

    spin_lock_irqsave(&time_sync_lock, flags);
    time_sync->tv_sec = time_interval.interval_seconds;
    time_sync->tv_nsec = 0;
    spin_unlock_irqrestore(&time_sync_lock, flags);
}
KA_EXPORT_SYMBOL(dms_get_time_interval);