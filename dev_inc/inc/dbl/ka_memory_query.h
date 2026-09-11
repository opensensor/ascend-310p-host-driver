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
 * Create: 2025-10-22
 */
#ifndef KA_MEMORY_QUERY_H
#define KA_MEMORY_QUERY_H
#include <linux/types.h>

struct svm_mem_query_ops {
    int (*get_svm_mem_pa)(u32 devid, int tgid, u64 addr, u64 size, u32 pa_num, u64 *pa_list);
    int (*put_svm_mem_pa)(u32 devid, int tgid, u64 addr, u64 size, u32 pa_num, u64 *pa_list);
    u32 (*get_svm_mem_page_size)(u32 devid, int tgid, u64 addr, u64 size);
};

int hal_kernel_get_mem_pa_list(u32 devid, int tgid, u64 addr, u64 size, u32 pa_num, u64 *pa_list);
int hal_kernel_put_mem_pa_list(u32 devid, int tgid, u64 addr, u64 size, u32 pa_num, u64 *pa_list);
u32 hal_kernel_get_mem_page_size(u32 devid, int tgid, u64 addr, u64 size);
int hal_kernel_register_mem_query_ops(struct svm_mem_query_ops *query_ops);
int hal_kernel_unregister_mem_query_ops(void);
#endif
