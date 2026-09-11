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
 * Create: 2024-11-12
 */
#ifndef EMU_ST
#include "drv_share_log.h"
#include "svm_log.h"
void devmm_share_log_err_inner(const char *fmt, ...)
{
#ifdef CFG_FEATURE_SHARE_LOG
    va_list args;

    va_start(args, fmt);
    share_log_err_ex(DEVMM_SHARE_LOG_START, fmt, args);
    va_end(args);
#endif
}

void devmm_share_log_run_info_inner(const char *fmt, ...)
{
#ifdef CFG_FEATURE_SHARE_LOG
    va_list args;

    va_start(args, fmt);
    share_log_run_info_ex(DEVMM_SHARE_LOG_RUNINFO_START, fmt, args);
    va_end(args);
#endif
}
#else
void devmm_share_log_err_inner(const char *fmt, ...)
{
}

void devmm_share_log_run_info_inner(const char *fmt, ...)
{
}
#endif
