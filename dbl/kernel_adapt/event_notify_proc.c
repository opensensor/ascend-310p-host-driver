/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* Description:
* Author: huawei
* Create: 2024-3-11
*/
#include <linux/export.h>
#include <linux/notifier.h>
#include <linux/version.h>
#include <linux/mm_types.h>

#include "ka_module_init.h"
#include "kernel_adapt.h"
#include "event_notify_proc.h"

/*
 * DC: Host side do not use this，Device side use
 * MDC: use Custom kernel, not user this
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)) && (defined CFG_FEATURE_KA_EVENT_NOTIFY_PROC)

STATIC BLOCKING_NOTIFIER_HEAD(task_exit_notifier);

int task_exit_notify_register(struct notifier_block *n)
{
    return blocking_notifier_chain_register(&task_exit_notifier, n);
}
EXPORT_SYMBOL_GPL(task_exit_notify_register);

int task_exit_notify_unregister(struct notifier_block *n)
{
    return blocking_notifier_chain_unregister(&task_exit_notifier, n);
}
EXPORT_SYMBOL_GPL(task_exit_notify_unregister);

static void task_exit_notify_call(struct task_struct *task)
{
    blocking_notifier_call_chain(&task_exit_notifier, 0, task);
}

STATIC void kernel_adapt_task_exit_info(void *name, struct task_struct *task)
{
    task_exit_notify_call(task);
}

int event_notify_proc_init(void)
{
    int ret = register_trace_sched_process_exit(kernel_adapt_task_exit_info, NULL);
    if (ret != 0) {
        ka_err("Failed to register proc exit trace. (ret=%d)\n", ret);
        return -1;
    }
    return 0;
}

void event_notify_proc_uninit(void)
{
    unregister_trace_sched_process_exit(kernel_adapt_task_exit_info, NULL);
    tracepoint_synchronize_unregister();
    ka_info("Unregister proc exit trace.\n");
}

int profile_event_register(enum profile_type type, struct notifier_block *n)
{
    (void)type;
    return task_exit_notify_register(n);
}
EXPORT_SYMBOL_GPL(profile_event_register);

int profile_event_unregister(enum profile_type type, struct notifier_block *n)
{
    (void)type;
    return task_exit_notify_unregister(n);
}
EXPORT_SYMBOL_GPL(profile_event_unregister);
#else
int event_notify_proc_init(void)
{
    return 0;
}

void event_notify_proc_uninit(void)
{
    return;
}
#endif