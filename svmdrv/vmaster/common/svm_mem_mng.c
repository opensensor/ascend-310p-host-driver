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

#include <linux/types.h>
#include <linux/hugetlb.h>

#include "kernel_version_adapt.h"
#include "devmm_common.h"
#include "devmm_adapt.h"
#include "svm_log.h"
#include "svm_mem_mng.h"

ka_pgprot_t devmm_make_remote_pgprot(u32 flg)
{
    return ka_mm_pgprot_writecombine(devmm_make_pgprot(flg, false));
}

ka_pgprot_t devmm_make_remote_pgprot_ex(u32 flg, struct devmm_pgprot_cfg_info cfg_info)
{
    cfg_info.no_cache = false;
    return ka_mm_pgprot_writecombine(devmm_make_pgprot_ex(flg, cfg_info));
}

ka_pgprot_t devmm_make_nocache_pgprot(u32 flg)
{
    return ka_mm_pgprot_device(devmm_make_pgprot(flg, false));
}

ka_pgprot_t devmm_make_readonly_pgprot(ka_pgprot_t prot)
{
    u32 prot_val = pgprot_val(prot);
#if defined(CONFIG_X86_64)
    prot_val &= ~_PAGE_RW;
#elif defined(CONFIG_ARM64)
    prot_val &= ~PTE_WRITE;
#endif
    return __pgprot(prot_val);
}

bool devmm_is_readonly_mem(u32 page_prot)
{
    bool is_readonly = page_prot & DEVMM_PAGE_READONLY_FLG;
    return is_readonly;
}

ka_page_t *devmm_pa_to_page(u64 paddr)
{
    return ka_mm_pfn_to_page(KA_MM_PFN_DOWN(paddr));
}

#ifndef EMU_ST
ka_vm_area_struct_t *devmm_find_vma_from_mm(ka_mm_struct_t *mm, u64 vaddr)
{
    ka_vm_area_struct_t *vma = NULL;

    vma = ka_mm_find_vma(mm, vaddr);
    if ((vma != NULL) && (ka_mm_get_vm_start(vma) <= vaddr)) {
        return vma;
    }
    return NULL;
}
#endif

static void devmm_print_svm_process_vma(ka_vm_area_struct_t *vma[], u32 vma_num)
{
    u32 i;

    for (i = 0; i < vma_num; i++) {
        if (vma[i] == NULL) {
            return;
        }
        devmm_drv_err("Svm vma. (idx=%u; vm_start=0x%lx; vm_end=0x%lx; vm_pgoff=0x%lx; vm_flags=0x%lx)\n",
            i, ka_mm_get_vm_start(vma[i]), ka_mm_get_vm_end(vma[i]), vma[i]->vm_pgoff, ka_mm_get_vm_flags(vma[i]));
    }
}

ka_vm_area_struct_t *_devmm_find_vma_proc(ka_vm_area_struct_t *vma[], u32 vma_num, u64 vaddr)
{
    u32 index;

    for (index = 0; index < vma_num; index++) {
        if ((vma[index] != NULL) &&
            ((vaddr >= ka_mm_get_vm_start(vma[index])) && (vaddr < ka_mm_get_vm_end(vma[index])))) {
            return vma[index];
        }
    }
    return NULL;
}

ka_vm_area_struct_t *devmm_find_vma_proc(ka_mm_struct_t *mm, ka_vm_area_struct_t *vma[],
    u32 vma_num, u64 vaddr)
{
    ka_vm_area_struct_t *tmp_vma = NULL;

    tmp_vma = _devmm_find_vma_proc(vma, vma_num, vaddr);
    if (tmp_vma != NULL) {
        return tmp_vma;
    }
    devmm_drv_err("Find vma info. (vaddr=0x%llx; vma_num=%u)\n", vaddr, vma_num);
    devmm_print_svm_process_vma(vma, vma_num);
#ifdef EMU_ST
    tmp_vma = ka_mm_find_vma(mm, vaddr);
    if ((tmp_vma == NULL) || (ka_mm_get_vm_start(tmp_vma) > vaddr)) {
        devmm_drv_err("Vma not exist. (vaddr=0x%llx)\n", vaddr);
        return NULL;
    }
    /* os vma may change, set again */
    devmm_set_proc_vma(mm, vma, vma_num);
#endif
    return tmp_vma;
}

static long _devmm_get_user_pages_remote(ka_task_struct_t *tsk, ka_mm_struct_t *mm,
    u64 va, int write, u32 num, ka_page_t **pages)
{
    long got_num;
    int locked;

    locked = 1;
    ka_task_down_read(ka_mm_get_mmap_sem(mm));
    got_num = ka_mm_get_user_pages_remote(tsk, mm, va, write, num, pages);
    ka_task_up_read(ka_mm_get_mmap_sem(mm));

    return got_num;
}

int devmm_get_user_pages_remote(ka_task_struct_t *tsk, ka_mm_struct_t *mm,
    u64 va, int write, u32 num, ka_page_t **pages)
{
    u32 i, try_times = 5; /* try 5 times, maybe success */
    long got_num;

    for (i = 0; i < try_times; i++) {
        got_num = _devmm_get_user_pages_remote(tsk, mm, va, write, num, pages);
        if (got_num == (long)num) {
            return 0;
        }
#ifndef EMU_ST
        if (got_num < 0) {
            return -EFAULT;
        }
        devmm_unpin_user_pages(pages, (u64)num, (u64)got_num);
#endif
    }
    return -EFAULT;
}

u64 devmm_kpg_size(ka_page_t *pg)
{
    u64 kpg_size;
#ifndef EMU_ST
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
    kpg_size = page_size(pg);
#else
    kpg_size = (KA_MM_PAGE_SIZE << ka_mm_compound_order(pg));
#endif
#else
    kpg_size = page_size(pg);
#endif
    return kpg_size;
}

bool devmm_is_giant_page(ka_page_t **pages, u64 pg_num)
{
    u64 i;

    for (i = 0; i < pg_num; i++) {
        if (pages[i] != NULL) {
            return (devmm_kpg_size(pages[i]) == DEVMM_GIANT_PAGE_SIZE);
        }
    }
    return false;
}