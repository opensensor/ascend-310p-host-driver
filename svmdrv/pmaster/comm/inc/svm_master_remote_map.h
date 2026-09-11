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
#ifndef DEVMM_RECOMMEND_H
#define DEVMM_RECOMMEND_H

#include "devmm_proc_info.h"
#include "svm_cmd.h"
#include "svm_shmem_interprocess.h"

void devmm_get_sys_mem(void);
int devmm_init_shm_pro_node(struct devmm_svm_process *svm_proc);
void devmm_unint_shm_pro_node(struct devmm_svm_process *svm_proc);
void devmm_remote_unmap_and_delete_node(struct devmm_svm_process *svm_proc, struct devmm_shm_node *node);
void devmm_shm_node_dev_res_release(struct devmm_shm_node *node);
void devmm_destory_shm_node(struct devmm_shm_node *node);
void devmm_delete_shm_pro_node(struct devmm_shm_pro_node *shm_pro_node);
struct devmm_shm_node *devmm_get_shm_node_by_dva(struct devmm_shm_head *shm_head,
    u64 dst_va, u64 size, u32 devid, u32 vfid);
int devmm_ioctl_mem_remote_map(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
int devmm_ioctl_mem_remote_unmap(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
void devmm_destory_shm_pro_node(struct devmm_svm_process *svm_proc);
void devmm_uninit_shm_by_devid(u32 dev_id);
void devmm_try_destroy_remote_map_nodes(struct devmm_svm_process *svm_proc, u32 log_dev, u32 phy_dev, u32 vfid);
void devmm_destory_remote_map_mem_by_devid(struct devmm_svm_process *svm_proc,
    struct devmm_svm_heap *heap, u32 logical_devid);
int devmm_unmap_mem(struct devmm_svm_process *svm_proc, u64 va, u64 size);
int devmm_map_svm_mem_to_agent(struct devmm_svm_process *svm_proc, struct devmm_devid *devids,
    struct devmm_mem_remote_map_para *map_para, struct devmm_memory_attributes *attr);
int devmm_remote_pa_to_bar_pa(u32 devid, u64 *remote_pa, u64 pa_num, u64 *bar_pa);

int devmm_locked_host_unmap(struct devmm_svm_process *svm_proc,
    struct devmm_devid *devids, u64 src_va, u64 size, u64 dst_va);
int devmm_shm_config_txatu(struct devmm_svm_process *svm_proc, u32 devid);
ka_semaphore_t *devmm_get_shm_sem(struct devmm_svm_process *svm_proc, u32 map_type);
struct devmm_shm_pro_node *devmm_get_shm_pro_node(struct devmm_svm_process *svm_proc, u32 map_type);
struct devmm_shm_process_head *devmm_get_shm_pro_head(u32 map_type);

#endif /* __DEVMM_RECOMMEND_H */
