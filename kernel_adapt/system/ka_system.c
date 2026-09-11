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
 * Create: 2024-11-11
 */

#include "securec.h"
#include "ka_memory_pub.h"
#include "ka_system_pub.h"
#include "ka_system.h"

// provide get and set interface
ka_system_states_t ka_system_get_system_state(void)
{
    return system_state;
}
EXPORT_SYMBOL_GPL(ka_system_get_system_state);

// kernel/cpu.c
// (const struct cpumask *)
// provide get-interface
const ka_cpumask_t *ka_system_get_cpu_online_mask(void)
{
    return cpu_online_mask;
}
EXPORT_SYMBOL_GPL(ka_system_get_cpu_online_mask);

// kernel/cpu.c
// (const struct cpumask *)
// provide get-interface
const ka_cpumask_t *ka_system_get_cpu_possible_mask(void)
{
    return cpu_possible_mask;
}
EXPORT_SYMBOL_GPL(ka_system_get_cpu_possible_mask);

// kernel/irq/manage.c
// original interface: const void *free_irq(unsigned int irq, void *dev_id)
const void *ka_system_free_irq(unsigned int irq, void *dev_id)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
    return free_irq(irq, dev_id);
#else
    free_irq(irq, dev_id);
    return NULL;
#endif
}
EXPORT_SYMBOL_GPL(ka_system_free_irq);

int ka_system_timespec_compare(const ka_timespec_t *lhs, const ka_timespec_t *rhs)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 5, 0)
    if (lhs->tv_sec < rhs->tv_sec)
        return -1;
    if (lhs->tv_sec > rhs->tv_sec)
        return 1;
    return lhs->tv_nsec - rhs->tv_nsec;
#else
    return timespec_compare(lhs, rhs);
#endif
}
EXPORT_SYMBOL_GPL(ka_system_timespec_compare);

ka_timespec_t ka_system_current_kernel_time(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0)
    struct timespec ka_ts = {0};
    struct timespec64 ts64;

    ktime_get_coarse_real_ts64(&ts64);
    ka_ts.tv_sec = (__kernel_long_t)ts64.tv_sec;
    ka_ts.tv_nsec = ts64.tv_nsec;
    return ka_ts;
#else
    return current_kernel_time();
#endif
}
EXPORT_SYMBOL_GPL(ka_system_current_kernel_time);

// kernel/time/time.c
// struct timezone sys_tz
// provide get-interface
ka_timezone_t *ka_system_get_sys_tz(void)
{
    return &sys_tz;
}
EXPORT_SYMBOL_GPL(ka_system_get_sys_tz);

// kernel/time/timekeeping.c
// original interface: int do_settimeofday64(const struct timespec64 *ts)
int ka_system_do_settimeofday64(const ka_timespec64_t *ts)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
    return do_settimeofday64((const struct timespec64 *)ts);
#else
    return do_settimeofday((const struct timespec *)ts);
#endif
}
EXPORT_SYMBOL_GPL(ka_system_do_settimeofday64);

ka_time64_t ka_system_timespec64_get_tv_sec(const ka_timespec64_t *ts)
{
    return ts->tv_sec;
}
EXPORT_SYMBOL_GPL(ka_system_timespec64_get_tv_sec);

long ka_system_timespec64_get_tv_nsec(const ka_timespec64_t *ts)
{
    return ts->tv_nsec;
}
EXPORT_SYMBOL_GPL(ka_system_timespec64_get_tv_nsec);

void ka_system_do_gettimeofday(ka_timeval_t *tv)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0)
    struct timespec64 ts;
    if (tv == NULL) {
        return;
    }
    ktime_get_real_ts64(&ts);
    tv->tv_sec = ts.tv_sec;
    tv->tv_usec = ts.tv_nsec / NSEC_PER_USEC;
#else
    do_gettimeofday((struct timeval *)tv);
#endif
}
EXPORT_SYMBOL_GPL(ka_system_do_gettimeofday);

long ka_system_get_timeval_sec(ka_timeval_t *tv)
{
    return ((struct timeval *)tv)->tv_sec;
}
EXPORT_SYMBOL_GPL(ka_system_get_timeval_sec);

long ka_system_get_timeval_nsec(ka_timeval_t *tv)
{
    return ((struct timeval *)tv)->tv_usec;
}
EXPORT_SYMBOL_GPL(ka_system_get_timeval_nsec);

// linux/timer.h
// original macro: timer_setup(timer, fn, flags)
typedef void (*timer_func_type)(unsigned long);
void ka_system_timer_setup(ka_timer_list_t *timer, void (*fn)(ka_timer_list_t *), unsigned int flags)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
    timer_setup(timer, fn, flags);
#else
    setup_timer(timer, (timer_func_type)fn, (unsigned long)(uintptr_t)timer);
#endif
}
EXPORT_SYMBOL_GPL(ka_system_timer_setup);

/**
 * @brief get privileged kernel capability
 * @return privileged kernel capability value
 */
ka_kernel_cap_t ka_system_get_privileged_kernel_cap(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
    kernel_cap_t privileged =
        (kernel_cap_t) {((uint64_t)(CAP_TO_MASK(CAP_AUDIT_READ + 1) -1) << 32) | (((uint64_t)~0) >> 32)};
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
    kernel_cap_t privileged = (kernel_cap_t){{ ~0, (CAP_TO_MASK(CAP_AUDIT_READ + 1) -1)}};
#else
    kernel_cap_t privileged = CAP_FULL_SET;
#endif
    return privileged;
}
EXPORT_SYMBOL_GPL(ka_system_get_privileged_kernel_cap);

/**
 * @brief compare kernel capability
 * @param [in] cap1: capability
 * @param [in] cap2: capability
 * @return true means same, false means different
 */
bool ka_system_kernel_cap_compare(ka_kernel_cap_t cap1, ka_kernel_cap_t cap2)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
    if ((cap1.val & cap2.val) != cap2.val) {
        return false;
    }
#else
    unsigned int i;
    CAP_FOR_EACH_U32(i) {
        if ((cap1.cap[i] & cap2.cap[i]) != cap2.cap[i]) {
            return false;
        }
    }
#endif
    return true;
}
EXPORT_SYMBOL_GPL(ka_system_kernel_cap_compare);

void ka_system_get_abs_or_rel_mstime(u64 *time_now)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
    struct timespec64 now;
    ktime_get_real_ts64(&now);
    // milliseconds = seconds * 1000 + nanoseconds / 1000000
    *time_now = (u64)(now.tv_sec * 1000 + now.tv_nsec / 1000000);
#else
    struct timeval now;
    do_gettimeofday(&now);
    // milliseconds = seconds * 1000 + microseconds / 1000
    *time_now = (u64)(now.tv_sec * 1000 + now.tv_usec / 1000);
#endif
}
EXPORT_SYMBOL_GPL(ka_system_get_abs_or_rel_mstime);

void ka_system_rtc_time_convert(ka_rtc_time_t *tm, ka_timespec64_t sys_time)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
    *tm = ka_system_rtc_ktime_to_tm(ka_system_timespec64_to_ktime(sys_time));
#else
    ka_system_rtc_time_to_tm(sys_time.tv_sec, tm);
#endif
}
EXPORT_SYMBOL_GPL(ka_system_rtc_time_convert);

int ka_system_get_rtc_time_sec(ka_rtc_time_t *tm)
{
    return tm->tm_sec;
}
EXPORT_SYMBOL_GPL(ka_system_get_rtc_time_sec);

int ka_system_get_rtc_time_min(ka_rtc_time_t *tm)
{
    return tm->tm_min;
}
EXPORT_SYMBOL_GPL(ka_system_get_rtc_time_min);

int ka_system_get_rtc_time_hour(ka_rtc_time_t *tm)
{
    return tm->tm_hour;
}
EXPORT_SYMBOL_GPL(ka_system_get_rtc_time_hour);

int ka_system_get_rtc_time_mday(ka_rtc_time_t *tm)
{
    return tm->tm_mday;
}
EXPORT_SYMBOL_GPL(ka_system_get_rtc_time_mday);

int ka_system_get_rtc_time_mon(ka_rtc_time_t *tm)
{
    return tm->tm_mon;
}
EXPORT_SYMBOL_GPL(ka_system_get_rtc_time_mon);

int ka_system_get_rtc_time_year(ka_rtc_time_t *tm)
{
    return tm->tm_year;
}
EXPORT_SYMBOL_GPL(ka_system_get_rtc_time_year);