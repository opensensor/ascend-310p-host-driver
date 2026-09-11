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

#ifndef __DAVINCI_INTF_COMMON_H__
#define __DAVINCI_INTF_COMMON_H__

#include <linux/sched.h>
#include "drv_log.h"


#define module_davinci_intf "ascend_dev"
#define log_intf_err(fmt, ...)    \
    drv_err(module_davinci_intf, "<%s:%d,%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__)
#define log_intf_warn(fmt, ...)    \
    drv_warn(module_davinci_intf, "<%s:%d,%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__)
#define log_intf_info(fmt, ...)    \
    drv_info(module_davinci_intf, "<%s:%d,%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__)
#define log_intf_event(fmt, ...)    \
    drv_event(module_davinci_intf, "<%s:%d,%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__)
#define log_intf_debug(fmt, ...)    \
    drv_pr_debug(module_davinci_intf, "<%s:%d,%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__)

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#ifndef NULL
#define NULL 0
#endif

#define VALID 1
#define INVALID 0

#ifdef FALSE
#undef FALSE
#endif
#define FALSE 0

#ifdef TRUE
#undef TRUE
#endif
#define TRUE 1

#endif
