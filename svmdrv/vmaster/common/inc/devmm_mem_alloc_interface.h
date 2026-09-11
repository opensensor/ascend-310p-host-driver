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

#ifndef DEVMM_MEM_ALLOC_INTERFACE_H
#define DEVMM_MEM_ALLOC_INTERFACE_H

#include "ka_memory.h"
#include "ka_memory_pub.h"
#include "ascend_hal_define.h"
/*
 * KA_SUB_MODULE_TYPE_0 : ctrl memory
 * KA_SUB_MODULE_TYPE_1 : ai memory normal page
 * KA_SUB_MODULE_TYPE_2 : ai memory huge page
 */

/* KA_SUB_MODULE_TYPE_0 : ctrl memory */
#define devmm_kmalloc_ex(size, flags) ka_kmalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))
#define devmm_kzalloc_ex(size, flags) ka_kzalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))
#define devmm_kcalloc_ex(size, flags) ka_kcalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))
#define devmm_kfree_ex(addr)          ka_kfree(addr, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))

#define devmm_vzalloc_ex(size)        ka_vzalloc(size, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))
#ifndef EMU_ST
#define __devmm_vmalloc_ex(size, gfp_mask, prot) \
    __ka_vmalloc(size, gfp_mask, prot, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))
#else
    void *__devmm_vmalloc_ex(size_t size, ka_gfp_t gfp_mask, ka_pgprot_t prot);
#endif
#define devmm_vfree_ex(addr) ka_vfree(addr, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))

#define devmm_get_free_pages_ex(gfp_mask, order) ka_get_free_pages(gfp_mask, order, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))
#define devmm_alloc_pages_ex(gfp_mask, order) ka_alloc_pages(gfp_mask, order, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))
#define devmm_free_pages_ex(addr, order) ka_free_pages(addr, order, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))

#define devmm_alloc_pages_exact_ex(size, gfp_mask) ka_alloc_pages_exact(size, gfp_mask, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))
#define devmm_free_pages_exact_ex(virt, size) ka_free_pages_exact(virt, size, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))

#define devmm_kvzalloc_ex(size, flags) ka_kvzalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))
#define devmm_kvmalloc_ex(size, flags) ka_kvmalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))
#define devmm_kvfree_ex(addr)  ka_kvfree(addr, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_0))

/* KA_SUB_MODULE_TYPE_1 : ai memory normal page */
#define devmm_alloc_pages_node_ex(nid, gfp_mask, order) ka_alloc_pages_node(nid, gfp_mask, order, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_1))
#define __devmm_free_pages_ex(page, order) __ka_free_pages(page, order, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_1))
#define devmm_kvmalloc_node(size, flags, node) ka_kvmalloc_node(size, flags, node, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_1))
#define devmm_kvfree_node(addr)  ka_kvfree(addr, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_1))

/* KA_SUB_MODULE_TYPE_2 : ai memory huge page */
#define devmm_hugetlb_alloc_hugepage_ex(nid, flag) ka_hugetlb_alloc_hugepage(nid, flag, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_2))
#ifdef DAVINCI_DEVICE
static inline void devmm_hugetlb_free_hugepage_ex(ka_page_t *page)
{
    ka_hugetlb_free_hugepage(page, ka_get_module_id(HAL_MODULE_TYPE_DEVMM, KA_SUB_MODULE_TYPE_2));
}
#else
static inline void devmm_hugetlb_free_hugepage_ex(ka_page_t *page)
{
    ka_mm_put_page(page);
}
#endif
#endif
