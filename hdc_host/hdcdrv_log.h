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
* Create: 2025-3-3
*/

#ifndef _HDCDRV_LOG_H_
#define _HDCDRV_LOG_H_

#include "drv_log.h"
#include "drv_share_log.h"

#include "ka_dfx_pub.h"
#include "ka_system_pub.h"
#include "ka_task_pub.h"
#include "ka_base_pub.h"

#ifdef HDC_ENV_DEVICE
#include <linux/securec.h>
#include "kernel_version_adapt.h"
#else
#include "securec.h"
#endif
#define module_hdcdrv "hdcdrv"

#ifdef DRV_UT
#define drv_err_log(fmt, ...) do { \
    drv_err(module_hdcdrv, "<%s:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), ##__VA_ARGS__); \
} while (0)
#define drv_info_log(fmt, ...) do { \
    drv_info(module_hdcdrv, "<%s:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), ##__VA_ARGS__); \
} while (0)
#else
#ifdef HDC_ENV_DEVICE
#define drv_err_log(fmt, ...) do { \
    drv_err(module_hdcdrv, "<%s:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), ##__VA_ARGS__); \
} while (0)
#define drv_info_log(fmt, ...) do { \
    drv_info(module_hdcdrv, "<%s:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), ##__VA_ARGS__); \
} while (0)
#else
#define drv_err_log(fmt, ...) do { \
    drv_err(module_hdcdrv, "<%s:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), ##__VA_ARGS__); \
    share_log_err(HDC_SHARE_LOG_START, fmt, ##__VA_ARGS__); \
} while (0)
#define drv_info_log(fmt, ...) do { \
    share_log_run_info(HDC_SHARE_LOG_RUNINFO_START, fmt, ##__VA_ARGS__); \
} while (0)
#endif
#endif

static u64 g_log_count = 0;
#define LOG_LIMIT_RATE 30

static inline u64 hdc_get_time_interval(u64 *caller_jiffies, u64 *time_now)
{
    ka_system_get_abs_or_rel_mstime(time_now);
    return (*time_now - *caller_jiffies);
}

static inline bool hdc_print_timed_ratelimit(u64 *caller_jiffies, u64 interval_msecs)
{
    u64 time_now, timer;

    timer =  hdc_get_time_interval(caller_jiffies, &time_now);
    if ((*caller_jiffies != 0) && (timer <= interval_msecs)) {
        return false;
    }
    *caller_jiffies = time_now;

    return true;
}

static inline bool hdc_log_rate_limit(u32 *count, u64 *last_jiffies, u32 branch_rate, u64 limit_time)
{
    if (hdc_print_timed_ratelimit(last_jiffies, limit_time)) {
        *count = 0;
        g_log_count = 0;
        return false;
    } else {
        (*count)++;
        g_log_count++;
        return  ((*count >= branch_rate)  || (g_log_count  >=  LOG_LIMIT_RATE));
    }
}

#define HDC_LOG_LIMIT_TIME 3000       /* 3s */
#define HDC_LOG_LIMIT_BRANCH_RATE 1   /* print 1 counts per 3s */
#define HDC_LOG_ERR_LIMIT(print_cnt, last_jiffies, fmt, ...) do { \
    if (!hdc_log_rate_limit(print_cnt, last_jiffies, HDC_LOG_LIMIT_BRANCH_RATE, HDC_LOG_LIMIT_TIME)) \
        drv_err_log(fmt, ##__VA_ARGS__); \
} while (0)

#define HDC_WARN_LOG_LIMIT_TIME 60000       /* 60s */
#define HDC_WARN_LOG_LIMIT_BRANCH_RATE 1    /* print 1 counts per 60s */
#define HDC_LOG_WARN_LIMIT(print_cnt, last_jiffies, fmt, ...) do { \
    if (!hdc_log_rate_limit(print_cnt, last_jiffies, HDC_WARN_LOG_LIMIT_BRANCH_RATE, HDC_WARN_LOG_LIMIT_TIME)) { \
        drv_warn(module_hdcdrv, "<%s:%d> " fmt, ka_task_get_current_comm(), \
        ka_task_get_current_tgid(), ##__VA_ARGS__); \
    } \
} while (0)

#define hdcdrv_err_limit(fmt, ...) do { \
    if (ka_dfx_printk_ratelimit() != 0) { \
        drv_err_log(fmt, ##__VA_ARGS__); \
    } \
} while (0)
#define hdcdrv_warn_limit(fmt, ...) do { \
    if (ka_dfx_printk_ratelimit() != 0) { \
        drv_warn(module_hdcdrv, "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
        ka_task_get_current_pid(), ##__VA_ARGS__); \
    } \
} while (0)
#define hdcdrv_info_limit(fmt, ...) do { \
    if (ka_dfx_printk_ratelimit() != 0) { \
        drv_info(module_hdcdrv, "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
        ka_task_get_current_pid(), ##__VA_ARGS__); \
    } \
} while (0)
#define hdcdrv_err(fmt, ...) do { \
    drv_err_log(fmt, ##__VA_ARGS__); \
} while (0)
#define hdcdrv_warn(fmt, ...) do { \
    drv_warn(module_hdcdrv, "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
    ka_task_get_current_pid(), ##__VA_ARGS__); \
} while (0)
#define hdcdrv_event(fmt, ...) do { \
    drv_event(module_hdcdrv, "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
    ka_task_get_current_pid(), ##__VA_ARGS__); \
} while (0)
#define hdcdrv_info(fmt, ...) do { \
    drv_info(module_hdcdrv, "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
    ka_task_get_current_pid(), ##__VA_ARGS__); \
} while (0)
#define hdcdrv_info_limit_share(fmt, ...) do { \
    if (ka_dfx_printk_ratelimit() != 0) \
        drv_info_log(fmt, ##__VA_ARGS__); \
} while (0)
#define hdcdrv_info_share(fmt, ...) do { \
    drv_info_log(fmt, ##__VA_ARGS__); \
} while (0)
#define hdcdrv_critical_info(fmt, ...) do { \
    drv_err_log(fmt, ##__VA_ARGS__); \
} while (0)

#define hdcdrv_dbg(fmt, ...) do { \
    drv_debug(module_hdcdrv, "[DEBUG]<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
    ka_task_get_current_pid(), ##__VA_ARGS__); \
} while (0)

// Used as a variable name, regardless of the value of the enumeration
enum hdcdrv_limit_exclusive_log {
    // common log
    HDCDRV_LIMIT_LOG_0x00 = 0,
    HDCDRV_LIMIT_LOG_0x01,
    HDCDRV_LIMIT_LOG_0x02,
    HDCDRV_LIMIT_LOG_0x03,
    HDCDRV_LIMIT_LOG_0x04,
    HDCDRV_LIMIT_LOG_0x05,
    HDCDRV_LIMIT_LOG_0x06,
    HDCDRV_LIMIT_LOG_0x07,
    HDCDRV_LIMIT_LOG_0x08,
    HDCDRV_LIMIT_LOG_0x09,
    HDCDRV_LIMIT_LOG_0x0A,
    HDCDRV_LIMIT_LOG_0x0B,
    HDCDRV_LIMIT_LOG_0x0C,
    HDCDRV_LIMIT_LOG_0x0D,
    HDCDRV_LIMIT_LOG_0x0E,
    HDCDRV_LIMIT_LOG_0x0F,
    HDCDRV_LIMIT_LOG_0x10,
    HDCDRV_LIMIT_LOG_0x11,
    HDCDRV_LIMIT_LOG_0x12,
    HDCDRV_LIMIT_LOG_0x13,
    HDCDRV_LIMIT_LOG_0x14,
    HDCDRV_LIMIT_LOG_0x15,

    // only ADC use log
    HDCDRV_LIMIT_LOG_0x100,
    HDCDRV_LIMIT_LOG_0x101,
    HDCDRV_LIMIT_LOG_0x102,
    HDCDRV_LIMIT_LOG_0x103,
    HDCDRV_LIMIT_LOG_0x104,
    HDCDRV_LIMIT_LOG_0x105,
    HDCDRV_LIMIT_LOG_0x106,
    HDCDRV_LIMIT_LOG_0x107,
    HDCDRV_LIMIT_LOG_0x108,
    HDCDRV_LIMIT_LOG_0x109,
    HDCDRV_LIMIT_LOG_0x10A,

    HDCDRV_LIMIT_LOG_MAX
};
// not more than 10 kernel messages every 5s
#define EXCLUSIVE_RATELIMIT_INTERVAL	(30 * KA_HZ)
#define EXCLUSIVE_RATELIMIT_BURST		5
#define hdcdrv_limit_exclusive(level, id, fmt, ...) do { \
    static KA_BASE_DEFINE_RATELIMIT_STATE(id,               \
                      EXCLUSIVE_RATELIMIT_INTERVAL, \
                      EXCLUSIVE_RATELIMIT_BURST);   \
    if (__ka_base_ratelimit(&id) != 0) { \
        drv_##level(module_hdcdrv, "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
        ka_task_get_current_pid(), ##__VA_ARGS__); \
    } \
} while (0)

#endif /* _HDCDRV_LOG_H_ */
