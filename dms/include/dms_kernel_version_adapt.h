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

#ifndef __DMS_KERNEL_VERSION_ADAPT_H__
#define __DMS_KERNEL_VERSION_ADAPT_H__

#include <linux/version.h>

#include "kernel_version_adapt.h"

#ifdef AOS_LLVM_BUILD

#ifndef USEC_PER_MSEC
#define USEC_PER_MSEC 1000L
#endif

#ifndef NSEC_PER_USEC
#define NSEC_PER_USEC 1000L
#endif

#endif
#if (!defined CFG_HOST_ENV) && (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0))
/* used by drv_memory */
#include <linux/time.h>
#include <linux/time64.h>
#include <linux/timekeeping.h>

static inline void do_gettimeofday(struct timeval *tv)
{
    struct timespec64 ts;
    ktime_get_real_ts64(&ts);
    tv->tv_sec = ts.tv_sec;
    tv->tv_usec = ts.tv_nsec / NSEC_PER_USEC;
}
#endif

#endif
