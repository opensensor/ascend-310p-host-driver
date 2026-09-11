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

#ifndef SVM_LOG_H
#define SVM_LOG_H

#ifndef EMU_ST
#include <linux/types.h>
#include "drv_log.h"
#else
#include "ut_log.h"
#endif

#define module_devmm "devmm"

void devmm_share_log_err_inner(const char *fmt, ...);
void devmm_share_log_run_info_inner(const char *fmt, ...);

#ifndef EMU_ST
#define devmm_drv_err(fmt, ...) do { \
    drv_err(module_devmm, "<%s:%d,%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), ka_task_get_current_pid(), ##__VA_ARGS__); \
    devmm_share_log_err_inner(fmt, ##__VA_ARGS__); \
} while (0)
#else
#define devmm_drv_err(fmt, ...)
#endif

#define devmm_drv_run_info(fmt, ...) do { \
    drv_info(module_devmm, "<%s:%d,%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), ka_task_get_current_pid(), ##__VA_ARGS__); \
    devmm_share_log_run_info_inner(fmt, ##__VA_ARGS__); \
} while (0)

#define devmm_drv_warn(fmt, ...) \
    drv_warn(module_devmm, "<%s:%d,%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), ka_task_get_current_pid(), ##__VA_ARGS__)

#define devmm_drv_info(fmt, ...) \
    drv_info(module_devmm, "<%s:%d,%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), ka_task_get_current_pid(), ##__VA_ARGS__)

#define devmm_drv_debug_arg(fmt, ...) \
    drv_debug(module_devmm, "<%s:%d,%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), ka_task_get_current_pid(), ##__VA_ARGS__)

#define devmm_drv_debug(fmt, ...) \
    drv_pr_debug(module_devmm, "<%s:%d,%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), ka_task_get_current_pid(), ##__VA_ARGS__)

#define devmm_drv_info_if(cond, fmt, ...) \
    if (cond)                             \
    devmm_drv_info(fmt, ##__VA_ARGS__)

#define devmm_drv_warn_if(cond, fmt, ...) \
    if (cond)                             \
    devmm_drv_warn(fmt, ##__VA_ARGS__)

#define devmm_drv_err_if(cond, fmt, ...) \
    if (cond)                            \
    devmm_drv_err(fmt, ##__VA_ARGS__)

#define devmm_drv_no_err_if(cond, fmt, ...) \
    if (cond) {                             \
        devmm_drv_info(fmt, ##__VA_ARGS__); \
    } else {                                \
        devmm_drv_err(fmt, ##__VA_ARGS__);  \
    }
#endif /* __SVM_LOG_H__ */
