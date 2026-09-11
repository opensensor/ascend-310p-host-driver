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

#ifndef SVM_MEM_QUERY_H
#define SVM_MEM_QUERY_H

#include "svm_cmd.h"

#define DEVMM_SVM_ADDR 0
#define DEVMM_SHM_ADDR 1

struct devmm_pa_lists_info {
    u64 *pa_list;
    u32 pa_num;
    bool pin_pa_list;
};

bool devmm_check_addr_valid(struct devmm_svm_process_id *process_id, u64 addr, u64 size);
int devmm_get_mem_pa_list(struct devmm_svm_process_id *process_id, u64 addr, u64 size,
    u64 *pa_list, u32 pa_num);
void devmm_put_mem_pa_list(struct devmm_svm_process_id *process_id, u64 addr, u64 size,
    u64 *pa_list, u32 pa_num);
u32 devmm_get_mem_page_size(struct devmm_svm_process_id *process_id, u64 addr, u64 size);
bool devmm_svm_need_ib_register_peer(void);
int devmm_check_thread_valid(int hostpid, const char *sign, u32 len);

int devmm_svm_check_addr_valid(struct devmm_svm_process_id *process_id, u64 addr, u64 size);
int devmm_shm_check_addr_valid(struct devmm_svm_process_id *process_id, u64 addr, u64 size);

int devmm_svm_get_pa_list(struct devmm_svm_process_id *process_id,
    u64 aligned_va, u64 aligned_size, struct devmm_pa_lists_info *info);
int devmm_svm_get_and_pin_pa_list(struct devmm_svm_process_id *process_id,
    u64 aligned_va, u64 aligned_size, u64 *pa_list, u32 pa_num);
int devmm_shm_get_pa_list(struct devmm_svm_process_id *process_id, u64 addr, u64 size, u64 *pa_list, u32 pa_num);
void devmm_shm_put_pa_list(struct devmm_svm_process_id *process_id, u64 va, u64 *pa_list, u32 pa_num);
void devmm_svm_put_pa_list(struct devmm_svm_process_id *process_id, u64 va, u64 *pa_list, u32 pa_num);
u32 devmm_shm_get_page_size(struct devmm_svm_process_id *process_id, u64 va, u64 size);
int devmm_get_mem_side(struct devmm_svm_process_id *process_id, u64 addr, u32 *side);
int devmm_svm_mem_query_ops_register(void);
void devmm_svm_mem_query_ops_unregister(void);

#endif /* __SVM_MEM_QUERY_H__ */
