/*
* Copyright (c) Huawei Technologies Co., Ltd. 2019-2022. All rights reserved.
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
* Create: 2019-10-15
*/

#include <linux/types.h>

#include "tsdrv_delay_recycle.h"
#include "tsdrv_kernel_common.h"
#include "tsdrv_log.h"

#ifndef TSDRV_KERNEL_UT

#include "tsdrv_osal_mm.h"
#include "ka_task_pub.h"
#include "ka_list_pub.h"

struct mem_delay_free_info {
    void *virt_addr;
    u32 size;
    ka_list_head_t list;
};

struct tsdrv_mem_delay_free {
    ka_list_head_t mem_list;
    ka_task_spinlock_t spinlock;
};

static struct tsdrv_mem_delay_free delay_free[TSDRV_MAX_DAVINCI_NUM];

void tsdrv_add_mem_delay_free(u32 devid, void *vaddr, u32 size)
{
#ifndef TSDRV_UT
    struct mem_delay_free_info *mem_info = NULL;

    mem_info = tsdrv_kzalloc(sizeof(struct mem_delay_free_info), KA_GFP_KERNEL);
    if (mem_info == NULL) {
        return;
    }

    mem_info->virt_addr = vaddr;
    mem_info->size = size;
    TSDRV_PRINT_DEBUG("devid=%u, vaddr=%pK\n", devid, (void *)(uintptr_t)vaddr);
    ka_task_spin_lock(&delay_free[devid].spinlock);
    ka_list_add(&mem_info->list, &delay_free[devid].mem_list);
    ka_task_spin_unlock(&delay_free[devid].spinlock);
#endif
}

void tsdrv_delay_free_mem(u32 devid)
{
    struct mem_delay_free_info *mem_info = NULL;
    ka_list_head_t *pos = NULL, *n = NULL;

    ka_task_spin_lock(&delay_free[devid].spinlock);
    if (ka_list_empty_careful(&delay_free[devid].mem_list) != 0) {
        ka_task_spin_unlock(&delay_free[devid].spinlock);
        return;
    }
#ifndef TSDRV_UT
    ka_list_for_each_safe(pos, n, &delay_free[devid].mem_list) {
        mem_info = ka_list_entry(pos, struct mem_delay_free_info, list);
        ka_list_del(&mem_info->list);

        if (mem_info->virt_addr != NULL) {
            TSDRV_PRINT_DEBUG("devid=%u, vaddr=%pK\n", devid, (void *)(uintptr_t)mem_info->virt_addr);
            if (mem_info->size != 0) {
                tsdrv_free_pages_exact(mem_info->virt_addr, mem_info->size);
            } else {
                tsdrv_kfree(mem_info->virt_addr);
            }
            mem_info->virt_addr = NULL;
        }
        tsdrv_kfree(mem_info);
    }
    ka_task_spin_unlock(&delay_free[devid].spinlock);
#endif
}

void tsdrv_mem_delay_free_init(u32 devid)
{
    KA_INIT_LIST_HEAD(&delay_free[devid].mem_list);
    ka_task_spin_lock_init(&delay_free[devid].spinlock);
}

#else
void tsdrv_mem_delay_free_init(u32 devid)
{
}
#endif
