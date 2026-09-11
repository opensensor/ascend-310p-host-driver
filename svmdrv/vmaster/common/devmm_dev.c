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

#include "devmm_adapt.h"

#include "svm_cmd.h"
#include "devmm_proc_info.h"
#include "devmm_proc_mem_copy.h"
#include "devmm_channel.h"
#include "devdrv_interface.h"
#include "devmm_common.h"
#include "svm_dma.h"
#include "svm_proc_mng.h"
#include "svm_heap_mng.h"
#include "svm_mem_mng.h"
#include "svm_cgroup_mng.h"
#include "svm_mem_query.h"
#include "svm_interface.h"
#include "svm_page_cnt_stats.h"
#include "svm_proc_gfp.h"
#include "kernel_version_adapt.h"
#include "devmm_dev.h"
#include "devmm_mem_alloc_interface.h"
#include "svm_dynamic_addr.h"

#ifdef CFG_FEATURE_VFIO
#include "devmm_pm_vpc.h"
#include "devmm_pm_adapt.h"
#endif

#define DEVMM_MAX(a, b) (((a) > (b)) ? (a) : (b))

#ifndef EMU_ST
bool devmm_device_is_pf(u32 devid)
{
    if (devdrv_get_pfvf_type_by_devid(devid) == DEVDRV_SRIOV_TYPE_PF) {
        return true;
    }

    return false;
}
#endif

enum devmm_endpoint_type devmm_get_end_type(void)
{
    return DEVMM_END_TYPE;
}

STATIC void devmm_chan_update_msg_process_id(struct devmm_chan_msg_head *msg_head)
{
    if (devmm_get_end_type() == DEVMM_END_DEVICE) {
        msg_head->process_id.devid = msg_head->dev_id;
    } else {
        msg_head->vfid = msg_head->process_id.vfid;
        msg_head->process_id.vm_id = 0;
        msg_head->process_id.vfid = 0;
    }
}

bool devmm_is_static_reserve_addr(struct devmm_svm_process *svm_proc, u64 va)
{
    return ((va >= svm_proc->start_addr && va <= svm_proc->end_addr)
        || (va >= svm_proc->host_pin_start_addr && va <= svm_proc->host_pin_end_addr));
}

bool devmm_va_is_not_svm_process_addr(const struct devmm_svm_process *svm_process, unsigned long va)
{
    if (svm_process == NULL) {
        return DEVMM_TRUE;
    }

    if (devmm_is_static_reserve_addr((struct devmm_svm_process *)svm_process, va)) {
        return false;
    }

    return !svm_is_da_addr((struct devmm_svm_process *)svm_process, va, 1);
}

#ifndef DRV_UT
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 5, 0)
static ka_pte_t *devmm_pte_offset_map(ka_pmd_t *pmd, unsigned long addr, ka_pmd_t *pmdvalp)
{
    ka_pmd_t pmdval;

    /* rcu_read_lock() to be added later */
    pmdval = pmdp_get_lockless(pmd);
    if (pmdvalp) {
        *pmdvalp = pmdval;
    }
    if (ka_unlikely(pmd_none(pmdval) || is_pmd_migration_entry(pmdval))) {
        goto nomap;
    }
    if (ka_unlikely(pmd_trans_huge(pmdval) || pmd_devmap(pmdval))) {
        goto nomap;
    }
    if (ka_unlikely(pmd_bad(pmdval))) {
        pmd_ERROR(*pmd);
        pmd_clear(pmd);
        goto nomap;
    }
    return __pte_map(&pmdval, addr);

nomap:
    /* rcu_read_unlock() to be added later */
    return NULL;
}
#endif

/**
 * @brief get the page table entry of the va
 * @attention
 * kpg_size=KA_MM_PAGE_SIZE(4K) -> pte
 * kpg_size=KA_HPAGE_SIZE(2M) -> pmd
 * kpg_size=PUD_SIZE(1G) -> pud
 * @param [in] vma: ka_vm_area_struct_t
 * @param [in] va: va
 * @param [out] kpg_size: real page size from kernel
 * @return NULL for fail, others for success, means pte pointer
 */
void *devmm_get_pte(const ka_vm_area_struct_t *vma, u64 va, u64 *kpg_size)
{
    ka_pgd_t *pgd = NULL;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
    p4d_t *p4d = NULL;
#endif
    pud_t *pud = NULL;
    ka_pmd_t *pmd = NULL;
    ka_pte_t *pte = NULL;

    if ((vma == NULL) || (vma->vm_mm == NULL)) {
        devmm_drv_err("Vm_mm none. (va=0x%llx)\n", va);
        return NULL;
    }
    /* too much log, not print */
    pgd = ka_mm_pgd_offset(vma->vm_mm, va);
    if (PXD_JUDGE(pgd) != 0) {
        return NULL;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
    p4d = ka_mm_p4d_offset(pgd, va);
    if (PXD_JUDGE(p4d) != 0) {
        return NULL;
    }

    /* if kernel version is above 4.11.0,then 5 level pt arrived.
    ka_mm_pud_offset(pgd,va) changed to ka_mm_pud_offset(p4d,va) for x86
    but not changed in arm64 */
    pud = ka_mm_pud_offset(p4d, va);
#else
    pud = ka_mm_pud_offset(pgd, va);
#endif
    if (PUD_GIANT(pud) != 0) {
        *kpg_size = DEVMM_GIANT_PAGE_SIZE;
        return pud;
    } else {
        if (PXD_JUDGE(pud) != 0) {
            return NULL;
        }
    }

    pmd = pmd_offset(pud, va);
    /* huge page pmd can not judge bad flag */
    if (PMD_HUGE(pmd) != 0) {
        *kpg_size = KA_HPAGE_SIZE;
        return pmd;
    } else {
        if (PMD_JUDGE(pmd) != 0) {
            return NULL;
        }
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 5, 0)
    pte = devmm_pte_offset_map(pmd, va, NULL);
#else
    pte = ka_mm_pte_offset_map(pmd, va);
#endif
    if ((ka_mm_pte_none(*pte) != 0) || (ka_mm_pte_present(*pte) == 0)) {
        return NULL;
    }
    *kpg_size = KA_MM_PAGE_SIZE;
    return pte;
}

ka_pmd_t *devmm_get_va_to_pmd(const ka_vm_area_struct_t *vma, unsigned long va) // 待删除
{
    ka_pgd_t *pgd = NULL;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
    p4d_t *p4d = NULL;
#endif
    pud_t *pud = NULL;
    ka_pmd_t *pmd = NULL;

    if ((vma == NULL) || (vma->vm_mm == NULL)) {
        devmm_drv_err("Vm_mm none. (va=0x%lx)\n", va);
        return NULL;
    }
    /* too much log, not print */
    pgd = ka_mm_pgd_offset(vma->vm_mm, va);
    if (PXD_JUDGE(pgd)) {
        return NULL;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
    p4d = ka_mm_p4d_offset(pgd, va);
    if (PXD_JUDGE(p4d) != 0) {
        return NULL;
    }

    /* if kernel version is above 4.11.0,then 5 level pt arrived.
    ka_mm_pud_offset(pgd,va) changed to ka_mm_pud_offset(p4d,va) for x86
    but not changed in arm64 */
    pud = ka_mm_pud_offset(p4d, va);
    if (PXD_JUDGE(pud) != 0) {
        return NULL;
    }
#else
    pud = ka_mm_pud_offset(pgd, va);
    if (PXD_JUDGE(pud) != 0) {
        return NULL;
    }
#endif

    pmd = pmd_offset(pud, va);
    return pmd;
}

int devmm_va_to_pmd(const ka_vm_area_struct_t *vma, unsigned long va, int huge_flag, ka_pmd_t **tem_pmd)
{
    ka_pmd_t *pmd = NULL;

    pmd = devmm_get_va_to_pmd(vma, va);
    *tem_pmd = pmd;
    if (huge_flag != 0) {
        /* huge page pmd can not judge bad flag */
        if (PMD_HUGE(pmd) == 0) {
            return -EDOM;
        }
    } else {
        if (PMD_JUDGE(pmd) != 0) {
            return -EDOM;
        }
    }
    return 0;
}

int devmm_va_to_pfn(const ka_vm_area_struct_t *vma, u64 va, u64 *pfn, u64 *kpg_size)
{
    ka_pte_t *pte = NULL;

    pte = (ka_pte_t *)devmm_get_pte(vma, va, kpg_size);
    if (pte == NULL) {
        return -ERANGE;
    }

    *pfn = ka_mm_pte_pfn(*pte);
    return 0;
}

int devmm_va_to_pa(const ka_vm_area_struct_t *vma, u64 va, u64 *pa)
{
    u64 aligned_va = ka_base_round_down(va, KA_MM_PAGE_SIZE);
    u64 pfn, kpg_size;
    int ret;

    ret = devmm_va_to_pfn(vma, aligned_va, &pfn, &kpg_size);
    if (ret != 0) {
        /* too much log, not print */
        return ret;
    }

    *pa = KA_MM_PFN_PHYS(pfn);
    *pa += (va - aligned_va);

    return 0;
}

STATIC int devmm_va_to_pa_pmd_range(ka_pmd_t *pmd, u64 start, u64 end, u64 *pas, u64 *num)
{
    ka_pte_t *pte = NULL;
    u64 got_num = 0;
    u64 va = start;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 5, 0)
    pte = devmm_pte_offset_map(pmd, va, NULL);
#else
    pte = ka_mm_pte_offset_map(pmd, va);
#endif

    for (; va != end; pte++, va += KA_MM_PAGE_SIZE) {
        if ((ka_mm_pte_none(*pte) != 0) || (ka_mm_pte_present(*pte) == 0)) {
            return -ERANGE;
        }

        pas[got_num] = KA_MM_PFN_PHYS(ka_mm_pte_pfn(*pte));
        got_num++;
    }

    *num = got_num;
    return 0;
}

STATIC int devmm_va_to_pa_pud_range(pud_t *pud, u64 start, u64 end, u64 *pas, u64 *num)
{
    ka_pmd_t *pmd = NULL;
    u64 got_num = 0;
    u64 va = start;
    u64 next;

    pmd = pmd_offset(pud, va);
    for (; va != end; pmd++, va = next) {
        int ret;
        u64 n;

        if (PXD_JUDGE(pmd) != 0) {
            return -EDOM;
        }

        next = pmd_addr_end(va, end);
        ret = devmm_va_to_pa_pmd_range(pmd, va, next, &pas[got_num], &n);
        if (ret != 0) {
            return ret;
        }
        got_num += n;
    }

    *num = got_num;
    return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
STATIC int devmm_va_to_pa_p4d_range(p4d_t *p4d, u64 start, u64 end, u64 *pas, u64 *num)
{
    pud_t *pud = NULL;
    u64 got_num = 0;
    u64 va = start;
    u64 next;

    pud = ka_mm_pud_offset(p4d, va);
    for (; va != end; pud++, va = next) {
        int ret;
        u64 n;

        if (PXD_JUDGE(pud) != 0) {
            return -EDOM;
        }

        next = pud_addr_end(va, end);
        ret = devmm_va_to_pa_pud_range(pud, va, next, &pas[got_num], &n);
        if (ret != 0) {
            return ret;
        }
        got_num += n;
    }

    *num = got_num;
    return 0;
}

STATIC int devmm_va_to_pa_pgd_range(ka_pgd_t *pgd, u64 start, u64 end, u64 *pas, u64 *num)
{
    p4d_t *p4d = NULL;
    u64 got_num = 0;
    u64 va = start;
    u64 next;

    p4d = ka_mm_p4d_offset(pgd, va);
    for (; va != end; p4d++, va = next) {
        int ret;
        u64 n;

        if (PXD_JUDGE(p4d) != 0) {
            return -EDOM;
        }

        next = p4d_addr_end(va, end);
        ret = devmm_va_to_pa_p4d_range(p4d, va, next, &pas[got_num], &n);
        if (ret != 0) {
            return ret;
        }
        got_num += n;
    }

    *num = got_num;
    return 0;
}
#else
STATIC int devmm_va_to_pa_pgd_range(ka_pgd_t *pgd, u64 start, u64 end, u64 *pas, u64 *num)
{
    pud_t *pud = NULL;
    u64 got_num = 0;
    u64 va = start;
    u64 next;

    pud = ka_mm_pud_offset(pgd, va);
    for (; va != end; pud++, va = next) {
        int ret;
        u64 n;

        if (PXD_JUDGE(pud) != 0) {
            return -EDOM;
        }

        next = pud_addr_end(va, end);
        ret = devmm_va_to_pa_pud_range(pud, va, next, &pas[got_num], &n);
        if (ret != 0) {
            return ret;
        }
        got_num += n;
    }

    *num = got_num;
    return 0;
}
#endif

int devmm_va_to_pa_range(const ka_vm_area_struct_t *vma, u64 va, u64 num, u64 *pas)
{
    u64 start, end, len, next;
    ka_pgd_t *pgd = NULL;
    u64 got_num = 0;

    start = ka_base_round_down(va, KA_MM_PAGE_SIZE);
    len = num << KA_MM_PAGE_SHIFT;
    end = start + len;
    if (end < start) {
        return -EINVAL;
    }

    pgd = ka_mm_pgd_offset(vma->vm_mm, start);
    for (; start != end; pgd++, start = next) {
        int ret;
        u64 n;

        if (PXD_JUDGE(pgd) != 0) {
            return -EDOM;
        }

        next = pgd_addr_end(start, end);
        ret = devmm_va_to_pa_pgd_range(pgd, start, next, &pas[got_num], &n);
        if (ret != 0) {
            return ret;
        }
        got_num += n;
    }

    return 0;
}
#endif

#ifndef HOST_AGENT
int devmm_get_svm_pages(ka_vm_area_struct_t *vma, u64 va, u64 num, ka_page_t **pages)
{
    u64 *pas = (u64 *)pages;
    int ret;
    u64 i;

    ret = devmm_va_to_pa_range(vma, va, num, pas); /* tmp store pa */
    if (ret != 0) {
        devmm_drv_err("Query pa fail. (va=0x%llx; num=%llu)\n", va, num);
        return ret;
    }

    for (i = 0; i < num; i++) {
        pages[i] = devmm_pa_to_page(pas[i]);
        ka_mm_get_page(pages[i]);
    }

    return 0;
}

int devmm_get_pages_list(ka_mm_struct_t *mm, u64 va, u64 num, ka_page_t **pages)
{
    struct devmm_svm_process *svm_proc = NULL;
    struct devmm_svm_heap *heap = NULL;
    u64 size = num << KA_MM_PAGE_SHIFT;
    int ret;

    if ((mm == NULL) || (pages == NULL)) {
        devmm_drv_err("Invaild para.\n");
        return -EINVAL;
    }

    svm_proc = devmm_svm_proc_get_by_mm(mm);
    if (svm_proc == NULL) {
        devmm_drv_err("Get svm_proc failed.\n");
        return -EINVAL;
    }
    heap = devmm_svm_heap_get(svm_proc, va);
    if (heap == NULL) {
        devmm_svm_proc_put(svm_proc);
        devmm_drv_err("Get heap failed. (va=0x%llx)\n", va);
        return -EINVAL;
    }

    ret = devmm_check_va_add_size_by_heap(heap, va, size);
    if (ret != 0) {
        devmm_svm_heap_put(heap);
        devmm_svm_proc_put(svm_proc);
        devmm_drv_err("Out of range. (va=0x%llx; size=%llu)\n", va, size);
        return -EINVAL;
    }

    ret = devmm_get_svm_pages_with_lock(svm_proc, va, num, pages);
    devmm_svm_heap_put(heap);
    devmm_svm_proc_put(svm_proc);

    return ret;
}
KA_EXPORT_SYMBOL_GPL(devmm_get_pages_list);
#endif

/**
 * num input the size of the pa, output the real found page-block-num.
 */
int devmm_va_to_palist(const ka_vm_area_struct_t *vma, u64 va, u64 sz, u64 *pa, u32 *num)
{
    u64 vaddr, paddr;
    u32 pg_num = 0;
    int ret = 0;

    for (vaddr = ka_base_round_down(va, KA_MM_PAGE_SIZE); vaddr < ka_base_round_up(va + sz, KA_MM_PAGE_SIZE);
         vaddr += KA_MM_PAGE_SIZE) {
        if (devmm_va_to_pa(vma, vaddr, &paddr) != 0) {
            /* too much log, not print */
            ret = -ENOENT;
            break;
        }
        if (pg_num >= *num) {
            /* va size more then array num */
            break;
        }
        pa[pg_num++] = paddr;
    }
    *num = pg_num;
    return ret;
}

void devmm_zap_vma_ptes(ka_vm_area_struct_t *vma, unsigned long vaddr, unsigned long size)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 18, 0)
    int ret;
#endif
    if (size == 0) {
        return;
    }

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 18, 0)
    ret = ka_mm_zap_vma_ptes(vma, vaddr, size);
    if (ret != 0) {
        devmm_drv_err("Zap_vma_ptes fail. (va=0x%lx; ret=%d; flags=0x%lx; start=0x%lx; end=0x%lx)\n",
                      vaddr, ret, ka_mm_get_vm_flags(vma), ka_mm_get_vm_start(vma), ka_mm_get_vm_end(vma));
    }
#else
    ka_mm_zap_vma_ptes(vma, vaddr, size);
#endif
}

static u64 devmm_get_mapped_page_num(ka_vm_area_struct_t *vma, u64 vaddr, u64 *paddrs, u64 page_num)
{
    u32 stamp = (u32)ka_jiffies;
    u64 i;
    int ret;

    for (i = 0; i < page_num; ++i) {
        ret = devmm_va_to_pa(vma, vaddr + i * KA_MM_PAGE_SIZE, &paddrs[i]);
        if (ret != 0) {
            break;
        }
        devmm_try_cond_resched(&stamp);
    }
    return i;
}

static void devmm_free_pages_range(struct devmm_svm_process *svm_proc, u64 *paddrs, u64 page_num,
    struct devmm_phy_addr_attr *attr, bool is_owner)
{
    ka_page_t *pg = NULL;
    u32 stamp = (u32)ka_jiffies;
    u64 i;

    for (i = 0; i < page_num; ++i) {
        if (devmm_pa_is_remote_addr(paddrs[i])) {
            continue;
        }
        pg = devmm_pa_to_page(paddrs[i]);
        if (is_owner) {
            devmm_proc_free_pages(svm_proc, attr, &pg, 1);
        } else {
            devmm_put_normal_page(pg);
        }
        devmm_try_cond_resched(&stamp);
    }
}

static void devmm_cont_unmap_pages(struct devmm_svm_process *svm_proc, ka_vm_area_struct_t *vma,
    struct devmm_pa_info_para *pa_info_para, bool is_owner)
{
    struct devmm_phy_addr_attr attr = {0};
    u64 i, start_addr, mapped_num;
    u64 vaddr = pa_info_para->vaddr;
    u64 page_num = pa_info_para->pa_num;
    u64 *paddrs = pa_info_para->paddrs;
    u32 stamp = (u32)ka_jiffies;

    if (is_owner) {
        devmm_phy_addr_attr_pack(svm_proc, DEVMM_NORMAL_PAGE_TYPE, 0, false, &attr);
    }

    for (i = 0; i < page_num;) {
        struct devmm_pa_info_para info = {0};

        start_addr = vaddr + i * KA_MM_PAGE_SIZE;
        mapped_num = devmm_get_mapped_page_num(vma, start_addr, &paddrs[i], page_num - i);
        if (mapped_num == 0) {
            i++;
            continue;
        }

        info.vaddr = start_addr;
        info.paddrs = &paddrs[i];
        info.pa_num = mapped_num;
        info.page_size = KA_MM_PAGE_SIZE;
#ifndef HOST_AGENT
        if (devmm_va_is_support_sdma_kernel_clear(svm_proc, start_addr) && is_owner) {
            devmm_sdma_kernel_mem_clear(&attr, svm_proc->ssid, &info);
        }
#endif
        devmm_zap_vma_ptes(vma, start_addr, mapped_num * KA_MM_PAGE_SIZE);
        if (devmm_va_is_in_svm_range(start_addr)) {
            devmm_free_pages_range(svm_proc, &paddrs[i], mapped_num, &attr, is_owner);
        }
        i += mapped_num;
        devmm_try_cond_resched(&stamp);
    }
}

static bool devmm_try_cont_unmap_page(struct devmm_svm_process *svm_proc,
    ka_vm_area_struct_t *vma, u64 vaddr, u64 page_num, bool is_owner)
{
    struct devmm_pa_info_para pa_info_para;
    u64 *pa_list = NULL;
    u64 i;
    /* The maximum continuous physical page size is 32M, with 4K per page, so the per max page num is 8192 */
    u64 per_max_num = ka_base_min_t(u64, page_num, 8192);
    u32 stamp = (u32)ka_jiffies;

    pa_list = (u64 *)devmm_kmalloc_ex(sizeof(u64) * per_max_num, KA_GFP_ATOMIC | __KA_GFP_NOWARN | __KA_GFP_ACCOUNT);
    if (pa_list == NULL) {
        return false;
    }

    for (i = 0; i < page_num;) {
        pa_info_para.vaddr = vaddr + i * KA_MM_PAGE_SIZE;
        pa_info_para.pa_num = ka_base_min_t(u64, (page_num - i), per_max_num);
        pa_info_para.paddrs = pa_list;
        devmm_cont_unmap_pages(svm_proc, vma, &pa_info_para, is_owner);
        i += pa_info_para.pa_num;
        devmm_try_cond_resched(&stamp);
    }

    devmm_kfree_ex(pa_list);
    return true;
}
void devmm_unmap_page_from_vma_owner(struct devmm_svm_process *svm_proc,
    ka_vm_area_struct_t *vma, u64 vaddr, u64 num)
{
    struct devmm_phy_addr_attr attr = {0};
    u32 stamp = (u32)ka_jiffies;
    u64 i, temp_addr, paddr;
    ka_page_t *pg = NULL;

    if (devmm_try_cont_unmap_page(svm_proc, vma, vaddr, num, true) == true) {
        return;
    }

    devmm_phy_addr_attr_pack(svm_proc, DEVMM_NORMAL_PAGE_TYPE, 0, false, &attr);

    for (i = 0; i < num; i++) {
        int ret;
        temp_addr = vaddr + (i << KA_MM_PAGE_SHIFT);
        ret = devmm_va_to_pa(vma, temp_addr, &paddr);
        if (ret != 0) {
            continue;
        }
        devmm_zap_vma_ptes(vma, temp_addr, KA_MM_PAGE_SIZE);
        if (devmm_va_is_in_svm_range(temp_addr)) {
            if (devmm_pa_is_remote_addr(paddr)) {
                continue;
            }
            pg = devmm_pa_to_page(paddr);
            devmm_proc_free_pages(svm_proc, &attr, &pg, 1);
            devmm_try_cond_resched(&stamp);
        }
    }

    return;
}

void devmm_unmap_page_from_vma_custom(struct devmm_svm_process *svm_proc,
    ka_vm_area_struct_t *vma, u64 vaddr, u64 num)
{
    u32 stamp = (u32)ka_jiffies;
    u64 i, temp_addr, paddr;

    if (devmm_try_cont_unmap_page(svm_proc, vma, vaddr, num, false) == true) {
        return;
    }

    for (i = 0; i < num; i++) {
        int ret;
        temp_addr = vaddr + (i << KA_MM_PAGE_SHIFT);
        ret = devmm_va_to_pa(vma, temp_addr, &paddr);
        if (ret != 0) {
            continue;
        }
        devmm_zap_vma_ptes(vma, temp_addr, KA_MM_PAGE_SIZE);
        if (devmm_pa_is_remote_addr(paddr)) {
            continue;
        }

        devmm_put_normal_page(devmm_pa_to_page(paddr));
        devmm_try_cond_resched(&stamp);
    }

    return;
}

static void _devmm_unmap_pages_owner(struct devmm_svm_process *svm_proc, u64 vaddr, u64 num)
{
    ka_vm_area_struct_t *vma = NULL;

    vma = devmm_find_vma(svm_proc, vaddr);
    if (vma == NULL) {
        devmm_drv_err("Can not find vma. (vaddr=0x%llx; hostpid=%d; devid=%d; vfid=%d)\n",
            vaddr, svm_proc->process_id.hostpid, svm_proc->process_id.devid, svm_proc->process_id.vfid);
        return;
    }
    devmm_unmap_page_from_vma_owner(svm_proc, vma, vaddr, num);
}

void devmm_unmap_pages_owner(struct devmm_svm_process *svm_proc, u64 vaddr, u64 num)
{
    u32 i;

    for (i = 0; i < DEVMM_CUSTOM_PROCESS_NUM; i++) {
        ka_task_mutex_lock(&svm_proc->custom[i].proc_lock);
    }
    if (devmm_is_support_double_pgtable() && devmm_is_static_reserve_addr(svm_proc, vaddr)) {
        _devmm_unmap_pages_owner(svm_proc, devmm_double_pgtable_get_offset_addr(vaddr), num);
    }
    _devmm_unmap_pages_owner(svm_proc, vaddr, num);
    for (i = 0; i < DEVMM_CUSTOM_PROCESS_NUM; i++) {
        ka_task_mutex_unlock(&svm_proc->custom[i].proc_lock);
    }

    return;
}

static void devmm_unmap_pages_custom(struct devmm_svm_process *svm_proc, u64 vaddr, u64 num)
{
    ka_vm_area_struct_t *vma = NULL;
    u32 i;

    for (i = 0; i < DEVMM_CUSTOM_PROCESS_NUM; i++) {
        ka_task_mutex_lock(&svm_proc->custom[i].proc_lock);
        if (svm_proc->custom[i].status != DEVMM_CUSTOM_USED) {
            ka_task_mutex_unlock(&svm_proc->custom[i].proc_lock);
            continue;
        }
        vma = devmm_find_vma_custom(svm_proc, i, vaddr);
        if (vma == NULL) {
            ka_task_mutex_unlock(&svm_proc->custom[i].proc_lock);
            continue;
        }
        devmm_unmap_page_from_vma_custom(svm_proc, vma, vaddr, num);
        ka_task_mutex_unlock(&svm_proc->custom[i].proc_lock);
    }

    return;
}

void devmm_unmap_pages(struct devmm_svm_process *svm_proc, u64 vaddr, u64 page_num)
{
    devmm_unmap_pages_custom(svm_proc, vaddr, page_num);
    devmm_unmap_pages_owner(svm_proc, vaddr, page_num);
}

static void _devmm_zap_owner_ptes_range(struct devmm_svm_process *svm_proc, u64 va, u64 page_num)
{
    ka_vm_area_struct_t *vma = NULL;

    vma = devmm_find_vma(svm_proc, va);
    if (vma == NULL) {
        return;
    }

    devmm_zap_vma_ptes(vma, va, KA_MM_PAGE_SIZE * page_num);
    return;
}

static void devmm_zap_owner_ptes_range(struct devmm_svm_process *svm_proc, u64 va, u64 page_num)
{
    _devmm_zap_owner_ptes_range(svm_proc, va, page_num);
    if (devmm_is_support_double_pgtable() && devmm_is_static_reserve_addr(svm_proc, va)) {
        _devmm_zap_owner_ptes_range(svm_proc, devmm_double_pgtable_get_offset_addr(va), page_num);
    }
    return;
}

static u64 devmm_get_cont_page_num(ka_page_t **inpages, u64 page_num)
{
    u64 post_pfn, cont_num;
    u64 pre_pfn = ka_mm_page_to_pfn(inpages[0]);

    for (cont_num = 1; cont_num < page_num; ++cont_num) {
        post_pfn = ka_mm_page_to_pfn(inpages[cont_num]);
        if ((pre_pfn + 1) != post_pfn) {
            break;
        }
        pre_pfn = post_pfn;
    }
    return cont_num;
}

int devmm_insert_pages_to_vma_owner(ka_vm_area_struct_t *vma, u64 va,
    u64 page_num, ka_page_t **inpages, ka_pgprot_t vm_page_prot)
{
    u64 i, cont_num;
    u32 stamp = (u32)ka_jiffies;
    int ret;

    for (i = 0; i < page_num;) {
        cont_num = devmm_get_cont_page_num(&inpages[i], page_num - i);
        ret = ka_mm_remap_pfn_range(
            vma, va + KA_MM_PAGE_SIZE * i, ka_mm_page_to_pfn(inpages[i]), KA_MM_PAGE_SIZE * cont_num, vm_page_prot);
        if (ret != 0) {
            devmm_drv_err("Vm_insert_page failed. (ret=%d; va=0x%llx; i=%llu; cont_num=%llu; page_num=%llu)\n",
                ret, va, i, cont_num, page_num);
            devmm_zap_vma_ptes(vma, va, KA_MM_PAGE_SIZE * i);
            return -ENOMEM;
        }
        i += cont_num;
        devmm_try_cond_resched(&stamp);
    }
    return 0;
}

static int _devmm_pages_remap_owner(struct devmm_svm_process *svm_proc, u64 va, u64 page_num,
    ka_page_t **inpages, ka_pgprot_t vm_page_prot)
{
    ka_vm_area_struct_t *vma = NULL;
    int ret;

    vma = devmm_find_vma(svm_proc, va);
    if (vma == NULL) {
        return -EADDRNOTAVAIL;
    }

    ret = devmm_insert_pages_to_vma_owner(vma, va, page_num, inpages, vm_page_prot);
    if (ret != 0) {
        devmm_drv_info("Can not insert_pages_vma cp. (va=0x%llx; ret=%d)\n", va, ret);
        return ret;
    }

    return 0;
}

int devmm_pages_remap_owner(struct devmm_svm_process *svm_proc, u64 va, u64 page_num,
    ka_page_t **inpages, u32 page_prot)
{
    int ret;

    ret = _devmm_pages_remap_owner(svm_proc, va, page_num, inpages, devmm_make_pgprot(page_prot, false));
    if (ret != 0) {
        return ret;
    }

    if (devmm_is_support_double_pgtable() && devmm_is_static_reserve_addr(svm_proc, va)) {
        ret = _devmm_pages_remap_owner(svm_proc, devmm_double_pgtable_get_offset_addr(va),
            page_num, inpages, devmm_make_pgprot(page_prot, true));
        if (ret != 0) {
            _devmm_zap_owner_ptes_range(svm_proc, va, page_num);
            return ret;
        }
    }

    return 0;
}

static u64 devmm_get_no_mapped_page_num(ka_vm_area_struct_t *vma, u64 vaddr, u64 page_num)
{
    unsigned long tmp_pfn;
    u64 i;
    int ret;

    for (i = 0; i < page_num; ++i) {
        ret = ka_mm_follow_pfn(vma, vaddr + i * KA_MM_PAGE_SIZE, &tmp_pfn);
        if (ret == 0) {
            break;
        }
    }
    return i;
}

int devmm_insert_pages_to_vma_custom(ka_vm_area_struct_t *vma, u64 va,
    u64 page_num, ka_page_t **inpages, u32 pgprot)
{
    u64 i, no_mapped_num, cont_num;
    u32 stamp = (u32)ka_jiffies;
    int ret;

    for (i = 0; i < page_num;) {
        no_mapped_num = devmm_get_no_mapped_page_num(vma, va + i * KA_MM_PAGE_SIZE, page_num - i);
        if (no_mapped_num == 0) {
            i++;
            continue;
        }

        cont_num = devmm_get_cont_page_num(&inpages[i], no_mapped_num);
        ret = ka_mm_remap_pfn_range(vma, va + i * KA_MM_PAGE_SIZE, ka_mm_page_to_pfn(inpages[i]),
            cont_num * KA_MM_PAGE_SIZE, devmm_make_pgprot(pgprot, false));
        if (ret != 0) {
            devmm_drv_err("Vm_insert_page failed. (ret=%d; va=0x%llx; i=%llu; cont_num=%llu; page_num=%llu)\n",
                ret, va, i, cont_num, page_num);
            devmm_zap_vma_ptes(vma, va, KA_MM_PAGE_SIZE * i);
            return -ENOMEM;
        }
        i += cont_num;
        devmm_try_cond_resched(&stamp);
    }

    return 0;
}

static int devmm_pages_remap_custom(struct devmm_svm_process *svm_proc, u64 va, u64 page_num, ka_page_t **inpages,
    u32 page_prot)
{
    ka_mm_struct_t *custom_mm = NULL;
    ka_vm_area_struct_t *vma = NULL;
    ka_pid_t custom_pid;
    int ret, get_lock;
    u32 i;

    for (i = 0, ret = 0; i < DEVMM_CUSTOM_PROCESS_NUM && (ret == 0); i++) {
        ka_task_mutex_lock(&svm_proc->custom[i].proc_lock);
        if (svm_proc->custom[i].status != DEVMM_CUSTOM_USED) {
            ka_task_mutex_unlock(&svm_proc->custom[i].proc_lock);
            continue;
        }
        custom_pid = svm_proc->custom[i].custom_pid;
        ka_task_mutex_unlock(&svm_proc->custom[i].proc_lock);

        custom_mm = devmm_custom_mm_get(custom_pid);
        if (custom_mm == NULL) {
            continue;
        }

        get_lock = ka_task_down_write_trylock(get_mmap_sem(custom_mm));
        vma = ka_mm_find_vma(custom_mm, va);
        if ((vma == NULL) || (ka_mm_get_vm_start(vma) > va) || (devmm_is_svm_vma_magic(ka_mm_get_vm_private_data(vma)) == false)) {
#ifndef EMU_ST
            if (get_lock) {
                ka_task_up_write(get_mmap_sem(custom_mm));
            }
            devmm_custom_mm_put(custom_mm);
            continue;
#else
            vma = devmm_find_vma_custom(svm_proc, i, va);
#endif
        }

        ret = devmm_insert_pages_to_vma_custom(vma, va, page_num, inpages, page_prot);
        if (ret == 0) {
            devmm_pin_user_pages(inpages, page_num);
        }
        if (get_lock) {
            ka_task_up_write(get_mmap_sem(custom_mm));
        }
        devmm_custom_mm_put(custom_mm);
    }

    return ret;
}

int devmm_pages_remap(struct devmm_svm_process *svm_proc, u64 va, u64 page_num, ka_page_t **inpages, u32 page_prot)
{
    int ret;

    ret = devmm_pages_remap_owner(svm_proc, va, page_num, inpages, page_prot);
    if (ret != 0) {
        devmm_drv_info("Can not devmm_insert_pages_to_vma. (page_num=%llu; ret=%d)\n",
            page_num, ret);
        return ret;
    }
    ret = devmm_pages_remap_custom(svm_proc, va, page_num, inpages, page_prot);
    if (ret != 0) {
        devmm_drv_err("Devmm_insert_pages_custom fail. (page_num=%llu; ret=%d)\n",
            page_num, ret);
        devmm_zap_owner_ptes_range(svm_proc, va, page_num);
        return ret;
    }

    return 0;
}

void devmm_zap_normal_pages(struct devmm_svm_process *svm_proc, u64 va, u64 page_num)
{
    devmm_unmap_pages_custom(svm_proc, va, page_num);               // TO-DO: optimize name later
    devmm_zap_owner_ptes_range(svm_proc, va, page_num);
}

int devmm_remap_pages(struct devmm_svm_process *svm_proc, u64 va,
    ka_page_t **pages, u64 pg_num, u32 pg_type)
{
    if (pg_type == DEVMM_NORMAL_PAGE_TYPE) {
        return devmm_pages_remap(svm_proc, va, pg_num, pages, 0);   // TO-DO: optimize name later
    } else if (pg_type == DEVMM_HUGE_PAGE_TYPE) {
        if (devmm_is_giant_page(pages, pg_num)) { // hbm remap
            return devmm_remap_giant_pages(svm_proc, va, pages, pg_num, 0, true);
        } else {
            return devmm_remap_huge_pages(svm_proc, va, pages, pg_num, 0);
        }
    } else { // dram remap
        return devmm_remap_giant_pages(svm_proc, va, pages, pg_num, 0, false);
    }
}

void devmm_zap_pages(struct devmm_svm_process *svm_proc, u64 va, u64 pg_num, u32 pg_type)
{
    if (pg_type == DEVMM_NORMAL_PAGE_TYPE) {
        devmm_zap_normal_pages(svm_proc, va, pg_num);
    } else if (pg_type == DEVMM_HUGE_PAGE_TYPE) {
        devmm_zap_huge_pages(svm_proc, va, pg_num);
    } else {
        devmm_zap_giant_pages(svm_proc, va, pg_num);
    }
}

int devmm_insert_normal_pages(struct page_map_info *page_map_info, struct devmm_svm_process *svm_proc)
{
    struct devmm_phy_addr_attr attr = {0};
    u32 mem_type = page_map_info->mem_type;
    bool is_continuous = page_map_info->is_continuty;
    int ret;

    devmm_phy_addr_attr_pack(svm_proc, DEVMM_NORMAL_PAGE_TYPE, mem_type, is_continuous, &attr);
    ret = devmm_proc_alloc_pages(svm_proc, &attr, page_map_info->inpages, page_map_info->page_num);
    if (ret != 0) {
        return ret;
    }

    ret = devmm_pages_remap(svm_proc, page_map_info->va, page_map_info->page_num, page_map_info->inpages,
        page_map_info->page_prot);
    if (ret != 0) {
        devmm_drv_info("Devmm_insert_pages_to_vma. (page_num=%llu; page_prot=%d; ret=%d)\n",
            page_map_info->page_num, page_map_info->page_prot, ret);
        devmm_proc_free_pages(svm_proc, &attr, page_map_info->inpages, page_map_info->page_num);
        return ret;
    }

    return ret;
}

STATIC struct devmm_svm_heap *devmm_svm_get_heap_proc(struct devmm_svm_process *svm_proc, unsigned long va)
{
    struct devmm_svm_heap *heap = NULL;
    u32 heap_idx;

    if (devmm_is_in_host_pin_range(va)) {
        return svm_proc->host_pin_heap;
    }

    heap_idx = (u32)((va - svm_proc->start_addr) / DEVMM_HEAP_SIZE);
    heap = devmm_get_heap_by_idx(svm_proc, heap_idx);
    if (devmm_check_heap_is_entity(heap) == false) {
        return NULL;
    }

    return heap;
}

struct devmm_svm_heap *devmm_svm_get_heap(struct devmm_svm_process *svm_proc, unsigned long va)
{
    if (devmm_is_in_host_pin_range(va)) {
        return svm_proc->host_pin_heap;
    }

    if (devmm_va_is_not_svm_process_addr(svm_proc, va)) {
        return NULL;
    }

    return devmm_svm_get_heap_proc(svm_proc, va);
}

struct devmm_svm_heap *devmm_svm_heap_get(struct devmm_svm_process *svm_proc, unsigned long va)
{
    struct devmm_svm_heap *heap = NULL;

    if (devmm_va_is_not_svm_process_addr(svm_proc, va)) {
        return NULL;
    }
    ka_task_down_read(&svm_proc->heap_sem);
    heap = devmm_svm_get_heap_proc(svm_proc, va);
    if ((heap == NULL) || heap->is_invalid) {
        ka_task_up_read(&svm_proc->heap_sem);
        return NULL;
    }
    ka_base_atomic64_inc(&heap->occupy_cnt);
    ka_task_up_read(&svm_proc->heap_sem);

    return heap;
}

void devmm_svm_heap_put(struct devmm_svm_heap *heap)
{
    if (heap != NULL) {
        ka_base_atomic64_dec(&heap->occupy_cnt);
    }
}

int devmm_svm_proc_and_heap_get(struct devmm_svm_process_id *process_id, u64 va,
    struct devmm_svm_process **svm_proc, struct devmm_svm_heap **heap)
{
    *svm_proc = devmm_svm_proc_get_by_process_id_ex(process_id);
    if (*svm_proc == NULL) {
        devmm_drv_err("Process is exit. (va=0x%llx; hostpid=%d; devid=%d; vfid=%d)\n",
                      va, process_id->hostpid, process_id->devid, process_id->vfid);
        return -ESRCH;
    }
    *heap = devmm_svm_heap_get(*svm_proc, va);
    if (*heap == NULL) {
        devmm_svm_proc_put(*svm_proc);
        devmm_drv_err("Vaddress is errorr. (va=0x%llx; hostpid=%d; devid=%d; vfid=%d)\n",
                      va, process_id->hostpid, process_id->devid, process_id->vfid);
        return -EADDRNOTAVAIL;
    }

    return 0;
}

void devmm_svm_proc_and_heap_put(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap)
{
    devmm_svm_heap_put(heap);
    devmm_svm_proc_put(svm_proc);
}

void devmm_chan_set_host_device_page_size(void)
{
    devmm_svm->host_page_size = devmm_svm_pageshift2pagesize(devmm_svm->host_page_shift);
    devmm_svm->host_hpage_size = devmm_svm_pageshift2pagesize(devmm_svm->host_hpage_shift);

    devmm_svm->device_page_size = devmm_svm_pageshift2pagesize(devmm_svm->device_page_shift);
    devmm_svm->device_hpage_size = devmm_svm_pageshift2pagesize(devmm_svm->device_hpage_shift);

    devmm_svm->svm_page_size = DEVMM_MAX(devmm_svm->host_page_size, devmm_svm->device_page_size);
    devmm_svm->svm_page_shift = DEVMM_MAX(devmm_svm->host_page_shift, devmm_svm->device_page_shift);

    if (devmm_svm->device_hpage_shift < devmm_svm->host_page_shift) {
        devmm_drv_err("Device_huge_page_shfit less than host_page_shfit. (device_hpage_shift=%u; "
                      "host_page_shift=%u)\n", devmm_svm->device_hpage_shift, devmm_svm->host_page_shift);
        return;
    } else {
        /* device huge page size is 2M ,host do not has huge page,just 4k/16k/64k */
        devmm_svm->host_page2device_hpage_order = devmm_svm->device_hpage_shift - devmm_svm->host_page_shift;
    }

    if (devmm_svm->host_page_shift < devmm_svm->device_page_shift) {
        devmm_drv_err("Host_page_shift less than device_page_shift. (host_page_shift=%u; device_page_shift=%u)\n",
            devmm_svm->host_page_shift, devmm_svm->device_page_shift);
        return;
    } else {
        /* device page size is 4k ,host will be 4k/16k/64k */
        devmm_svm->host_page2device_page_order = devmm_svm->host_page_shift - devmm_svm->device_page_shift;
    }

    if (devmm_svm->host_hpage_shift < devmm_svm->device_hpage_shift) {
        devmm_drv_err("Host_hpage_shift less than device_hpage_shift. (host_hpage_shift=%u; "
                      "device_hpage_shift=%u)\n", devmm_svm->host_hpage_shift, devmm_svm->device_hpage_shift);
        return;
    } else {
        devmm_svm->host_hpage2device_hpage_order = devmm_svm->host_hpage_shift - devmm_svm->device_hpage_shift;
    }

    devmm_svm->page_size_inited = 1;

    devmm_drv_info("Shift info. (host_page_shift=%u; host_hpage_shift=%u; evice_page_shift=%u; "
                   "device_hpage_shift=%u; h2dh_adjustorder=%u; h2d_adjustorder=%u) \n",
                   devmm_svm->host_page_shift, devmm_svm->host_hpage_shift, devmm_svm->device_page_shift,
                   devmm_svm->device_hpage_shift, devmm_svm->host_page2device_hpage_order,
                   devmm_svm->host_page2device_page_order);

    devmm_drv_info("Size info. (host_page_size=%u; host_hpage_size=%u; "
                   "device_page_size=%u; device_hpage_size=%u; svm_page_size=%u)\n",
                   devmm_svm->host_page_size, devmm_svm->host_hpage_size, devmm_svm->device_page_size,
                   devmm_svm->device_hpage_size, devmm_svm->svm_page_size);
}

STATIC int devmm_svm_get_channel_lock(struct devmm_svm_process *svm_proc,
    const struct devmm_svm_process_id *process_id, const struct devmm_chan_handlers_st *msg_process, u32 msg_id)
{
    u32 msg_bitmap = msg_process[msg_id].msg_bitmap;

    if (svm_proc == NULL) {
        devmm_drv_err_if(((msg_bitmap & DEVMM_MSG_RETURN_OK_MASK) == 0),
            "Cp is exited, above message. (hostpid=%d; devid=%d; vfid=%d; msg_id=%u)\n",
            process_id->hostpid, process_id->devid, process_id->vfid, msg_id);
        /* errcode ESRCH return to user */
        return ((msg_bitmap & DEVMM_MSG_RETURN_OK_MASK) != 0) ? -EOWNERDEAD : -ESRCH;
    }

    if (devmm_svm_mem_is_enable(svm_proc) == false) {
#ifndef EMU_ST
        devmm_drv_err_if(((msg_bitmap & DEVMM_MSG_RETURN_OK_MASK) == 0),
            "Mmap failed, svm is disable.(hostpid=%d; devid=%d; vfid=%d; msg_id=%u).\n",
            svm_proc->process_id.hostpid, svm_proc->process_id.devid, svm_proc->process_id.vfid, msg_id);
        /* errcode ESRCH return to user */
        return -ESRCH;
#endif
    }

    ka_task_mutex_lock(&svm_proc->proc_lock);
    if ((svm_proc->proc_status & DEVMM_SVM_PROC_ABORT_STATE) != 0) {
        ka_task_mutex_unlock(&svm_proc->proc_lock);
        devmm_drv_err_if(((msg_bitmap & DEVMM_MSG_RETURN_OK_MASK) == 0),
            "Cp(aicpu) is exiting, above message. (hostpid=%d; devid=%d; vfid=%d; msg_id=%u).\n",
            svm_proc->process_id.hostpid, svm_proc->process_id.devid, svm_proc->process_id.vfid, msg_id);
        /* errcode ESRCH return to user */
        return ((msg_bitmap & DEVMM_MSG_RETURN_OK_MASK) != 0) ? -EOWNERDEAD : -ESRCH;
    }
    svm_proc->msg_processing++;
    ka_task_mutex_unlock(&svm_proc->proc_lock);

    if (devmm_get_end_type() == DEVMM_END_HOST) {
        ka_task_down_read(&svm_proc->bitmap_sem);
    } else {
        if ((msg_bitmap & DEVMM_MSG_WRITE_LOCK_MASK) != 0) {
            ka_task_down_write(&svm_proc->msg_chan_sem);
        }
    }

    return 0;
}

STATIC void devmm_svm_put_channel_lock(struct devmm_svm_process *svm_proc,
    const struct devmm_chan_handlers_st *msg_process, u32 msg_id)
{
    if (svm_proc == NULL) {
        return;
    }
    ka_task_mutex_lock(&svm_proc->proc_lock);
    if (svm_proc->msg_processing > 0) {
        svm_proc->msg_processing--;
    }
    ka_task_mutex_unlock(&svm_proc->proc_lock);

    if (devmm_get_end_type() == DEVMM_END_HOST) {
        ka_task_up_read(&svm_proc->bitmap_sem);
    } else {
        if ((msg_process[msg_id].msg_bitmap & DEVMM_MSG_WRITE_LOCK_MASK) != 0) {
            ka_task_up_write(&svm_proc->msg_chan_sem);
        }
    }
}

int devmm_svm_other_proc_occupy_num_add(struct devmm_svm_process *svm_proc)
{
    ka_task_mutex_lock(&svm_proc->proc_lock);
    if (((svm_proc->proc_status & DEVMM_SVM_PROC_ABORT_STATE) != 0) ||
        (svm_proc->inited != DEVMM_SVM_INITED_FLAG)) {
        ka_task_mutex_unlock(&svm_proc->proc_lock);
        return -EFAULT;
    }

    svm_proc->other_proc_occupying++;
    ka_task_mutex_unlock(&svm_proc->proc_lock);
    return 0;
}

void devmm_svm_other_proc_occupy_num_sub(struct devmm_svm_process *svm_proc)
{
    ka_task_mutex_lock(&svm_proc->proc_lock);
    if (svm_proc->other_proc_occupying > 0) {
        svm_proc->other_proc_occupying--;
    }
    ka_task_mutex_unlock(&svm_proc->proc_lock);
}

int devmm_svm_other_proc_occupy_get_lock(struct devmm_svm_process *svm_proc)
{
    int ret;

    ret = devmm_svm_other_proc_occupy_num_add(svm_proc);
    if (ret != 0) {
        return ret;
    }

    ka_task_down_read(&svm_proc->ioctl_rwsem);
    return 0;
}

void devmm_svm_other_proc_occupy_put_lock(struct devmm_svm_process *svm_proc)
{
    ka_task_up_read(&svm_proc->ioctl_rwsem);
    devmm_svm_other_proc_occupy_num_sub(svm_proc);
}

static void devmm_chan_put_svm_proc_and_unlock(struct devmm_chan_msg_head *head_msg,
    const struct devmm_chan_handlers_st *msg_process, struct devmm_svm_process *svm_proc)
{
    devmm_svm_put_channel_lock(svm_proc, msg_process, head_msg->msg_id);
    devmm_svm_proc_put(svm_proc);
}

STATIC int devmm_chan_get_svm_proc_and_lock(struct devmm_chan_msg_head *head_msg,
    const struct devmm_chan_handlers_st *msg_process, struct devmm_svm_process **svm_proc)
{
    u32 msg_id = head_msg->msg_id;
    int ret;

    if ((msg_process[msg_id].msg_bitmap & DEVMM_MSG_NOT_NEED_PROC_MASK) != 0) {
        *svm_proc = NULL;
        return 0;
    }
    *svm_proc = devmm_svm_proc_get_by_process_id(&head_msg->process_id);
    ret = devmm_svm_get_channel_lock(*svm_proc, &head_msg->process_id, msg_process, msg_id);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_svm_proc_put(*svm_proc);
        *svm_proc = NULL;
#endif
    }
    return ret;
}

STATIC int devmm_chan_get_heap(struct devmm_chan_addr_head *addr_head, const struct devmm_chan_handlers_st *msg_process,
    struct devmm_svm_process *svm_proc, struct devmm_svm_heap **heap)
{
    u32 msg_id = addr_head->head.msg_id;
    u32 msg_bitmap = msg_process[msg_id].msg_bitmap;

    if ((svm_proc == NULL) || ((msg_bitmap & DEVMM_MSG_GET_HEAP_MASK) == 0)) {
        *heap = NULL;
        return 0;
    }
    *heap = devmm_svm_get_heap(svm_proc, addr_head->va);
    if (*heap == NULL) {
        devmm_drv_err_if(((msg_bitmap & DEVMM_MSG_RETURN_OK_MASK) == 0),
            "Address is free or not alloc, above message. (hostpid=%d; devid=%d; vfid=%d; msg_id=%u; va=0x%llx)\n",
            svm_proc->process_id.hostpid, svm_proc->process_id.devid, svm_proc->process_id.vfid, msg_id, addr_head->va);
        return ((msg_bitmap & DEVMM_MSG_RETURN_OK_MASK) != 0) ? -EREMCHG : -EINVAL;
    }
    return 0;
}

static int devmm_chan_enable_cgroup(const struct devmm_chan_handlers_st *msg_process, u32 msg_id,
    struct devmm_svm_process *svm_proc, ka_mem_cgroup_t **old_memcg, ka_mem_cgroup_t **memcg)
{
    if ((msg_process[msg_id].msg_bitmap & DEVMM_MSG_ADD_CGROUP_MASK) != 0 && svm_proc != NULL) {
        *old_memcg = devmm_enable_cgroup(memcg, svm_proc->devpid);
        if (*memcg == NULL) {
#ifndef EMU_ST
            return -ESRCH;
#endif
        }
    }
    return 0;
}

static void devmm_chan_disable_cgroup(const struct devmm_chan_handlers_st *msg_process, u32 msg_id,
    struct devmm_svm_process *svm_proc, ka_mem_cgroup_t *old_memcg, ka_mem_cgroup_t *memcg)
{
    if ((msg_process[msg_id].msg_bitmap & DEVMM_MSG_ADD_CGROUP_MASK) != 0 && svm_proc != NULL) {
        devmm_disable_cgroup(memcg, old_memcg);
    }
}

int devmm_chan_msg_dispatch(void *msg, u32 in_data_len, u32 out_data_len, u32 *ack_len,
    const struct devmm_chan_handlers_st *msg_process)
{
    struct devmm_chan_msg_head *head_msg = (struct devmm_chan_msg_head *)msg;
    u32 data_len = DEVMM_MAX(in_data_len, out_data_len);
    u32 head_len = sizeof(struct devmm_chan_msg_head);
    struct devmm_svm_process *svm_proc = NULL;
    struct devmm_svm_heap *heap = NULL;
    ka_mem_cgroup_t *old_memcg = NULL;
    ka_mem_cgroup_t *memcg = NULL;
    u32 msg_id, proc_len;
    int ret;

    head_msg->result = 0;
    *ack_len = 0;
    msg_id = head_msg->msg_id;
    if ((msg_id >= DEVMM_CHAN_MAX_ID) || (msg_process[msg_id].chan_msg_processes == NULL)) {
        devmm_drv_err("Invalid message_id or none process func. (msg_id=%u)\n", msg_id);
        ret = -ENOMSG;
        goto save_msg_ret;
    }
    proc_len = msg_process[msg_id].msg_size + head_msg->extend_num * msg_process[msg_id].extend_size;
    if (data_len < proc_len) {
        devmm_drv_err("Invalid process_len. (proc_len=%u; in_len=%u; out_len=%u)\n",
            proc_len, in_data_len, out_data_len);
        ret = -EMSGSIZE;
        goto save_msg_ret;
    }

#ifdef CFG_FEATURE_VFIO
    if (devmm_pm_is_vm_scene(msg)) {
        ret = devmm_pm_chan_msg_dispatch(msg, in_data_len, out_data_len, ack_len);
        goto save_msg_ret;
    }
#endif

    if (head_msg->process_id.vfid >= DEVMM_MAX_VF_NUM) {
        devmm_drv_err("Message_id has invalid. (msg_id=%u; vfid=%d)\n", msg_id, head_msg->process_id.vfid);
        ret = -EINVAL;
        goto save_msg_ret;
    }
    devmm_chan_update_msg_process_id(head_msg);

    ret = devmm_chan_get_svm_proc_and_lock(head_msg, msg_process, &svm_proc);
    if (ret != 0) {
        /* set DEVMM_MSG_RETURN_OK_MASK when thread exit will return ower died ret, msg return ok */
        ret = (ret == -EOWNERDEAD) ? 0 : ret;
        goto save_msg_ret;
    }

    ret = devmm_chan_get_heap((struct devmm_chan_addr_head *)msg, msg_process, svm_proc, &heap);
    if (ret != 0) {
        /* set DEVMM_MSG_RETURN_OK_MASK when heap destory will return Remote address changed, msg return ok */
        ret = (ret == -EREMCHG) ? 0 : ret;
        goto put_svm_proc_and_chan_lock;
    }
    ret = devmm_chan_update_msg_logic_id(svm_proc, head_msg);
    if (ret != 0) {
        goto put_svm_proc_and_chan_lock;
    }
    ret = devmm_chan_enable_cgroup(msg_process, msg_id, svm_proc, &old_memcg, &memcg);
    if (ret != 0) {
#ifndef EMU_ST
        goto put_svm_proc_and_chan_lock;
#endif
    }
    ret = msg_process[msg_id].chan_msg_processes(svm_proc, heap, msg, ack_len);
    devmm_chan_disable_cgroup(msg_process, msg_id, svm_proc, old_memcg, memcg);
put_svm_proc_and_chan_lock:
    devmm_chan_put_svm_proc_and_unlock(head_msg, msg_process, svm_proc);
save_msg_ret:
    /*
     * 1. if svm process is wrong, the error code is returned by head_msg->result
     * 2. for the performance, if svm process is right, return nothing
     * 3. devmm_chan_msg_dispatch always return 0, beacuse another error code means pcie msg wrong
     */
    if (ret != 0) {
        head_msg->result = (short)ret;
        *ack_len = (*ack_len > head_len) ? *ack_len : head_len;
    }

    return 0;
}
