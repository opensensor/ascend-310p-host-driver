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

#ifndef SVM_SHMEM_INTERPROCESS_H
#define SVM_SHMEM_INTERPROCESS_H

#include "devmm_proc_info.h"
#include "devmm_common.h"

struct devmm_ipc_owner_attr {
    u32 devid;
    u32 vfid;
    u32 sdid;
    ka_pid_t pid;
    u64 va;
    u64 mem_map_route;
};

void devmm_destroy_ipc_mem_node_by_proc(struct devmm_svm_process *svm_proc, u32 devid);
int devmm_ioctl_ipc_mem_query(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
int devmm_ioctl_ipc_mem_close(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
int devmm_ioctl_ipc_mem_open(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
int devmm_ioctl_ipc_mem_destroy(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
int devmm_ioctl_ipc_mem_set_pid(struct devmm_svm_process *svm_pro, struct devmm_ioctl_arg *arg);
int devmm_ioctl_ipc_mem_create(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
struct devmm_svm_process *devmm_ipc_query_owner_info(struct devmm_svm_process *svm_proc,
    u64 va, u64 *owner_va, struct devmm_svm_process_id *id, u32 *sdid);
void devmm_try_free_ipc_mem(struct devmm_svm_process *svm_proc, u64 vptr, u64 page_num, u32 page_size);
int devmm_ipc_get_owner_proc_attr(struct devmm_svm_process *svm_proc, struct devmm_memory_attributes *attr,
    struct devmm_svm_process **owner_proc, struct devmm_svm_heap **heap, struct devmm_memory_attributes *owner_attr);
void devmm_ipc_put_owner_proc_attr(struct devmm_svm_process *owner_proc, struct devmm_svm_heap *heap);

int devmm_ipc_query_owner_attr_by_va(struct devmm_svm_process *svm_proc, u64 va, void *node_attr,
    struct devmm_ipc_owner_attr *owner_attr);

void devmm_ipc_mem_init(void);
void devmm_ipc_mem_uninit(void);
int devmm_ioctl_ipc_set_attr(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
int devmm_ioctl_ipc_get_attr(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);

#endif /* __SVM_SHMEM_INTERPROCESS_H */
