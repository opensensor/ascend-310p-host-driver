/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
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
 * Create: 2024-04-25
 */
#ifndef DEVMM_REGISTER_DMA_H
#define DEVMM_REGISTER_DMA_H

#include "devmm_proc_info.h"
#include "svm_heap_mng.h"
#include "svm_proc_mng.h"
#include "svm_master_proc_mng.h"

struct devmm_register_dma_node {
    ka_rb_node_t rbnode;
    ka_kref_t ref;

    u64 src_va; /* User input para */
    u64 align_va;
    u64 src_size; /* User input para */
    u64 align_size;
    u32 devid;

    struct devmm_dma_block *blks;
    u64 blks_num;
    u64 num; /* actual num after merge pa */
};

int devmm_ioctl_register_dma(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
int devmm_ioctl_unregister_dma(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
void devmm_destory_register_dma_mng(struct devmm_svm_process *svm_proc);
void devmm_destory_register_dma_mng_by_devid(struct devmm_svm_process *svm_proc, u32 devid);
struct devmm_register_dma_node *devmm_register_dma_node_get(struct devmm_register_dma_mng *mng,
    u64 addr, u64 size);
void devmm_register_dma_node_put(struct devmm_register_dma_node *node);

#endif /* __DEVMM_REGISTER_DMA_H__ */
