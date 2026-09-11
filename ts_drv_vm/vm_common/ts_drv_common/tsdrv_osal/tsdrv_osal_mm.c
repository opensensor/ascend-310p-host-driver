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

#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/gfp.h>

#include "kernel_cgroup_mem_adapt.h"
#include "ka_memory_pub.h"
#include "tsdrv_osal_mm.h"

#ifndef AOS_LLVM_BUILD
static void tsdrv_make_alloc_exact(struct page *p, u32 order, size_t size)
{
#ifndef CFG_MANAGER_HOST_ENV
    ka_page_t *cur_page = NULL;
    u64 alloced_num, page_num, i;

    if (order == 0) {
        return;
    }
    ka_mm_split_page(p, order);
#if !defined(EMU_ST) && !defined(TSDRV_UT)
    ka_split_page_memcg(p, order);
#endif
    alloced_num = 1ull << order;
    page_num = KA_MM_PAGE_ALIGN(size) / KA_MM_PAGE_SIZE;

    for (i = page_num; i < alloced_num; i++) {
        cur_page = ka_mm_nth_page(p, i);
        __ka_mm_free_page(cur_page);
    }
#endif
}
/**
 * alloc_pages_exact_nid not exprot, realize function as alloc_pages_exact_nid
 * - allocate an exact number of physically-contiguous pages on a node.
 * @nid: the preferred node ID where memory should be allocated
 * @size: the number of bytes to allocate
 * @gfp_mask: GFP flags for the allocation, must not contain __GFP_COMP and __GFP_ACCOUNT
 * __GFP_ACCOUNT:cgroup can not statistics page free num, euler integrating this patch
 */
void *tsdrv_alloc_pages_exact_nid(int nid, size_t size, ka_gfp_t gfp_mask)
{
    u32 order = ka_mm_get_order(size);
    struct page *p;

    if (gfp_mask & __KA_GFP_COMP) {
        gfp_mask &= ~__KA_GFP_COMP;
    }

    p = alloc_pages_node(nid, gfp_mask | __KA_GFP_ZERO, order);
    if (p == NULL) {
        return NULL;
    }

    tsdrv_make_alloc_exact(p, order, size);

    return (void *)ka_mm_page_address(p);
}
#endif
void *tsdrv_alloc_pages_exact(size_t size, ka_gfp_t gfp_mask)
{
#ifndef AOS_LLVM_BUILD
    return ka_alloc_pages_exact_ex(size, gfp_mask | __KA_GFP_ZERO);
#else
    return tsdrv_kmalloc(size, KA_GFP_USER);
#endif
}

void tsdrv_free_pages_exact(void *virt, size_t size)
{
#ifndef AOS_LLVM_BUILD
    ka_free_pages_exact_ex(virt, size);
#else
    tsdrv_kfree(virt);
#endif
}
