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
#ifndef QUEUE_MODULE_H
#define QUEUE_MODULE_H

#include "ka_task_pub.h"
#include "ka_system_pub.h"

#ifndef EMU_ST
#include "drv_log.h"
#include "drv_share_log.h"
#else
#include "ut_log.h"
#endif

#include "ascend_hal_define.h"
#include "queue_drv_alloc_interface.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC    static
#endif

#define QUEUE_DEV_NAME        "hi-queue-manage"

#define MAX_SHARE_GRP  4  /* surport max sp-group in queue_manage, "zone" means same */

#ifdef DRV_HOST
#define MAX_DEVICE 1124
#else
#define MAX_DEVICE 64
#endif

#ifndef MAX_STR_LEN
#define MAX_STR_LEN 128
#endif

#define MIN_VALID_QUEUE_DEPTH    2         /* min depth of queue */
#define MAX_QUEUE_DEPTH 8192
#define QUEUE_HDC_SERVICE_TYPE 16

#ifdef CFG_SOC_PLATFORM_ESL_FPGA
#define MAX_SURPORT_QUEUE_NUM 128
#else
#define MAX_SURPORT_QUEUE_NUM 4096 /* user space should to be modified synchronously. */
#endif

#define module_queue_manage "queue_manage"
#define QUEUE_CURRENT_TGID (ka_task_get_current_tgid())
#define QUEUE_CURRENT_COMM (ka_task_get_current_comm())
#define QUEUE_CURRENT_PID (ka_task_get_current_pid())
#define QUEUE_CURRENT_MM (ka_task_get_current_mm())

#ifndef EMU_ST
#define queue_err(fmt, ...) do { \
    drv_err(module_queue_manage, "<%s:%d> " fmt, QUEUE_CURRENT_COMM, QUEUE_CURRENT_TGID, ##__VA_ARGS__); \
    share_log_err(QUEUE_SHARE_LOG_START, fmt, ##__VA_ARGS__); \
} while (0)
#define queue_warn(fmt, ...) do { \
    drv_warn(module_queue_manage, "<%s:%d> " fmt, QUEUE_CURRENT_COMM, QUEUE_CURRENT_TGID, ##__VA_ARGS__); \
} while (0)
#define queue_info(fmt, ...) do { \
    drv_info(module_queue_manage, "<%s:%d> " fmt, QUEUE_CURRENT_COMM, QUEUE_CURRENT_TGID, ##__VA_ARGS__); \
} while (0)
#define queue_debug(fmt, ...) do { \
    drv_pr_debug(module_queue_manage, "<%s:%d> " fmt, QUEUE_CURRENT_COMM, QUEUE_CURRENT_TGID, ##__VA_ARGS__); \
} while (0)
#define queue_event(fmt, ...) do { \
    drv_event(module_queue_manage, "<%s:%d> " fmt, QUEUE_CURRENT_COMM, QUEUE_CURRENT_TGID, ##__VA_ARGS__); \
} while (0)
#define queue_run_info(fmt, ...) do { \
    share_log_run_info(QUEUE_SHARE_LOG_RUNINFO_START, fmt, QUEUE_CURRENT_COMM, QUEUE_CURRENT_TGID, ##__VA_ARGS__); \
} while (0)

#else
#define queue_err(fmt, ...) printf
#define queue_warn(fmt, ...)
#define queue_info(fmt, ...)
#define queue_debug(fmt, ...)
#define queue_event(fmt, ...)
#define queue_run_info(fmt, ...)
#endif

static inline long long int queue_get_ktime_ms(void)
{
    return ka_system_ktime_to_ms(ka_system_ktime_get_boottime());
}

static inline long long int queue_get_ktime_us(void)
{
    return ka_system_ktime_to_us(ka_system_ktime_get_boottime());
}

static inline bool queue_is_mcast_event(u32 subevent_id)
{
    return ((subevent_id == DRV_SUBEVENT_ENQUEUE_MSG) || (subevent_id == DRV_SUBEVENT_DEQUEUE_MSG)
        || (subevent_id == DRV_SUBEVENT_SUBF2NF_MSG) || (subevent_id == DRV_SUBEVENT_UNSUBF2NF_MSG)
        || (subevent_id == DRV_SUBEVENT_SUBE2NE_MSG) || (subevent_id == DRV_SUBEVENT_UNSUBE2NE_MSG)
        || (subevent_id == DRV_SUBEVENT_PEEK_MSG));
}

#endif
