/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2021. All rights reserved.
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
#ifndef _DEVDRV_UTIL_H_
#define _DEVDRV_UTIL_H_

#include <linux/sched.h>
#include "drv_log.h"

#define module_devdrv "drv_pcie"

#ifndef DRV_UT
#define devdrv_err(fmt, ...) do { \
    drv_err(module_devdrv, "<%s:%d:%d> " fmt, \
    current->comm, current->tgid, current->pid, ##__VA_ARGS__); \
    share_log_err(COMMON_SHARE_LOG_START, fmt, ##__VA_ARGS__); \
} while (0);
#else
#define devdrv_err(fmt, ...)
#endif

#define devdrv_warn(fmt, ...) do { \
    drv_warn(module_devdrv, "<%s:%d:%d> " fmt, \
    current->comm, current->tgid, current->pid, ##__VA_ARGS__); \
} while (0);
#define devdrv_info(fmt, ...) do { \
    drv_info(module_devdrv, "<%s:%d:%d> " fmt, \
    current->comm, current->tgid, current->pid, ##__VA_ARGS__); \
} while (0);
#define devdrv_event(fmt, ...) do { \
    drv_event(module_devdrv, "<%s:%d:%d> " fmt, \
    current->comm, current->tgid, current->pid, ##__VA_ARGS__); \
} while (0);
#define devdrv_debug(fmt, ...)

#define devdrv_err_limit(fmt, ...) do { \
    if (printk_ratelimit() != 0) \
        drv_err(module_devdrv, "<%s:%d:%d> " fmt, \
        current->comm, current->tgid, current->pid, ##__VA_ARGS__); \
} while (0);

#define devdrv_warn_limit(fmt, ...) do { \
    if (printk_ratelimit() != 0) { \
        drv_warn(module_devdrv, "<%s:%d:%d> " fmt, \
        current->comm, current->tgid, current->pid, ##__VA_ARGS__); \
    } \
} while (0);

#define devdrv_err_spinlock(fmt, ...)
#define devdrv_warn_spinlock(fmt, ...)
#define devdrv_info_spinlock(fmt, ...)
#define devdrv_event_spinlock(fmt, ...)
#define devdrv_debug_spinlock(fmt, ...)

#define UNUSED(expr) do { \
    (void)(expr); \
} while (0)

#ifdef DRV_UT
#define STATIC
#else
#define STATIC static
#endif

#define DEVDRV_INVALID 0
#define DEVDRV_VALID 1
#define AGENTDRV_INVALID 0
#define AGENTDRV_EVAID 1

#define DEVDRV_DISABLE 0
#define DEVDRV_ENABLE 1u
#define AGENTDRV_DISABLE 0
#define AGENTDRV_ENABLE 1

#define AGENTDRV_MSLEEP_10 10

#define AGENTDRV_NVME_LOW_LEVEL_DB_IRQ_NUM 2
#define AGENTDRV_NVME_DB_IRQ_STRDE 8

#define BOARD_MINI_PCIE_CARD 0x0
#define BOARD_MINI_PCIE_CARD_BOARDID 1
#define BOARD_MINI_MDC 0x1
#define BOARD_MINI_MDC_BOARDID 2
#define BOARD_MINI_EVB 0x2
#define BOARD_MINI_EVB_BOARDID_900 900
#define BOARD_MINI_EVB_BOARDID_901 901
#define BOARD_MINI_EVB_BOARDID_902 902
#define BOARD_MINI_OTHERS 0x3
#define BOARD_CLOUD_PCIE_CARD 0x100
#define BOARD_CLOUD_AI_SERVER 0x101
#define BOARD_CLOUD_EVB 0x102
#define BOARD_CLOUD_OTHERS 0x103

#define BOARD_CLOUD_EVB_DEV_NUM 2
#define BOARD_CLOUD_AI_SERVER_DEV_NUM 4
#define BOARD_CLOUD_MAX_DEV_NUM 8
#define BOARD_MINIV2_DEV_NUM 1

#define BOARD_CLOUD_V2_EVB 0x201

#ifdef CFG_HOST
#define DEVDRV_MAX_DEVICE 64
#else
#define DEVDRV_MAX_DEVICE 4
#endif

#endif
