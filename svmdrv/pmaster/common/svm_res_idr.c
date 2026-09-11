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

#include "svm_log.h"
#include "devmm_common.h"
#include "svm_res_idr.h"

void devmm_idr_init(struct svm_idr *idr, int max_id)
{
    ka_base_idr_init(&idr->idr);
    idr->next_id = 0;
    idr->max_id = max_id;
    ka_task_mutex_init(&idr->lock);
}

int devmm_idr_alloc(struct svm_idr *idr, char *ptr, int *id_out)
{
    int id;

    ka_task_mutex_lock(&idr->lock);
    id = ka_base_idr_alloc(&idr->idr, ptr, idr->next_id, idr->max_id, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (id < 0) {
        if (idr->next_id != 0) {
            id = ka_base_idr_alloc(&idr->idr, ptr, 0, idr->next_id, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
        }

        if (id < 0) {
            ka_task_mutex_unlock(&idr->lock);
            devmm_drv_err("Idr alloc fail. (ret=%d, max_num=%d)\n", id, idr->max_id);
            return id;
        }
    }

    idr->next_id = (id + 1) % idr->max_id;
    *id_out = id;
    ka_task_mutex_unlock(&idr->lock);

    return 0;
}

char *devmm_idr_get_and_remove(struct svm_idr *idr, int id, idr_remove_condition condition)
{
    char *ptr = NULL;

    ka_task_mutex_lock(&idr->lock);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0)
    ptr = ka_base_idr_find(&idr->idr, id);
#else
    ptr = ka_base_idr_find(&idr->idr, (u64)id);
#endif
    if (ptr != NULL) {
        if ((condition != NULL) && (condition(ptr) == false)) {
            ptr = NULL;
            goto find_fail;
        }
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0)
        ka_base_idr_remove(&idr->idr, id);
#else
        (void)ka_base_idr_remove(&idr->idr, (u64)id);
#endif
    }
find_fail:
    ka_task_mutex_unlock(&idr->lock);
    return ptr;
}

bool devmm_idr_is_empty(struct svm_idr *idr)
{
    return ka_base_idr_is_empty(&idr->idr);
}

void devmm_idr_destroy(struct svm_idr *idr, void (*free_ptr)(const void *ptr))
{
    char *entry = NULL;
    int id;
    ka_task_mutex_lock(&idr->lock);
    ka_base_idr_for_each_entry(&idr->idr, entry, id) {
        if (free_ptr != NULL) {
            free_ptr(entry);
            entry = NULL;
        }
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0)
        ka_base_idr_remove(&idr->idr, id);
#else
        ka_base_idr_remove(&idr->idr, (unsigned int)id);
#endif
    }
    ka_base_idr_destroy(&idr->idr);
    ka_task_mutex_unlock(&idr->lock);
}

