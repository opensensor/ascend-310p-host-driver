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
 * Create: 2022-08-13
 */

#ifndef __SVM_INTERFACE_H__
#define __SVM_INTERFACE_H__

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/list.h>
#include <linux/mm.h>

struct devmm_pfn_owner_info {
    struct list_head list;
    int dev_pid;
    u16 size_shift;
    u64 va;
};

/*
 * The caller needs to ensure the mutual exclusivity of the list and the memory release of the list node.
 * hal_kernel_svm_query_pfn_owner_info : head should be an empty list, otherwise the query fails.
 * hal_kernel_svm_clear_pfn_owner_info_list : head should be the value returned by hal_kernel_svm_query_pfn_owner_info.
 */
int hal_kernel_svm_query_pfn_owner_info(u32 devid, u64 pfn, struct list_head *head);
void hal_kernel_svm_clear_pfn_owner_info_list(struct list_head *head);
int devmm_get_pages_list(struct mm_struct *mm, u64 va, u64 num, struct page **pages);
#endif

struct devmm_set_convert_len_para {
    unsigned long long total_convert_len;
};

struct devmm_get_convert_len_para {
    unsigned long long total_convert_len;
};

int halMemGet(unsigned long long addr, unsigned long long size);
int halMemPut(unsigned long long addr, unsigned long long size);

#endif
