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
 * Create: 2024-11-05
 */

#ifndef KA_MEMORY_PUB_H
#define KA_MEMORY_PUB_H

#include <asm/barrier.h>
#include <asm/io.h>
#include <uapi/linux/sysinfo.h>
#include <linux/types.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#include <linux/nodemask.h>
#include <linux/gfp.h>
#include <linux/pagemap.h>
#include <linux/string.h>
#include <linux/thread_info.h>

#include <linux/mmzone.h>
#include <linux/rwsem.h>
#include <linux/mm_types.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0))
#include <linux/sched/mm.h>
#else
#include <linux/sched.h>
#endif
#include <linux/memcontrol.h>
#include <asm/pgtable.h>
#include <linux/page-flags.h>

#include "ka_macro_pub.h"
#include "ka_common_pub.h"
#include "ka_base_pub.h"

#define __KA_GFP_DMA                   __GFP_DMA
#define __KA_GFP_HIGHMEM               __GFP_HIGHMEM
#define __KA_GFP_DMA32                 __GFP_DMA32
#define __KA_GFP_MOVABLE               __GFP_MOVABLE
#define __KA_GFP_RECLAIMABLE           __GFP_RECLAIMABLE
#define __KA_GFP_HIGH                  __GFP_HIGH
#define __KA_GFP_IO                    __GFP_IO
#define __KA_GFP_FS                    __GFP_FS
#define __KA_GFP_WRITE                 __GFP_WRITE
#define __KA_GFP_NOWARN                __GFP_NOWARN
#define __KA_GFP_NOFAIL                __GFP_NOFAIL
#define __KA_GFP_NORETRY               __GFP_NORETRY
#define __KA_GFP_MEMALLOC              __GFP_MEMALLOC
#define __KA_GFP_COMP                  __GFP_COMP
#define __KA_GFP_ZERO                  __GFP_ZERO
#define __KA_GFP_NOMEMALLOC            __GFP_NOMEMALLOC
#define __KA_GFP_HARDWALL              __GFP_HARDWALL
#define __KA_GFP_THISNODE              __GFP_THISNODE
#define __KA_GFP_RECLAIM               __GFP_RECLAIM

#ifndef __GFP_ACCOUNT
#ifdef __GFP_KMEMCG
    #define __GFP_ACCOUNT __GFP_KMEMCG /* for linux version 3.10 */
#endif
#ifdef __GFP_NOACCOUNT
    #define __GFP_ACCOUNT 0 /* for linux version 4.1 */
#endif
#endif
#define __KA_GFP_ACCOUNT               __GFP_ACCOUNT

/* Useful GFP flag combinations */
#define KA_GFP_ATOMIC                  GFP_ATOMIC
#define KA_GFP_KERNEL                  GFP_KERNEL
#define KA_GFP_NOWAIT                  GFP_NOWAIT
#define KA_GFP_NOIO                    GFP_NOIO
#define KA_GFP_NOFS                    GFP_NOFS
#define KA_GFP_USER                    GFP_USER
#define KA_GFP_DMA                     GFP_DMA
#define KA_GFP_DMA32                   GFP_DMA32
#define KA_GFP_HIGHUSER                GFP_HIGHUSER
#define KA_GFP_HIGHUSER_MOVABLE        GFP_HIGHUSER_MOVABLE
#define KA_GFP_TRANSHUGE_LIGHT         GFP_TRANSHUGE_LIGHT
#define KA_GFP_TRANSHUGE               GFP_TRANSHUGE

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0))
#define __KA_GFP_RETRY_MAYFAIL __GFP_RETRY_MAYFAIL
#else
#define __KA_GFP_RETRY_MAYFAIL __GFP_REPEAT
#endif

#define __KA_GFP_DIRECT_RECLAIM        __ka_get_mem_gfp_direct_reclaim()
#define __KA_GFP_KSWAPD_RECLAIM        __ka_get_mem_gfp_kswapd_reclaim()
#define __KA_GFP_NOLOCKDEP             __ka_get_mem_gfp_nolockdep()
#define KA_GFP_KERNEL_ACCOUNT           ka_get_mem_gfp_kernel_account()
#define __KA_GFP_KMEMCG                __ka_get_mem_gfp_kmemcg()
#define __KA_GFP_NOACCOUNT             __ka_get_mem_gfp_noaccount()

#define KA_MM_PAGE_SIZE                    PAGE_SIZE
#define KA_MM_PAGE_MASK                    PAGE_MASK
#define KA_MM_HPAGE_SHIFT                  HPAGE_SHIFT
#define KA_MM_PAGE_ALIGN(addr)             PAGE_ALIGN(addr)
#define KA_MM_IS_ALIGNED(x, a)              IS_ALIGNED(x, a)
#define KA_MM_PAGE_ALIGNED(addr)            PAGE_ALIGNED(addr)

#define KA_PROT_READ     PROT_READ        /* page can be read */
#define KA_PROT_WRITE    PROT_WRITE        /* page can be written */
#define KA_FOLL_WRITE    FOLL_WRITE
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0)
#define KA_FOLL_LONGTERM FOLL_LONGTERM
#endif

#define KA_VM_PFNMAP     VM_PFNMAP
#define KA_VM_LOCKED     VM_LOCKED
#define KA_VM_WRITE      VM_WRITE

#define KA_VM_IO                        VM_IO
#define KA_VM_SHARED                    VM_SHARED
#define KA_VM_DONTCOPY                  VM_DONTCOPY
#define KA_VM_DONTEXPAND                VM_DONTEXPAND
#define KA_VM_DONTDUMP                  VM_DONTDUMP

#define KA_HPAGE_SIZE                   HPAGE_SIZE
#define KA_VM_HUGETLB                   VM_HUGETLB
#define KA_VM_FAULT_SIGBUS              VM_FAULT_SIGBUS
#define KA_VM_FAULT_SIGSEGV             VM_FAULT_SIGSEGV

#define KA_MAP_FIXED                    MAP_FIXED

#define ka_pgprot_t                   pgprot_t
#define ka_pgprot_val(x)              pgprot_val(x)

#define KA_PAGE_NONE                  PAGE_NONE
#define KA_PAGE_SHARED                PAGE_SHARED
#define KA_PAGE_SHARED_EXEC           PAGE_SHARED_EXEC
#define KA_PAGE_READONLY              PAGE_READONLY
#define KA_PAGE_READONLY_EXEC         PAGE_READONLY_EXEC
#define KA_PAGE_KERNEL                PAGE_KERNEL

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)
#define ka_mm_pgprot_device(prot)        pgprot_device(prot)
#else
#define ka_mm_pgprot_device(prot)        pgprot_noncached(prot)
#endif
#define ka_mm_pgprot_writecombine(prot)  pgprot_writecombine(prot)
#define ka_mm_pgprot_noncached(prot)     pgprot_noncached(prot)

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 16, 0)
typedef int ka_vm_fault_t;
#else
#define ka_vm_fault_t vm_fault_t
#endif

#define ka_pmd_t   pmd_t
#define ka_pgd_t   pgd_t
#define ka_pte_t   pte_t
#define ka_nodemask_t   nodemask_t
#define ka_mm_nodes_clear(dst)          nodes_clear(dst)
#define ka_mm_node_set(node, dst)       node_set(node, dst)
typedef struct dma_pool ka_dma_pool_t;
typedef struct kmem_cache ka_kmem_cache_t;
typedef struct mm_struct ka_mm_struct_t;
typedef struct address_space ka_address_space_t;
typedef struct vm_fault ka_vm_fault_struct_t;
typedef struct mempolicy ka_mempolicy_t;
typedef struct mmu_notifier ka_mmu_notifier_t;
typedef struct mmu_notifier_ops ka_mmu_notifier_ops_t;
typedef struct vm_operations_struct  ka_vm_operations_struct_t;
typedef struct vm_area_struct        ka_vm_area_struct_t;
typedef struct mmu_notifier_range    ka_mmu_notifier_range_t;
typedef enum dma_data_direction     ka_dma_data_direction_t;

#define ka_mm_mmu_notifier_range_init(range,event,flags,vma,mm,start,end) \
          mmu_notifier_range_init(range,event,flags,vma,mm,start,end)
#define ka_mm_mmu_notifier_invalidate_range_start(range)    mmu_notifier_invalidate_range_start(range)
#define ka_mm_mmu_notifier_invalidate_range_end(range)   mmu_notifier_invalidate_range_end(range)
#define ka_mm_mmu_notifier_get(ops,mm)                   mmu_notifier_get_locked(ops,mm)
#define ka_mm_mmu_notifier_put(mm)                       mmu_notifier_put(mm)
#define ka_mm_mmu_notifier_register(subscription, mm)    mmu_notifier_register(subscription, mm)
#define ka_mm_mmu_notifier_range_blockable(range)        mmu_notifier_range_blockable(range)
#define ka_mm_mmu_notifier_unregister_no_release(mn, mm) mmu_notifier_unregister_no_release(mn, mm)

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#define KA_MM_MMU_NOTIFIER_OPS_INITIALIZER(invalidate_range_start_fn, release_fn, alloc_notifier_fn, free_notifier_fn) \
    {   \
        .invalidate_range_start = invalidate_range_start_fn, \
        .release = release_fn, \
        .alloc_notifier = alloc_notifier_fn,\
        .free_notifier = free_notifier, \
    }
#else
#define KA_MM_MMU_NOTIFIER_OPS_INITIALIZER(invalidate_range_start_fn, release_fn, alloc_notifier_fn, free_notifier_fn) \
    {   \
        .invalidate_range_start = invalidate_range_start_fn, \
        .release = release_fn, \
    }
#endif
#define KA_MM_DEFINE_MMU_NOTIFIER_OPS(name, \
    invalidate_range_start_fn, release_fn, alloc_notifier_fn, free_notifier_fn) \
    struct mmu_notifier_ops name =  \
        MMU_NOTIFIER_OPS_INITIALIZER(invalidate_range_start_fn, release_fn, alloc_notifier_fn, free_notifier_fn)

typedef struct ka_sysinfo {
    unsigned long totalram;
    unsigned long freeram;
    unsigned long sharedram;
} ka_sysinfo_t;


#define ka_dma_addr_t           dma_addr_t
#define KA_DMA_BIDIRECTIONAL    DMA_BIDIRECTIONAL
#define KA_DMA_TO_DEVICE        DMA_TO_DEVICE
#define KA_DMA_FROM_DEVICE      DMA_FROM_DEVICE
#define KA_DMA_NONE             DMA_NONE
#define KA_DMA_BIT_MASK         DMA_BIT_MASK
#define ka_mm_pgprot_t          pgprot_t

#define KA_MM_PAGE_SHIFT      PAGE_SHIFT
#define KA_MM_PFN_ALIGN(x)    PFN_ALIGN(x)
#define KA_MM_PFN_UP(x)       PFN_UP(x)
#define KA_MM_PFN_DOWN(x)     PFN_DOWN(x)
#define KA_MM_PHYS_PFN(x)     PHYS_PFN(x)
#define KA_MM_PFN_PHYS(x)     PFN_PHYS(x)

#define KA_MODULE_TYPE_BIT 16U
#define ka_mm_get_module_id(module_type, sub_module_type) ((module_type << KA_MODULE_TYPE_BIT) | sub_module_type)

#define ka_mm_kmalloc(size, flags) kmalloc(size, flags)
#define ka_mm_kzalloc(size, flags) kzalloc(size, flags)
#define ka_mm_kcalloc(n, size, flags) kcalloc(n, size, flags)
#define ka_mm_kzalloc_node(size, flags, node) kzalloc_node(size, flags, node)
#define ka_mm_kfree(addr) kfree(addr)

#define ka_mm_vmalloc(size)   vmalloc(size)
#define ka_mm_vzalloc(size)   vzalloc(size)

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
#define __ka_mm_vmalloc(size, gfp_mask, prot) __vmalloc(size, gfp_mask)
#else
#define __ka_mm_vmalloc(size, gfp_mask, prot) __vmalloc(size, gfp_mask, prot)
#endif

#define ka_mm_vfree(addr)                             vfree(addr)
#define ka_mm_get_free_pages(gfp_mask, order)         __get_free_pages(gfp_mask, order)
#define ka_mm_alloc_pages(gfp_mask, order)            alloc_pages(gfp_mask, order)
#define ka_mm_free_pages(addr, order)                 free_pages(addr, order)
#define __ka_mm_free_page(page)                        __free_page(page)
#define ka_mm_alloc_pages_exact(size, gfp_mask)       alloc_pages_exact(size, gfp_mask)
#define ka_mm_free_pages_exact(virt, size)             free_pages_exact(virt, size)
#define ka_mm_alloc_pages_node(nid, gfp_mask, order)   alloc_pages_node(nid, gfp_mask, order)
#define __ka_mm_free_pages(page, order)                __free_pages(page, order)

#define ka_mm_kvmalloc_node(size, gfp_flags, node) kvmalloc_node(size, gfp_flags, node)
#define ka_mm_kvzalloc(size, gfp_flags) kvzalloc(size, gfp_flags)
#define ka_mm_kvmalloc(size, gfp_flags) kvmalloc(size, gfp_flags)
#define ka_mm_kvfree(addr) kvfree(addr)

#define ka_mm_page_count(page)                        page_count(page)
#define ka_mm_get_page(page)                          get_page(page)
#define ka_mm_vmalloc_node(size, node)                vmalloc_node((size), node)
#define ka_mm_vzalloc_node(size, node)                vzalloc_node(size, node)
#define ka_mm_vmalloc_32(size)                        vmalloc_32(size)
#define ka_mm_vmalloc_32_user(size)                   vmalloc_32_user(size)

#define ka_mm_dma_alloc_coherent(dev, size, dma_handle, gfp)       dma_alloc_coherent(dev, size, dma_handle, gfp)
#define ka_mm_dma_free_coherent(dev, size, cpu_addr, dma_handle)   dma_free_coherent(dev, size, cpu_addr, dma_handle)
#define ka_mm_put_page(page)                                    put_page(page)
#define ka_mm_dma_map_page(d, p, o, s, r)                     dma_map_page(d, p, o, s, r)
#define ka_mm_dma_unmap_page(d, a, s, r)                      dma_unmap_page(d, a, s, r)
#define ka_mm_sg_dma_len(sg)                                  sg_dma_len(sg)
#define ka_mm_sg_dma_address(sg)                              sg_dma_address(sg)

#define ka_mm_kmem_cache_create(name, size, align, flags,ctor)  \
            kmem_cache_create(name, size, align, flags,ctor)
#define ka_mm_kmem_cache_destroy(cache)    kmem_cache_destroy(cache)
#define ka_mm_kmem_cache_alloc(ka_cache, gfp)    kmem_cache_alloc(ka_cache,gfp)
#define ka_mm_kmem_cache_free(cache, x)          kmem_cache_free(cache, x)
#define ka_mm_kmem_cache_alloc_node(s, gfpflags, node)   kmem_cache_alloc_node(s, gfpflags, node)

#define ka_mm_lock_page(page)       lock_page(page)
#define ka_mm_unlock_page(page)     unlock_page(page)
#define ka_mm_follow_pfn(ka_vma, address, pfn)  follow_pfn(ka_vma, address, pfn)

#define ka_mm_vmalloc_to_page(vmalloc_addr)   vmalloc_to_page(vmalloc_addr)
#define ka_mm_vmalloc_to_pfn(vmalloc_addr)    vmalloc_to_pfn(vmalloc_addr)
#define ka_mm_find_vma(mm, addr)    find_vma(mm, addr)
#define ka_mm_io_remap_pfn_range(vma, addr, pfn, size, prot)   io_remap_pfn_range(vma, addr, pfn, size, prot)
#define ka_mm_remap_pfn_range(vma, addr, pfn, size, ka_prot)    remap_pfn_range(vma, addr, pfn, size, ka_prot)
#define ka_mm_vm_munmap(addr, len)    vm_munmap(addr, len)
#define ka_mm_remap_vmalloc_range(vma, addr, pgoff)    remap_vmalloc_range(vma, addr, pgoff)
#define ka_mm_can_do_mlock(void)    can_do_mlock(void)
#define ka_mm_kstrdup(s, gfp_flag)    kstrdup(s, gfp_flag)
#define ka_mm_kstrdup_const(s, gfp_flag)    kstrdup_const(s, gfp_flag)
#define ka_mm_kfree_const(addr)    kfree_const(addr)
#define ka_mm_kmemdup(src, len, gfp_flag)    kmemdup(src, len, gfp_flag)
#define ka_mm_memdup_user(src, len)    memdup_user(src, len)
#define ka_mm_page_mapped(page)    page_mapped(page)
#define ka_mm_set_page_dirty(page)    set_page_dirty(page)
#define ka_mm_set_page_dirty_lock(page)    set_page_dirty_lock(page)
#define ka_mm_unmap_mapping_range(mapping, holebegin, holelen, even_cows)    unmap_mapping_range(mapping, holebegin, holelen, even_cows)
#define ka_mm_memcpy_fromio(to, from, count)    memcpy_fromio(to, from, count)
#define ka_mm_memcpy_toio(to, from, count)    memcpy_toio(to, from, count)
#define ka_mm_memset_io(dst, c, count)    memset_io(dst, c, count)
#define ka_mm_ioremap(addr, size)    ioremap(addr, size)
#define ka_mm_ioremap_nocache(addr, size)    ioremap_nocache(addr, size)
#define ka_mm_ioremap_wc(addr, size)    ioremap_wc(addr, size)
#define ka_mm_ioremap_cache(phys_addr, size)    ioremap_cache(phys_addr, size)
#define ka_mm_iounmap(va)    iounmap(va)
#define ka_mm_dma_set_coherent_mask(dev, mask)    dma_set_coherent_mask(dev, mask)
#define ka_mm_dma_set_mask(dev, mask)    dma_set_mask(dev, mask)
#define ka_mm_dma_map_single(dev, ptr, size, direction)    dma_map_single(dev, ptr, size, direction)
#define ka_mm_dma_mapping_error(dev, addr)    dma_mapping_error(dev, addr)
#define ka_mm_dma_unmap_single(dev, dma_addr, size, direction)    dma_unmap_single(dev, dma_addr, size, direction)
#define ka_mm_dma_sync_single_for_device(dev, addr, size, dir)    dma_sync_single_for_device(dev, addr, size, dir)
#define ka_mm_dma_sync_single_for_cpu(dev, addr, size, dir)    dma_sync_single_for_cpu(dev, addr, size, dir)
#define ka_mm_writel(b, addr)    writel(b, addr)
#define ka_mm_readl(addr)    readl(addr)
#define ka_mm_readl_relaxed(addr)    readl_relaxed(addr)
#define ka_mm_writeq(b, addr)    writeq(b, addr)
#define ka_mm_writel_relaxed(value, addr)    writel_relaxed(value, addr)
#define ka_mm_writeq_relaxed(value, addr)    writeq_relaxed(value, addr)

#define ka_mm_page_align(addr)    page_align(addr)
#define ka_mm_page_to_pfn(page)    page_to_pfn(page)
#define ka_mm_pfn_to_page(pfn)     pfn_to_page(pfn)
#define ka_mm_page_to_phys(page)    page_to_phys(page)
#define ka_mm_virt_to_phys(address)    virt_to_phys(address)
#define ka_mm_phys_to_pfn(addr)    phys_to_pfn(addr)
#define ka_mm_pfn_valid(pfn)    pfn_valid(pfn)
#define ka_mm_split_page(page, order)    split_page(page, order)
#define ka_mm_nth_page(addr, n)    nth_page(addr, n)
#define ka_mm_get_order(size)    get_order(size)
#define ka_mm_free_page(page)    free_page(page)
#define ka_mm_mmput(mm)    mmput(mm)
#define ka_mm_is_vmalloc_addr(addr)    is_vmalloc_addr(addr)
#define ka_mm_page_address(page)    page_address(page)

#define ka_mm_flush_tlb_range(vma, start, end)    flush_tlb_range(vma, start, end)

#define ka_mm_PagePoisoned(page)    PagePoisoned(page)
#define ka_mm_PageHWPoison(page)    PageHWPoison(page)
#define ka_mm_compound_head(page)    compound_head(page)
#define ka_mm_compound_order(page)    compound_order(page)
#define ka_mm_page_to_nid(page)    page_to_nid(page)
#define ka_mm_vmap(pages, count, flags, prot)    vmap(pages, count, flags, prot)
#define ka_mm_pte_offset_map(dir, address)  pte_offset_map(dir, address)
#define ka_mm_pte_none(pte)                 pte_none(pte)
#define ka_mm_pte_present(pte)              pte_present(pte)
#define ka_mm_pte_pfn(pte)                  pte_pfn(pte)
#define ka_mm_pmd_huge(pmd)                 pmd_huge(pmd)
#define ka_mm_pgd_offset(mm, address)       pgd_offset(mm, address)
#define ka_mm_p4d_offset(pgd, address)      p4d_offset(pgd, address)
#define ka_mm_pud_offset                    pud_offset

unsigned int __ka_get_mem_gfp_account(void);
void ka_si_meminfo(struct ka_sysinfo *val);
#define ka_mm_memdup_user_nul(src, len) memdup_user_nul(src, len)
#define ka_mm_get_mem_cgroup_from_mm(mm) get_mem_cgroup_from_mm(mm)
ka_rw_semaphore_t *ka_mm_get_mmap_sem(ka_mm_struct_t *mm);
long ka_mm_get_user_pages_remote(ka_task_struct_t *tsk, ka_mm_struct_t *mm,
    unsigned long long va, int write, unsigned int num, ka_page_t **pages);
#define ka_mm_put_devmap_managed_page(page) put_devmap_managed_page(page)
#define ka_mm_dma_set_mask_and_coherent(dev, mask) dma_set_mask_and_coherent(dev, mask)
#define ka_mm_dma_map_resource(dev, phys_addr, size, dir, attrs) dma_map_resource(dev, phys_addr, size, dir, attrs)
#define ka_mm_dma_unmap_resource(dev, addr, size, dir, attrs) dma_unmap_resource(dev, addr, size, dir, attrs)
#define ka_mm_memcpy_flushcache(dst, src, cnt) memcpy_flushcache(dst, src, cnt)
#define ka_mm_get_user_pages_fast(start, nr_pages, gup_flags, pages) get_user_pages_fast(start, nr_pages, gup_flags, pages)
void *ka_mm_page_to_virt(ka_page_t *page);
unsigned long ka_mm_get_vm_flags(ka_vm_area_struct_t *vma);
void ka_mm_set_vm_flags(ka_vm_area_struct_t *vma, unsigned long flags);
void ka_mm_set_vm_private_data(ka_vm_area_struct_t *vma, void *private_data);
void *ka_mm_get_vm_private_data(ka_vm_area_struct_t *vma);
void ka_mm_set_vm_start(ka_vm_area_struct_t *vma, unsigned long start);
unsigned long ka_mm_get_vm_start(ka_vm_area_struct_t *vma);
void ka_mm_set_vm_end(ka_vm_area_struct_t *vma, unsigned long end);
unsigned long ka_mm_get_vm_end(ka_vm_area_struct_t *vma);
ka_mm_pgprot_t *ka_mm_get_vm_pgprot(ka_vm_area_struct_t *vma);
void ka_mm_set_vm_pgprot(ka_vm_area_struct_t *vma);
void ka_mm_set_vm_ops(ka_vm_area_struct_t *vma, const ka_vm_operations_struct_t *vm_ops);
const ka_vm_operations_struct_t *ka_mm_get_vm_ops(ka_vm_area_struct_t *vma);
int ka_mm_zap_vma_ptes(ka_vm_area_struct_t *vma, unsigned long address, unsigned long size);
int ka_mm_pin_user_pages_fast(unsigned long start, int nr_pages, unsigned int gup_flags, ka_page_t **pages);
void ka_mm_unpin_user_page(ka_page_t *page);
void ka_mm_mmget(ka_mm_struct_t *mm);

typedef ka_vm_fault_t (*vmf_fault_func)(ka_vm_fault_struct_t *);
typedef int (*vm_fault_func)(ka_vm_area_struct_t *, ka_vm_fault_struct_t *);
void ka_mm_set_vm_fault_host(ka_vm_operations_struct_t *ops_managed, vmf_fault_func vmf_func, vm_fault_func vm_func);
void ka_mm_set_vm_mremap(ka_vm_operations_struct_t *ops_managed, int (*mremap_func)(ka_vm_area_struct_t *));

#define ka_mm_get_ds() get_ds()

#endif
