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
 * Create: 2024-11-9
 */
#ifndef SVM_DMA_PREPARE_POOL_MNG_H
#define SVM_DMA_PREPARE_POOL_MNG_H

#include "devdrv_interface.h"

void devmm_dma_prepare_pool_init(u32 devid);
void devmm_dma_prepare_pool_uninit(u32 devid);
void *devmm_dma_prepare_get_from_pool(u32 devid, u32 dma_node_num, struct devdrv_dma_prepare **dma_prepare);
void devmm_dma_prepare_put_to_pool(u32 devid, void *fd);

#endif
