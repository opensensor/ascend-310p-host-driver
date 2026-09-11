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

#ifndef SVM_MEM_SPLIT_H
#define SVM_MEM_SPLIT_H

#include <linux/types.h>

#define SUPPORT_MEMORY_SPLIT    1

void devmm_set_memory_split_feature(u32 flag);

int devmm_normal_free_mem_size_sub(u32 devid, u32 vfid, int nid, u64 page_num);
void devmm_normal_free_mem_size_add(u32 devid, u32 vfid, int nid, u64 page_num);
int devmm_huge_free_mem_size_sub(u32 devid, u32 vfid, int nid, u64 page_num, u32 hugetlb_alloc_flag);
void devmm_huge_free_mem_size_add(u32 devid, u32 vfid, int nid, u64 page_num, u32 hugetlb_alloc_flag);
void devmm_alloc_numa_info_init(u32 devid, u32 vfid, int nids[], u32 *nid_num);
void devmm_alloc_numa_enable_threshold(u32 devid, u32 vfid, int nid);

#endif /* SVM_MEM_SPLIT_H */

