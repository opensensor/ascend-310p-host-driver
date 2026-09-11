/*
* Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
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
* Create: 2022-6-10
*/

#include "workqueue_affinity.h"

#include <linux/irq.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/kallsyms.h>
#include <linux/module.h>

#include "ka_base_pub.h"
#include "ka_memory_pub.h"

#include "kernel_version_adapt.h"
#include "drv_log.h"
#include "drv_cpu_type.h"

#ifdef DRV_COMMON_UT
#define STATIC
#define symbol_get __symbol_get
#else
#define STATIC static
#endif

#define PRINT_ERR(fmt, ...) drv_err("workqueue", "<%s:%d> " fmt, current->comm, current->tgid, ##__VA_ARGS__)
#define PRINT_WARN(fmt, ...)  drv_warn("workqueue", "<%s:%d> " fmt, current->comm, current->tgid, ##__VA_ARGS__)
#define PRINT_INFO(fmt, ...)  drv_info("workqueue", "<%s:%d> " fmt, current->comm, current->tgid, ##__VA_ARGS__)
#define PRINT_EVENT(fmt, ...) drv_event("workqueue", "<%s:%d> " fmt, current->comm, current->tgid, ##__VA_ARGS__)
#define PRINT_DEBUG(fmt, ...) drv_pr_debug("workqueue", "<%s:%d> " fmt, current->comm, current->tgid, ##__VA_ARGS__)

/**
 *  The alloc_workqueue_attrs and free_workqueue_attrs functions is not
 *  exported in the kernel and needs to be searched through the symbol table.
 */
typedef struct workqueue_attrs *(alloc_workqueue_attrs_func)(gfp_t gfp_mask);
typedef void (free_workqueue_attrs_func)(struct workqueue_attrs *attrs);

/**
 * alloc workqueue attrs
 *
 * The alloc_workqueue_attrs function cannot be directly invoked because it is not exported.
 * need to search the kernel symbol table for this function.
 */
STATIC struct workqueue_attrs *symbol_alloc_workqueue_attrs(gfp_t gfp_mask)
{
    static alloc_workqueue_attrs_func *alloc_workqueue_attrs_pt = NULL;

    if (!alloc_workqueue_attrs_pt) {
        alloc_workqueue_attrs_pt = (alloc_workqueue_attrs_func *)
            (uintptr_t)__kallsyms_lookup_name("alloc_workqueue_attrs");
        if (IS_ERR_OR_NULL(alloc_workqueue_attrs_pt)) {
            PRINT_ERR("fail to find symbol alloc workqueue attrs\n");
            return NULL;
        }
    }

    return alloc_workqueue_attrs_pt(gfp_mask);
}

/**
 * free workqueue attrs
 *
 * The free_workqueue_attrs function cannot be directly invoked because it is not exported.
 * need to search the kernel symbol table for this function.
 */
STATIC void symbol_free_workqueue_attrs(struct workqueue_attrs *attrs)
{
    static free_workqueue_attrs_func *free_workqueue_attrs_pt = NULL;

    if (attrs == NULL) {
        return;
    }

    if (!free_workqueue_attrs_pt) {
        free_workqueue_attrs_pt = (free_workqueue_attrs_func *)
            (uintptr_t)__kallsyms_lookup_name("free_workqueue_attrs");
        if (IS_ERR_OR_NULL(free_workqueue_attrs_pt)) {
            PRINT_ERR("fail to find symbol free workqueue attrs\n");
            return;
        }
    }
    free_workqueue_attrs_pt(attrs);
}

/**
 * Setting the Affinity of the Workqueue Thread
 * @wq: unbound type workqueue
 * @flag: WQ_HIGHPRI or 0
 *
 * Bind the work queue thread to the control CPU.
 */
int set_workqueue_affinity(struct workqueue_struct *wq, u32 flag, const struct cpumask *wq_cpumask)
{
    struct workqueue_attrs *attrs = NULL;
    int ret;

    if (wq == NULL) {
        PRINT_ERR("alloc workqueue attrs failed\n");
        return -EINVAL;
    }
    if (wq_cpumask == NULL) {
        PRINT_ERR("workqueue's cpumask is null\n");
        return -EINVAL;
    }

    if (ka_base_cpumask_empty(wq_cpumask)) {
        PRINT_ERR("alloc workqueue attrs failed.\n");
        return -EINVAL;
    }

    attrs = symbol_alloc_workqueue_attrs(KA_GFP_KERNEL);
    if (attrs == NULL) {
        PRINT_ERR("alloc workqueue attrs failed\n");
        return -ENOMEM;
    }
    attrs->nice = (flag & WQ_HIGHPRI) ? MIN_NICE : 0;
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 5, 0)
    attrs->no_numa = true;
#endif
    ka_base_cpumask_copy(attrs->cpumask, wq_cpumask);

    ret = apply_workqueue_attrs(wq, attrs);
    if (ret != 0) {
        PRINT_ERR("apply workqueue attrs failed %d\n", ret);
    }
    symbol_free_workqueue_attrs(attrs);
    return ret;
}
