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
 * Create: 2022-6-23
 */
#ifndef RUNENV_CONFIG_MODULE_H
#define RUNENV_CONFIG_MODULE_H

#include "ka_task_pub.h"
#include "drv_log.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#define module_recfg "dbl_runenv_config"

#define recfg_err(fmt, ...) drv_err(module_recfg, \
    "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), ka_task_get_current_pid(), ##__VA_ARGS__)
#define recfg_warn(fmt, ...) drv_warn(module_recfg, \
    "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), ka_task_get_current_pid(), ##__VA_ARGS__)
#define recfg_info(fmt, ...) drv_info(module_recfg, \
    "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), ka_task_get_current_pid(), ##__VA_ARGS__)
#define recfg_event(fmt, ...) drv_event(module_recfg, \
    "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), ka_task_get_current_pid(), ##__VA_ARGS__)
#define recfg_debug(fmt, ...) drv_pr_debug(module_recfg, \
    "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), ka_task_get_current_pid(), ##__VA_ARGS__)

#endif /* RUNENV_CONFIG_MODULE_H */

