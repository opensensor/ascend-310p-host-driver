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
#ifndef TS_AGENT_LOG_H
#define TS_AGENT_LOG_H

#define module_ts_agent "ts_agent"
#ifndef TS_AGENT_UT

#include "drv_log.h"

#define ts_agent_err(fmt, ...) \
    drv_err(module_ts_agent, "<%s:%d,%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__)
#define ts_agent_warn(fmt, ...) \
    drv_warn(module_ts_agent, "<%s:%d,%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__)
#define ts_agent_info(fmt, ...) \
    drv_info(module_ts_agent, "<%s:%d,%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__)

#ifdef TS_AGENT_DEBUG
#define ts_agent_debug(fmt, ...) \
    drv_pr_debug(module_ts_agent, "<%s:%d,%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__)
#else
#define ts_agent_debug(fmt, ...) \
    pr_debug("[%s] [%s %d] " fmt, module_ts_agent, __func__, __LINE__, ##__VA_ARGS__)
#endif

#else

#include "stdio.h"

#define ts_agent_err(fmt, ...) \
    printf("[%s %s][%s %d]" fmt "\n", "error", module_ts_agent, __func__, __LINE__, ##__VA_ARGS__)
#define ts_agent_warn(fmt, ...) \
    printf("[%s %s][%s %d]" fmt "\n", "warn", module_ts_agent, __func__, __LINE__, ##__VA_ARGS__)
#define ts_agent_info(fmt, ...) \
    printf("[%s %s][%s %d]" fmt "\n", "info", module_ts_agent, __func__, __LINE__, ##__VA_ARGS__)
#define ts_agent_debug(fmt, ...) \
    printf("[%s %s][%s %d]" fmt "\n", "debug", module_ts_agent, __func__, __LINE__, ##__VA_ARGS__)

#endif

#endif // TS_AGENT_LOG_H
