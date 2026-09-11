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
#include <linux/gfp.h>

#include "ka_memory_mng.h"
#include "ka_memory.h"

void *ka_kmalloc(size_t size, gfp_t flags, unsigned int module_id)
{
    void *va = kmalloc(size, flags);
    ka_mem_alloc_stat_add(module_id, size, (unsigned long)(uintptr_t)va);
    return va;
}
EXPORT_SYMBOL(ka_kmalloc);

void *ka_kzalloc(size_t size, gfp_t flags, unsigned int module_id)
{
    void *va = kzalloc(size, flags);
    ka_mem_alloc_stat_add(module_id, size, (unsigned long)(uintptr_t)va);
    return va;
}
EXPORT_SYMBOL(ka_kzalloc);

void *ka_kcalloc(unsigned long n, size_t size, gfp_t flags, unsigned int module_id)
{
    void *va = kcalloc(n, size, flags);
    ka_mem_alloc_stat_add(module_id, size, (unsigned long)(uintptr_t)va);
    return va;
}
EXPORT_SYMBOL(ka_kcalloc);

void *ka_kzalloc_node(size_t size, gfp_t flags, unsigned int node, unsigned int module_id)
{
    void *va = kzalloc_node(size, flags, node);
    ka_mem_alloc_stat_add(module_id, size, (unsigned long)(uintptr_t)va);
    return va;
}
EXPORT_SYMBOL(ka_kzalloc_node);

void ka_kfree(const void *addr, unsigned int module_id)
{
    ka_mem_alloc_stat_del((unsigned long)(uintptr_t)addr, module_id);
    kfree(addr);
}
EXPORT_SYMBOL(ka_kfree);

void *ka_vzalloc(size_t size, unsigned int module_id)
{
    void *va = vzalloc(size);
    ka_mem_alloc_stat_add(module_id, size, (unsigned long)(uintptr_t)va);
    return va;
}
EXPORT_SYMBOL(ka_vzalloc);

void *__ka_vmalloc(size_t size, gfp_t gfp_mask, pgprot_t prot, unsigned int module_id)
{
    void *va = ka_vmalloc_ex(size, gfp_mask, prot);
    ka_mem_alloc_stat_add(module_id, size, (unsigned long)(uintptr_t)va);
    return va;
}
EXPORT_SYMBOL(__ka_vmalloc);

void ka_vfree(const void *addr, unsigned int module_id)
{
    ka_mem_alloc_stat_del((unsigned long)(uintptr_t)addr, module_id);
    vfree(addr);
}
EXPORT_SYMBOL(ka_vfree);

unsigned long ka_get_free_pages(gfp_t gfp_mask, unsigned int order, unsigned int module_id)
{
    unsigned long va = __get_free_pages(gfp_mask, order);
    ka_mem_alloc_stat_add(module_id, PAGE_SIZE << order, va);
    return va;
}
EXPORT_SYMBOL(ka_get_free_pages);

struct page *ka_alloc_pages(gfp_t gfp_mask, unsigned int order, unsigned int module_id)
{
    void *va = alloc_pages(gfp_mask, order);
    ka_mem_alloc_stat_add(module_id, PAGE_SIZE << order, (unsigned long)(uintptr_t)va);
    return va;
}
EXPORT_SYMBOL(ka_alloc_pages);

void ka_free_pages(unsigned long addr, unsigned int order, unsigned int module_id)
{
    ka_mem_alloc_stat_del(addr, module_id);
    free_pages(addr, order);
}
EXPORT_SYMBOL(ka_free_pages);

void *ka_alloc_pages_exact(size_t size, gfp_t gfp_mask, unsigned int module_id)
{
    void *va = alloc_pages_exact(size, gfp_mask);
    ka_mem_alloc_stat_add(module_id, size, (unsigned long)(uintptr_t)va);
    return va;
}
EXPORT_SYMBOL(ka_alloc_pages_exact);

void ka_free_pages_exact(void *virt, size_t size, unsigned int module_id)
{
    ka_mem_alloc_stat_del((unsigned long)(uintptr_t)virt, module_id);
    free_pages_exact(virt, size);
}
EXPORT_SYMBOL(ka_free_pages_exact);

struct page *__ka_alloc_pages_node(int nid, gfp_t gfp_mask, unsigned int order)
{
    return alloc_pages_node(nid, gfp_mask, order);
}
EXPORT_SYMBOL(__ka_alloc_pages_node);

struct page *ka_alloc_pages_node(int nid, gfp_t gfp_mask, unsigned int order, unsigned int module_id)
{
    void *va = __ka_alloc_pages_node(nid, gfp_mask, order);
    ka_mem_alloc_stat_add(module_id, PAGE_SIZE << order, (unsigned long)(uintptr_t)va);
    return va;
}
EXPORT_SYMBOL(ka_alloc_pages_node);

void __ka_free_pages(struct page *page, unsigned int order, unsigned int module_id)
{
    ka_mem_alloc_stat_del((unsigned long)(uintptr_t)page, module_id);
    __free_pages(page, order);
}
EXPORT_SYMBOL(__ka_free_pages);

void *ka_kvzalloc(size_t size, gfp_t flags, unsigned int module_id)
{
    void *va = kvzalloc(size, flags);
    ka_mem_alloc_stat_add(module_id, size, (unsigned long)(uintptr_t)va);
    return va;
}
EXPORT_SYMBOL(ka_kvzalloc);

void *ka_kvmalloc_node(size_t size, gfp_t flags, int node, unsigned int module_id)
{
    void *va = kvmalloc_node(size, flags, node);
    ka_mem_alloc_stat_add(module_id, size, (unsigned long)(uintptr_t)va);
    return va;
}
EXPORT_SYMBOL(ka_kvmalloc_node);

void *ka_kvmalloc(size_t size, gfp_t flags, unsigned int module_id)
{
    void *va = kvmalloc(size, flags);
    ka_mem_alloc_stat_add(module_id, size, (unsigned long)(uintptr_t)va);
    return va;
}
EXPORT_SYMBOL(ka_kvmalloc);

void ka_kvfree(const void *addr, unsigned int module_id)
{
    ka_mem_alloc_stat_del((unsigned long)(uintptr_t)addr, module_id);
    kvfree(addr);
}
EXPORT_SYMBOL(ka_kvfree);

void *ka_dma_alloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_handle,
    gfp_t gfp, unsigned int module_id)
{
    void *va = dma_alloc_coherent(dev, size, dma_handle, gfp);
    ka_mem_alloc_stat_add(module_id, size, (unsigned long)(uintptr_t)va);
    return va;
}
EXPORT_SYMBOL(ka_dma_alloc_coherent);

void ka_dma_free_coherent(struct device *dev, size_t size, void *cpu_addr, dma_addr_t dma_handle,
    unsigned int module_id)
{
    ka_mem_alloc_stat_del((unsigned long)(uintptr_t)cpu_addr, module_id);
    dma_free_coherent(dev, size, cpu_addr, dma_handle);
}
EXPORT_SYMBOL(ka_dma_free_coherent);
