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
 */
#ifndef SVM_DYNAMIC_ADDR_H
#define SVM_DYNAMIC_ADDR_H

#include "ka_memory_pub.h"
#include "ka_base_pub.h"

struct devmm_svm_process;
struct devmm_svm_heap;

#define DEVMM_DA_STATUS_NONE        0U
#define DEVMM_DA_STATUS_UNMAPING    1U

struct svm_da_info {
    ka_rw_semaphore_t rwsem; /* for safety access vma */
    ka_rwlock_t rbtree_rwlock;
    ka_rb_root_t rbtree;
};

void svm_da_init(struct devmm_svm_process *svm_proc);
void svm_use_da(struct devmm_svm_process *svm_proc);
void svm_unuse_da(struct devmm_svm_process *svm_proc);
void svm_occupy_da(struct devmm_svm_process *svm_proc);
void svm_release_da(struct devmm_svm_process *svm_proc);

void svm_da_recycle(struct devmm_svm_process *svm_proc);
int svm_da_add_addr(struct devmm_svm_process *svm_proc, u64 va, u64 size, ka_vm_area_struct_t *vma);
int svm_da_del_addr(struct devmm_svm_process *svm_proc, u64 va, u64 size);
bool svm_is_da_addr(struct devmm_svm_process *svm_proc, u64 va, u64 size);
bool svm_is_da_match(struct devmm_svm_process *svm_proc, u64 va, u64 size);
ka_vm_area_struct_t *svm_da_query_vma(struct devmm_svm_process *svm_proc, u64 va);
int svm_da_set_custom_vma_nolock(struct devmm_svm_process *svm_proc, u64 va, ka_vm_area_struct_t *vma);
int svm_da_set_custom_vma(struct devmm_svm_process *svm_proc, u64 va, ka_vm_area_struct_t *vma);
ka_vm_area_struct_t *svm_da_query_custom_vma(struct devmm_svm_process *svm_proc, u64 va);
u32 svm_da_query_addr_num(struct devmm_svm_process *svm_proc);
/* callback func in spinlock, can not block in func */
int svm_da_for_each_addr(struct devmm_svm_process *svm_proc, bool is_occupy,
    int (*func)(struct devmm_svm_process *svm_proc, u64 va, u64 size, void *priv), void *priv);

void svm_set_da_heap(struct devmm_svm_process *svm_proc, u32 heap_idx, struct devmm_svm_heap *heap);
struct devmm_svm_heap *svm_get_da_heap_by_idx(struct devmm_svm_process *svm_proc, u32 heap_idx);
void svm_set_da_status(struct devmm_svm_process *svm_proc, u64 va, u64 size, u32 status);
bool svm_is_da_unmap(struct devmm_svm_process *svm_proc, u64 va, u64 size);
#endif

