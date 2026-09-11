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
 * Create: 2024-11-11
 */

#include <linux/slab.h>

#include "securec.h"
#include "ka_memory_pub.h"
#include "ka_mem.h"

void ka_si_meminfo(struct ka_sysinfo *val)
{
    struct sysinfo sys_info = {0};
    si_meminfo(&sys_info);
    val->totalram = sys_info.totalram;
    val->freeram = sys_info.freeram;
    val->sharedram = sys_info.sharedram;
}
EXPORT_SYMBOL(ka_si_meminfo);

ka_rw_semaphore_t *ka_mm_get_mmap_sem(ka_mm_struct_t *mm)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
    return &mm->mmap_lock;
#else
    return &mm->mmap_sem;
#endif
}
EXPORT_SYMBOL(ka_mm_get_mmap_sem);

long ka_mm_get_user_pages_remote(ka_task_struct_t *tsk, ka_mm_struct_t *mm,
    unsigned long long va, int write, unsigned int num, ka_page_t **pages)
{
    long got_num;
    int locked;

    locked = 1;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 5, 0)
    got_num = pin_user_pages_remote(mm, va, num, (write != 0) ? FOLL_WRITE : 0, pages, NULL);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
    got_num = pin_user_pages_remote(mm, va, num, (write != 0) ? FOLL_WRITE : 0, pages, NULL, NULL);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
    got_num = get_user_pages_remote(mm, va, num, (write != 0) ? FOLL_WRITE : 0, pages, NULL, NULL);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
    got_num = get_user_pages_remote(tsk, mm, va, num, (write != 0) ? FOLL_WRITE : 0, pages, NULL, NULL);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)
    got_num = get_user_pages_remote(tsk, mm, va, num, (write != 0) ? FOLL_WRITE : 0, pages, NULL);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 6, 0)
    got_num = get_user_pages_remote(tsk, mm, va, num, (write != 0) ? FOLL_WRITE : 0, 0, pages, NULL);
#else
    got_num = get_user_pages_locked(tsk, mm, va, num, (write != 0) ? FOLL_WRITE : 0, 0, pages, &locked);
#endif
    return got_num;
}
EXPORT_SYMBOL(ka_mm_get_user_pages_remote);

void *ka_mm_page_to_virt(ka_page_t *page)
{
#ifdef page_to_virt
    return page_to_virt(page);
#else
    return __va(((phys_addr_t)(page_to_pfn(page)) << PAGE_SHIFT));
#endif
}
EXPORT_SYMBOL(ka_mm_page_to_virt);

unsigned long ka_mm_get_vm_flags(ka_vm_area_struct_t *vma)
{
    return vma->vm_flags;
}
EXPORT_SYMBOL(ka_mm_get_vm_flags);

void ka_mm_set_vm_flags(ka_vm_area_struct_t *vma, unsigned long flags)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0))
    vm_flags_set(vma, (vm_flags_t)flags);
#else
    vma->vm_flags |= flags;
#endif
}
EXPORT_SYMBOL(ka_mm_set_vm_flags);

void ka_mm_set_vm_private_data(ka_vm_area_struct_t *vma, void *private_data)
{
    vma->vm_private_data = private_data;
}
EXPORT_SYMBOL(ka_mm_set_vm_private_data);

void *ka_mm_get_vm_private_data(ka_vm_area_struct_t *vma)
{
    return vma->vm_private_data;
}
EXPORT_SYMBOL(ka_mm_get_vm_private_data);

void ka_mm_set_vm_start(ka_vm_area_struct_t *vma, unsigned long start)
{
    vma->vm_start = start;
}
EXPORT_SYMBOL(ka_mm_set_vm_start);

unsigned long ka_mm_get_vm_start(ka_vm_area_struct_t *vma)
{
    return vma->vm_start;
}
EXPORT_SYMBOL(ka_mm_get_vm_start);

void ka_mm_set_vm_end(ka_vm_area_struct_t *vma, unsigned long end)
{
    vma->vm_end = end;
}
EXPORT_SYMBOL(ka_mm_set_vm_end);

unsigned long ka_mm_get_vm_end(ka_vm_area_struct_t *vma)
{
    return vma->vm_end;
}
EXPORT_SYMBOL(ka_mm_get_vm_end);

void ka_mm_set_vm_pgprot(ka_vm_area_struct_t *vma)
{
    vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
}
EXPORT_SYMBOL(ka_mm_set_vm_pgprot);

ka_mm_pgprot_t *ka_mm_get_vm_pgprot(ka_vm_area_struct_t *vma)
{
    return &(vma->vm_page_prot);
}
EXPORT_SYMBOL(ka_mm_get_vm_pgprot);

int ka_mm_zap_vma_ptes(ka_vm_area_struct_t *vma, unsigned long address, unsigned long size)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
    zap_vma_ptes(vma, address, size);
    return 0;
#else
    return zap_vma_ptes(vma, address, size);
#endif
}
EXPORT_SYMBOL(ka_mm_zap_vma_ptes);

void ka_mm_set_vm_ops(ka_vm_area_struct_t *vma, const ka_vm_operations_struct_t *vm_ops)
{
    vma->vm_ops = vm_ops;
}
EXPORT_SYMBOL(ka_mm_set_vm_ops);

const ka_vm_operations_struct_t *ka_mm_get_vm_ops(ka_vm_area_struct_t *vma)
{
    return vma->vm_ops;
}
EXPORT_SYMBOL(ka_mm_get_vm_ops);

void ka_mm_mmget(ka_mm_struct_t *mm)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0))
    mmget(mm);
#else
    atomic_inc(&mm->mm_users);
#endif
}
EXPORT_SYMBOL(ka_mm_mmget);

int ka_mm_pin_user_pages_fast(unsigned long start, int nr_pages, unsigned int gup_flags, ka_page_t **pages)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
    return pin_user_pages_fast(start, nr_pages, gup_flags | KA_FOLL_LONGTERM, pages);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0)
    return get_user_pages_fast(start, nr_pages, gup_flags | KA_FOLL_LONGTERM, pages);
#else
    int write = (int)gup_flags;
    return get_user_pages_fast(start, nr_pages, write, pages);
#endif
}
EXPORT_SYMBOL(ka_mm_pin_user_pages_fast);

void ka_mm_unpin_user_page(ka_page_t *page)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
    unpin_user_page(page);
#else
    put_page(page);
#endif
}
EXPORT_SYMBOL(ka_mm_unpin_user_page);

void ka_mm_set_vm_fault_host(ka_vm_operations_struct_t *ops_managed, vmf_fault_func vmf_func, vm_fault_func vm_func)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
    ops_managed->fault = vmf_func;
#else
    ops_managed->fault = vm_func;
#endif
}
EXPORT_SYMBOL_GPL(ka_mm_set_vm_fault_host);

void ka_mm_set_vm_mremap(ka_vm_operations_struct_t *ops_managed, int (*mremap_func)(ka_vm_area_struct_t *))
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 3, 0)
    ops_managed->mremap = mremap_func;
#endif
    return;
}
EXPORT_SYMBOL_GPL(ka_mm_set_vm_mremap);
