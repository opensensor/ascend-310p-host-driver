/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
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
 * Create: 2024-05-28
 */
#ifndef LOG_DRV_AGENT_H
#define LOG_DRV_AGENT_H

#include "log_drv_agent_alloc_interface.h"
#include "drv_log.h"

int log_get_ringbuffer(char *buff, u32 buf_len, u32 *out_len);
#endif