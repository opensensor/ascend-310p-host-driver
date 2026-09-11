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
* Create: 2022-12-25
*/
#ifndef DRV_SYSTIME_H
#define DRV_SYSTIME_H

#include <linux/time.h>
#include "kernel_version_adapt.h"

#ifdef DRV_SUPPORT_SYSCOUNT
static inline unsigned long long get_syscnt(void)
{
    unsigned long long syscnt;
    asm volatile("mrs %0, CNTPCT_EL0" : "=r"(syscnt)::"memory");
    return syscnt;
}
#else

#define USECS_PER_SEC   (1000*1000)
#define NSECS_PER_USEC  (1000)
static inline unsigned long long get_syscnt(void)
{
    struct timespec stamp = current_kernel_time();
    return (unsigned long long)(stamp.tv_sec * USECS_PER_SEC + stamp.tv_nsec / NSECS_PER_USEC);
}
#endif

#endif
