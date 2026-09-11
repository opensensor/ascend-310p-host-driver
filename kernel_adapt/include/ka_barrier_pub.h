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
 *
 * Description:
 * Author: huawei
 * Create: 2025-01-21
 */
#ifndef KA_BARRIER_PUB_H
#define KA_BARRIER_PUB_H

#define ka_barrier()    barrier()
#define ka_mb()         mb()
#define ka_rmb()        rmb()
#define ka_wmb()        wmb()

#define ka_dma_mb()     dma_mb()
#define ka_dma_rmb()    dma_rmb()
#define ka_dma_wmb()    dma_wmb()

#define ka_smp_mb()     smp_mb()
#define ka_smp_wmb()    smp_wmb()

#define ka_smp_load_acquire(p) smp_load_acquire(p)
#define ka_smp_store_release(p, v) smp_store_release(p, v)

#endif