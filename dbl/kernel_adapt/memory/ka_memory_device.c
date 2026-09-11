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
 * Create: 2024-07-15
 */

#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/nodemask.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/hugetlb.h>

#include "ka_memory_mng.h"
#include "ka_memory.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
#ifndef HUGETLB_ALLOC_NONE
#define HUGETLB_ALLOC_NONE             0x00
#endif
#ifndef HUGETLB_ALLOC_NORMAL
#define HUGETLB_ALLOC_NORMAL           0x01
#endif
#ifndef HUGETLB_ALLOC_BUDDY
#define HUGETLB_ALLOC_BUDDY            0x02
#endif
#ifndef HUGETLB_ALLOC_NORECLAIM
#define HUGETLB_ALLOC_NORECLAIM        0x04
#endif
#ifndef HUGETLB_ALLOC_GIANT
#define HUGETLB_ALLOC_GIANT            0x08
#endif
#define HUGEPAGE_2M (2 * 1024 * 1024UL)
#define HUGEPAGE_2M_ORDER 9

struct page *ka_hugetlb_alloc_hugepage(int nid, int flag, unsigned int module_id)
{
    size_t size = (flag == HUGETLB_ALLOC_GIANT) ? PUD_SIZE : HPAGE_SIZE;
    struct page *page = NULL;
    gfp_t gfp_mask;

    if (flag == HUGETLB_ALLOC_GIANT) {
        page = (struct page *)alloc_hugetlb_folio_size(nid, PUD_SIZE);
    } else if (flag == HUGETLB_ALLOC_NORMAL) {
        page = (struct page *)alloc_hugetlb_folio_size(nid, HPAGE_SIZE);
    } else if (flag == (HUGETLB_ALLOC_BUDDY | HUGETLB_ALLOC_NORECLAIM)) {
        gfp_mask = ((GFP_HIGHUSER_MOVABLE | __GFP_THISNODE | GFP_KERNEL | __GFP_COMP | __GFP_ACCOUNT | __GFP_NOWARN |
            __GFP_RETRY_MAYFAIL) & (~__GFP_RECLAIM));
        page = (struct page *)alloc_temporary_hugetlb_folio(nid, gfp_mask);
    } else if (flag == HUGETLB_ALLOC_BUDDY) {
        gfp_mask = (GFP_HIGHUSER_MOVABLE | __GFP_THISNODE | GFP_KERNEL | __GFP_COMP | __GFP_ACCOUNT | __GFP_NOWARN |
            __GFP_RETRY_MAYFAIL);
        page = (struct page *)alloc_temporary_hugetlb_folio(nid, gfp_mask);
    }
    ka_mem_alloc_stat_add(module_id, size, (unsigned long)(uintptr_t)page);
    return page;
}
EXPORT_SYMBOL(ka_hugetlb_alloc_hugepage);

struct folio *ka_alloc_hugetlb_folio_size(int nid, unsigned long size, unsigned int module_id)
{
    struct folio *folio = alloc_hugetlb_folio_size(nid, size);
    ka_mem_alloc_stat_add(module_id, size, (unsigned long)(uintptr_t)folio);
    return folio;
}
EXPORT_SYMBOL(ka_alloc_hugetlb_folio_size);

#else
struct page *ka_hugetlb_alloc_hugepage(int nid, int flag, unsigned int module_id)
{
    struct page *page = hugetlb_alloc_hugepage(nid, flag);
    size_t size = (flag == HUGETLB_ALLOC_GIANT) ? PUD_SIZE : HPAGE_SIZE;
    ka_mem_alloc_stat_add(module_id, size, (unsigned long)(uintptr_t)page);
    return page;
}
EXPORT_SYMBOL(ka_hugetlb_alloc_hugepage);
#endif

void ka_hugetlb_free_hugepage(struct page *page, unsigned int module_id)
{
    ka_mem_alloc_stat_del((unsigned long)(uintptr_t)page, module_id);
    put_page(page);
}
EXPORT_SYMBOL(ka_hugetlb_free_hugepage);

void ka_free_single_page(struct page *page_addr, unsigned int order, int alloc_flag, unsigned int module_type,
                         unsigned int level)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
    if (alloc_flag == KA_ALLOC_MEM_BY_HUGE_PAGE) {
        ka_hugetlb_free_hugepage(page_addr, ka_get_module_id(module_type, level));
    } else {
        __ka_free_pages(page_addr, order, ka_get_module_id(module_type, level));
    }
#else
    ka_hugetlb_free_hugepage(page_addr, ka_get_module_id(module_type, level));
#endif
}
EXPORT_SYMBOL(ka_free_single_page);

struct page *ka_alloc_hugetlb(int nid, unsigned int size, int flag, unsigned int module_type,
                              unsigned int level)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
    return (struct page *)ka_alloc_hugetlb_folio_size(nid, size, ka_get_module_id(module_type, level));
#else
    return ka_hugetlb_alloc_hugepage(nid, flag, ka_get_module_id(module_type, level));
#endif
}
EXPORT_SYMBOL_GPL(ka_alloc_hugetlb);

struct page *ka_alloc_hugepage(int nid, gfp_t gfp_mask, unsigned int order, int *alloc_flag, int flag,
                               unsigned int module_type, unsigned int level)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
    /* If the static huge page application fails, call the kernel general interface to apply for 2M memory. */
    (*alloc_flag) = KA_ALLOC_MEM_BY_PAGE_NODE;
    return ka_alloc_pages_node(nid, gfp_mask, order, ka_get_module_id(module_type, level));
#else
    (*alloc_flag) = KA_ALLOC_MEM_BY_HUGE_PAGE;
    return ka_hugetlb_alloc_hugepage(nid, flag, ka_get_module_id(module_type, level));
#endif
}
EXPORT_SYMBOL_GPL(ka_alloc_hugepage);