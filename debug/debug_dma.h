/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
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
 * Create: 2023-09-07
 */

#ifndef DEBUG_DMA_H
#define DEBUG_DMA_H

#include <linux/types.h>

typedef struct dma_param {
    u64 host_addr;
    u64 device_addr;
    u64 size;
    u8 direction;
};

int dma_copy_sync(u32 logical_devid, u32 devid, u32 tsid, int pid, struct dma_param *param);

extern int hal_kernel_svm_dev_va_to_dma_addr(int hostpid, u32 logical_devid, u64 va, u64 *dma_addr);

#endif
