/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
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
 * Create: 2022-08-13
 */
#ifndef __DP_PROC_MNG_LOG_H__
#define __DP_PROC_MNG_LOG_H__
#ifndef EMU_ST
#include "drv_log.h"
#include "drv_share_log.h"

#define module_dp_proc_mng "dp_proc_mng"
#define dp_proc_mng_drv_err(fmt, ...) do { \
    drv_err(module_dp_proc_mng, "<%s:%d,%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__); \
} while (0)

#define dp_proc_mng_drv_warn(fmt, ...) \
    drv_warn(module_dp_proc_mng, "<%s:%d,%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__)
#define dp_proc_mng_drv_info(fmt, ...) \
    drv_info(module_dp_proc_mng, "<%s:%d,%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__)
#define dp_proc_mng_drv_debug(fmt, ...) \
    drv_pr_debug(module_dp_proc_mng, "<%s:%d,%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__)
#else
#include "ut_log.h"

#define dp_proc_mng_drv_err(fmt, ...) do { \
    printf("[%s %d] <%s:%d,%d> " fmt, __FUNCTION__, __LINE__, current->comm, current->tgid, current->pid, ##__VA_ARGS__); \
} while (0)
#define dp_proc_mng_drv_warn(fmt, ...) do { \
    printf("[%s %d] <%s:%d,%d> " fmt, __FUNCTION__, __LINE__, current->comm, current->tgid, current->pid, ##__VA_ARGS__); \
} while (0)
#define dp_proc_mng_drv_info(fmt, ...) do { \
    printf("[%s %d] <%s:%d,%d> " fmt, __FUNCTION__, __LINE__, current->comm, current->tgid, current->pid, ##__VA_ARGS__); \
} while (0)
#define dp_proc_mng_drv_debug(fmt, ...) do { \
    printf("[%s %d] <%s:%d,%d> " fmt, __FUNCTION__, __LINE__, current->comm, current->tgid, current->pid, ##__VA_ARGS__); \
} while (0)
#endif

#endif /* __DP_PROC_MNG_LOG_H__ */
