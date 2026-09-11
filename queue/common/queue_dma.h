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

#ifndef QUEUE_DMA_H
#define QUEUE_DMA_H

#include "devdrv_interface.h"
#include "ka_memory_pub.h"

enum queue_dma_side {
    QUEUE_DMA_LOCAL,
    QUEUE_DMA_REMOTE,
    QUEUE_DMA_INVALID,
};

struct queue_dma_block {
    u64 sz;
    ka_dma_addr_t dma;
};

struct queue_dma_list {
    u64 va;
    u64 len;
    bool dma_flag;

    u64 page_num;
    ka_page_t **page;
    u64 blks_num;
    struct queue_dma_block *blks;
};

#define QUEUE_MAX_VA_NUM (QUEUE_MAX_IOVEC_NUM + 1) /* 1 for ctx base addr */

struct queue_va_info {
    u64 va;
    u64 len;
    u64 blks_num;
};

int queue_make_dma_list(ka_device_t *dev, bool hccs_vm_flag, u32 dev_id, struct queue_dma_list *dma_list);
void queue_clear_dma_list(ka_device_t *dev, bool hccs_vm_flag, struct queue_dma_list *dma_list);

int queue_dma_sync_link_copy(u32 dev_id, struct devdrv_dma_node *dma_node, u64 dma_node_num);
void *queue_kvalloc(u64 size, unsigned int flags);
void queue_kvfree(const void *ptr);

void queue_try_cond_resched(unsigned long *pre_stamp);

#endif
