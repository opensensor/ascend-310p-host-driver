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

#ifndef SVM_DMA_H
#define SVM_DMA_H

#include "devmm_proc_info.h"
#include "devdrv_interface.h"
#include "virtmng_interface.h"

#define DEVMM_DMA_SMALL_PACKET_SIZE       1024
#define DEVMM_DMA_QUERY_WAIT_PACKET_SIZE  262144ul /* 256k */

#define DEVMM_DMA_WAIT_MIN_TIME 100
#define DEVMM_DMA_WAIT_MAX_TIME 200
#define DEVMM_DMA_RETRY_CNT 1000
#define DMA_WAIT_QUREY_THREAD_NUM 8

#define DEVMM_ASYNC_DMA_WAIT_MIN_TIME 200
#define DEVMM_ASYNC_DMA_WAIT_MAX_TIME 400
#define DEVMM_ASYNC_DMA_RETRY_CNT 200000

typedef void (*dma_finish_notify)(void *, u32, u32);

int devmm_dma_sync_link_copy(u32 dev_id, u32 vfid, struct devdrv_dma_node *dma_node, u32 node_cnt);
int devmm_dma_sync_link_copy_limit(struct devmm_copy_res *res, int instance);
int devmm_dma_async_link_copy(struct devmm_copy_res *res, int instance, void *priv, dma_finish_notify call_back);
int vmng_bandwidth_limit_check(struct vmng_bandwidth_check_info *info);
#endif /* __DEVMM_PCIE_DMA_H__ */
