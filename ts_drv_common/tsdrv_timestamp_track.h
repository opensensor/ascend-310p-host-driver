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
* Create: 2024-08-05
*/

#ifndef TSDRV_TIMESTAMP_TRACK_H
#define TSDRV_TIMESTAMP_TRACK_H

#if defined(__x86_64__)
static uint64_t tsdrv_get_cur_system_counter(void)
{
    uint64_t cycles;
    uint32_t hi = 0;
    uint32_t lo = 0;
    const int uint32Bits = 32; // 32 is uint bit count

    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    cycles = ((uint64_t)lo) | ((uint64_t)hi << uint32Bits);

    return cycles;
}
#else
static inline u64 tsdrv_get_cur_system_counter(void)
{
    u64 cycles = 0;
    asm volatile("mrs %0, CNTVCT_EL0" : "=r"(cycles) :);
    return cycles;
}
#endif

static inline u64 tsdrv_get_cur_cpu_tick(void)
{
    return tsdrv_get_cur_system_counter();
}

#endif
