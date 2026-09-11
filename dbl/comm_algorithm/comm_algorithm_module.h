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
#ifndef COMM_ALGORITHM_MODULE_H
#define COMM_ALGORITHM_MODULE_H

#include "ka_task_pub.h"
#include "drv_log.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#define module_calgo "dbl_algorithm"

#define calgo_err(fmt, ...) drv_err(module_calgo, \
    "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), ka_task_get_current_pid(), ##__VA_ARGS__)
#define calgo_warn(fmt, ...) drv_warn(module_calgo, \
    "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), ka_task_get_current_pid(), ##__VA_ARGS__)
#define calgo_info(fmt, ...) drv_info(module_calgo, \
    "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), ka_task_get_current_pid(), ##__VA_ARGS__)
#define calgo_event(fmt, ...) drv_event(module_calgo, \
    "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), ka_task_get_current_pid(), ##__VA_ARGS__)
#define calgo_debug(fmt, ...) drv_pr_debug(module_calgo, \
    "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), ka_task_get_current_pid(), ##__VA_ARGS__)

#endif /* COMM_ALGORITHM_MODULE_H */

