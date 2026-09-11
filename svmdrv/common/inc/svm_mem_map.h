/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
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
 * Create: 2023-08-21
 */
#ifndef SVM_MEM_MAP_H
#define SVM_MEM_MAP_H

#include "devmm_proc_info.h"
#include "svm_vmma_mng.h"

int devmm_mem_map(struct devmm_svm_process *svm_proc, struct devmm_vmma_info *info, bool need_page_adjust);
void devmm_mem_unmap(struct devmm_svm_process *svm_proc, struct devmm_vmma_info *info);
void devmm_access_munmap_all(struct devmm_svm_process *svm_proc, struct devmm_vmma_struct *vmma);
u64 *devmm_mem_map_adjust_pa_create(u64 dst_pg_num, u64 dst_pg_size, u64 *src_addr, u64 src_pg_num, u64 adjust_num);
void devmm_mem_map_adjust_pa_destroy(u64 *adjust_pa);

#endif
