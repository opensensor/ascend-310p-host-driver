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

#include "virtmngagent_msg_common.h"
#include "virtmng_extension.h"
#include "virtmng_interface.h"
#include "vmng_mem_alloc_interface.h"
#include "virtmng_public_def.h"

#include "ka_memory_pub.h"
#include "ka_kernel_def_pub.h"

#ifndef DRV_UT
void *devdrv_dma_alloc_coherent(ka_device_t *dev, size_t size, ka_dma_addr_t *dma_addr, ka_gfp_t gfp)
{
    if ((dev == NULL) || (dma_addr == NULL) || (size == 0)) {
        return NULL;
    }
    return vmng_ka_dma_alloc_coherent(dev, size, dma_addr, gfp);
}
KA_EXPORT_SYMBOL(devdrv_dma_alloc_coherent);

void *devdrv_dma_zalloc_coherent(ka_device_t *dev, size_t size, ka_dma_addr_t *dma_addr, ka_gfp_t gfp)
{
    if ((dev == NULL) || (dma_addr == NULL) || (size == 0)) {
        return NULL;
    }

    return vmng_ka_dma_alloc_coherent(dev, size, dma_addr, gfp | __KA_GFP_ZERO);
}
KA_EXPORT_SYMBOL(devdrv_dma_zalloc_coherent);

void devdrv_dma_free_coherent(ka_device_t *dev, size_t size, void *addr, ka_dma_addr_t dma_addr)
{
    if ((dev == NULL) || (addr == NULL) || (size == 0)) {
        return;
    }
    vmng_ka_dma_free_coherent(dev, size, addr, dma_addr);
}
KA_EXPORT_SYMBOL(devdrv_dma_free_coherent);

ka_dma_addr_t devdrv_dma_map_single(ka_device_t *dev, void *ptr, size_t size, ka_dma_data_direction_t dir)
{
    if ((dev == NULL) || (ptr == NULL) || (size == 0)) {
        return (~(ka_dma_addr_t)0);
    }
    return ka_mm_dma_map_single(dev, ptr, size, dir);
}
KA_EXPORT_SYMBOL(devdrv_dma_map_single);

void devdrv_dma_unmap_single(ka_device_t *dev, ka_dma_addr_t addr, size_t size, ka_dma_data_direction_t dir)
{
    if ((dev == NULL) || (size == 0)) {
        return;
    }
    ka_mm_dma_unmap_single(dev, addr, size, dir);
}
KA_EXPORT_SYMBOL(devdrv_dma_unmap_single);

ka_dma_addr_t devdrv_dma_map_page(ka_device_t *dev, ka_page_t *page,
    size_t offset, size_t size, ka_dma_data_direction_t dir)
{
    if ((dev == NULL) || (page == NULL) || (size == 0) || (offset > KA_HPAGE_SIZE)) {
        return (~(ka_dma_addr_t)0);
    }
    return ka_mm_dma_map_page(dev, page, offset, size, dir);
}
KA_EXPORT_SYMBOL(devdrv_dma_map_page);

void devdrv_dma_unmap_page(ka_device_t *dev, ka_dma_addr_t addr, size_t size, ka_dma_data_direction_t dir)
{
    if ((dev == NULL) || (size == 0)) {
        return;
    }
    ka_mm_dma_unmap_page(dev, addr, size, dir);
}
KA_EXPORT_SYMBOL(devdrv_dma_unmap_page);
#else
// UT file coverage function_stub
void devdrv_dma_free_coherent(ka_device_t *dev, size_t size, void *addr, ka_dma_addr_t dma_addr)
{
    return;
}
#endif