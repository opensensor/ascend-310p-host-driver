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
 * Create: 2024-7-15
 */

#ifndef DEVDRV_MEM_ALLOC_H
#define DEVDRV_MEM_ALLOC_H

#include <linux/types.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/mman.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#include <linux/nodemask.h>

#include "ka_memory.h"
#include "ascend_hal_define.h"

#define devdrv_kvzalloc(size, flags) \
        ka_kvzalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_PCIE, KA_SUB_MODULE_TYPE_0))
#define devdrv_kvfree(addr) ka_kvfree(addr, ka_get_module_id(HAL_MODULE_TYPE_PCIE, KA_SUB_MODULE_TYPE_0))

#define devdrv_kmalloc(size, flags) \
    ka_kmalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_PCIE, KA_SUB_MODULE_TYPE_0))
#define devdrv_kzalloc(size, flags) \
    ka_kzalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_PCIE, KA_SUB_MODULE_TYPE_0))
#define devdrv_kzalloc_node(size, flags, node) \
    ka_kzalloc_node(size, flags, node, ka_get_module_id(HAL_MODULE_TYPE_PCIE, KA_SUB_MODULE_TYPE_0))
#define devdrv_kcalloc(size, flags) \
    ka_kcalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_PCIE, KA_SUB_MODULE_TYPE_0))
#define devdrv_kfree(addr) ka_kfree(addr, ka_get_module_id(HAL_MODULE_TYPE_PCIE, KA_SUB_MODULE_TYPE_0))

#define devdrv_vzalloc(size)  ka_vzalloc(size, ka_get_module_id(HAL_MODULE_TYPE_PCIE, KA_SUB_MODULE_TYPE_0))
#define devdrv_vfree(addr) ka_vfree(addr, ka_get_module_id(HAL_MODULE_TYPE_PCIE, KA_SUB_MODULE_TYPE_0))

#define devdrv_alloc_pages(gfp_mask, order) \
    ka_alloc_pages(gfp_mask, order, ka_get_module_id(HAL_MODULE_TYPE_PCIE, KA_SUB_MODULE_TYPE_0))
#define devdrv_free_pages(addr, order) \
    ka_free_pages(addr, order, ka_get_module_id(HAL_MODULE_TYPE_PCIE, KA_SUB_MODULE_TYPE_0))

#define devdrv_alloc_pages_node(nid, gfp_mask, order) \
    ka_alloc_pages_node(nid, gfp_mask, order, ka_get_module_id(HAL_MODULE_TYPE_PCIE, KA_SUB_MODULE_TYPE_0))
#define __devdrv_free_pages(page, order) \
    __ka_free_pages(page, order, ka_get_module_id(HAL_MODULE_TYPE_PCIE, KA_SUB_MODULE_TYPE_0))

#define devdrv_ka_dma_alloc_coherent(dev, size, dma_handle, gfp) \
    ka_dma_alloc_coherent(dev, size, dma_handle, gfp, ka_get_module_id(HAL_MODULE_TYPE_PCIE, KA_SUB_MODULE_TYPE_0))
#define devdrv_ka_dma_free_coherent(dev, size, cpu_addr, dma_handle) \
    ka_dma_free_coherent(dev, size, cpu_addr, dma_handle, ka_get_module_id(HAL_MODULE_TYPE_PCIE, KA_SUB_MODULE_TYPE_0))
#endif
