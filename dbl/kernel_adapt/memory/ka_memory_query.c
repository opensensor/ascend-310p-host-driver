/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
 * Create: 2025-10-15
 */

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/rwsem.h>

#include "ka_module_init.h"
#include "ka_memory_query.h"

static struct svm_mem_query_ops g_svm_mem_query_ops = {0};
struct rw_semaphore g_svm_mem_query_lock;

int hal_kernel_get_mem_pa_list(u32 devid, int tgid, u64 addr, u64 size, u32 pa_num, u64 *pa_list)
{
    int ret = -EFAULT;
    down_read(&g_svm_mem_query_lock);
    if (g_svm_mem_query_ops.get_svm_mem_pa != NULL) {
        ret = g_svm_mem_query_ops.get_svm_mem_pa(devid, tgid, addr, size, pa_num, pa_list);
    }

    up_read(&g_svm_mem_query_lock);
    return ret;
}
EXPORT_SYMBOL(hal_kernel_get_mem_pa_list);

int hal_kernel_put_mem_pa_list(u32 devid, int tgid, u64 addr, u64 size, u32 pa_num, u64 *pa_list)
{
    int ret = -EFAULT;
    down_read(&g_svm_mem_query_lock);
    if (g_svm_mem_query_ops.put_svm_mem_pa != NULL) {
        ret = g_svm_mem_query_ops.put_svm_mem_pa(devid, tgid, addr, size, pa_num, pa_list);
    }

    up_read(&g_svm_mem_query_lock);
    return ret;
}
EXPORT_SYMBOL(hal_kernel_put_mem_pa_list);

u32 hal_kernel_get_mem_page_size(u32 devid, int tgid, u64 addr, u64 size)
{
    u32 page_size = 0;

    down_read(&g_svm_mem_query_lock);
    if (g_svm_mem_query_ops.get_svm_mem_page_size != NULL) {
        page_size = g_svm_mem_query_ops.get_svm_mem_page_size(devid, tgid, addr, size);
    }

    up_read(&g_svm_mem_query_lock);
    return page_size;
}
EXPORT_SYMBOL(hal_kernel_get_mem_page_size);

int hal_kernel_register_mem_query_ops(struct svm_mem_query_ops *query_ops)
{
    if ((query_ops == NULL) || (query_ops->get_svm_mem_pa == NULL) ||
        (query_ops->put_svm_mem_pa == NULL) || (query_ops->get_svm_mem_page_size == NULL)) {
        return -EINVAL;
    }

    down_write(&g_svm_mem_query_lock);

    g_svm_mem_query_ops.get_svm_mem_pa = query_ops->get_svm_mem_pa;
    g_svm_mem_query_ops.put_svm_mem_pa = query_ops->put_svm_mem_pa;
    g_svm_mem_query_ops.get_svm_mem_page_size = query_ops->get_svm_mem_page_size;

    up_write(&g_svm_mem_query_lock);

    return 0;
}
EXPORT_SYMBOL(hal_kernel_register_mem_query_ops);

int hal_kernel_unregister_mem_query_ops(void)
{
    down_write(&g_svm_mem_query_lock);

    g_svm_mem_query_ops.get_svm_mem_pa = NULL;
    g_svm_mem_query_ops.put_svm_mem_pa = NULL;
    g_svm_mem_query_ops.get_svm_mem_page_size = NULL;

    up_write(&g_svm_mem_query_lock);

    return 0;
}
EXPORT_SYMBOL(hal_kernel_unregister_mem_query_ops);
