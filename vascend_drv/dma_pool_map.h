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
 */

#ifndef _DVT_DMA_POOL_MAP_H_
#define _DVT_DMA_POOL_MAP_H_

#include "dvt.h"

/*
 * dev_dma_sgt is managed by dma_dom_info rather than dev_dom_info because
 * iova is non-contiguous in x86. Once we want to execute dma_get_iova, we
 * need to get iova like gfn->pfn->iova.
 */
struct dev_dma_sgt {
    gfn_t gfn;
    struct device *dev;
    dma_addr_t dma_addr;
    struct sg_table *dma_sgt;
    struct list_head list;
};

struct dev_dma_info {
    struct device *dev;
    dma_addr_t base_iova;               /* for arm */
    struct ram_range_info *ram_info;
    struct dev_dma_sgt **sgt_array;     /* for x86 */
    struct list_head list;
};

struct dma_info_2m {
    gfn_t gfn;
    unsigned long size;
    struct list_head dev_dma_sgt_head;
    struct page_info_list dma_page_list;
};

struct ram_range_info_list {
    unsigned int elem_num;
    struct list_head head;
};

struct ram_range_info {
    gfn_t base_gfn;
    unsigned long npages;
    unsigned long userspace_addr;
    int dma_array_len;
    struct dma_info_2m** dma_array;
    struct list_head list;
    atomic_t map_count;
};

int dev_dma_map_ram_range(struct hw_vdavinci *vdavinci,
                          struct ram_range_info *ram_info);

void dev_dma_unmap_ram_range(struct hw_vdavinci *vdavinci,
                             struct ram_range_info *ram_info);

int vf_map_ram_range(struct hw_vdavinci *vdavinci,
                     struct ram_range_info *ram_info);

void vf_unmap_ram_range(struct hw_vdavinci *vdavinci,
                        struct ram_range_info *ram_info);
#endif
