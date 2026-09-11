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
 * Create: 2024-08-13
 */

#ifndef DEVMM_MEM_ALLOC_INTERFACE_H
#define DEVMM_MEM_ALLOC_INTERFACE_H

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
#ifdef DAVINCI_DEVICE
#include <linux/share_pool.h>
#endif

#include "ka_memory.h"
#include "ascend_hal_define.h"

/*
 * KA_SUB_MODULE_TYPE_0 : ctrl memory
 * KA_SUB_MODULE_TYPE_1 : ai memory normal page
 */

/* KA_SUB_MODULE_TYPE_0 : ctrl memory */
#define dp_proc_kmalloc(size, flags) ka_kmalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))
#define dp_proc_kzalloc(size, flags) ka_kzalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))
#define dp_proc_kcalloc(size, flags) ka_kcalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))

#define dp_proc_kfree(addr) ka_kfree(addr, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))

#define dp_proc_vzalloc(size)  ka_vzalloc(size, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))
#define __dp_proc_vmalloc(size, gfp_mask, prot) __ka_vmalloc(size, gfp_mask, prot, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))

#define dp_proc_vfree(addr) ka_vfree(addr, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))

#define dp_proc_get_free_pages(gfp_mask, order) ka_get_free_pages(gfp_mask, order, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))
#define dp_proc_alloc_pages(gfp_mask, order) ka_alloc_pages(gfp_mask, order, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))
#define dp_proc_free_pages(addr, order) ka_free_pages(addr, order, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))

#define dp_proc_alloc_pages_exact(size, gfp_mask) ka_alloc_pages_exact(size, gfp_mask, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))
#define dp_proc_free_pages_exact(virt, size) ka_free_pages_exact(virt, size, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))

#define dp_proc_kvzalloc(size, flags) ka_kvzalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))
#define dp_proc_kvmalloc(size, flags) ka_kvmalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))
#define dp_proc_kvfree(addr)  ka_kvfree(addr, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))

/* KA_SUB_MODULE_TYPE_1 : ai memory normal page */
#define dp_proc_alloc_pages_node(nid, gfp_mask, order) ka_alloc_pages_node(nid, gfp_mask, order, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_1))
#define __dp_proc_free_pages(page, order) __ka_free_pages(page, order, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_1))
#define dp_proc_kvmalloc_node(size, flags, node) ka_kvmalloc_node(size, flags, node, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_1))
#define dp_proc_kvfree_node(addr)  ka_kvfree(addr, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_1))

#endif
