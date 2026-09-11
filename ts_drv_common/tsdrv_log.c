/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
 * Create: 2025-02-07
 */

#ifndef TSDRV_UT
#include "drv_share_log.h"
#include "tsdrv_log.h"

void tsdrv_share_log_err_inner(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    share_log_err_ex(TSDRV_SHARE_LOG_START, fmt, args);
    va_end(args);
}
#else
void tsdrv_share_log_err_inner(const char *fmt, ...)
{
}
#endif

