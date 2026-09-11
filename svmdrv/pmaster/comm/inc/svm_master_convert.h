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
#ifndef SVM_MASTER_CONVERT_H
#define SVM_MASTER_CONVERT_H

#include <linux/spinlock_types.h>
#include <linux/rbtree.h>
#include <linux/kref.h>

#include "ascend_kernel_hal.h"

#include "svm_cmd.h"
#include "devmm_common.h"
#include "devmm_proc_info.h"
#include "svm_task_dev_res_mng.h"

struct devmm_convert_node_info {
    struct DMA_ADDR dma_addr;
    struct svm_id_inst id_inst;
    u64 src_va;
    u64 dst_va;
    u64 spitch;
    u64 dpitch;
    u64 width;
    u64 height;
    u64 fixed_size;

    u64 offset;
    u32 index;

    int host_pid;
};

struct devmm_convert_node {
    struct devmm_task_dev_res_node *task_dev_res;

    ka_rb_node_t task_dev_res_node;

    ka_kref_t ref;

    ka_rwlock_t rwlock;
    int state;

    struct devmm_dma_copy_task copy_task;
    struct devmm_convert_node_info info;
};

int devmm_convert_node_create(struct devmm_svm_process *svm_proc, struct devmm_convert_node_info *info);
int devmm_convert_node_destroy(struct devmm_convert_node *node, int state, bool check_ts_finish);
void devmm_convert_nodes_destroy_by_task_release(struct devmm_task_dev_res_node *task_dev_res, u32 *num);
void devmm_convert_nodes_subres_recycle_by_dev_res_mng(struct devmm_dev_res_mng *mng, u32 *num);
int devmm_convert_node_state_trans(struct devmm_convert_node *node, int src_state, int dst_state);
struct devmm_convert_node *devmm_convert_node_get(struct devmm_task_dev_res_node *task_dev_res, u64 handle);
struct devmm_convert_node *devmm_convert_node_get_by_task(struct devmm_svm_process *svm_proc, u64 handle);
void devmm_convert_node_put(struct devmm_convert_node *node);

int devmm_destroy_addr_batch(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
int devmm_destroy_addr_batch_async(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
int devmm_destroy_addr_batch_sync(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);

u32 devmm_get_convert_limit_size(u32 devid, u32 vfid);
u32 devmm_get_convert_64m_size(void);
u32 devmm_get_convert_128m_size(void);
u32 devmm_get_convert_dma_depth(void);
int devmm_ioctl_convert_addr(struct devmm_svm_process *svm_pro, struct devmm_ioctl_arg *arg);
int devmm_ioctl_destroy_addr(struct devmm_svm_process *svm_pro, struct devmm_ioctl_arg *arg);
int devmm_ioctl_destroy_addr_batch(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
bool devmm_check_va_is_convert(struct devmm_svm_process *svm_proc, u64 va);

int devmm_convert_one_addr(struct devmm_svm_process *svm_pro, struct devmm_mem_convrt_addr_para *convrt_para);
void devmm_destroy_one_addr(struct devmm_copy_res *res);

#endif /* SVM_MASTER_CONVERT_H */

