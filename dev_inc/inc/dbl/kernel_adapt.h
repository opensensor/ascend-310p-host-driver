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
#ifndef KERNEL_ADPAT_H
#define KERNEL_ADPAT_H
#ifndef EMU_ST

#include <linux/mm_types.h>
#include <linux/notifier.h>
#include <trace/events/sched.h>

int task_exit_notify_register(struct notifier_block *n);
int task_exit_notify_unregister(struct notifier_block *n);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
enum profile_type {
    PROFILE_TASK_EXIT,
    PROFILE_MUNMAP
};

int profile_event_register(enum profile_type type, struct notifier_block *n);
int profile_event_unregister(enum profile_type type, struct notifier_block *n);
#endif
#endif
#endif