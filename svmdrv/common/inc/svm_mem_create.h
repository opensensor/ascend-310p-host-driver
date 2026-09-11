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
 * Create: 2023-08-11
 */
#ifndef SVM_MEM_CREATE_H
#define SVM_MEM_CREATE_H

#include "svm_gfp.h"
#include "devmm_proc_info.h"

#define MEM_CREATE_INVALID_ID (-1)

int devmm_mem_create_to_new_blk(struct devmm_svm_process *svm_proc,
    struct devmm_phy_addr_attr *attr, u64 total_pg_num, u64 to_create_pg_num, int *id);
int devmm_mem_create_to_old_blk(struct devmm_svm_process *svm_proc,
    struct devmm_phy_addr_attr *attr, u64 to_create_pg_num, int id);
int devmm_mem_release(struct devmm_svm_process *svm_proc, int id, u64 to_free_pg_num, u32 free_type);
int _devmm_mem_release(struct devmm_svm_process *svm_proc, struct devmm_phy_addr_blk_mng *mng, int id,
    u64 to_free_pg_num, u32 free_type);

#endif
