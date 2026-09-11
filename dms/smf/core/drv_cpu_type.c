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
#include "drv_cpu_type.h"

#ifndef COMM_UT
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/cpu.h>
#include <linux/kthread.h>
#include <linux/fs.h>
#include <linux/kallsyms.h>
#include <linux/bitmap.h>
#include "linux/firmware.h"
#include <linux/aos/cpu_domain_info.h>

#include "ka_base_pub.h"

#include "drv_log.h"

#define MODULE_NAME "drv_comm"
#ifdef UT_VCAST
#define drv_comm_err(fmt, ...) drv_err(MODULE_NAME, fmt, ##__VA_ARGS__)
#define drv_comm_warn(fmt, ...) drv_warn(MODULE_NAME, fmt, ##__VA_ARGS__)
#define drv_comm_info(fmt, ...) drv_info(MODULE_NAME, fmt, ##__VA_ARGS__)
#define drv_comm_event(fmt, ...) drv_event(MODULE_NAME, fmt, ##__VA_ARGS__)
#else
#define drv_comm_err(fmt, ...) \
    drv_err(MODULE_NAME, "<%s:%d> " fmt, current->comm, current->tgid, ##__VA_ARGS__)
#define drv_comm_warn(fmt, ...) \
    drv_warn(MODULE_NAME, "<%s:%d> " fmt, current->comm, current->tgid, ##__VA_ARGS__)
#define drv_comm_info(fmt, ...) \
    drv_info(MODULE_NAME, "<%s:%d> " fmt, current->comm, current->tgid, ##__VA_ARGS__)
#define drv_comm_event(fmt, ...) \
    drv_event(MODULE_NAME, "<%s:%d> " fmt, current->comm, current->tgid, ##__VA_ARGS__)
#endif

static void drv_bitmap_to_cpumask(unsigned long bitmap, cpumask_t *cpumask)
{
    u32 cpu_bit;

    while (bitmap > 0) {
        cpu_bit = ka_base_ffs(bitmap) - 1;
        cpumask_set_cpu(cpu_bit, cpumask);
        bitmap &= ~(1U << cpu_bit);
    }
}

int __attribute__((weak)) drv_get_ctrlcpu_mask(cpumask_t *cpumask)
{
    struct cpu_domain_info  cpu_domain;
    int ret;

    ret = get_cpudomain_info(&cpu_domain);
    if (ret != 0) {
        drv_comm_err("get cpudomain info failed. (ret=%d)\n", ret);
        return ret;
    }

    if (cpu_domain.ctrlcpu_bitmap == 0) {
        drv_comm_warn("ctrl cpu mask is 0, cpumask will not be set\n");
        return -EEXIST;
    }

    /* get cpumask from cpu */
    drv_bitmap_to_cpumask(cpu_domain.ctrlcpu_bitmap, cpumask);
    return 0;
}
#else
int drv_get_ctrlcpu_mask(cpumask_t *cpumask)
{
    return 0;
}
#endif
