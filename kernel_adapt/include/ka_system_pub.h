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

#ifndef KA_SYSTEM_PUB_H
#define KA_SYSTEM_PUB_H

#include <uapi/linux/posix_types.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/rtc.h>
#include <linux/capability.h>
#include <linux/fs.h>
#include <linux/cred.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kmod.h>
#include <linux/atomic.h>
#include <linux/moduleparam.h>
#include <linux/rcupdate.h>
#include <linux/cpumask.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/hrtimer.h>
#include <linux/lockdep.h>
#include <linux/nodemask.h>
#include <linux/byteorder/generic.h>
#include <linux/numa.h>
#include <linux/preempt.h>
#include <linux/stat.h>
#include <linux/ktime.h>
#include <linux/gfp.h>
#include <linux/topology.h>

#include "ka_macro_pub.h"
#include "ka_common_pub.h"

#define KA_NSEC_PER_SEC         NSEC_PER_SEC
#define KA_NSEC_PER_MSEC	    NSEC_PER_MSEC

#define	KA_NUMA_NO_NODE	        NUMA_NO_NODE

#define KA_CLOCK_MONOTONIC      CLOCK_MONOTONIC

/*
 * Values used for system_state. Ordering of the states must not be changed
 * as code checks for <, <=, >, >= STATE.
 */
typedef enum system_states ka_system_states_t;
#define KA_SYSTEM_BOOTING       SYSTEM_BOOTING
#define KA_SYSTEM_SCHEDULING    SYSTEM_SCHEDULING
#define KA_SYSTEM_RUNNING       SYSTEM_RUNNING
#define KA_SYSTEM_HALT          SYSTEM_HALT
#define KA_SYSTEM_POWER_OFF     SYSTEM_POWER_OFF
#define KA_SYSTEM_RESTART       SYSTEM_RESTART
#define KA_SYSTEM_SUSPEND       SYSTEM_SUSPEND

typedef enum hrtimer_restart    ka_hrtimer_restart_t;
#define KA_HRTIMER_NORESTART    HRTIMER_NORESTART
#define KA_HRTIMER_RESTART      HRTIMER_RESTART

typedef enum hrtimer_mode        ka_hrtimer_mode_t;
#define KA_HRTIMER_MODE_ABS                   HRTIMER_MODE_ABS
#define KA_HRTIMER_MODE_REL                   HRTIMER_MODE_REL
#define KA_HRTIMER_MODE_PINNED                HRTIMER_MODE_PINNED
#define KA_HRTIMER_MODE_SOFT                  HRTIMER_MODE_SOFT
#define KA_HRTIMER_MODE_HARD                  HRTIMER_MODE_HARD
#define KA_HRTIMER_MODE_ABS_PINNED            HRTIMER_MODE_ABS_PINNED
#define KA_HRTIMER_MODE_REL_PINNED            HRTIMER_MODE_REL_PINNED
#define KA_HRTIMER_MODE_ABS_SOFT              HRTIMER_MODE_ABS_SOFT
#define KA_HRTIMER_MODE_REL_SOFT              HRTIMER_MODE_REL_SOFT
#define KA_HRTIMER_MODE_ABS_PINNED_SOFT       HRTIMER_MODE_ABS_PINNED_SOFT
#define KA_HRTIMER_MODE_REL_PINNED_SOFT       HRTIMER_MODE_REL_PINNED_SOFT
#define KA_HRTIMER_MODE_ABS_HARD              HRTIMER_MODE_ABS_HARD
#define KA_HRTIMER_MODE_REL_HARD              HRTIMER_MODE_REL_HARD
#define KA_HRTIMER_MODE_ABS_PINNED_HARD       HRTIMER_MODE_ABS_PINNED_HARD
#define KA_HRTIMER_MODE_REL_PINNED_HARD       HRTIMER_MODE_REL_PINNED_HARD

typedef struct rtc_time ka_rtc_time_t;
typedef struct irq_desc ka_irq_desc_t;
typedef struct kernel_param_ops ka_kernel_param_ops_t;
typedef struct tasklet_struct ka_tasklet_struct_t;

#ifndef EMU_ST
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 5, 0)
#ifndef STRUCT_TIMEVAL_SPEC
#define STRUCT_TIMEVAL_SPEC
struct timespec {
    __kernel_old_time_t tv_sec;     /* seconds */
    long tv_nsec;                   /* nanoseconds */
};

struct timeval {
    __kernel_old_time_t tv_sec;     /* seconds */
    __kernel_suseconds_t tv_usec;   /* microseconds */
};
#endif
#endif
#endif /* ifndef EMUST */

#define ka_ktime_t           ktime_t
typedef struct timespec64    ka_timespec64_t;
typedef struct timespec      ka_timespec_t;
typedef struct hrtimer       ka_hrtimer_t;
typedef struct timezone      ka_timezone_t;

#define ka_kernel_cap_t                 kernel_cap_t
#define KA_SYSTEM_CAP_TO_MASK(x)        CAP_TO_MASK(x)

#define ka_time64_t             time64_t
typedef struct timezone         ka_timezone_t;
typedef struct timeval          ka_timeval_t;
typedef struct timer_list       ka_timer_list_t;
typedef struct tm               ka_tm_t;
typedef struct subprocess_info  ka_ka_subprocess_info_t;
typedef struct kernel_param_ops ka_kernel_param_ops_t;

#define KA_MAX_NUMNODES       MAX_NUMNODES
#define ka_jiffies            jiffies
#define ka_jiffies_64         jiffies_64
#define ka_nr_cpu_ids         nr_cpu_ids

#define ka_nr_cpus_node(node) nr_cpus_node(node)
#define ka_system_irq_of_parse_and_map(node, index)    irq_of_parse_and_map(node, index)
#define ka_system_put_cred(cred)     put_cred(cred)
#define ka_system_in_group_p(grp)    in_group_p(grp)
#define  ka_system_devm_free_irq(dev, irq, dev_id)     devm_free_irq(dev, irq, dev_id)
#define ka_system_devm_request_threaded_irq(dev, irq, handler, thread_fn, irqflags, devname, dev_id)    \
            devm_request_threaded_irq(dev, irq, handler, thread_fn, irqflags, devname, dev_id)
#define ka_system_devm_request_irq(dev, irq, handler, irqflags, devname, dev_id)    \
            devm_request_irq(dev, irq, handler, irqflags, devname, dev_id)
#define ka_system_disable_irq(irq)    disable_irq(irq)
#define ka_system_disable_irq_nosync(irq)    disable_irq_nosync(irq)
#define ka_system_enable_irq(irq)    enable_irq(irq)
#define ka_system_request_irq(irq, handler, irqflags, devname, dev_id)   \
            request_irq(irq, handler, irqflags, devname, dev_id)
#define ka_system_request_threaded_irq(irq, handler, thread_fn, irqflags, devname, dev_id)    \
            request_threaded_irq(irq, handler, thread_fn, irqflags, devname, dev_id)
#define  ka_system_synchronize_irq(irq)     synchronize_irq(irq)
#define __ka_system_symbol_get(symbol)    __symbol_get(symbol)
#define __ka_system_request_module    __request_module
#define  __ka_system_module_get(module)     __module_get(module)
#define  __ka_system_symbol_put(symbol)     __symbol_put(symbol)
#define ka_system_module_refcount(mod)    module_refcount(mod)
#define ka_system_try_module_get(module)    try_module_get(module)
#define ka_system_module_put(module)     module_put(module)
#define ka_system_tasklet_schedule(t)     tasklet_schedule(t)
#define ka_system_tasklet_kill(t)     tasklet_kill(t)
#define ka_system_msecs_to_jiffies(m)    msecs_to_jiffies(m)
#define ka_system_jiffies_64_to_clock_t(x)    jiffies_64_to_clock_t(x)
#define ka_system_jiffies_to_clock_t(x)    jiffies_to_clock_t(x)
#define ka_system_jiffies_to_msecs(j)    jiffies_to_msecs(j)
#define ka_system_jiffies_to_usecs(j)    jiffies_to_usecs(j)
#define ka_system_time_before(a, b)    time_before(a, b)
#define ka_system_time_after(a, b)     time_after(a,b)
#define ka_system_time_before_eq(a, b)    time_before_eq(a, b)
#define ka_system_ktime_get()    ktime_get()
#define ka_system_ktime_set(secs, nsecs)    ktime_set(secs, nsecs)
#define ka_system_ktime_to_ns(kt)    ktime_to_ns(kt)
#define ka_system_ktime_get_ns()    ktime_get_ns()
#define ka_system_ktime_get_raw()    ktime_get_raw()
#define ka_system_ktime_to_ms(kt)    ktime_to_ms(kt)
#define ka_system_ktime_get_boottime()    ktime_get_boottime()
#define ka_system_ktime_to_us(kt)    ktime_to_us(kt)
#define ka_system_add_timer(timer)     add_timer(timer)
#define ka_system_del_timer(timer)    del_timer(timer)
#define ka_system_del_timer_sync(timer)    del_timer_sync(timer)
#define ka_system_add_timer_on(timer, cpu)     add_timer_on(timer, cpu)
#define ka_system_mod_timer(timer, expires)    mod_timer(timer, expires)
#define ka_system_mod_timer_pending(timer, expires)    mod_timer_pending(timer, expires)
#define ka_system_msleep(msecs)    msleep(msecs)
#define ka_system_msleep_interruptible(msecs)    msleep_interruptible(msecs)
#define ka_system_schedule_timeout(timeout)    schedule_timeout(timeout)
#define ka_system_schedule_timeout_killable(timeout)    schedule_timeout_killable(timeout)
#define ka_system_schedule_timeout_uninterruptible(timeout)   schedule_timeout_uninterruptible(timeout)
#define ka_system_usleep_range(min, max)     usleep_range(min, max)
#define ka_system_call_usermodehelper_exec(sub_info, wait)    call_usermodehelper_exec(sub_info, wait)
#define ka_system_get_cpu_mask(cpu)    get_cpu_mask(cpu)
#define ka_system_raw_smp_processor_id()    raw_smp_processor_id()
#define ka_system_smp_processor_id() smp_processor_id()
#define ka_system_numa_node_id()    numa_node_id()
#define ka_system_cpu_to_node(cpu)    cpu_to_node(cpu)
#define ka_system_num_online_cpus()    num_online_cpus()
#define ka_system_cpu_to_be32(x)    cpu_to_be32(x)
#define ka_system_cpu_to_le32(x)    cpu_to_le32(x)
#define ka_system_cpu_to_be64(x)    cpu_to_be64(x)
#define ka_system_cpu_to_le64(x)    cpu_to_le64(x)
static inline void ka_system_set_hrtimer_func(ka_hrtimer_t *timer, ka_hrtimer_restart_t (*func)(ka_hrtimer_t *))
{
    timer->function = func;
}
#define ka_system_hrtimer_start(timer, tim, mode)    hrtimer_start(timer, tim, mode)
#define ka_system_hrtimer_cancel(timer)    hrtimer_cancel(timer)
#define ka_system_in_interrupt()    in_interrupt()
#define ka_system_ktime_get_ts64(ka_ts)    ktime_get_ts64(ka_ts)
#define ka_system_capable(cap)    capable(cap)
#define ka_system_tasklet_init(t, func, data)      tasklet_init(t, func, data)
#define ka_system_ktime_get_coarse_real_ts64(ts)    ktime_get_coarse_real_ts64(ts)
#define ka_system_ktime_get_coarse_ts64(ts)    ktime_get_coarse_ts64(ts)
#define ka_system_ktime_get_raw_ts64(ts)    ktime_get_raw_ts64(ts)
#define ka_system_ktime_get_real_ts64(ts)    ktime_get_real_ts64(ts)
#define ka_system_ktime_get_raw_ns()    ktime_get_raw_ns()
#define ka_system_ms_to_ktime(ms)    ms_to_ktime(ms)
#define ka_system_timespec64_to_ktime(ts)    timespec64_to_ktime(ts)
#define ka_system_schedule_timeout_idle(timeout)   schedule_timeout_idle(timeout)
#define ka_system_jiffies_to_timeval(jiffies, value)  jiffies_to_timeval(jiffies, value)
#define ka_system_hrtimer_forward_now(timer, interval)    hrtimer_forward_now(timer, interval)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
#define ka_system_rtc_ktime_to_tm(kt)    rtc_ktime_to_tm(kt)
#else
#define ka_system_rtc_time_to_tm(time, tm)   rtc_time_to_tm(time, tm)
#endif
#define ka_system_time_is_before_jiffies(a)              ka_system_time_after(ka_jiffies, a)
#define ka_system_hrtimer_init(timer, clock_id, mode)    hrtimer_init(timer, clock_id, mode)
#define ka_system_ssleep(seconds) ssleep(seconds)

ka_system_states_t ka_system_get_system_state(void);
const ka_cpumask_t *ka_system_get_cpu_online_mask(void);
const ka_cpumask_t *ka_system_get_cpu_possible_mask(void);
const void *ka_system_free_irq(unsigned int irq, void *dev_id);
const ka_kernel_param_ops_t *ka_system_get_param_array_ops(void);
const ka_kernel_param_ops_t *ka_system_get_param_ops_bool(void);
const ka_kernel_param_ops_t *ka_system_get_param_ops_charp(void);
#define __ka_system_local_bh_enable_ip(ip, cnt) __local_bh_enable_ip(ip, cnt)
#define ka_system_jiffies_to_timespec64(jiffies, value) jiffies_to_timespec64(jiffies, value)
#define ka_system_jiffies64_to_nsecs(j) jiffies64_to_nsecs(j)
int ka_system_timespec_compare(const ka_timespec_t *lhs, const ka_timespec_t *rhs);
ka_timespec_t ka_system_current_kernel_time(void);
ka_timezone_t *ka_system_get_sys_tz(void);
#define ka_system_time64_to_tm(totalsecs, offset, result) time64_to_tm(totalsecs, offset, result)
void ka_system_ktime_get_coarse_real_ts64(ka_timespec64_t *ts);
void ka_system_do_gettimeofday(ka_timeval_t *tv);

long ka_system_get_timeval_sec(ka_timeval_t *tv);
long ka_system_get_timeval_nsec(ka_timeval_t *tv);

#ifndef from_timer
#define from_timer(var, callback_timer, timer_fieldname) container_of(callback_timer, typeof(*var), timer_fieldname)
#endif
#define ka_system_from_timer(var, callback_timer, timer_fieldname) from_timer(var, callback_timer, timer_fieldname) 
void ka_system_timer_setup(ka_timer_list_t *timer, void (*fn)(ka_timer_list_t *), unsigned int flags);

#define ka_system_call_usermodehelper(path, argv, envp, wait) call_usermodehelper(path, argv, envp, wait)
int ka_system_security_ib_endport_manage_subnet(void *sec, const char *dev_name, u8 port_num);

int ka_system_security_ib_pkey_access(void *sec, u64 subnet_prefix, u16 pkey);
ka_kernel_cap_t ka_system_get_privileged_kernel_cap(void);
bool ka_system_kernel_cap_compare(ka_kernel_cap_t cap1, ka_kernel_cap_t cap2);
int ka_system_do_settimeofday64(const ka_timespec64_t *ts);
ka_time64_t ka_system_timespec64_get_tv_sec(const ka_timespec64_t *ts);
long ka_system_timespec64_get_tv_nsec(const ka_timespec64_t *ts);
void ka_system_get_abs_or_rel_mstime(u64 *time_now);
void ka_system_rtc_time_convert(ka_rtc_time_t *tm, ka_timespec64_t sys_time);
int ka_system_get_rtc_time_sec(ka_rtc_time_t *tm);
int ka_system_get_rtc_time_min(ka_rtc_time_t *tm);
int ka_system_get_rtc_time_hour(ka_rtc_time_t *tm);
int ka_system_get_rtc_time_mday(ka_rtc_time_t *tm);
int ka_system_get_rtc_time_mon(ka_rtc_time_t *tm);
int ka_system_get_rtc_time_year(ka_rtc_time_t *tm);
#endif
