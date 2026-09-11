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

#ifndef DEVMM_VM_ADAPT_H
#define DEVMM_VM_ADAPT_H

#include "virtmng_interface.h"

#include "devmm_proc_info.h"

int devmm_vm_chan_page_fault_d2h_process(struct vmng_rx_msg_proc_info *proc_info);
struct devmm_shm_process_head *devmm_get_shm_pro_head(u32 map_type);
struct devmm_shm_pro_node *devmm_get_shm_pro_node(struct devmm_svm_process *svm_proc, u32 map_type);
ka_semaphore_t *devmm_get_shm_sem(struct devmm_svm_process *svm_proc, u32 map_type);
void devmm_remote_unmap_and_delete_node(struct devmm_svm_process *svm_proc, struct devmm_shm_node *node);
void devmm_shm_node_dev_res_release(struct devmm_shm_node *node);
void devmm_destory_shm_node(struct devmm_shm_node *node);
int devmm_unmap_mem(struct devmm_svm_process *svm_proc, u64 va, u64 size);
void devmm_mem_free_preprocess_by_dev_and_va(struct devmm_svm_process *svm_proc, u32 devid, u64 free_va, u64 free_len);
void devmm_mem_free_preprocess_by_dev(struct devmm_svm_process *svm_proc, u32 devid);

#endif
