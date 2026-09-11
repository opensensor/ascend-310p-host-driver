/* SPDX-License-Identifier: GPL-2.0 */
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
#ifndef KA_DFX_PUB_H
#define KA_DFX_PUB_H

#include <linux/printk.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/gfp.h>
#include <linux/reboot.h>
#include <linux/notifier.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/console.h>
#include <linux/vmalloc.h>
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(5, 16, 0))
#include <linux/profile.h>
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 14, 0))
#include <linux/panic_notifier.h>
#endif
#include <linux/compiler.h>

#include "ka_common_pub.h"

typedef struct atomic_notifier_head ka_atomic_notifier_head_t;
typedef struct blocking_notifier_head ka_blocking_notifier_head_t;

typedef struct notifier_block ka_notifier_block_t;

typedef enum con_flush_mode ka_con_flush_mode_t;
#define KA_CONSOLE_FLUSH_PENDING CONSOLE_FLUSH_PENDING
#define KA_CONSOLE_REPLAY_ALL    CONSOLE_REPLAY_ALL

typedef struct raw_notifier_head ka_raw_notifier_head_t;

#define KA_KERN_SOH             KERN_SOH       /* ASCII Start Of Header */
#define KA_KERN_SOH_ASCII       KERN_SOH_ASCII
#define KA_KERN_EMERG           KERN_EMERG     /* system is unusable */
#define KA_KERN_ALERT           KERN_ALERT     /* action must be taken immediately */
#define KA_KERN_CRIT            KERN_CRIT     /* critical conditions */
#define KA_KERN_ERR             KERN_ERR     /* error conditions */
#define KA_KERN_WARNING         KERN_WARNING     /* warning conditions */
#define KA_KERN_NOTICE          KERN_NOTICE     /* normal but significant condition */
#define KA_KERN_INFO            KERN_INFO     /* informational */
#define KA_KERN_DEBUG           KERN_DEBUG     /* debug-level messages */

#define KA_KERN_DEFAULT         KERN_DEFAULT            /* the default kernel loglevel */

/* integer equivalents of KERN_<LEVEL> */
#define KA_LOGLEVEL_SCHED       LOGLEVEL_SCHED      /* Deferred messages from sched code are set to this special level */
#define KA_LOGLEVEL_DEFAULT     LOGLEVEL_DEFAULT    /* default (or last) loglevel */
#define KA_LOGLEVEL_EMERG       LOGLEVEL_EMERG      /* system is unusable */
#define KA_LOGLEVEL_ALERT       LOGLEVEL_ALERT       /* action must be taken immediately */
#define KA_LOGLEVEL_CRIT        LOGLEVEL_CRIT       /* critical conditions */
#define KA_LOGLEVEL_ERR         LOGLEVEL_ERR        /* error conditions */
#define KA_LOGLEVEL_WARNING     LOGLEVEL_WARNING    /* warning conditions */
#define KA_LOGLEVEL_NOTICE      LOGLEVEL_NOTICE     /* normal but significant condition */
#define KA_LOGLEVEL_INFO        LOGLEVEL_INFO       /* informational */
#define KA_LOGLEVEL_DEBUG       LOGLEVEL_DEBUG      /* debug-level messages */

#define ka_dfx_pr_notice pr_notice
#define ka_dfx_pr_err    pr_err
#define ka_dfx_pr_info   pr_info
#define ka_dfx_pr_warn   pr_warn
#define ka_dfx_pr_debug  pr_debug

#define KA_NOTIFY_BAD           NOTIFY_BAD
#define KA_NOTIFY_DONE          NOTIFY_DONE

#define KA_DFX_ATOMIC_INIT_NOTIFIER_HEAD(nb_head) ATOMIC_INIT_NOTIFIER_HEAD(nb_head)

#define KA_DFX_BLOCKING_INIT_NOTIFIER_HEAD(nb_head) BLOCKING_INIT_NOTIFIER_HEAD(nb_head)

int ka_dfx_atomic_notifier_panic_chain_register(ka_notifier_block_t *nb);
void ka_dfx_atomic_notifier_panic_chain_unregister(ka_notifier_block_t *nb);
#define ka_dfx_register_reboot_notifier(nb) register_reboot_notifier(nb)
#define ka_dfx_unregister_reboot_notifier(nb) unregister_reboot_notifier(nb)
#define ka_dfx_touch_softlockup_watchdog() touch_softlockup_watchdog()
#define ka_dfx_printk printk
#define ka_dfx_printk_timed_ratelimit(caller_jiffies, erval_msecs) printk_timed_ratelimit(caller_jiffies, erval_msecs)
#define ka_dfx_printk_ratelimited printk_ratelimited
#define ka_dfx_arch_touch_nmi_watchdog() arch_touch_nmi_watchdog()
#define ka_dfx_vprintk(fmt, args) vprintk(fmt, args)
#define ka_dfx_printk_ratelimit() printk_ratelimit()
int ka_dfx_vprintk_emit(int facility, int level, const char *fmt, va_list args);
unsigned long ka_dfx_kallsyms_lookup_name(const char *name);

#define ka_dfx_raw_notifier_call_chain(nh, val, v) raw_notifier_call_chain(nh, val, v)
#define ka_dfx_raw_notifier_chain_register(nh, nb) raw_notifier_chain_register(nh, nb)
#define ka_dfx_raw_notifier_chain_unregister(nh, nb) raw_notifier_chain_unregister(nh, nb)
#define ka_dfx_atomic_notifier_call_chain(nh, val,v) atomic_notifier_call_chain(nh, val,v)
#define ka_dfx_atomic_notifier_chain_register(nh, nb) atomic_notifier_chain_register(nh, nb)
#define ka_dfx_atomic_notifier_chain_unregister(nh, nb) atomic_notifier_chain_unregister(nh, nb)

#if LINUX_VERSION_CODE <= KERNEL_VERSION(5, 16, 0)
typedef enum profile_type ka_profile_type_t;
#define KA_PROFILE_TASK_EXIT PROFILE_TASK_EXIT
#define KA_PROFILE_MUNMAP PROFILE_MUNMAP

#define ka_dfx_profile_event_register(pt, nb) profile_event_register(pt, nb)
#define ka_dfx_profile_event_unregister(pt, nb) profile_event_unregister(pt, nb)
#else
/* func not support  */
typedef enum {
    KA_PROFILE_TASK_EXIT,
    KA_PROFILE_MUNMAP
} ka_profile_type_t;

static inline int ka_dfx_profile_event_register(ka_profile_type_t type, ka_notifier_block_t *nb)
{
    (void)type;
    (void)nb;
    return 0;
}
static inline int ka_dfx_profile_event_unregister(ka_profile_type_t type, ka_notifier_block_t *nb)
{
    (void)type;
    (void)nb;
    return 0;
}
#endif

#define KA_DFX_TRACE_EVENT(name, proto, args, struct, assign, print) TRACE_EVENT(name, proto, args, struct, assign, print)
#define KA_DFX_TP_PROTO TP_PROTO
#define KA_DFX_TP_ARGS TP_ARGS
#define KA_DFX_TP_STRUCT__entry TP_STRUCT__entry
#define KA_DFX_TP_printk TP_printk
#define KA_DFX_TP_fast_assign TP_fast_assign
#define __ka_dfx_string(item, src) __string(item, src)
#define __ka_dfx_field_struct(type, item) __field_struct(type, item)
#define __ka_dfx_assign_str(dst, src) __assign_str(dst, src)
#define __ka_entry __entry
#define __ka_dfx_get_str(field) __get_str(field)
#endif
