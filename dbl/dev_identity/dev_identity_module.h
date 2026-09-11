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
 * Author: huawei
 * Create: 2022-6-27
 */
#ifndef DEV_IDENTITY_MODULE_H
#define DEV_IDENTITY_MODULE_H

#include "ka_task_pub.h"
#include "drv_log.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#define module_didty "dbl_dev_identity"

#define didty_err(fmt, ...) drv_err(module_didty, \
    "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), ka_task_get_current_pid(), ##__VA_ARGS__)
#define didty_warn(fmt, ...) drv_warn(module_didty, \
    "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), ka_task_get_current_pid(), ##__VA_ARGS__)
#define didty_info(fmt, ...) drv_info(module_didty, \
    "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), ka_task_get_current_pid(), ##__VA_ARGS__)
#define didty_event(fmt, ...) drv_event(module_didty, \
    "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), ka_task_get_current_pid(), ##__VA_ARGS__)
#define didty_debug(fmt, ...) drv_pr_debug(module_didty, \
    "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), ka_task_get_current_pid(), ##__VA_ARGS__)

#endif /* DEV_IDENTITY_MODULE_H */

