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

#include "svm_cmd.h"
#include "devmm_proc_mem_copy.h"
#include "devmm_channel.h"
#include "svm_msg_client.h"
#include "devdrv_manager_comm.h"
#include "devmm_common.h"
#include "svm_shmem_interprocess.h"
#include "devmm_page_cache.h"
#include "devmm_register_dma.h"
#include "svm_dma.h"
#include "svm_master_remote_map.h"
#include "davinci_interface.h"
#include "davinci_api.h"
#include "svm_heap_mng.h"
#include "svm_master_addr_map.h"
#include "svm_proc_mng.h"
#include "svm_master_memset.h"
#include "svm_master_convert.h"
#include "svm_master_advise.h"
#include "svm_mmu_notifier.h"
#include "svm_master_get_host_info.h"
#include "svm_master_dev_capability.h"
#include "svm_master_memcpy.h"
#include "svm_proc_fs.h"
#include "svm_hot_reset.h"
#include "svm_task_dev_res_mng.h"
#include "svm_page_cnt_stats.h"
#include "svm_proc_gfp.h"
#include "svm_phy_addr_blk_mng.h"
#include "svm_master_mem_create.h"
#include "svm_master_mem_map.h"
#include "svm_master_addr_ref_ops.h"
#include "svm_master_proc_mng.h"
#include "svm_vmma_mng.h"
#include "svm_master_mem_share.h"
#include "devmm_mem_alloc_interface.h"
#include "svm_master_mem_repair.h"
#include "svm_mem_stats.h"
#include "svm_mem_query.h"
#include "kernel_version_adapt.h"
#include "svm_master_query.h"

#ifdef CFG_FEATURE_VFIO
#include "devmm_pm_adapt.h"
#include "devmm_pm_vpc.h"
#endif
#include "devmm_proc_info.h"

bool devmm_va_is_support_sdma_kernel_clear(struct devmm_svm_process *svm_proc, u64 va)
{
    return false;
}

void devmm_sdma_kernel_mem_clear(struct devmm_phy_addr_attr *attr, int ssid, struct devmm_pa_info_para *pa_info)
{
    return;
}

int devmm_remap_huge_pages(struct devmm_svm_process *svm_proc,
    u64 va, ka_page_t **hpages, u64 pg_num, u32 pg_prot)
{
    ka_vm_area_struct_t *vma = NULL;
    u64 i;
    int ret;

    vma = devmm_find_vma(svm_proc, va);
    if (vma == NULL) {
        devmm_drv_err("Find vma. (va=0x%llx; page_num=%llu)\n", va, pg_num);
        return -EADDRNOTAVAIL;
    }

    for (i = 0; i < pg_num; i++) {
        ret = ka_mm_remap_pfn_range(vma, va + SVM_MASTER_HUGE_PAGE_SIZE * i,
            ka_mm_page_to_pfn(hpages[i]), SVM_MASTER_HUGE_PAGE_SIZE, devmm_make_pgprot(pg_prot, false));
        if (ret != 0) {
            devmm_drv_err("Vm_insert_page failed. (ret=%d; va=0x%llx; i=%llu; page_num=%llu)\n",
                ret, va, i, pg_num);
            devmm_zap_vma_ptes(vma, va, SVM_MASTER_HUGE_PAGE_SIZE * i);
            return -ENOMEM;
        }
    }

    return 0;
}

int devmm_remap_giant_pages(struct devmm_svm_process *svm_proc,
    u64 va, ka_page_t **hpages, u64 pg_num, u32 pg_prot, bool has_interval)
{
    ka_vm_area_struct_t *vma = NULL;
    u32 stamp = (u32)ka_jiffies;
    u64 i;
    int ret;

    if (has_interval) {
        devmm_drv_err("not support interval. (va=0x%llx; page_num=%llu)\n", va, pg_num);
        return -EINVAL;
    }

    vma = devmm_find_vma(svm_proc, va);
    if (vma == NULL) {
        devmm_drv_err("Find vma. (va=0x%llx; page_num=%llu)\n", va, pg_num);
        return -EADDRNOTAVAIL;
    }

    for (i = 0; i < pg_num; i++) {
        ret = ka_mm_remap_pfn_range(vma, va + SVM_MASTER_GIANT_PAGE_SIZE * i,
            ka_mm_page_to_pfn(hpages[i]), SVM_MASTER_GIANT_PAGE_SIZE, devmm_make_pgprot(pg_prot, false));
        if (ret != 0) {
            devmm_drv_err("Vm_insert_page failed. (ret=%d; va=0x%llx; i=%llu; page_num=%llu)\n",
                ret, va, i, pg_num);
            devmm_zap_vma_ptes(vma, va, SVM_MASTER_GIANT_PAGE_SIZE * i);
            return -ENOMEM;
        }
        devmm_try_cond_resched(&stamp);
    }

    return 0;
}

void devmm_zap_huge_pages(struct devmm_svm_process *svm_proc, u64 va, u64 page_num)
{
    ka_vm_area_struct_t *vma = NULL;

    vma = devmm_find_vma(svm_proc, va);
    if (vma == NULL) {
        devmm_drv_err("Find vma. (va=0x%llx; page_num=%llu)\n", va, page_num);
        return;
    }

#ifndef EMU_ST
    ka_mm_zap_vma_ptes(vma, va, SVM_MASTER_HUGE_PAGE_SIZE * page_num);
#else
    u64 i;
    for (i = 0; i < page_num; i++) {
        ka_mm_zap_vma_ptes(vma, va + SVM_MASTER_HUGE_PAGE_SIZE * i, SVM_MASTER_HUGE_PAGE_SIZE);
    }
#endif
    return;
}

void devmm_zap_giant_pages(struct devmm_svm_process *svm_proc, u64 va, u64 page_num)
{
    ka_vm_area_struct_t *vma = NULL;

    vma = devmm_find_vma(svm_proc, va);
    if (vma == NULL) {
        devmm_drv_err("Find vma. (va=0x%llx; page_num=%llu)\n", va, page_num);
        return;
    }

#ifndef EMU_ST
    ka_mm_zap_vma_ptes(vma, va, SVM_MASTER_GIANT_PAGE_SIZE * page_num);
#else
    u64 i;
    for (i = 0; i < page_num; i++) {
        ka_mm_zap_vma_ptes(vma, va + SVM_MASTER_GIANT_PAGE_SIZE * i, SVM_MASTER_GIANT_PAGE_SIZE);
    }
#endif
    return;
}

STATIC int devmm_free_page_process(struct devmm_svm_process *svm_pro, struct devmm_svm_heap *heap, u32 *page_bitmap,
    u64 va, bool reuse);

bool devmm_dev_is_same_system(u32 src_devid, u32 dst_devid)
{
    if (src_devid >= DEVMM_MAX_DEVICE_NUM || dst_devid >= DEVMM_MAX_DEVICE_NUM) {
        return DEVMM_FALSE;
    }

    if (devmm_svm->device_info.cluster_id[src_devid] == devmm_svm->device_info.cluster_id[dst_devid]) {
        return DEVMM_TRUE;
    }
    return DEVMM_FALSE;
}

u32 *devmm_get_page_bitmap_with_heap(struct devmm_svm_heap *heap, u64 va)
{
    unsigned long pfn;
    int ret;

    ret = devmm_get_virt_pfn_by_heap(heap, va, &pfn);
    if (ret != 0) {
        devmm_drv_err("Fail to get virt pfn of va. (va=0x%llx)\n", va);
        return NULL;
    }

    return &heap->page_bitmap[pfn];
}

u32 *devmm_get_page_bitmap(struct devmm_svm_process *svm_process, u64 va)
{
    struct devmm_svm_heap *heap = NULL;

    heap = devmm_svm_get_heap(svm_process, va);
    if (heap == NULL) {
        devmm_drv_debug("Va isn't alloced. (va=0x%llx)\n", va);
        return NULL;
    }

    return devmm_get_page_bitmap_with_heap(heap, va);
}

STATIC u32 *devmm_get_fst_alloc_bitmap_by_heap(struct devmm_svm_process *svm_process,
    struct devmm_svm_heap *heap, u64 va)
{
    u32 *page_bitmap = NULL;

    page_bitmap = devmm_get_page_bitmap_with_heap(heap, va);
    if (page_bitmap == NULL) {
        devmm_drv_err("Get page bitmap failed. (heap_idx=%u; va=0x%llx)\n", heap->heap_idx, va);
        return NULL;
    }

    if (!devmm_page_bitmap_is_page_alloced(page_bitmap) || !devmm_page_bitmap_is_first_page(page_bitmap)) {
        devmm_drv_err("Va isn't first page. (va=0x%llx; page_bitmap=0x%x; heap_idx=%u)\n", va,
                      devmm_page_read_bitmap(page_bitmap), heap->heap_idx);
        return NULL;
    }

    return page_bitmap;
}

u32 *devmm_get_alloced_va_fst_page_bitmap_with_heap(struct devmm_svm_heap *heap, u64 va)
{
    unsigned long pfn;

    if (devmm_get_alloced_va_fst_pfn(heap, va, &pfn) != 0) {
        devmm_drv_err("Get pfn failed. (va=0x%llx)\n", va);
        return NULL;
    }

    return &heap->page_bitmap[pfn];
}

u32 *devmm_get_alloced_va_fst_page_bitmap(struct devmm_svm_process *svm_proc, u64 va)
{
    struct devmm_svm_heap *heap = devmm_svm_get_heap(svm_proc, va);

    if (heap == NULL) {
        devmm_drv_err("Get heap failed. (va=0x%llx)\n", va);
        return NULL;
    }

    return devmm_get_alloced_va_fst_page_bitmap_with_heap(heap, va);
}

int devmm_get_alloced_va_with_heap(struct devmm_svm_heap *heap, u64 va, u64 *alloced_va)
{
    unsigned long pfn;

    if (devmm_get_alloced_va_fst_pfn(heap, va, &pfn) != 0) {
        devmm_drv_err("Get pfn failed. (va=0x%llx)\n", va);
        return -EINVAL;
    }

    *alloced_va = heap->start + heap->chunk_page_size * pfn;

    return 0;
}

int devmm_get_alloced_va(struct devmm_svm_process *svm_proc, u64 va, u64 *alloced_va)
{
    struct devmm_svm_heap *heap = devmm_svm_get_heap(svm_proc, va);

    if (heap == NULL) {
        devmm_drv_err("Get heap failed. (va=0x%llx)\n", va);
        return -EINVAL;
    }

    return devmm_get_alloced_va_with_heap(heap, va, alloced_va);
}

int devmm_get_alloced_size(struct devmm_svm_process *svm_proc, u64 va, u64 *alloced_size)
{
    struct devmm_svm_heap *heap = devmm_svm_get_heap(svm_proc, va);

    if (heap == NULL) {
        devmm_drv_err("Get heap failed. (va=0x%llx)\n", va);
        return -EINVAL;
    }

    *alloced_size = devmm_get_alloced_size_from_va(heap, va);
    if (*alloced_size == 0) {
        devmm_drv_err("Get alloced size failed. (va=0x%llx)\n", va);
        return -EINVAL;
    }

    return 0;
}

int devmm_check_status_va_info(struct devmm_svm_process *svm_process, u64 va, u64 count)
{
    struct devmm_svm_heap *heap = NULL;

    heap = devmm_svm_get_heap(svm_process, va);
    if (heap == NULL) {
        devmm_drv_err("Heap is NULL. (va=0x%llx; start_addr=0x%lx)\n", va, svm_process->start_addr);
        return -EINVAL;
    }
    return devmm_check_va_add_size_by_heap(heap, va, count);
}

#ifndef DRV_UT
STATIC u32 devmm_get_shared_page_num(u32 *page_bitmap, u32 bitmap_num, u32 devid)
{
    u32 share_num;

    /* order sizes mask 5 bit, page sizes max 32 */
    for (share_num = 0; share_num < DEVMM_P2P_FAULT_PAGE_MAX_NUM;) {
        /* page alloced and mapped by dev, */
        if (devmm_page_bitmap_is_page_available(&page_bitmap[share_num]) &&
            devmm_page_bitmap_is_dev_mapped(&page_bitmap[share_num]) &&
            (devid != devmm_page_bitmap_get_devid(&page_bitmap[share_num]))) {
            /* set share flag */
            devmm_page_bitmap_set_flag(&page_bitmap[share_num], DEVMM_PAGE_ADVISE_MEMORY_SHARED_MASK);
        } else {
            break;
        }
        share_num++;
        /* next page is the end of this heap */
        if (share_num >= bitmap_num) {
            break;
        }
        /*  if next page first flag is seted, the page is other usr alloc */
        if (devmm_page_bitmap_is_first_page(&page_bitmap[share_num])) {
            break;
        }
    }

    return share_num;
}
#endif

void devmm_svm_set_mapped_with_heap(struct devmm_svm_process *svm_process, unsigned long va, size_t size,
    u32 logic_id, struct devmm_svm_heap *heap)
{
    u32 *page_bitmap = devmm_get_page_bitmap_with_heap(heap, va);
    if (page_bitmap == NULL) {
        devmm_drv_err("Can't find page bitmap. (va=0x%lx; size=%lu; logic_id=%u)\n", va, size, logic_id);
        return;
    }
    devmm_svm_set_bitmap_mapped(page_bitmap, size, heap->chunk_page_size, logic_id);
}

void devmm_svm_clear_mapped_with_heap(struct devmm_svm_process *svm_process, unsigned long va, size_t size,
    u32 devid, struct devmm_svm_heap *heap)
{
    u32 *page_bitmap = NULL;

    page_bitmap = devmm_get_page_bitmap_with_heap(heap, va);
    if (page_bitmap == NULL) {
        devmm_drv_err("Can't find page bitmap. (hostpid=%d; va=0x%lx; size=0x%lx; devid=%u)\n",
            svm_process->process_id.hostpid, va, size, devid);
        return;
    }
    devmm_svm_clear_bitmap_mapped(page_bitmap, size, heap->chunk_page_size, devid);
}

u32 devmm_svm_va_to_devid(struct devmm_svm_process *svm_proc, unsigned long va)
{
    struct devmm_svm_heap *heap = NULL;
    u32 *page_bitmap = NULL;
    u32 devid;

    heap = devmm_svm_heap_get(svm_proc, va);
    if (heap == NULL) {
        devmm_drv_err("Va isn't alloced. (va=0x%llx)\n", va);
        return SVM_MAX_AGENT_NUM;
    }

    page_bitmap = devmm_get_page_bitmap_with_heap(heap, va);
    if (page_bitmap == NULL) {
        devmm_svm_heap_put(heap);
        devmm_drv_err("Can't find bitmap current pid. (va=0x%lx)\n", va);
        return SVM_MAX_AGENT_NUM;
    }

    if (!devmm_page_bitmap_is_page_available(page_bitmap) || !devmm_page_bitmap_is_dev_mapped(page_bitmap)) {
        devmm_svm_heap_put(heap);
        /* host fault will return SVM_MAX_AGENT_NUM  */
        return SVM_MAX_AGENT_NUM;
    }

    devid = devmm_page_bitmap_get_devid(page_bitmap);
    devmm_svm_heap_put(heap);

    return devid;
}

void devmm_print_pre_alloced_va(struct devmm_svm_process *svm_process, u64 va)
{
    u64 start_va, end_va;

    if (devmm_check_alloced_va(svm_process, va, &start_va, &end_va, DEVMM_PRE_ALLOCED_FLAG) == 0) {
        devmm_drv_err_if((svm_process->device_fault_printf != 0),
            "Check alloced va. (hostpid=%d; va=0x%llx; start_va = 0x%llx; end_va = 0x%llx)\n",
            svm_process->process_id.hostpid, va, start_va, end_va);
    } else {
        devmm_drv_err_if((svm_process->device_fault_printf != 0),
            "Memory before check va is not alloced or not svm addr. (hostpid=%d; check_va=0x%llx; "
            "is_svm_addr=%u)\n", svm_process->process_id.hostpid, va, devmm_va_is_in_svm_range(va));
    }
    return;
}

int devmm_check_alloced_va(struct devmm_svm_process *svm_process, u64 va, u64 *start_va, u64 *end_va, u32 direction)
{
    u64 max_alloc_num, pfn, i, j, page_size, fst_alloced, end_alloced, page_cnt;
    struct devmm_svm_heap *heap = NULL;
    u32 *page_bitmap = NULL;
    int ret;

    heap = devmm_svm_get_heap(svm_process, va);
    ret = (heap == NULL) || (heap->heap_type == DEVMM_HEAP_IDLE) || (heap->page_bitmap == NULL);
    if (ret != 0) {
        return -EADDRNOTAVAIL;
    }

    page_size = (u64)heap->chunk_page_size;
    pfn = (va - heap->start) / page_size;
    page_bitmap = heap->page_bitmap;
    page_cnt = heap->heap_size / page_size;
    /* find first alloc page */
    i = pfn + 1;
    fst_alloced = 0;
    end_alloced = 0;
    /* direction: DEVMM_PRE_ALLOCED_FLAG -->pre_alloced, DEVMM_POST_ALLOCED_FLAG -->post_alloced */
    while (((direction != 0) ? (i > 0) : (i < page_cnt)) != 0) {
        if (devmm_page_bitmap_is_page_alloced(&page_bitmap[i - 1])) {
            if (end_alloced == 0) {
                end_alloced = i - 1;
            }
            if (devmm_page_bitmap_is_first_page(&page_bitmap[i - 1])) {
                fst_alloced = i - 1;
                break;
            }
        }
        (direction != 0) ? i-- : i++;
    }
    if (((direction != 0) ? (i == 0) : (i == page_cnt)) != 0) {
        return -EINVAL;
    }

    /* find last alloc page */
    ret = (end_alloced == pfn) || (end_alloced == 0);
    if (ret != 0) {
        max_alloc_num = devmm_get_page_num_by_pfn(heap, fst_alloced);
        for (j = pfn + 1; j < max_alloc_num; j++) {
            if (devmm_page_bitmap_is_page_available(&page_bitmap[j])) {
                continue;
            }
            end_alloced = j - 1;
            break;
        }
    }
    *start_va = fst_alloced * page_size + heap->start;
    *end_va = (end_alloced + 1) * page_size + heap->start - 1;

    return 0;
}

int devmm_page_fault_get_va_ref(struct devmm_svm_process *svm_proc, u64 va)
{
    struct devmm_heap_ref *ref = NULL;

    ref = devmm_find_first_page_ref(svm_proc, va, 0);
    if (ref == NULL) {
        return -EINVAL;
    }
    return devmm_set_page_ref_advise(ref);
}

void devmm_page_fault_put_va_ref(struct devmm_svm_process *svm_proc, u64 va)
{
    struct devmm_heap_ref *ref = NULL;

    ref = devmm_find_first_page_ref(svm_proc, va, 0);
    if (ref == NULL) {
        return;
    }
    devmm_clear_page_ref_advise(ref);
}

#ifndef EMU_ST
static int devmm_dev_page_fault_check_va_bitmap(struct devmm_svm_process *svm_proc, u32 *page_bitmap, u64 va)
{
    /* if this memory alloc by ipc open meaning is free by destory */
    int ret = ((devmm_page_bitmap_is_page_available(page_bitmap) == 0) ||
        devmm_page_bitmap_is_locked_host(page_bitmap) ||
        devmm_page_bitmap_is_ipc_open_mem(page_bitmap) ||
        devmm_page_bitmap_is_remote_mapped(page_bitmap));
    return (ret != 0) ? -EINVAL : 0;
}
#endif

int devmm_dev_page_fault_get_vaflgs(struct devmm_svm_process *svm_process, struct devmm_svm_heap *heap,
    struct devmm_chan_page_query *flg_msg)
{
    struct devmm_svm_process_id *process_id = &flg_msg->head.process_id;
    u32 logic_id = flg_msg->head.logical_devid;
    u32 dev_id = flg_msg->head.dev_id;
    unsigned long va = flg_msg->va;
    u32 *bitmap = &flg_msg->bitmap;
    u32 pfn_num, phyid_from_bitmap;
    u32 *page_bitmap = NULL;

    page_bitmap = devmm_get_page_bitmap_with_heap(heap, va);
    if (page_bitmap == NULL) {
        devmm_drv_err_if((svm_process->device_fault_printf != 0),
            "Va don't allow fault by device, bitmap is null. (hostpid=%d; va=0x%lx; devid=%u)\n",
            process_id->hostpid, va, dev_id);
        return -EINVAL;
    }
#ifndef EMU_ST
    if (devmm_dev_page_fault_check_va_bitmap(svm_process, page_bitmap, va) != 0) {
        devmm_drv_err_if((svm_process->device_fault_printf != 0),
            "Va error don't allow fault by device. (hostpid=%d; va=0x%lx; devid=%u; bitmap=0x%x)\n",
            process_id->hostpid, va, dev_id, devmm_page_read_bitmap(page_bitmap));
        devmm_print_pre_alloced_va(svm_process, va);
        return -EINVAL;
    }
#endif
    *bitmap = devmm_page_read_bitmap(page_bitmap);
    *bitmap &= ~(DEVMM_PAGE_NOSYNC_FLG);
    if (devmm_page_bitmap_is_host_mapped(page_bitmap) == 0) {
#ifndef DRV_UT
        pfn_num =(u32)(((heap->start + heap->heap_size - 1) - va) / heap->chunk_page_size + 1);
        flg_msg->shr_page_num = devmm_get_shared_page_num(page_bitmap, pfn_num, logic_id);
        if (flg_msg->shr_page_num == 0) {
            if (devmm_page_bitmap_is_dev_mapped(page_bitmap)) {
                devmm_drv_err_if((svm_process->device_fault_printf != 0),
                    "Address is mapped and not p2p pa. (hostpid=%d; va=0x%lx; devid=%d; bitmap=0x%x)\n",
                    process_id->hostpid, va, logic_id, devmm_page_read_bitmap(page_bitmap));
                return -EINVAL;
            }
            if (heap->heap_sub_type == SUB_RESERVE_TYPE) {
                devmm_drv_err_if((svm_process->device_fault_printf != 0),
                    "Reserve addr is invalid, should call halMemMap. (hostpid=%d; va=0x%lx; devid=%d; bitmap=0x%x)\n",
                    process_id->hostpid, va, logic_id, devmm_page_read_bitmap(page_bitmap));
                return -EINVAL;
            }

            devmm_svm_set_bitmap_mapped(page_bitmap, heap->chunk_page_size, heap->chunk_page_size, logic_id);
            *bitmap |= DEVMM_PAGE_NOSYNC_FLG;
            /* page_bitmap dev id bits did not set when bitmap assign */
            devmm_dev_fault_flag_set(bitmap, DEVMM_PAGE_DEVID_SHIT, DEVMM_PAGE_DEVID_WID, dev_id);
        } else {
            /* p2p process: phyid from bitmap need to send to device */
            phyid_from_bitmap = devmm_page_bitmap_get_phy_devid(svm_process, page_bitmap);
            devmm_page_bitmap_set_devid(bitmap, phyid_from_bitmap);
        }
#endif
        devmm_drv_debug("Need not synchronize host pagetable. "
            "(hostpid=%d; va=0x%lx; devid=%u; *bitmap=0x%x; *page_bitmap=0x%x; shr_page_num=%d\n",
            process_id->hostpid, va, dev_id, *bitmap, *page_bitmap, flg_msg->shr_page_num);
    } else {
        /*  page_bitmap dev id bits did not set,  will seted at data copy time */
        devmm_dev_fault_flag_set(bitmap, DEVMM_PAGE_DEVID_SHIT, DEVMM_PAGE_DEVID_WID, dev_id);
    }

    return 0;
}

STATIC u32 devmm_setup_device_get_heap_info(struct devmm_svm_process *svm_pro,
    struct devmm_chan_setup_device *chan_setup, u32 idx_start)
{
    u32 i;

    chan_setup->heap_cnt = 0;
    for (i = idx_start; i < svm_pro->max_heap_use; i++) {
        struct devmm_svm_heap *heap = devmm_get_heap_by_idx(svm_pro, i);
        if (devmm_check_heap_is_entity(heap) == true) {
            chan_setup->heap_info[chan_setup->heap_cnt].heap_idx = heap->heap_idx;
            chan_setup->heap_info[chan_setup->heap_cnt].heap_type = heap->heap_type;
            chan_setup->heap_info[chan_setup->heap_cnt].heap_sub_type = heap->heap_sub_type;
            chan_setup->heap_info[chan_setup->heap_cnt].heap_size = heap->heap_size;

            devmm_drv_debug("Update heap info. (heap_type=0x%x; heap_idx=%u; heap_size=%llu; i=%u)\n",
                heap->heap_type, heap->heap_idx, heap->heap_size, i);
            chan_setup->heap_cnt++;
            if (chan_setup->heap_cnt >= DEVMM_CHAN_MAX_HEAP_INFO_NUM) {
                break;
            }
            i = (u32)(heap->heap_size / DEVMM_HEAP_SIZE - 1 + i);
        }
    }
    chan_setup->head.extend_num = (u16)chan_setup->heap_cnt;
    return i;
}

STATIC int devmm_setup_update_other_device_heap(struct devmm_svm_process *svm_pro,
    struct devmm_chan_setup_device *chan_setup, u32 devid, u32 updated_idx, u32 heap_cnt)
{
    u32 updated_heap_cnt = chan_setup->heap_cnt;
    u32 index = updated_idx;
    int ret;

    while (updated_heap_cnt < heap_cnt) {
        chan_setup->head.dev_id = (u16)devid;
        chan_setup->cmd = DEVMM_POLLING_CMD_UPDATE_HEAP;
        index = devmm_setup_device_get_heap_info(svm_pro, chan_setup, index + 1);
        updated_heap_cnt += chan_setup->heap_cnt;
        ret = devmm_chan_msg_send(chan_setup, sizeof(struct devmm_chan_setup_device) +
            chan_setup->heap_cnt * sizeof(struct devmm_chan_heap_info), sizeof(struct devmm_chan_setup_device));
        if (ret != 0) {
            devmm_drv_err("Update heap error. (devid=%u; ret=0x%x; updated_idx=%u; updated_heap_cnt=%u)\n",
                devid, ret, index, updated_heap_cnt);
            return ret;
        }
    }
    return 0;
}

STATIC int devmm_setup_device_msg_send(struct devmm_svm_process *svm_pro, struct devmm_ioctl_arg *arg,
    struct devmm_chan_setup_device *chan_setup)
{
    u32 logical_devid = arg->head.logical_devid;
    u32 devid = arg->head.devid;
    int ret;

    chan_setup->cmd = DEVMM_POLLING_CMD_CREATE;
    chan_setup->head.dev_id = (u16)devid;
    chan_setup->logic_devid = logical_devid;

    ret = devmm_chan_msg_send(chan_setup, sizeof(struct devmm_chan_setup_device) +
        chan_setup->heap_cnt * sizeof(struct devmm_chan_heap_info), sizeof(struct devmm_chan_setup_device));
    if ((ret != 0) || (chan_setup->devpid < 0)) {
        devmm_drv_err("Setup device error. (devid=%u; ret=%d; devpid=%d)\n", devid, ret, chan_setup->devpid);
        return -EFAULT;
    }

    return 0;
}

static int devmm_setup_device_proc(struct devmm_svm_process *svm_pro, struct devmm_ioctl_arg *arg)
{
    struct svm_id_inst id_inst = {.devid = arg->head.devid, arg->head.vfid};
    struct devmm_chan_setup_device *chan_setup = NULL;
    u32 i, updated_idx, updated_heap_cnt, heap_cnt;
    struct devmm_task_dev_res_node *node = NULL;
    u32 logical_devid = arg->head.logical_devid;
    u32 phy_devid =  arg->head.devid;
    int ret;

    if (svm_pro->deviceinfo[logical_devid].devpid != DEVMM_SETUP_INVAL_PID) {
        return 0;
    }

    /* Collect the PID information of the device and bind it to the physical id of the device. */
    if (logical_devid < DEVMM_MAX_PHY_DEVICE_NUM) {
        ret = devmm_add_pid_into_business(phy_devid, svm_pro->process_id.hostpid);
        if (ret != 0) {
            devmm_drv_err("Add pid to business info failed. (devid=%u; pid=%d)\n",
                          phy_devid, ka_task_get_current_pid());
            return ret;
        }
    }

    for (i = 0, heap_cnt = 0; i < svm_pro->max_heap_use; i++) {
        struct devmm_svm_heap *heap = devmm_get_heap_by_idx(svm_pro, i);
        if (devmm_check_heap_is_entity(heap) == true) {
            i = (u16)(heap->heap_size / DEVMM_HEAP_SIZE - 1 + i);
            heap_cnt++;
        }
    }
    chan_setup = (struct devmm_chan_setup_device *)devmm_kzalloc_ex(sizeof(struct devmm_chan_setup_device) +
        (ka_base_min(heap_cnt, (u32)DEVMM_CHAN_MAX_HEAP_INFO_NUM)) * sizeof(struct devmm_chan_heap_info), KA_GFP_KERNEL);
    if (chan_setup == NULL) {
        devmm_drv_err("Kzalloc chan_setup failed. (logical_devid=%u)\n", logical_devid);
        ret = -ENOMEM;
        goto remove_pid_from_business;
    }

    updated_idx = devmm_setup_device_get_heap_info(svm_pro, chan_setup, 0);
    updated_heap_cnt = chan_setup->heap_cnt;
    chan_setup->head.process_id.hostpid = svm_pro->process_id.hostpid;
    chan_setup->head.process_id.vfid = (u16)arg->head.vfid;
    chan_setup->head.msg_id = DEVMM_CHAN_SETUP_DEVICE_H2D;
    ret = devmm_setup_device_msg_send(svm_pro, arg, chan_setup);
    if (ret != 0) {
        devmm_drv_err("Setup device failed. (logical_devid=%u)\n", logical_devid);
        goto setup_device_free_chan_setup_ptr;
    }

    svm_pro->deviceinfo[logical_devid].devpid = chan_setup->devpid;
    svm_pro->deviceinfo[logical_devid].ssid = chan_setup->ssid;

    chan_setup->heap_cnt = updated_heap_cnt;
    ret = devmm_setup_update_other_device_heap(svm_pro, chan_setup, phy_devid, updated_idx, heap_cnt);
    if (ret != 0) {
        devmm_drv_err("Update device heap failed. (ret=%d; updated_idx=%d; heap_cnt=%d)\n", ret, updated_idx, heap_cnt);
        goto setup_device_free_chan_setup_ptr;
    }

    if (devmm_current_is_vdev() == false) {
        node = devmm_task_dev_res_node_create(svm_pro, &id_inst);
        if (node == NULL) {
            devmm_drv_err("Create task_dev_res_node failed. (devid=%u; vfid=%u)\n", id_inst.devid, id_inst.vfid);
#ifndef EMU_ST
            ret = -ENXIO;
            goto setup_device_free_chan_setup_ptr;
#endif
        }
    }

    devmm_proc_dev_set_async_allow(svm_pro, phy_devid, true);
    devmm_drv_run_info("Setup device succeeded. (logical_devid=%d; devid=%d; vfid=%u; hostpid=%d; devpid=%d)\n",
        logical_devid, phy_devid, arg->head.vfid, svm_pro->process_id.hostpid, chan_setup->devpid);

    devmm_kfree_ex(chan_setup);

    return 0;

setup_device_free_chan_setup_ptr:
#ifndef EMU_ST
    devmm_kfree_ex(chan_setup);
#endif
remove_pid_from_business:
#ifndef EMU_ST
    if (logical_devid < DEVMM_MAX_PHY_DEVICE_NUM) {
        devmm_remove_pid_from_business(phy_devid, svm_pro->process_id.hostpid);
    }
#endif
    return ret;
}

static void devmm_setup_dev_info(struct devmm_svm_process *svm_proc, u32 dev_id, struct devmm_setup_dev_para *dev_para)
{
    if (dev_id >= DEVMM_MAX_DEVICE_NUM) {
        return;
    }
    dev_para->dvpp_mem_size = devmm_dev_capability_dvpp_mem_size(dev_id);
    dev_para->support_bar_mem = devmm_dev_capability_support_bar_mem(dev_id);
    dev_para->support_dev_read_only = devmm_dev_capability_support_read_only(dev_id);
    dev_para->support_dev_mem_map_host = devmm_dev_capability_support_dev_mem_map_host(dev_id);
    dev_para->support_bar_huge_mem = devmm_dev_capability_support_bar_huge_mem(dev_id);
    dev_para->host_support_pin_user_pages_interface = true;
    dev_para->support_host_rw_dev_ro = devmm_dev_capability_support_host_rw_dev_ro(dev_id);
    dev_para->double_pgtable_offset = devmm_dev_capability_double_pgtable_offset(dev_id);
    dev_para->support_host_pin_pre_register = devmm_dev_capability_support_host_pin_pre_register(dev_id);
    dev_para->support_host_mem_pool = devmm_dev_capability_support_host_mem_pool(dev_id);
    dev_para->support_agent_giant_page = devmm_dev_capability_support_giant_page(dev_id);
    dev_para->support_remote_mmap = devmm_dev_capability_support_remote_mmap(dev_id);
    dev_para->support_shmem_map_exbus = devmm_dev_capability_support_shmem_map_exbus(dev_id);
    dev_para->support_mem_host_uva = devmm_dev_capability_support_mem_host_uva(dev_id);
}

STATIC int devmm_ioctl_setup_device(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_svm_proc_master *master_data = (struct devmm_svm_proc_master *)svm_proc->priv_data;
    int ret;

    devmm_setup_dev_info(svm_proc, arg->head.devid, &arg->data.setup_dev_para);

    ka_task_down(&master_data->dev_setup_sem[arg->head.logical_devid]);
    ret = devmm_setup_device_proc(svm_proc, arg);
    ka_task_up(&master_data->dev_setup_sem[arg->head.logical_devid]);
    if (ret == 0) {
        devmm_mem_stats_va_map(svm_proc, arg->head.logical_devid, arg->data.setup_dev_para.mem_stats_va);
        devmm_dev_proc_fs_create(arg->head.logical_devid);
    }

    return ret;
}

static int devmm_master_alloc_continuous_pages(struct devmm_svm_process *svm_proc, struct devmm_phy_addr_attr *hp_attr,
    ka_page_t *hpages[], u64 hpage_num)
{
    int ret;
    u64 i;
    struct devmm_phy_addr_attr attr = *hp_attr;
    ka_page_t **pages = NULL;
    u64 page_num_per_hpage = DEVMM_HUGE_PAGE_SIZE / KA_MM_PAGE_SIZE;
    u64 page_num = hpage_num * page_num_per_hpage;

    pages = devmm_kvzalloc_ex(sizeof(ka_page_t *) * page_num, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (pages == NULL) {
        devmm_drv_err("Kvzalloc failed. (pg_num=%llu)\n", page_num);
        return -ENOMEM;
    }

    attr.is_continuous = true;
    attr.pg_type = MEM_NORMAL_PAGE_TYPE;
    for (i = 0; i < hpage_num; i++) {
        ret = devmm_proc_alloc_pages(svm_proc, &attr, pages + (i * page_num_per_hpage), page_num_per_hpage);
        if (ret != 0) {
#ifndef EMU_ST
            if (i > 0) {
                devmm_proc_free_pages(svm_proc, &attr, pages, i * page_num_per_hpage);
            }
            devmm_kvfree_ex(pages);
#endif
            return ret;
        }
        hpages[i] = pages[i * page_num_per_hpage];
    }

    devmm_kvfree_ex(pages);
    return 0;
}

static int devmm_master_free_continuous_pages(struct devmm_svm_process *svm_proc, struct devmm_phy_addr_attr *hp_attr,
    ka_page_t *hpages[], u64 hpage_num)
{
#ifndef EMU_ST
    u64 i, j;
    struct devmm_phy_addr_attr attr = *hp_attr;
    ka_page_t **pages = NULL;
    u64 page_num_per_hpage = DEVMM_HUGE_PAGE_SIZE / KA_MM_PAGE_SIZE;
    u64 page_num = hpage_num * page_num_per_hpage;

    pages = devmm_kvzalloc_ex(sizeof(ka_page_t *) * page_num, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (pages == NULL) {
        devmm_drv_err("Kvzalloc failed. (pg_num=%llu)\n", page_num);
        return -ENOMEM;
    }

    attr.pg_type = MEM_NORMAL_PAGE_TYPE;
    for (i = 0; i < hpage_num; i++) {
        for (j = 0; j < page_num_per_hpage; j++) {
            pages[i * page_num_per_hpage + j] = hpages[i] + j;
        }
    }

    devmm_proc_free_pages(svm_proc, &attr, pages, page_num);
    devmm_kvfree_ex(pages);
#endif
    return 0;
}

static int devmm_alloc_with_page_type(struct devmm_svm_process *svm_proc, struct devmm_phy_addr_attr *attr,
    ka_page_t *pages[], u64 page_num)
{
    int ret;
    if (attr->side == DEVMM_SIDE_MASTER && attr->pg_type == MEM_HUGE_PAGE_TYPE) {
        ret = devmm_master_alloc_continuous_pages(svm_proc, attr, pages, page_num);
    } else {
        ret = devmm_proc_alloc_pages(svm_proc, attr, pages, page_num);
    }
    return ret;
}

static void devmm_free_with_page_type(struct devmm_svm_process *svm_proc, struct devmm_phy_addr_attr *attr,
    ka_page_t *pages[], u64 page_num)
{
#ifndef EMU_ST
    if (attr->side == DEVMM_SIDE_MASTER && attr->pg_type == MEM_HUGE_PAGE_TYPE) {
        devmm_master_free_continuous_pages(svm_proc, attr, pages, page_num);
    } else {
        devmm_proc_free_pages(svm_proc, attr, pages, page_num);
    }
#endif
}

int devmm_alloc_host_range(struct devmm_svm_process *svm_proc, u64 va, u64 page_num, enum devmm_page_type page_type)
{
    struct devmm_phy_addr_attr attr = {0};
    ka_page_t **pages = NULL;
    u32 page_size = page_type == DEVMM_HUGE_PAGE_TYPE ? DEVMM_HUGE_PAGE_SIZE : KA_MM_PAGE_SIZE;
    u64 vaddr = ka_base_round_down(va, page_size);
    int ret;

    pages = devmm_kvzalloc_ex(sizeof(ka_page_t *) * page_num, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (pages == NULL) {
        devmm_drv_err("Kvzalloc failed. (pg_num=%llu)\n", page_num);
        return -ENOMEM;
    }

    devmm_phy_addr_attr_pack(svm_proc, page_type, 0, false, &attr);
    ret = devmm_alloc_with_page_type(svm_proc, &attr, pages, page_num);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_run_info("Can not alloc pages. (ret=%d; pg_num=%llu)\n", ret, page_num);
#endif
        devmm_kvfree_ex(pages);
        return ret;
    }

    ret = devmm_remap_pages(svm_proc, vaddr, pages, page_num, page_type);
    if (ret != 0) {
        devmm_drv_err("Remap pages failed. (ret=%d; va=0x%llx; pg_num=%llu)\n", ret, vaddr, page_num);
        devmm_free_with_page_type(svm_proc, &attr, pages, page_num);
    }

    devmm_kvfree_ex(pages);
    return ret;
}

STATIC int devmm_ioctl_alloc(struct devmm_svm_process *svm_process, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_alloc_para *alloc_para = &arg->data.alloc_svm_para;
    struct devmm_svm_heap *heap = NULL;
    u32 *page_bitmap = NULL;
    u64 chunk_cnt, i, j;
    u32 stamp = (u32)ka_jiffies;
    int ret;

    heap = devmm_svm_get_heap(svm_process, (unsigned long)alloc_para->p);
    if ((heap == NULL) || (devmm_check_va_add_size_by_heap(heap, alloc_para->p, alloc_para->size) != 0)) {
        devmm_drv_err("Can't find heap. (va=0x%llx; size=%lu)\n", alloc_para->p, alloc_para->size);
        return -EADDRNOTAVAIL;
    }

    page_bitmap = devmm_get_page_bitmap_with_heap(heap, alloc_para->p);
    if (page_bitmap == NULL) {
        devmm_drv_err("Can't find page_bitmap. (va=0x%llx; size=%lu)\n", alloc_para->p, alloc_para->size);
        return -EINVAL;
    }

    chunk_cnt = devmm_get_pagecount_by_size(alloc_para->p, alloc_para->size, heap->chunk_page_size);
    if (chunk_cnt == 0) {
        devmm_drv_err("Count error. (va=0x%llx; size=%lu)\n", alloc_para->p, alloc_para->size);
        return -EINVAL;
    }

    for (i = 0; i < chunk_cnt; i++) {
        if (devmm_page_bitmap_check_and_set_flag(page_bitmap + i, DEVMM_PAGE_ALLOCED_MASK) != 0) {
            devmm_drv_err("Already alloced i. (i=%llu; va=0x%llx; size=%lu)\n",
                          i, alloc_para->p, alloc_para->size);
            goto alloc_fail_handle;
        }
        devmm_try_cond_resched(&stamp);
    }
    devmm_page_bitmap_set_flag(page_bitmap, DEVMM_PAGE_IS_FIRST_PAGE_MASK);
    ret = devmm_set_page_ref(heap, alloc_para->p, chunk_cnt);
    if (ret != 0) {
        devmm_drv_err("Set page failed! (ret=%d)\n", ret);
        goto alloc_fail_handle;
    }

    return 0;

alloc_fail_handle:
    for (j = 0; j < i; j++) {
        devmm_page_clean_bitmap(page_bitmap + j);
        devmm_try_cond_resched(&stamp);
    }

    return -EINVAL;
}

#define SHARE_POOL_ADDR_START 0xe00000000000ULL
#define SHARE_POOL_ADDR_END   0xf00000000000ULL
#ifndef EMU_ST
static bool devmm_is_in_share_pool_range(u64 addr, u64 size)
{
    if ((addr < SHARE_POOL_ADDR_START) || (addr >= SHARE_POOL_ADDR_END) ||
        (size > SHARE_POOL_ADDR_END - addr)) {
        return false;
    }
    return true;
}

static bool devmm_is_in_dcache_range(u64 addr, u64 size)
{
    if ((addr < DEVMM_DCACHE_ADDR_START) || (addr >= (DEVMM_DCACHE_ADDR_START + DEVMM_DCACHE_OFFSET)) ||
         (size > DEVMM_DCACHE_ADDR_START + DEVMM_DCACHE_OFFSET - addr)) {
        return false;
    }
    return true;
}
#endif

int devmm_get_local_dev_mem_attrs(struct devmm_svm_process *svm_proc, u64 addr, u64 size, u32 logical_devid,
    struct devmm_memory_attributes *attr)
{
#ifndef EMU_ST
    u32 devid;

    if (logical_devid >= SVM_MAX_AGENT_NUM) {
        devmm_drv_err("Invalid logical_devid. (logical_devid=%u)\n", logical_devid);
        return -EINVAL;
    }

    if ((!devmm_is_in_share_pool_range(addr, size)) && (!devmm_is_in_dcache_range(addr, size))) {
        devmm_drv_run_info("Dev local addr copy support sharepool addr or dcache addr. (addr=0x%llx)\n", addr);
        return -EOPNOTSUPP;
    }

    devid = svm_proc->phy_devid[logical_devid];
    if (devid >= SVM_MAX_AGENT_NUM) {
        devmm_drv_err("Invalid devid. (logical_devid=%u; devid=%u)\n", logical_devid, devid);
        return -EINVAL;
    }

    *attr = (struct devmm_memory_attributes){0};

    if (devmm_dev_capability_support_pcie_dma_sva(devid) == false) {
        devmm_drv_run_info("Not support cpy local dev addr. (devid=%u)\n", devid);
        return -EOPNOTSUPP;
    }

    attr->is_local_device = true;

    attr->logical_devid = logical_devid;
    attr->devid = devid;
    attr->vfid = svm_proc->vfid[logical_devid];

    attr->va = addr;
    attr->ssid = svm_proc->deviceinfo[logical_devid].ssid;
    attr->copy_use_va = devmm_dev_capability_support_pcie_dma_sva(attr->devid);
    attr->page_size = devmm_svm->device_page_size;

    attr->host_page_size = KA_MM_PAGE_SIZE;
    attr->granularity_size = KA_MM_PAGE_SIZE;
#endif
    return 0;
}

void devmm_get_local_host_mem_attrs(struct devmm_svm_process *svm_proc, u64 addr,
    struct devmm_memory_attributes *attr)
{
    *attr = (struct devmm_memory_attributes){0};

    attr->is_local_host = true;

    attr->va = addr;
    attr->page_size = KA_MM_PAGE_SIZE;

    attr->host_page_size = KA_MM_PAGE_SIZE;
    attr->granularity_size = KA_MM_PAGE_SIZE;
}

static int devmm_get_reserve_mem_attr(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap, u64 addr,
    struct devmm_memory_attributes *attr)
{
    struct devmm_share_id_map_node *map_node = NULL;
    struct devmm_vmma_struct *vmma = NULL;

    vmma = devmm_vmma_get(&heap->vmma_mng, addr);
    if (vmma == NULL) {
        devmm_drv_err("Reserve addr hasn't been mapped. (addr=0x%llx)\n", addr);
        return -EADDRNOTAVAIL;
    }
    map_node = devmm_share_id_map_node_get(svm_proc, vmma->info.devid, vmma->info.phy_addr_blk_id);
    if (map_node != NULL) {
        if (map_node->blk_type == SVM_PYH_ADDR_BLK_EXPORT_TYPE) {
            attr->is_mem_export = true;
        } else {
            attr->is_mem_import = true;
        }
        attr->mem_share_devid = map_node->shid_map_node_info.share_devid;
        attr->mem_share_id = map_node->shid_map_node_info.share_id;
        devmm_share_id_map_node_put(map_node);
    }
    attr->is_reserve_addr = true;
    attr->page_size = vmma->info.pg_size;
    devmm_vmma_put(vmma);
    return 0;
}

static int _devmm_get_svm_mem_attrs(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap,
    u32 *bitmap, u64 addr, struct devmm_memory_attributes *attr)
{
    *attr = (struct devmm_memory_attributes){0};

    if (heap->heap_sub_type == SUB_RESERVE_TYPE) {
        int ret = devmm_get_reserve_mem_attr(svm_proc, heap, addr, attr);
        if (ret != 0) {
            return ret;
        }
    } else {
        attr->page_size = heap->chunk_page_size;
    }

    attr->is_svm = true;
    attr->va = addr;
    attr->host_page_size = KA_MM_PAGE_SIZE;
    attr->heap_size = heap->heap_size;
    attr->granularity_size = heap->chunk_page_size;

    attr->bitmap = devmm_page_read_bitmap(bitmap);
    attr->is_svm_huge = (heap->heap_type == DEVMM_HEAP_HUGE_PAGE) ? true : false;
    attr->is_locked_host = devmm_page_bitmap_is_locked_host(bitmap);
    attr->is_locked_device = devmm_page_bitmap_is_locked_device(bitmap);
    attr->is_ipc_open = devmm_page_bitmap_is_ipc_open_mem(bitmap);
    attr->is_svm_host = devmm_page_bitmap_is_host_mapped(bitmap);
    attr->is_svm_device = devmm_page_bitmap_is_dev_mapped(bitmap);
    attr->is_svm_non_page = (!attr->is_svm_host && !attr->is_svm_device);
    attr->is_svm_remote_maped = devmm_page_bitmap_is_remote_mapped(bitmap);
    attr->is_svm_continuty = devmm_page_bitmap_is_advise_continuty(bitmap);
    attr->is_svm_readonly = devmm_page_bitmap_is_advise_readonly(bitmap);
    attr->is_svm_dev_readonly = (heap->heap_sub_type == SUB_DEV_READ_ONLY_TYPE) ? true : false;

    if ((attr->is_svm_device) || (attr->is_svm_remote_maped)) {
        devmm_get_svm_id(svm_proc, bitmap, &attr->logical_devid, &attr->devid, &attr->vfid);
        attr->ssid = svm_proc->deviceinfo[attr->logical_devid].ssid;
        attr->is_svm_host_agent = devmm_is_host_agent(attr->logical_devid) ? true : false;
    }

    /* If devs are interconnected through PCIe, use sva copy import mem will cause PCIe linkdown. */
    if (devmm_dev_capability_support_pcie_dma_sva(attr->devid) && (!devmm_is_host_agent(attr->devid) && attr->is_svm_device &&
        (attr->is_mem_import == false) && (attr->is_svm_dev_readonly == false))) {
        attr->copy_use_va = true;
    }

    return 0;
}

int devmm_get_svm_mem_attrs(struct devmm_svm_process *svm_proc, u64 addr, struct devmm_memory_attributes *attr)
{
    struct devmm_svm_heap *heap = NULL;
    u32 *bitmap = NULL;

    *attr = (struct devmm_memory_attributes){0};

    heap = devmm_svm_get_heap(svm_proc, addr);
    if (heap == NULL) {
        devmm_drv_err("Heap is NULL. (addr=0x%llx)\n", addr);
        return -EADDRNOTAVAIL;
    }

    bitmap = devmm_get_page_bitmap_with_heap(heap, addr);
    if (bitmap == NULL) {
        devmm_drv_err("Get bitmap failed. (addr=0x%llx)\n", addr);
        return -EINVAL;
    }

    return _devmm_get_svm_mem_attrs(svm_proc, heap, bitmap, addr, attr);
}

int devmm_get_memory_attributes(struct devmm_svm_process *svm_proc, u64 addr, struct devmm_memory_attributes *attr)
{
    if (devmm_va_is_not_svm_process_addr(svm_proc, addr)) {
        devmm_get_local_host_mem_attrs(svm_proc, addr, attr);
        return 0;
    } else {
        return devmm_get_svm_mem_attrs(svm_proc, addr, attr);
    }
}

bool devmm_acquire_aligned_addr_and_cnt(u64 address, u64 byte_count, int is_svm_huge,
    u64 *aligned_down_addr, u64 *aligned_count)
{
    u64 aligned_up_addr;

    if (((address + byte_count) < address) || ((address + byte_count) < byte_count)) {
        return false;
    }

    if (is_svm_huge != 0) {
        *aligned_down_addr = ka_base_round_down(address, devmm_svm->device_hpage_size);
        aligned_up_addr = ka_base_round_up(address + byte_count, devmm_svm->device_hpage_size);
    } else {
        *aligned_down_addr = ka_base_round_down(address, KA_MM_PAGE_SIZE);
        aligned_up_addr = ka_base_round_up(address + byte_count, KA_MM_PAGE_SIZE);
    }

    *aligned_count = aligned_up_addr - *aligned_down_addr;

    devmm_drv_debug("Information about aligned address. "
        "(aligned_down_addr=0x%llx; aligned_up_addr=0x%llx; aligned_count=%llu)\n",
        *aligned_down_addr, aligned_up_addr, *aligned_count);

    return true;
}

int devmm_insert_host_page_range(struct devmm_svm_process *svm_pro, u64 dst,
    u64 byte_count, struct devmm_memory_attributes *fst_attr)
{
    u64 aligned_addr, mapped_addr, aligned_count, page_cnt, i;
    u32 stamp = (u32)ka_jiffies;
    u32 *bitmap = NULL;
    u32 adjust_order;
    int ret;

    if (devmm_svm_mem_is_enable(svm_pro) == false) {
        devmm_drv_err("Host mmap failed, can't insert host page.\n");
        return -EINVAL;
    }

    if (!devmm_acquire_aligned_addr_and_cnt(dst, byte_count, fst_attr->is_svm_huge, &aligned_addr,
                                       &aligned_count)) {
        devmm_drv_err("Acquire aligned addr and cnt failed. (dev_ptr=0x%llx; byte_count=%llx\n", dst, byte_count);
        return -EINVAL;
    }

    adjust_order = fst_attr->is_svm_huge ? devmm_host_hugepage_fault_adjust_order() : 0;
    ret = devmm_check_status_va_info(svm_pro, aligned_addr, aligned_count);
    if (ret != 0) {
        devmm_drv_err("Va may out of the size of heap. (ret=%d; aligned_addr=0x%llx; aligned_count=%llu)\n",
            ret, aligned_addr, aligned_count);
        return ret;
    }

    page_cnt = aligned_count / fst_attr->page_size;
    bitmap = devmm_get_page_bitmap(svm_pro, aligned_addr);
    if (bitmap == NULL) {
        devmm_drv_err("Page bitmap is NULL. (aligned_addr=0x%llx; aligned_count=%llu)\n", aligned_addr, aligned_count);
        return -EADDRNOTAVAIL;
    }

    ka_task_down_write(&svm_pro->host_fault_sem);
    for (i = 0; i < page_cnt; i++, bitmap++) {
        ret = (!devmm_page_bitmap_is_page_available(bitmap)) || (devmm_page_bitmap_is_locked_device(bitmap))
            || (devmm_page_bitmap_is_remote_mapped(bitmap));
        if (ret != 0) {
            devmm_drv_err("Address isn't alloced or page_bitmap is locked_device/remote_map. "
                          "(aligned_addr=0x%llx; aligned_count=%llu; i=%llu; bitmap=0x%x)\n ",
                          aligned_addr, aligned_count, i, *bitmap);
            devmm_print_pre_alloced_va(svm_pro, (aligned_addr + fst_attr->page_size * i));
            ka_task_up_write(&svm_pro->host_fault_sem);
            return -EINVAL;
        }

        if (!devmm_page_bitmap_is_host_mapped(bitmap)) {
            /* insert use host page size, heap adjust order, hugepage at devmm_alloc_pages will try 2M per time */
            mapped_addr = aligned_addr + fst_attr->page_size * i;
            ret = devmm_alloc_host_range(svm_pro, mapped_addr, 1ul << adjust_order, DEVMM_NORMAL_PAGE_TYPE);
            if (ret != 0) {
                devmm_drv_err("Insert host range failed. (addr=0x%llx;size=%u)", mapped_addr, fst_attr->page_size);
                ka_task_up_write(&svm_pro->host_fault_sem);
                return ret;
            }
            devmm_page_bitmap_set_flag(bitmap, DEVMM_PAGE_HOST_MAPPED_MASK);
        }
        devmm_try_cond_resched(&stamp);
    }
    ka_task_up_write(&svm_pro->host_fault_sem);

    return 0;
}

int _devmm_insert_virt_range(struct devmm_svm_process *svm_proc, u32 pg_type, u64 vaddr,
    u64 *paddr, u32 pg_num)
{
    ka_pgprot_t page_prot = devmm_make_remote_pgprot(0);
    u64 i, j, page_size, temp_paddr, tmp_vaddr;
    ka_vm_area_struct_t *vma = NULL;
    u32 stamp = (u32)ka_jiffies;
    int ret;

    if (pg_type != DEVMM_NORMAL_PAGE_TYPE) {
        devmm_drv_err("Invalid pg_type. (pg_type=%u; hostpid=%d; devid=%d; vfid=%d)\n",
            pg_type, svm_proc->process_id.hostpid, svm_proc->process_id.devid, svm_proc->process_id.vfid);
        return -EINVAL;
    }

    vma = devmm_find_vma(svm_proc, vaddr);
    if (vma == NULL) {
        devmm_drv_err("Not find vma. (va=0x%llx; hostpid=%d; devid=%d; vfid=%d)\n",
            vaddr, svm_proc->process_id.hostpid, svm_proc->process_id.devid, svm_proc->process_id.vfid);
        return -EINVAL;
    }

    page_size = KA_MM_PAGE_SIZE;
    for (i = 0; i < pg_num; i++) {
        tmp_vaddr = vaddr + i * page_size;
        ret = devmm_va_to_pa(vma, (u64)tmp_vaddr, &temp_paddr);
        if (ret == 0) {
            goto clear_pfn_range;
        }

       ret = remap_pfn_range(vma, tmp_vaddr, KA_MM_PFN_DOWN(paddr[i]), page_size, page_prot);
       if (ret != 0) {
           devmm_drv_err("remap_pfn_range failed. (va=0x%llx; i=%llu; hostpid=%d; devid=%d; vfid=%d)\n",
               tmp_vaddr, i, svm_proc->process_id.hostpid, svm_proc->process_id.devid, svm_proc->process_id.vfid);
           goto clear_pfn_range;
       }

       devmm_try_cond_resched(&stamp);
    }
    return 0;
clear_pfn_range:
#ifndef EMU_ST
    for (j = 0, tmp_vaddr = vaddr; j < i; j++, tmp_vaddr += page_size) {
        devmm_zap_vma_ptes(vma, tmp_vaddr, page_size);
        devmm_try_cond_resched(&stamp);
    }
#endif
    return -EINVAL;
}

bool devmm_is_master(struct devmm_memory_attributes *attr)
{
    return (attr->is_svm_host || attr->is_local_host || attr->is_host_pin || attr->is_svm_host_agent);
}

bool devmm_is_device_agent(struct devmm_memory_attributes *attr)
{
    return ((attr->is_svm_device && (!devmm_is_host_agent(attr->devid))) || attr->is_local_device);
}

STATIC void devmm_set_translate_bitmap(struct devmm_svm_process *svm_pro, u64 va)
{
    struct devmm_svm_heap *heap = NULL;
    u32 *page_bitmap = NULL;

    heap = devmm_svm_get_heap(svm_pro, va);
    if (heap == NULL) {
        devmm_drv_err("Get heap failed. (va=0x%llx)\n", va);
        return;
    }
    page_bitmap = devmm_get_page_bitmap_with_heap(heap, va);
    if (page_bitmap == NULL) {
        devmm_drv_err("Src va error. (va=0x%llx)\n", va);
        return;
    }
    devmm_page_bitmap_set_flag(page_bitmap, DEVMM_PAGE_IS_TRANSLATE_MASK);

    return;
}

STATIC int devmm_fill_trans_info(struct devmm_svm_process *svm_proc, struct devmm_memory_attributes *attr,
    struct devmm_translate_info *trans)
{
    trans->logical_devid = attr->logical_devid;
    trans->dev_id = attr->devid;
    trans->vfid = attr->vfid;
    trans->va = attr->va;
    trans->page_size = attr->page_size;
    trans->page_insert_dev_id = attr->logical_devid;
    trans->is_svm_continuty = attr->is_svm_continuty;
    trans->is_vm_translate = 0;

    if (trans->is_svm_continuty) {
        int ret;
        ret = devmm_get_alloced_va(svm_proc, trans->va, &trans->alloced_va);
        if (ret != 0) {
            devmm_drv_err("Get alloced va failed. (va=0x%llx; ret=%d)\n", trans->va, ret);
            return ret;
        }

        ret = devmm_get_alloced_size(svm_proc, trans->va, &trans->alloced_size);
        if (ret != 0) {
            devmm_drv_err("Get alloced size failed. (va=0x%llx; ret=%d)\n", trans->va, ret);
            return ret;
        }
    }

    return 0;
}

STATIC int devmm_ioctl_translate(struct devmm_svm_process *svm_pro, struct devmm_ioctl_arg *arg)
{
    unsigned long vaddr = (unsigned long)arg->data.translate_para.vptr;
    struct devmm_memory_attributes attr;
    struct devmm_translate_info trans;
    u64 pa_offset;
    int ret;

    ret = devmm_get_memory_attributes(svm_pro, vaddr, &attr);
    if (ret != 0) {
        devmm_drv_err("Devmm_get_memory_attributes failed. (vaddr=0x%lx)\n", vaddr);
        return ret;
    }

    if (attr.is_svm_non_page || !attr.is_svm_device) {
        devmm_drv_err("Vaddr is svm non page or isn't svm device. (vaddr=0x%lx; is_svm_device=%d)\n",
            vaddr, attr.is_svm_device);
        return -EINVAL;
    }

    if (attr.is_svm_readonly) {
        devmm_drv_err("Va_attr is readonly, not allowed translate. (vaddr=0x%lx)\n", vaddr);
        return -EINVAL;
    }

    if (devmm_is_host_agent(attr.devid)) {
        devmm_drv_err("Vaddr attr_devid is host agent, not surport translate. (vaddr=0x%lx; attr_devid=%d)\n",
                      vaddr, attr.devid);
        return -EINVAL;
    }

    ret = devmm_fill_trans_info(svm_pro, &attr, &trans);
    if (ret != 0) {
        devmm_drv_err("Fill trans info failed.\n");
        return ret;
    }

    ret = devmm_set_translate_pa_addr_to_device(svm_pro, trans, &pa_offset);
    if (ret != 0) {
        return ret;
    }

    devmm_set_translate_bitmap(svm_pro, vaddr);
    arg->data.translate_para.pptr = pa_offset;

    devmm_drv_debug("Translate info. (vaddr=0x%lx; offset=%llu)\n", vaddr, pa_offset);

    return 0;
}

STATIC int devmm_device_inited(struct devmm_svm_process *svm_pro, u32 logic_id)
{
    if (logic_id >= SVM_MAX_AGENT_NUM) {
        devmm_drv_err("Logic_id is invalid. (logic_id=%d)\n", logic_id);
        return -1;
    }

    if (svm_pro->deviceinfo[logic_id].devpid > 0) {
        return 0;
    } else {
        return -1;
    }
}

STATIC void devmm_send_free_msg_to_p2p_device(struct devmm_svm_process *svm_pro,
    struct devmm_chan_free_pages *free_info, struct devmm_chan_free_pages free_pgs)
{
    u32 logic_id = free_info->head.logical_devid;
    u32 devid = free_info->head.dev_id;
    u32 phyid, vfid, i;
    int ret;

    for (i = 0; i < SVM_MAX_AGENT_NUM; i++) {
        if (devmm_device_inited(svm_pro, i) != 0) {
            continue;
        }
        phyid = devmm_get_phyid_devid_from_svm_process(svm_pro, i);
        vfid = devmm_get_vfid_from_svm_process(svm_pro, i);
        if (devmm_dev_is_same_system(devid, phyid) &&
            (svm_pro->deviceinfo[logic_id].devpid == svm_pro->deviceinfo[i].devpid)) {
            continue;
        }

        free_pgs.head.process_id.vfid = (u16)vfid;
        free_pgs.head.dev_id = (u16)phyid;
        ret = devmm_chan_msg_send(&free_pgs, sizeof(struct devmm_chan_free_pages), sizeof(struct devmm_chan_msg_head));
        if (ret != 0) {
#ifndef EMU_ST
            devmm_drv_warn("Free share page. (devid=%d; ret=%d; va=0x%llx; real_size=%llu)\n",
                           free_info->head.dev_id, ret, free_info->va, free_info->real_size);
#endif
            continue;
        }
    }
}

int devmm_chan_send_msg_free_pages(struct devmm_chan_free_pages *free_info, struct devmm_svm_heap *heap,
                                   struct devmm_svm_process *svm_proc, int shared_flag, u32 free_self)
{
    struct devmm_chan_free_pages free_pgs = {{{0}}};
    u64 sect_addr, sect_offset;
    long long left_size;
    int ret = 0;

    free_pgs.head.msg_id = DEVMM_CHAN_FREE_PAGES_H2D_ID;
    free_pgs.head.process_id.hostpid = svm_proc->process_id.hostpid;
    sect_offset = (u64)DEVMM_FREE_SECTION_NUM * heap->chunk_page_size;

    devmm_drv_debug("Enter. (dev_id=%u; va=0x%llx; real_size=%llu)\n", free_info->head.dev_id,
        free_info->va, free_info->real_size);
    for (sect_addr = free_info->va, left_size = (long long)free_info->real_size; left_size > 0;) {
        free_pgs.va = sect_addr;
        free_pgs.real_size = ka_base_min((u64)left_size, sect_offset);
        sect_addr += free_pgs.real_size;
        left_size -= (long long)(free_pgs.real_size);

        if (shared_flag != 0) {
            devmm_send_free_msg_to_p2p_device(svm_proc, free_info, free_pgs);
        }
        if (free_self == 0) {
            continue;
        }
        if (devmm_device_inited(svm_proc, free_info->head.logical_devid) != 0) {
            return 0;
        }
        free_pgs.head.dev_id = free_info->head.dev_id;
        free_pgs.head.process_id.vfid = free_info->head.process_id.vfid;
        ret = devmm_chan_msg_send(&free_pgs, sizeof(struct devmm_chan_free_pages), sizeof(struct devmm_chan_msg_head));
        if (ret != 0) {
            devmm_drv_warn("Free page error. (dev_id=%d; ret=%d; va=0x%llx; real_size=%llu)\n",
                           free_info->head.dev_id, ret, free_info->va, free_info->real_size);
            return ret;
        }
    }

    return ret;
}

bool devmm_check_is_translate(struct devmm_svm_process *svm_pro, u64 va, u32 *page_bitmap,
    u32 page_size, u64 page_num)
{
    u32 stamp = (u32)ka_jiffies;
    u32 dev_id, vfid;
    int ret;
    u64 i;

    for (i = 0; i < page_num; i++) {
        if (devmm_page_bitmap_is_translate(page_bitmap + i)) {
            dev_id = devmm_page_bitmap_get_phy_devid(svm_pro, page_bitmap + i);
            vfid = devmm_page_bitmap_get_vfid(svm_pro, page_bitmap + i);
            ret = devmm_clear_translate_pa_addr(dev_id, vfid, va, page_num * page_size,
                (u32)svm_pro->process_id.hostpid);

            return (ret == 0) ? false : true;
        }
        devmm_try_cond_resched(&stamp);
    }

    return false;
}

STATIC int devmm_check_can_free(struct devmm_svm_process *svm_pro, struct devmm_svm_heap *heap, u32 *page_bitmap,
    u64 va, u64 page_num)
{
    if (devmm_check_is_translate(svm_pro, va, page_bitmap, heap->chunk_page_size, page_num)) {
        devmm_drv_err("Va can't free, is translated by rts, must delete task first.\n");
        return -EINVAL;
    }

    return 0;
}

STATIC void devmm_update_free_svmid(u32 free_flag, struct devmm_devid *svmid, struct devmm_devid *last_svmid)
{
    if (free_flag == 1) {
        struct devmm_devid tmp_svmid;
        (void)devmm_fill_svm_id(&tmp_svmid, svmid->logical_devid, svmid->devid, svmid->vfid);
        (void)devmm_fill_svm_id(svmid, last_svmid->logical_devid, last_svmid->devid, last_svmid->vfid);
        (void)devmm_fill_svm_id(last_svmid, tmp_svmid.logical_devid, tmp_svmid.devid, tmp_svmid.vfid);
    } else {
        (void)devmm_fill_svm_id(last_svmid, svmid->logical_devid, svmid->devid, svmid->vfid);
    }
}

STATIC void devmm_fill_free_pages_info(u64 va, struct devmm_devid *svmid, struct devmm_svm_heap *heap,
    struct devmm_free_dev_mem_info *dev_mem_info, struct devmm_chan_free_pages *free_info)
{
    free_info->va = ka_base_round_down(va, heap->chunk_page_size);
    free_info->real_size = max((dev_mem_info->last_devshared_index - dev_mem_info->first_devshared_index + 1),
        (dev_mem_info->last_devmapped_index - dev_mem_info->first_devmmaped_index + 1));
    free_info->real_size *= heap->chunk_page_size;
    free_info->head.dev_id = (u16)svmid->devid;
    free_info->head.logical_devid = (u16)svmid->logical_devid;
    free_info->head.process_id.vfid = (u16)svmid->vfid;
}

STATIC void devmm_update_free_dev_mem_info(struct devmm_free_dev_mem_info *dev_mem_info)
{
    dev_mem_info->first_devmmaped_index = dev_mem_info->last_devmapped_index + 1;
    dev_mem_info->first_devshared_index = dev_mem_info->last_devshared_index + 1;
    dev_mem_info->devmapped_flag = 0;
    dev_mem_info->shared_flag = 0;
    dev_mem_info->free_flag = 0;
}

STATIC bool devmm_is_need_send_free_msg(struct devmm_free_dev_mem_info *dev_mem_info,
    u64 cur_page_num, u64 max_page_num)
{
    return (dev_mem_info->free_flag == 1 || cur_page_num == max_page_num) &&
        ((dev_mem_info->devmapped_flag != 0) || (dev_mem_info->shared_flag != 0));
}

STATIC void devmm_free_page_clear_bitmap(struct devmm_svm_heap *heap, u32 *page_bitmap,
    u64 va, u64 page_num)
{
    struct devmm_heap_ref *ref = NULL;
    u32 stamp = (u32)ka_jiffies;
    u64 j;

    ref = devmm_get_page_ref(heap, va);
    if (ref == NULL) {
        devmm_drv_err("Can't get heap_ref. (va=0x%llx)\n", va);
        return; /* bitmap not null, ref will not null */
    }

    for (j = 0; j < page_num; j++) {
        devmm_page_clean_bitmap(page_bitmap + j);
        devmm_clean_page_ref(ref + j);
        devmm_try_cond_resched(&stamp);
    }
}

STATIC int devmm_free_page_process(struct devmm_svm_process *svm_pro, struct devmm_svm_heap *heap, u32 *page_bitmap,
    u64 va, bool reuse)
{
    struct devmm_devid last_svmid = {SVM_MAX_AGENT_NUM, SVM_MAX_AGENT_NUM, DEVMM_MAX_VF_NUM};
    struct devmm_devid svmid = {SVM_MAX_AGENT_NUM, SVM_MAX_AGENT_NUM, DEVMM_MAX_VF_NUM};
    struct devmm_free_dev_mem_info dev_mem_info = {0};
    struct devmm_chan_free_pages free_info = {{{0}}};
    u64 j, page_num, free_va, send_va;
    u32 stamp = (u32)ka_jiffies;
    int ret;

    page_num = devmm_get_page_num_from_va(heap, va);
    if (page_num == 0) {
        devmm_drv_err("Get page num failed. (va=0x%llx; page_num=%llu)\n", va, page_num);
        return -EADDRNOTAVAIL;
    }
    devmm_drv_debug("Free va. (va=0x%llx; page_num=%llu)\n", va, page_num);
    if (devmm_check_can_free(svm_pro, heap, page_bitmap, va, page_num) != 0) {
        devmm_drv_err("Va can't free, is used by rts, must call destory api first. (va=0x%llx)\n", va);
        return -EBUSY;
    }

    if (devmm_page_bitmap_is_ipc_create_mem(page_bitmap)) {
        devmm_try_free_ipc_mem(svm_pro, va, page_num, heap->chunk_page_size);
    }

    ka_task_down_read(&svm_pro->bitmap_sem);
    for (j = 0, free_va = va, send_va = va; j < page_num; j++, free_va += heap->chunk_page_size) {
        if (devmm_page_bitmap_is_remote_mapped(page_bitmap + j)) {
            ka_task_up_read(&svm_pro->bitmap_sem);
            devmm_drv_err("Free fail, please call halHostUnregisterEx. (addr=0x%llx)\n", free_va);
            return -EBUSY;
        }

        if (devmm_page_bitmap_is_host_mapped(page_bitmap + j)) {
            devmm_unmap_pages(svm_pro, free_va, heap->chunk_page_size / KA_MM_PAGE_SIZE);
        }

        if (devmm_page_bitmap_is_dev_mapped(page_bitmap + j)) {
            devmm_get_svm_id(svm_pro, page_bitmap + j, &svmid.logical_devid, &svmid.devid, &svmid.vfid);
            devmm_mem_free_preprocess_by_dev_and_va(svm_pro, svmid.devid, free_va, heap->chunk_page_size);
            dev_mem_info.free_flag = (j > 0 && last_svmid.logical_devid != svmid.logical_devid) ? 1 : 0;
            dev_mem_info.last_devmapped_index = (u32)((dev_mem_info.free_flag == 1) ? (j - 1) : j);
            dev_mem_info.devmapped_flag = 1;
            devmm_update_free_svmid(dev_mem_info.free_flag, &svmid, &last_svmid);
        }

        if (devmm_page_bitmap_advise_memory_shared(page_bitmap + j)) {
            dev_mem_info.last_devshared_index = (u32)((dev_mem_info.free_flag == 1) ? j - 1 : j);
            dev_mem_info.shared_flag = 1;
        }

        /* mapped and shared flag to prevented set shared but umap pa */
        if (devmm_is_need_send_free_msg(&dev_mem_info, j, page_num - 1)) {
            devmm_fill_free_pages_info(send_va, &svmid, heap, &dev_mem_info, &free_info);
            if (free_info.head.dev_id < SVM_MAX_AGENT_NUM) {
                devmm_free_pages_cache(svm_pro, free_info.head.logical_devid,
                    (u32)page_num, heap->chunk_page_size, free_info.va, reuse);
            }
            ret = devmm_chan_send_msg_free_pages(&free_info, heap, svm_pro, (int)dev_mem_info.shared_flag,
                (free_info.head.dev_id < SVM_MAX_AGENT_NUM));
            if (ret == -EBUSY) {
                ka_task_up_read(&svm_pro->bitmap_sem);
                devmm_drv_err("Addr is get by cp, cp must call put first. (addr=0x%llx)\n", free_va);
                return ret;
            }
            devmm_update_free_dev_mem_info(&dev_mem_info);
            send_va = free_va;
            stamp = (u32)ka_jiffies;
        }
        devmm_try_cond_resched(&stamp);
    }

    devmm_free_page_clear_bitmap(heap, page_bitmap, va, page_num);
    ka_task_up_read(&svm_pro->bitmap_sem);
    return 0;
}

static int devmm_ioctl_free_pages(struct devmm_svm_process *svm_pro, struct devmm_ioctl_arg *arg)
{
    struct devmm_free_pages_para *free_pages_para = &arg->data.free_pages_para;
    struct devmm_svm_heap *heap = NULL;
    u32 *page_bitmap = NULL;
    int ret;

    heap = devmm_svm_get_heap(svm_pro, free_pages_para->va);
    if (heap == NULL) {
        devmm_drv_err("Check heap error. (va=0x%llx)\n", free_pages_para->va);
        return -EADDRNOTAVAIL;
    }

    if (free_pages_para->va != ka_base_round_down(free_pages_para->va, heap->chunk_page_size)) {
        devmm_drv_err("Va isn't aligned. (va=0x%llx; chunk_page_size=%u)\n",
                      free_pages_para->va, heap->chunk_page_size);
        return -EADDRNOTAVAIL;
    }

    ka_task_down_read(&svm_pro->host_fault_sem);
    page_bitmap = devmm_get_fst_alloc_bitmap_by_heap(svm_pro, heap, free_pages_para->va);
    if (page_bitmap == NULL) {
        devmm_drv_err("Get bitmap error. (va=0x%llx)\n", free_pages_para->va);
        ka_task_up_read(&svm_pro->host_fault_sem);
        return -EINVAL;
    }
    ret = devmm_free_page_process(svm_pro, heap, page_bitmap, free_pages_para->va, true);
    ka_task_up_read(&svm_pro->host_fault_sem);
    return ret;
}


STATIC void devmm_svm_bitmap_writelock(struct devmm_svm_process *svm_proc)
{
    if (svm_proc == NULL) {
        return;
    }
    ka_task_down_write(&svm_proc->bitmap_sem);
}

STATIC void devmm_svm_bitmap_writeunlock(struct devmm_svm_process *svm_proc)
{
    if (svm_proc == NULL) {
        return;
    }
    ka_task_up_write(&svm_proc->bitmap_sem);
}

STATIC void devmm_free_heap_pagebitmap(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap)
{
    devmm_svm_bitmap_writelock(svm_proc);
    devmm_free_heap_pagebitmap_ref(heap);
    devmm_svm_bitmap_writeunlock(svm_proc);
}

void devmm_destory_all_heap_by_proc(struct devmm_svm_process *svm_pro)
{
    struct devmm_svm_heap *heap = NULL;
    u32 stamp = (u32)ka_jiffies;
    u32 heap_idx, heap_num;

    for (heap_idx = 0; heap_idx < svm_pro->max_heap_use; heap_idx++) {
        heap = devmm_get_heap_by_idx(svm_pro, heap_idx);
        if (devmm_check_heap_is_entity(heap) == false) {
            continue;
        }
        heap_num  = (u32)(heap->heap_size / DEVMM_HEAP_SIZE);
        /* host memory is freed by self mem destory api */
        devmm_free_heap_pagebitmap(svm_pro, heap);
        devmm_free_heap_struct(svm_pro, heap->heap_idx, heap_num);
        heap_idx += ((heap_num > 0) ? (heap_num - 1) : 0);
        devmm_try_cond_resched(&stamp);
    }
    devmm_drv_debug("Destroy all heap end. (hostpid=%d)\n", svm_pro->process_id.hostpid);
    devmm_page_cnt_stats_show(&svm_pro->pg_cnt_stats);
}

int devmm_notify_deviceprocess(struct devmm_svm_process *svm_proc)
{
    u32 i, phyid, vfid;
    int ret;

    for (i = 0; i < SVM_MAX_AGENT_NUM; i++) {
        if (devmm_device_inited(svm_proc, i) != 0) {
            continue;
        }

        phyid = devmm_get_phyid_devid_from_svm_process(svm_proc, i);
        vfid = devmm_get_vfid_from_svm_process(svm_proc, i);
        if ((phyid >= SVM_MAX_AGENT_NUM) || (vfid >= DEVMM_MAX_VF_NUM)) {
            continue;
        }
        devmm_drv_debug("Show details. "
            "(hostpid=%d; devid=%u; vfid=%u; logicid=%u; phyid=%u; vfid:%u; device_is_ready=%d)\n",
            svm_proc->process_id.hostpid, svm_proc->process_id.devid, svm_proc->process_id.vfid,
            i, phyid, vfid, devmm_device_is_ready(phyid));
        devmm_notify_ts_drv_to_release(i, svm_proc->process_id.hostpid);
        if (devmm_device_is_ready(phyid) == true) {
            ret = devmm_notify_device_close_process(svm_proc, i, phyid, vfid);
            if (ret != 0) {
                return ret;
            }
        }

        svm_proc->deviceinfo[i].devpid = DEVMM_SVM_INVALID_PID;
    }

    return 0;
}

void devmm_destory_heap_mem(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap)
{
    u64 chunk_page_cnt = heap->heap_size / heap->chunk_page_size;
    u32 *page_bitmap = heap->page_bitmap;
    u32 stamp = (u32)ka_jiffies;
    u64 i;

    if (heap->heap_sub_type == SUB_RESERVE_TYPE) {
        devmm_destroy_reserve_heap_mem(svm_proc, heap);
        return;
    }

    if ((heap->heap_type == DEVMM_HEAP_HUGE_PAGE) || (heap->heap_type == DEVMM_HEAP_CHUNK_PAGE)) {
        for (i = 0; i < chunk_page_cnt; i++) {
            if (devmm_page_bitmap_is_remote_mapped(page_bitmap + i)) {
                devmm_unmap_mem(svm_proc, heap->start + i * heap->chunk_page_size, heap->chunk_page_size);
                devmm_page_bitmap_clear_flag((page_bitmap + i), DEVMM_PAGE_REMOTE_MAPPED_MASK);
            }
            if (devmm_page_bitmap_is_page_alloced(page_bitmap + i) &&
                devmm_page_bitmap_is_host_mapped(page_bitmap + i)) {
                devmm_unmap_pages(svm_proc, heap->start + i * heap->chunk_page_size,
                    heap->chunk_page_size / KA_MM_PAGE_SIZE);
                devmm_page_bitmap_clear_flag((page_bitmap + i), DEVMM_PAGE_HOST_MAPPED_MASK);
            }
            devmm_try_cond_resched(&stamp);
        }
    } else if (heap->heap_type == DEVMM_HEAP_PINNED_HOST) {
        for (i = 0; i < chunk_page_cnt; i++) {
            if (devmm_page_bitmap_is_page_alloced(page_bitmap + i)) {
                devmm_unmap_pages(svm_proc, heap->start + i * heap->chunk_page_size,
                    heap->chunk_page_size / KA_MM_PAGE_SIZE);
                devmm_page_clean_bitmap(page_bitmap + i);
            }
            devmm_try_cond_resched(&stamp);
        }
    }
}

STATIC void devmm_destory_all_self_mem_by_proc(struct devmm_svm_process *svm_proc)
{
    struct devmm_svm_heap *heap = NULL;
    u32 stamp = (u32)ka_jiffies;
    u32 heap_idx;

    /* unpin host page and del txatu when process exit */
    devmm_destory_shm_pro_node(svm_proc);

    for (heap_idx = 0; heap_idx < svm_proc->max_heap_use; heap_idx++) {
        devmm_try_cond_resched(&stamp);
        heap = devmm_get_heap_by_idx(svm_proc, heap_idx);
        if (devmm_check_heap_is_entity(heap) == false) {
            continue;
        }

        devmm_destory_heap_mem(svm_proc, heap);
        heap_idx += (u32)(heap->heap_size / DEVMM_HEAP_SIZE - 1);
    }
    if (svm_proc->host_pin_heap != NULL) {
        devmm_destory_heap_mem(svm_proc, svm_proc->host_pin_heap);
    }
}

void devmm_notifier_release_private(struct devmm_svm_process *svm_proc)
{
    devmm_mem_free_preprocess_by_dev(svm_proc, SVM_MAX_AGENT_NUM);
    devmm_destory_all_self_mem_by_proc(svm_proc);
    devmm_phy_addr_blks_destroy(svm_proc);
}

STATIC void devmm_update_heap_broadcast(struct devmm_svm_process *svm_pro,
                                        struct devmm_chan_update_heap *update_heap)
{
    u32 phyid, vfid, i;

    update_heap->head.process_id.hostpid = svm_pro->process_id.hostpid;
    update_heap->head.msg_id = DEVMM_CHAN_UPDATE_HEAP_H2D_ID;

    for (i = 0; i < (u32)SVM_MAX_AGENT_NUM; i++) {
        int ret;
        if (devmm_device_inited(svm_pro, i) != 0) {
            continue;
        }
        phyid = devmm_get_phyid_devid_from_svm_process(svm_pro, i);
        vfid = devmm_get_vfid_from_svm_process(svm_pro, i);
        update_heap->head.dev_id = phyid;
        update_heap->head.process_id.vfid = (u16)vfid;
        ret = devmm_chan_msg_send(update_heap, sizeof(struct devmm_chan_update_heap), sizeof(struct devmm_chan_msg_head));
        if (ret != 0) {
#ifndef EMU_ST
            devmm_drv_warn("Update heap msg. (logicid=%u; phyid=%u; vfid=%u; ret=%d)\n", i, phyid, vfid, ret);
#endif
        }
    }
}

static int devmm_destory_svm_heap_bitmap(struct devmm_svm_process *svm_pro, struct devmm_svm_heap *heap)
{
    unsigned long heap_start_addr;
    u32 page_cnt, i, page_num;
    u32 *page_bitmap = NULL;
    u32 stamp = (u32)ka_jiffies;
    int ret = 0;

    page_cnt = (u32)(heap->heap_size / heap->chunk_page_size);
    devmm_drv_debug("Page_cnt. (page_cnt=%u)\n", page_cnt);
    for (i = 0; i < page_cnt; i++) {
        page_bitmap = heap->page_bitmap + i;
        if (devmm_page_bitmap_is_remote_mapped(page_bitmap)) {
            devmm_unmap_mem(svm_pro, heap->start + i * heap->chunk_page_size, heap->chunk_page_size);
            devmm_page_bitmap_clear_flag(page_bitmap, DEVMM_PAGE_REMOTE_MAPPED_MASK);
        }
        devmm_try_cond_resched(&stamp);
    }

    for (i = 0; i < page_cnt; i += page_num) {
        page_bitmap = heap->page_bitmap + i;
        if (!devmm_page_bitmap_is_page_alloced(page_bitmap) || !devmm_page_bitmap_is_first_page(page_bitmap)) {
            page_num = 1;
            continue;
        }
        heap_start_addr = heap->start + (unsigned long)heap->chunk_page_size * i;
        page_num = (u32)devmm_get_page_num_from_va(heap, heap_start_addr);
        ret = devmm_free_page_process(svm_pro, heap, page_bitmap, heap_start_addr, false);
        if (ret != 0) {
            devmm_drv_err("Va heap_start_addr can't free, is being used by rts, must call release api first. "
                "(heap_start_addr=0x%lx)\n", heap_start_addr);
            return -ETXTBSY;
        }
        devmm_try_cond_resched(&stamp);
    }

    devmm_drv_debug("Show page_cnt. (page_cnt=%u)\n", page_cnt);
    return ret;
}

static int devmm_check_and_destory_heap(struct devmm_svm_process *svm_pro, struct devmm_svm_heap *heap)
{
    int ret = 0;

    if (heap->heap_sub_type == SUB_RESERVE_TYPE) {
        devmm_destroy_reserve_heap_mem(svm_pro, heap);
        return 0;
    }

    if ((heap->heap_type == DEVMM_HEAP_HUGE_PAGE) || (heap->heap_type == DEVMM_HEAP_CHUNK_PAGE)) {
        ret = devmm_destory_svm_heap_bitmap(svm_pro, heap);
    } else if (heap->heap_type == DEVMM_HEAP_PINNED_HOST) {
        devmm_destory_heap_mem(svm_pro, heap);
    }

    return ret;
}

static int devmm_check_dev_heap_size(struct devmm_svm_process *svm_proc, struct devmm_update_heap_para *cmd,
    u32 devid)
{
    u64 total_ddr, total_hbm;
    u32 tmp_devid = devid;

    if (tmp_devid >= SVM_MAX_AGENT_NUM) {
        devmm_drv_err("Devid is invalid. (devid=%u)\n", tmp_devid);
#ifndef EMU_ST
        return -ENODEV;
#endif
    }
    tmp_devid = devmm_get_phyid_devid_from_svm_process(svm_proc, tmp_devid);
    if (tmp_devid >= SVM_MAX_AGENT_NUM) {
        devmm_drv_err("Process not init. (devid=%u)\n", tmp_devid);
#ifndef EMU_ST
        return -ESRCH;
#endif
    }

    if (tmp_devid == SVM_HOST_AGENT_ID) {
        total_ddr = devmm_svm->device_info.host_ddr;
        total_hbm = 0;
    } else {
        total_ddr = devmm_svm->device_info.ddr_size[tmp_devid][0];
        total_hbm = devmm_svm->device_info.hbm_size[tmp_devid][0];
    }

    if (cmd->heap_size > max(total_ddr, total_hbm)) {
        devmm_drv_err("Heap_size is invalid. (heap_size=%llu; total_ddr=%llu, total_hbm=%llu)\n",
            cmd->heap_size, total_ddr, total_hbm);
#ifndef EMU_ST
        return -EINVAL;
#endif
    }

    return 0;
}

static int devmm_check_svm_heap_size(struct devmm_svm_process *svm_proc, struct devmm_update_heap_para *cmd)
{
    /*svm heap max size 128G */
    return (cmd->heap_size > (DEVMM_HEAP_USED_BITS_NUM_MAX * HEAP_USED_PER_MASK_SIZE)) ? -EINVAL : 0;
}

static int devmm_check_host_heap_size(struct devmm_update_heap_para *cmd)
{
    if (cmd->heap_size > devmm_svm->device_info.host_ddr) {
        return -EINVAL;
    }

    return 0;
}

static int devmm_check_dvpp_heap_size(struct devmm_update_heap_para *cmd)
{
    if ((cmd->heap_size != DEVMM_MAX_HEAP_MEM_FOR_DVPP_4G) &&
        (cmd->heap_size != DEVMM_MAX_HEAP_MEM_FOR_DVPP_16G)) {
        return -EINVAL;
    }
    return 0;
}

static int devmm_check_read_only_heap_size(struct devmm_update_heap_para *cmd)
{
    return (cmd->heap_size != DEVMM_READ_ONLY_HEAP_SIZE) ? -EINVAL : 0;
}

static int devmm_check_dev_read_only_heap_size(struct devmm_update_heap_para *cmd)
{
    return (cmd->heap_size != DEVMM_DEV_READ_ONLY_HEAP_SIZE) ? -EINVAL : 0;
}

static int devmm_check_reserve_heap_size(struct devmm_svm_process *svm_proc, struct devmm_update_heap_para *cmd)
{
    /* dynamic heap */
    if (cmd->heap_idx >= DEVMM_MAX_HEAP_NUM) {
        /* 128T */
        return (cmd->heap_size > (DEVMM_DYN_HEAP_USED_BITS_NUM_MAX * HEAP_USED_PER_MASK_SIZE)) ? -EINVAL : 0;
    }

    /* reserve heap max size 128G */
    return (cmd->heap_size > (DEVMM_HEAP_USED_BITS_NUM_MAX * HEAP_USED_PER_MASK_SIZE)) ? -EINVAL : 0;
}

static int devmm_check_input_heap_size(struct devmm_svm_process *svm_proc, struct devmm_update_heap_para *cmd,
    u32 devid)
{
    int ret;

    /* heap type and heap size must match */
    if (cmd->heap_sub_type == SUB_SVM_TYPE) {
        ret = devmm_check_svm_heap_size(svm_proc, cmd);
    } else if (cmd->heap_sub_type == SUB_HOST_TYPE) {
        ret = devmm_check_host_heap_size(cmd);
    } else if (cmd->heap_sub_type == SUB_DVPP_TYPE) {
        ret = devmm_check_dvpp_heap_size(cmd);
    } else if (cmd->heap_sub_type == SUB_DEVICE_TYPE) {
        ret = devmm_check_dev_heap_size(svm_proc, cmd, devid);
    } else if (cmd->heap_sub_type == SUB_READ_ONLY_TYPE) {
        ret = devmm_check_read_only_heap_size(cmd);
    } else if (cmd->heap_sub_type == SUB_DEV_READ_ONLY_TYPE) {
        ret = devmm_check_dev_read_only_heap_size(cmd);
    } else if (cmd->heap_sub_type == SUB_RESERVE_TYPE) {
        ret = devmm_check_reserve_heap_size(svm_proc, cmd);
    } else {
        ret = -EINVAL;
    }

    return ret;
}

STATIC int devmm_ioctl_enable_heap(struct devmm_svm_process *svm_pro, u32 devid,
    struct devmm_update_heap_para *cmd)
{
    struct devmm_chan_update_heap update_heap = {{{0}}};
    struct devmm_update_heap_para *update_cmd = NULL;
    struct devmm_svm_heap *heap = NULL;
    u64 start = svm_pro->start_addr + cmd->heap_idx * DEVMM_HEAP_SIZE;
    int da_del_flag = 0;
    int ret;

    ret = devmm_check_input_heap_size(svm_pro, cmd, devid);
    if (ret != 0) {
        devmm_drv_err("Input error. (ret=%d; op=0x%x; heap_type=0x%x; heap_sub_type=0x%x; "
            "heap_idx=%u; heap_size=%llu)\n",
            ret, cmd->op, cmd->heap_type, cmd->heap_sub_type, cmd->heap_idx, cmd->heap_size);
        return ret;
    }

    if (!devmm_is_static_reserve_addr(svm_pro, start)) {
        if (!svm_is_da_addr(svm_pro, start, cmd->heap_size)) {
            /* device only share addr, host not maped, not has vma */
            ret = svm_da_add_addr(svm_pro, start, cmd->heap_size, NULL);
            if (ret != 0) {
                devmm_drv_err("Add da addr failed. (ret=%d; op=0x%x; heap_type=0x%x; heap_sub_type=0x%x; "
                    "heap_idx=%u; heap_size=0x%llx)\n",
                    ret, cmd->op, cmd->heap_type, cmd->heap_sub_type, cmd->heap_idx, cmd->heap_size);
                return ret;
            }
            da_del_flag = 1;
            devmm_drv_info("No mmap, only enable heap. (heap_idx=%u; start=0x%llx; heap_size=0x%llx)\n",
                cmd->heap_idx, start, cmd->heap_size);
        } else {
            if (!svm_is_da_match(svm_pro, start, cmd->heap_size)) {
                devmm_drv_err("Invalid para. (heap_idx=%u; start=0x%llx; heap_size=0x%llx)\n",
                    cmd->heap_idx, start, cmd->heap_size);
                return -EINVAL;
            }
            devmm_drv_info("Mmap da, enbale heap. (heap_idx=%u; start=0x%llx; heap_size=0x%llx)\n",
                cmd->heap_idx, start, cmd->heap_size);
        }
    }

    ret = devmm_update_heap_info(svm_pro, cmd, NULL);
    if (ret != 0) {
        if (da_del_flag == 1) {
            (void)svm_da_del_addr(svm_pro, start, cmd->heap_size);
        }
        devmm_drv_err("Heap update failed. "
            "(op=0x%x; heap_type=0x%x; heap_sub_type=0x%x; heap_idx=%u; heap_size=%llu)\n",
            cmd->op, cmd->heap_type, cmd->heap_sub_type, cmd->heap_idx, cmd->heap_size);
        return ret;
    }

    heap = devmm_get_heap_by_idx(svm_pro, cmd->heap_idx);
    update_cmd = &update_heap.cmd;
    update_cmd->op = cmd->op;
    update_cmd->heap_idx = cmd->heap_idx;
    update_cmd->heap_type = heap->heap_type;
    update_cmd->heap_sub_type = heap->heap_sub_type;
    update_cmd->heap_size = cmd->heap_size;
    devmm_update_heap_broadcast(svm_pro, &update_heap);

    return 0;
}

STATIC int devmm_ioctl_disable_heap(struct devmm_svm_process *svm_pro, struct devmm_update_heap_para *cmd)
{
    struct devmm_chan_update_heap update_heap = {{{0}}};
    struct devmm_update_heap_para *update_cmd = NULL;
    struct devmm_svm_heap *heap = NULL;
    u64 start = svm_pro->start_addr + cmd->heap_idx * DEVMM_HEAP_SIZE;
    u32 heap_num;

    heap = devmm_get_heap_by_idx(svm_pro, cmd->heap_idx);
    if ((heap == NULL)) {
        devmm_drv_err("Heap is areadly destroy. (op=0x%x; heap_type=0x%x; heap_idx=%u)\n",
            cmd->op, cmd->heap_type, cmd->heap_idx);
        return -EINVAL;
    }
    if (devmm_wait_svm_heap_unoccupied(svm_pro, heap) == false) {
        devmm_drv_err("Heap is being occupied, can't destory. (op=0x%x; heap_type=0x%x; heap_idx=%u)\n",
            cmd->op, cmd->heap_type, cmd->heap_idx);
        return -EBUSY;
    }
    /* avoid donot call free va, just call update_heap to dstory heap */
    if (devmm_check_and_destory_heap(svm_pro, heap) != 0) {
        devmm_drv_err("Memory is being used, can't destory. (op=0x%x; heap_type=0x%x; heap_idx=%u)\n",
            cmd->op, cmd->heap_type, cmd->heap_idx);
        return -EBUSY;
    }

    heap_num = (u32)(cmd->heap_size / DEVMM_HEAP_SIZE);
    ka_task_down_write(&svm_pro->heap_sem);
    devmm_clear_svm_heap_struct(svm_pro, cmd->heap_idx, heap_num);
    ka_task_up_write(&svm_pro->heap_sem);

    if (!devmm_is_static_reserve_addr(svm_pro, start)) {
        if (svm_da_query_vma(svm_pro, start) == NULL) {
            devmm_drv_info("No mmap, only disable heap. (heap_idx=%u; start=0x%llx; heap_size=0x%llx)\n",
                cmd->heap_idx, start, cmd->heap_size);
            (void)svm_da_del_addr(svm_pro, start, cmd->heap_size);
        } else {
            devmm_drv_info("Mmap da, disable heap. (heap_idx=%u; start=0x%llx; heap_size=0x%llx)\n",
                cmd->heap_idx, start, cmd->heap_size);
        }
    }

    update_cmd = &update_heap.cmd;
    update_cmd->op = cmd->op;
    update_cmd->heap_idx = cmd->heap_idx;
    update_cmd->heap_type = DEVMM_HEAP_IDLE;
    update_cmd->heap_size = heap->heap_size;
    cmd->heap_size = heap->heap_size;
    cmd->heap_sub_type = heap->heap_sub_type;
    devmm_update_heap_broadcast(svm_pro, &update_heap);
    devmm_free_heap_pagebitmap(svm_pro, heap);
    (void)devmm_update_heap_info(svm_pro, cmd, heap);

    return 0;
}

STATIC int devmm_ioctl_update_heap(struct devmm_svm_process *svm_pro, struct devmm_ioctl_arg *arg)
{
    struct devmm_update_heap_para *cmd = &arg->data.update_heap_para;
    int ret;

    devmm_drv_debug("Show details. "
        "(op=0x%x; heap_type=0x%x; heap_idx=%u; heap_size=%llu; hostpid=%d; devid=%u; vfid=%u)\n",
        cmd->op, cmd->heap_type, cmd->heap_idx, cmd->heap_size,
        svm_pro->process_id.hostpid, svm_pro->process_id.devid, svm_pro->process_id.vfid);
    if (devmm_check_input_heap_info(svm_pro, cmd, arg->head.devid) == false) {
        devmm_drv_info("Input show. op=0x%x, type=0x%x, sub_type=0x%x, idx=%u, size=%llu \n",
                      cmd->op, cmd->heap_type, cmd->heap_sub_type, cmd->heap_idx, cmd->heap_size);
        return -EINVAL;
    }
    svm_use_da(svm_pro);
    if (cmd->op == DEVMM_HEAP_ENABLE) {
        ret = devmm_ioctl_enable_heap(svm_pro, arg->head.devid, cmd);
    } else {
        ret = devmm_ioctl_disable_heap(svm_pro, cmd);
    }
    svm_unuse_da(svm_pro);

    return ret;
}

STATIC int devmm_set_svm_proc_docker_id(struct devmm_svm_process *svm_proc)
{
    return devdrv_manager_get_docker_id(&svm_proc->docker_id);
}

static int devmm_ioctl_init_process(struct devmm_svm_process *svm_process, struct devmm_ioctl_arg *arg)
{
    struct devmm_init_process_para *init_process_para = &arg->data.init_process_para;
    struct devmm_svm_process_id process_id = {devmm_get_current_pid(), {0}, 0};
    u32 old_status;
    int err;

    if (devmm_svm->device_hpage_size == 0) {
        devmm_drv_err("Device is not ready. (device_hpage_size=0)\n");
        return -ENODEV;
    }

    init_process_para->svm_page_size = devmm_svm->svm_page_size;
    init_process_para->local_page_size = KA_MM_PAGE_SIZE;
    init_process_para->huge_page_size = devmm_svm->device_hpage_size;
    init_process_para->is_enable_host_giant_page = svm_process->is_enable_host_giant_page;

    devmm_drv_debug("Show page size. (svm_page_size=%u; local_page_size=%lu; device_hpage_size=%u; is_enable_host_giant = %d)\n",
        init_process_para->svm_page_size, (unsigned long)KA_MM_PAGE_SIZE, devmm_svm->device_hpage_size,
        (int)init_process_para->is_enable_host_giant_page);
    ka_task_mutex_lock(&svm_process->proc_lock);
    old_status = svm_process->inited;
    if ((old_status != DEVMM_SVM_PRE_INITING_FLAG) && (old_status != DEVMM_SVM_INITING_FLAG)) {
        ka_task_mutex_unlock(&svm_process->proc_lock);
        devmm_drv_err("Svm_proc is already inited. (hostpid=%d; devpid=%d)\n",
            svm_process->process_id.hostpid, svm_process->devpid);
        return -ESRCH;
    }

    err = devmm_add_svm_proc_pid(svm_process, &process_id, devmm_get_current_pid());
    if (err != 0) {
        devmm_drv_err("Set svm proc pid fail. (err=%d)\n", err);
        ka_task_mutex_unlock(&svm_process->proc_lock);
        return err;
    }
    err = devmm_set_svm_proc_docker_id(svm_process);
    if (err != 0) {
        devmm_del_first_svm_proc_pid(svm_process);
        ka_task_mutex_unlock(&svm_process->proc_lock);
        devmm_drv_err("Set docker_id error. (err=%d; hostpid=%d)\n", err, process_id.hostpid);
        return err;
    }

    err = devmm_init_shm_pro_node(svm_process);
    if (err != 0) {
        devmm_del_first_svm_proc_pid(svm_process);
        ka_task_mutex_unlock(&svm_process->proc_lock);
        devmm_drv_err("Init share memory node failed. (err=%d)\n", err);
        return err;
    }

    devmm_init_dev_pages_cache(svm_process); // pages_cache will be recycled by devmm_svm_release_private_proc
    devmm_set_svm_proc_state(svm_process, DEVMM_SVM_PRE_INITED_FLAG);
    ka_task_mutex_unlock(&svm_process->proc_lock);

    err = devmm_mmu_notifier_register(svm_process);
    if (err != 0) {
        err = (err == -EINTR) ? -EAGAIN : err; // Interrupted by signals. Retry in user mode.
        devmm_drv_no_err_if((err == -EAGAIN), "Mmu_notifier_register not success, recycle function will invalid. (err=%d; hostpid=%d)\n",
            err, devmm_get_current_pid());
        goto mmu_notifier_register_failed;
    }

    err = devmm_add_to_svm_proc_hashtable(svm_process);
    if (err != 0) {
        devmm_drv_err("add hashtable fail. (err=%d; hostpid=%d; proc_idx=%u)\n",
            err, svm_process->process_id.hostpid, svm_process->proc_idx);
        goto add_hashtable_failed;
    }

    err = devmm_init_process_notice_pm(svm_process);
    if (err != 0) {
        devmm_drv_err("Vm notice pm fail. (err=%d)\n", err);
        goto notice_pm_failed;
    }

    devmm_proc_fs_add_task(svm_process);

    ka_task_mutex_lock(&svm_process->proc_lock);
    ka_task_down_read(ka_mm_get_mmap_sem(ka_task_get_current_mm()));
    err = devmm_check_and_set_svm_static_reserve_vma((void *)svm_process, svm_process->vma);
    if (err == 0) {
        devmm_svm_mem_enable(svm_process);
    } else {
        /* If devmm_svm_mmap is not called, directly close fd will call devmm_svm_release,
         * and the work attempt timeout will cause svm_proc and other resources to leak,
         * so need to wait forever
         */
        svm_process->release_work_timeout = DEVMM_RELEASE_WAIT_FOREVER;
    }

    devmm_set_svm_proc_state(svm_process, DEVMM_SVM_INITED_FLAG);
    ka_task_up_read(ka_mm_get_mmap_sem(ka_task_get_current_mm()));
    ka_task_mutex_unlock(&svm_process->proc_lock);

    devmm_drv_debug("Init_process details. (hostpid=%d; devid=%u; vfid=%u; devpid=%d; docker_id=%u)\n",
        svm_process->process_id.hostpid, svm_process->process_id.devid, svm_process->process_id.vfid,
        svm_process->devpid, svm_process->docker_id);

    return 0;

notice_pm_failed:
    devmm_del_from_svm_proc_hashtable_lock(svm_process);
add_hashtable_failed:
    devmm_mmu_notifier_unregister_no_release(svm_process);
mmu_notifier_register_failed:
    ka_task_mutex_lock(&svm_process->proc_lock);
    devmm_del_first_svm_proc_pid(svm_process);
    devmm_unint_shm_pro_node(svm_process);
    devmm_set_svm_proc_state(svm_process, old_status);
    ka_task_mutex_unlock(&svm_process->proc_lock);

    return err;
}

static int devmm_ioctl_get_va_status_info(struct devmm_svm_process *svm_process, struct devmm_ioctl_arg *arg)
{
    struct devmm_status_va_info_para *status_va_info_para = NULL;
    u32 *bitmap = NULL;

    status_va_info_para = &arg->data.status_va_info_para;

    devmm_drv_debug("Show details. (va=0x%llx)\n", status_va_info_para->va);
    bitmap = devmm_get_page_bitmap(svm_process, status_va_info_para->va);
    if (bitmap == NULL) {
        devmm_drv_err("Can't find page bitmap by va. (va=0x%llx)\n", status_va_info_para->va);
        return -EINVAL;
    }

    status_va_info_para->devid = 0;
    if (devmm_page_bitmap_is_dev_mapped(bitmap)) {
        status_va_info_para->mem_type = DV_MEM_SVM_DEVICE;
        status_va_info_para->devid = devmm_page_bitmap_get_value(bitmap, DEVMM_PAGE_DEVID_SHIT, DEVMM_PAGE_DEVID_WID);
    } else if (devmm_page_bitmap_is_host_mapped(bitmap)) {
        status_va_info_para->mem_type = DV_MEM_SVM_HOST;
    } else {
        status_va_info_para->mem_type = DV_MEM_SVM;
    }
    devmm_drv_debug("Show status_va_info_para. (mem_type=%u; devid=%u; va=0x%llx)\n", status_va_info_para->mem_type,
        status_va_info_para->devid, status_va_info_para->va);

    return 0;
}

static int devmm_ioctl_query_device_mem_info(struct devmm_svm_process *svm_process, struct devmm_ioctl_arg *arg)
{
    struct devmm_query_device_mem_usedinfo *query_device_mem_usedinfo = NULL;
    struct devmm_chan_device_meminfo device_meminfo = {{{0}}};
    int ret;

    query_device_mem_usedinfo = &arg->data.query_device_mem_usedinfo_para;

    device_meminfo.head.process_id.hostpid = svm_process->process_id.hostpid;
    device_meminfo.head.process_id.vfid = arg->head.vfid;
    device_meminfo.head.dev_id = (u16)arg->head.devid;
    device_meminfo.head.msg_id = (u16)DEVMM_CHAN_QUERY_MEMINFO_H2D_ID;
    device_meminfo.mem_type = query_device_mem_usedinfo->mem_type;

    devmm_drv_debug("Enter. (device=%u; mem_type=0x%x)\n",
        arg->head.devid, device_meminfo.mem_type);

    ret = devmm_chan_msg_send(&device_meminfo, sizeof(struct devmm_chan_device_meminfo),
                              sizeof(struct devmm_chan_device_meminfo));
    if (ret != 0) {
        devmm_drv_err("Query memory usedinfo error. (dev_id=%u; mem_type=0x%x; ret=%d)\n",
            device_meminfo.head.dev_id, device_meminfo.mem_type, ret);
        return ret;
    }

    query_device_mem_usedinfo->normal_total_size = device_meminfo.normal_total_size;
    query_device_mem_usedinfo->normal_free_size = device_meminfo.normal_free_size;
    query_device_mem_usedinfo->huge_total_size = device_meminfo.huge_total_size;
    query_device_mem_usedinfo->huge_free_size = device_meminfo.huge_free_size;
    query_device_mem_usedinfo->giant_total_size = device_meminfo.giant_total_size;
    query_device_mem_usedinfo->giant_free_size = device_meminfo.giant_free_size;

    devmm_drv_debug("Exit. (devid=%u; normal_free_size=%llu; normal_total_size=%llu; huge_free_size=%llu; "
        "huge_total_size=%llu; giant_total_size=%llu; giant_free_size=%llu)\n", arg->head.devid,
        device_meminfo.normal_free_size, device_meminfo.normal_total_size, device_meminfo.huge_free_size,
        device_meminfo.huge_total_size, device_meminfo.giant_total_size, device_meminfo.giant_free_size);
    return ret;
}

static int devmm_check_mem_info_para_check(struct devmm_check_mem_info *info)
{
    if (info->va == NULL) {
        devmm_drv_err("Va[] is null.\n");
        return -EINVAL;
    }

    if ((info->cnt == 0) || (info->cnt > DEVMM_DEV_ADDR_NUM_MAX)) {
        devmm_drv_err("Cnt is out of range.\n");
        return -EINVAL;
    }

    return 0;
}

static int devmm_meminfo_para_init(struct devmm_check_mem_info *info)
{
    u64 *tmp_va = NULL;
    u64 arg_size = sizeof(u64) * info->cnt;

    tmp_va = devmm_kvzalloc(arg_size);
    if (tmp_va == NULL) {
        devmm_drv_err("Kvzalloc fail. (num=%u)\n", info->cnt);
        return -ENOMEM;
    }

    if (ka_base_copy_from_user(tmp_va, (void __ka_user *)(uintptr_t)info->va, arg_size) != 0) {
        devmm_drv_err("Copy_from_user fail.\n");
        devmm_kvfree(tmp_va);
        return -EINVAL;
    }

    info->va = tmp_va;
    return 0;
}

static void devmm_meminfo_para_uninit(struct devmm_check_mem_info *info)
{
    devmm_kvfree(info->va);
    info->va = NULL;
}

static void devmm_print_pre_post_addr(struct devmm_svm_process *svm_proc, u64 va)
{
    u64 pre_start_va, pre_end_va, post_start_va, post_end_va;

    (void)devmm_check_alloced_va(svm_proc, va, &pre_start_va, &pre_end_va, DEVMM_PRE_ALLOCED_FLAG);
    (void)devmm_check_alloced_va(svm_proc, va, &post_start_va, &post_end_va, DEVMM_POST_ALLOCED_FLAG);

    devmm_drv_info("Addr's pre and post info. (addr=0x%llx; pre_addr=[0x%llx, 0x%llx]; post_addr=[0x%llx, 0x%llx])\n",
        va, pre_start_va, pre_end_va, post_start_va, post_end_va);
}

static int devmm_check_meminfo(struct devmm_svm_process *svm_proc,
    struct devmm_check_mem_info *info, u32 devid)
{
    struct devmm_svm_heap *heap = NULL;
    u32 stamp = (u32)ka_jiffies;
    u32 *page_bitmap = NULL;
    u32 i;

    for (i = 0; i < info->cnt; i++) {
        heap = devmm_svm_get_heap(svm_proc, info->va[i]);
        if (heap == NULL) {
            devmm_drv_err("Invalid addr. (i=%u; va=0x%llx)\n", i, info->va[i]);
            goto invalid_addr;
        }

        page_bitmap = devmm_get_page_bitmap_with_heap(heap, info->va[i]);
        if (page_bitmap == NULL) {
            devmm_drv_err("Invalid addr. (i=%u; va=0x%llx)\n", i, info->va[i]);
            goto invalid_addr;
        }

        if (devmm_page_bitmap_is_advise_populate(page_bitmap) == false) {
            devmm_drv_err("Invalid addr. (i=%u; va=0x%llx)\n", i, info->va[i]);
            goto invalid_addr;
        }

        if (devmm_heap_subtype_is_matched(heap->heap_sub_type, info->heap_subtype_mask) == false) {
            devmm_drv_err("Heap type is not match. (i=%u; va=0x%llx; heap_subtype_mask=0x%llx; heap_sub_type=%u)\n",
                i, info->va[i], (u64)info->heap_subtype_mask, heap->heap_sub_type);
            goto invalid_addr;
        }
#ifndef EMU_ST
        if ((heap->heap_sub_type == SUB_DVPP_TYPE) || (heap->heap_sub_type == SUB_DEVICE_TYPE) ||
            (heap->heap_sub_type == SUB_SVM_TYPE && devmm_page_bitmap_is_dev_mapped(page_bitmap)) ||
            (heap->heap_sub_type == SUB_RESERVE_TYPE && devmm_page_bitmap_is_dev_mapped(page_bitmap))) {
            if (devmm_page_bitmap_get_phy_devid(svm_proc, page_bitmap) != devid) {
                devmm_drv_err("Addr isn't belong to dev. (i=%u; va=0x%llx; in para's devid=%u; actually devid=%u)\n",
                    i, info->va[i], devid, devmm_page_bitmap_get_phy_devid(svm_proc, page_bitmap));
                goto invalid_addr;
            }
        }
#endif
        devmm_try_cond_resched(&stamp);
    }

    return 0;
invalid_addr:
    devmm_print_pre_post_addr(svm_proc, info->va[i]);
    return -EFAULT;
}

static int devmm_ioctl_check_meminfo(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_check_mem_info *info = &arg->data.check_meminfo_para;
    int ret;

    ret = devmm_check_mem_info_para_check(info);
    if (ret != 0) {
        return ret;
    }

    ret = devmm_meminfo_para_init(info);
    if (ret != 0) {
        return ret;
    }

    ret = devmm_check_meminfo(svm_proc, info, arg->head.devid);
    devmm_meminfo_para_uninit(info);
    return ret;
}

void devmm_notify_wait_device_close_process(struct devmm_svm_process *svm_proc,
    u32 logical_devid, u32 phy_devid, u32 vfid)
{
    int ret, i;

    for (i = 0; i < DEVMM_DEV_CLOSE_TIMES; i++) {
        ret = devmm_notify_device_close_process(svm_proc, logical_devid, phy_devid, vfid);
        if (ret == 0) {
            break;
        }
        ka_system_usleep_range(DEVMM_DEV_CLOSE_WAITTIME_MIN, DEVMM_DEV_CLOSE_WAITTIME_MAX);
    }
    return;
}

static void devmm_vmm_resources_destory_by_devid(struct devmm_svm_process *svm_proc, u32 devid)
{
    devmm_destroy_all_heap_vmmas_by_devid(svm_proc, devid);
    devmm_share_id_map_node_destroy_by_devid(svm_proc, devid, false);
}

STATIC int devmm_ioctl_close_device(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct svm_id_inst id_inst = {.devid = arg->head.devid, .vfid = arg->head.vfid};
    struct devmm_task_dev_res_node *node = NULL;
    u32 logical_devid = arg->head.logical_devid;
    u32 phy_devid = arg->head.devid;

    devmm_drv_run_info("Close logical_device. (logical_devid=%d; phyid=%d; vfid=%d; hostpid=%d; devpid=%d)\n",
        arg->head.logical_devid, arg->head.devid, arg->head.vfid,
        svm_proc->process_id.hostpid, svm_proc->deviceinfo[logical_devid].devpid);

    if (devmm_proc_dev_async_task_is_empty(svm_proc, phy_devid) == false) {
        devmm_drv_err("Async cpy is still executing, shouln't close dev. (devid=%u; vfid=%u; async_task_cnt=%u)\n",
            svm_proc->process_id.devid, svm_proc->process_id.vfid,
            devmm_get_proc_dev_async_task_cnt(svm_proc, phy_devid));
        return -EBUSY;
    }

    /*
     * must ensure the work is over,
     * or will access freed mem beacuse devmm_task_dev_res_node_get_by_task will put convert_node
     */
    devmm_srcu_work_uninit(&svm_proc->srcu_work);
    node = devmm_task_dev_res_node_get_by_task(svm_proc, &id_inst);
    if (node != NULL) {
        devmm_task_dev_res_node_destroy(node);
        devmm_task_dev_res_node_put(node);
    }
    devmm_mem_free_preprocess_by_dev(svm_proc, phy_devid);
    devmm_vmm_resources_destory_by_devid(svm_proc, phy_devid);
    devmm_destroy_ipc_mem_node_by_proc(svm_proc, phy_devid);
    devmm_try_destroy_remote_map_nodes(svm_proc, logical_devid, phy_devid, arg->head.vfid);
    devmm_destroy_dev_pages_cache(svm_proc, logical_devid);
    devmm_destory_register_dma_mng_by_devid(svm_proc, phy_devid);
    devmm_notify_wait_device_close_process(svm_proc, logical_devid, phy_devid, arg->head.vfid);
    devmm_modify_process_status(svm_proc, phy_devid, arg->head.vfid, STATUS_SVM_PAGE_FALUT_ERR_OCCUR, false);
    devmm_modify_process_status(svm_proc, phy_devid, arg->head.vfid, STATUS_SVM_PAGE_FALUT_ERR_CNT, false);
    svm_proc->deviceinfo[logical_devid].devpid = DEVMM_SETUP_INVAL_PID;
    devmm_remove_pid_from_business(phy_devid, svm_proc->process_id.hostpid);

    return 0;
}

STATIC int devmm_ioctl_prepare_close_device(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    u32 phy_devid = arg->head.devid;
    u32 wait_async_task_cnt = 100; /* wait 1s : 100 * 10ms */
    u32 i, async_task_cnt;

    for (i = 0; i < wait_async_task_cnt; i++) {
        async_task_cnt = devmm_get_proc_dev_async_task_cnt(svm_proc, phy_devid);
        if (async_task_cnt == 0) {
            devmm_drv_debug("Async cpy finish, can close dev. (devid=%u; vfid=%u; i=%u)\n",
                svm_proc->process_id.devid, svm_proc->process_id.vfid, i);
            break;
        }
        devmm_drv_debug("Async cpy is still executing. (devid=%u; vfid=%u; i=%u; async_task_cnt=%u)\n",
            svm_proc->process_id.devid, svm_proc->process_id.vfid, i, async_task_cnt);
        ka_system_msleep(10); /* wait 10ms */
    }

    if (async_task_cnt != 0) {
        devmm_drv_err("Async cpy is still executing, shouldn't close dev. (devid=%u; vfid=%u; async_task_cnt=%u)\n",
            svm_proc->process_id.devid, svm_proc->process_id.vfid, async_task_cnt);
        return -EBUSY;
    }
    /* When close device, the memory on the device side is released. Therefore,
     * the asynchronous copy task needs to be blocked in advance. Otherwise,
     * a large number of asynchronous copy tasks will trigger SMMU page faults
     */
    devmm_proc_dev_set_async_allow(svm_proc, phy_devid, false);

    return 0;
}

STATIC int devmm_ioctl_alloc_proc_set_to_file(ka_file_t *file, struct devmm_ioctl_arg *arg)
{
    return devmm_alloc_svm_proc_set_to_file(file);
}

STATIC int devmm_ioctl_get_mmap_para(ka_file_t *file, struct devmm_ioctl_arg *arg)
{
    struct devmm_get_mmap_para *mmap_para = &arg->data.mmap_para;

    if ((mmap_para->seg_num < devmm_svm->mmap_para.seg_num) || (mmap_para->segs == NULL)) {
        devmm_drv_err("Mmap array Must be greater than DEVMM_MAX_VMA_NUM. (seg_num=%u; segs=%d)\n",
            mmap_para->seg_num, (mmap_para->segs == NULL));
        return -EINVAL;
    }
    mmap_para->seg_num = devmm_svm->mmap_para.seg_num;
    if (ka_base_copy_to_user(mmap_para->segs, devmm_svm->mmap_para.segs,
        (sizeof(struct devmm_mmap_addr_seg) * mmap_para->seg_num)) != 0) {
        devmm_drv_err("Copy_to_user fail. (seg_num=%u)\n", mmap_para->seg_num);
        return -EINVAL;
    }

    return 0;
}

int (*const devmm_ioctl_file_arg_handlers[DEVMM_SVM_CMD_MAX_CMD])(ka_file_t *file, struct devmm_ioctl_arg *arg) = {
    [_KA_IOC_NR(DEVMM_SVM_ALLOC_PROC_STRUCT)] = devmm_ioctl_alloc_proc_set_to_file,
    [_KA_IOC_NR(DEVMM_SVM_GET_MMAP_INFO)] = devmm_ioctl_get_mmap_para,
};

struct devmm_ioctl_handlers_st devmm_ioctl_handlers[DEVMM_SVM_CMD_MAX_CMD] = {
    [_KA_IOC_NR(DEVMM_SVM_ALLOC)] = {devmm_ioctl_alloc, DEVMM_IS_MALLOC},
    [_KA_IOC_NR(DEVMM_SVM_FREE_PAGES)] = {devmm_ioctl_free_pages, DEVMM_IS_FREE},
    [_KA_IOC_NR(DEVMM_SVM_MEMCPY)] = {devmm_ioctl_memcpy_proc, DEVMM_HAS_MUTIL_ADDR | DEVMM_ADD_SUB_REF},
    [_KA_IOC_NR(DEVMM_SVM_MEMCPY2D)] = {devmm_ioctl_memcpy2d_proc, DEVMM_HAS_MUTIL_ADDR | DEVMM_ADD_SUB_REF},
    [_KA_IOC_NR(DEVMM_SVM_ASYNC_MEMCPY)] = {devmm_ioctl_async_memcpy_proc, DEVMM_HAS_MUTIL_ADDR | DEVMM_ADD_SUB_REF},
    [_KA_IOC_NR(DEVMM_SVM_MEMCPY_RESLUT_REFRESH)] = {devmm_ioctl_cpy_result_refresh, 0},
    [_KA_IOC_NR(DEVMM_SVM_SUMBIT_CONVERT_CPY)] = {devmm_ioctl_sumbit_convert_dma, 0},
    [_KA_IOC_NR(DEVMM_SVM_WAIT_CONVERT_CPY_RESLUT)] = {devmm_ioctl_wait_convert_dma_result, 0},
    [_KA_IOC_NR(DEVMM_SVM_CONVERT_ADDR)] = {devmm_ioctl_convert_addr, DEVMM_CONVERT_ID | DEVMM_HAS_MUTIL_ADDR |
        DEVMM_ADD_REF | DEVMM_CMD_NOT_SURPORT_HOST_AGENT},
    [_KA_IOC_NR(DEVMM_SVM_DESTROY_ADDR)] = {devmm_ioctl_destroy_addr, DEVMM_CONVERT_ID | DEVMM_CMD_NOT_SURPORT_HOST_AGENT},
    [_KA_IOC_NR(DEVMM_SVM_DESTROY_ADDR_BATCH)] = {devmm_ioctl_destroy_addr_batch, DEVMM_CMD_NOT_SURPORT_HOST_AGENT},
    [_KA_IOC_NR(DEVMM_SVM_ADVISE)] = {devmm_ioctl_advise, DEVMM_CONVERT_ID | DEVMM_IS_ADVISE},
    [_KA_IOC_NR(DEVMM_SVM_PREFETCH)] = {devmm_ioctl_prefetch, DEVMM_CONVERT_ID | DEVMM_IS_ADVISE},
    [_KA_IOC_NR(DEVMM_SVM_MEMSET8)] = {devmm_ioctl_memset8, DEVMM_ADD_SUB_REF},
    [_KA_IOC_NR(DEVMM_SVM_TRANSLATE)] = {devmm_ioctl_translate, DEVMM_ADD_SUB_REF |
        DEVMM_CMD_NOT_SURPORT_HOST_AGENT},
    [_KA_IOC_NR(DEVMM_SVM_IPC_MEM_OPEN)] = {devmm_ioctl_ipc_mem_open, DEVMM_IS_ADVISE | DEVMM_CMD_NOT_SURPORT_VDEV |
        DEVMM_CMD_NOT_SURPORT_HOST_AGENT | DEVMM_CONVERT_ID},
    [_KA_IOC_NR(DEVMM_SVM_IPC_MEM_CLOSE)] = {devmm_ioctl_ipc_mem_close, DEVMM_ADD_SUB_REF | DEVMM_CMD_NOT_SURPORT_VDEV |
        DEVMM_CMD_NOT_SURPORT_HOST_AGENT},
    [_KA_IOC_NR(DEVMM_SVM_IPC_MEM_CREATE)] = {devmm_ioctl_ipc_mem_create, DEVMM_ADD_SUB_REF | DEVMM_CMD_NOT_SURPORT_VDEV |
        DEVMM_CMD_NOT_SURPORT_HOST_AGENT},
    [_KA_IOC_NR(DEVMM_SVM_IPC_MEM_SET_PID)] = {devmm_ioctl_ipc_mem_set_pid, DEVMM_CMD_NOT_SURPORT_VDEV |
        DEVMM_CMD_NOT_SURPORT_HOST_AGENT},
    [_KA_IOC_NR(DEVMM_SVM_IPC_MEM_DESTROY)] = {devmm_ioctl_ipc_mem_destroy, DEVMM_CMD_NOT_SURPORT_VDEV |
        DEVMM_CMD_NOT_SURPORT_HOST_AGENT},
    [_KA_IOC_NR(DEVMM_SVM_IPC_MEM_QUERY)] = {devmm_ioctl_ipc_mem_query, DEVMM_CMD_NOT_SURPORT_VDEV |
        DEVMM_CMD_NOT_SURPORT_HOST_AGENT},
    [_KA_IOC_NR(DEVMM_SVM_IPC_MEM_SET_ATTR)] = {devmm_ioctl_ipc_set_attr, DEVMM_CMD_NOT_SURPORT_VDEV |
        DEVMM_CMD_NOT_SURPORT_HOST_AGENT},
    [_KA_IOC_NR(DEVMM_SVM_IPC_MEM_GET_ATTR)] = {devmm_ioctl_ipc_get_attr, DEVMM_CMD_NOT_SURPORT_VDEV |
        DEVMM_CMD_NOT_SURPORT_HOST_AGENT},
    [_KA_IOC_NR(DEVMM_SVM_SETUP_DEVICE)] = {devmm_ioctl_setup_device, DEVMM_CONVERT_ID},
    [_KA_IOC_NR(DEVMM_SVM_CLOSE_DEVICE)] = {devmm_ioctl_close_device, DEVMM_CONVERT_ID | DEVMM_CMD_WLOCK},
    [_KA_IOC_NR(DEVMM_SVM_INIT_PROCESS)] = {devmm_ioctl_init_process, DEVMM_CMD_NOLOCK},
    [_KA_IOC_NR(DEVMM_SVM_UPDATE_HEAP)] = {devmm_ioctl_update_heap, DEVMM_CMD_WLOCK},
    [_KA_IOC_NR(DEVMM_SVM_MEM_REMOTE_MAP)] = {devmm_ioctl_mem_remote_map, DEVMM_CONVERT_ID |
        DEVMM_CMD_NOT_SURPORT_VDEV | DEVMM_CMD_NOT_SURPORT_HOST_AGENT | DEVMM_ADD_REF},
    [_KA_IOC_NR(DEVMM_SVM_MEM_REMOTE_UNMAP)] = {devmm_ioctl_mem_remote_unmap, DEVMM_CONVERT_ID |
        DEVMM_CMD_NOT_SURPORT_VDEV | DEVMM_CMD_NOT_SURPORT_HOST_AGENT | DEVMM_OPS_SUCCESS_SUB_REF},
    [_KA_IOC_NR(DEVMM_SVM_DBG_VA_STATUS)] = {devmm_ioctl_get_va_status_info, 0},
    [_KA_IOC_NR(DEVMM_SVM_MAP_DEV_RESERVE)] = {devmm_ioctl_map_dev_reserve, DEVMM_CONVERT_ID |
        DEVMM_CMD_NOT_SURPORT_HOST_AGENT},
    [_KA_IOC_NR(DEVMM_SVM_PROCESS_STATUS_QUERY)] = {devmm_ioctl_query_process_status, DEVMM_CONVERT_ID |
        DEVMM_CMD_NOT_SURPORT_HOST_AGENT},
    [DEVMM_SVM_CMD_USE_PRIVATE_MAX_CMD] = {NULL}, /* above this svm process must inited */
    [_KA_IOC_NR(DEVMM_SVM_QUERY_MEM_USEDINFO)] = {devmm_ioctl_query_device_mem_info, DEVMM_CONVERT_ID | DEVMM_CMD_NOLOCK |
        DEVMM_CMD_NOT_SURPORT_HOST_AGENT},
    [_KA_IOC_NR(DEVMM_SVM_CHECK_MEMINFO)] = {devmm_ioctl_check_meminfo, DEVMM_CONVERT_ID |
        DEVMM_CMD_NOT_SURPORT_HOST_AGENT},
    [_KA_IOC_NR(DEVMM_SVM_MEM_CREATE)] = {devmm_ioctl_mem_create, DEVMM_CONVERT_ID | DEVMM_CMD_SURPORT_HOST_ID},
    [_KA_IOC_NR(DEVMM_SVM_MEM_RELEASE)] = {devmm_ioctl_mem_release, DEVMM_CONVERT_ID | DEVMM_CMD_SURPORT_HOST_ID},
    [_KA_IOC_NR(DEVMM_SVM_MEM_MAP)] = {devmm_ioctl_mem_map, DEVMM_CONVERT_ID | DEVMM_ADD_REF | DEVMM_CMD_SURPORT_HOST_ID},
    [_KA_IOC_NR(DEVMM_SVM_MEM_UNMAP)] = {devmm_ioctl_mem_unmap, DEVMM_CONVERT_ID | DEVMM_OPS_SUCCESS_SUB_REF |
        DEVMM_CMD_SURPORT_HOST_ID},
    [_KA_IOC_NR(DEVMM_SVM_MEM_QUERY_OWNER)] = {devmm_ioctl_mem_query_owner, DEVMM_CONVERT_ID | DEVMM_CMD_SURPORT_HOST_ID},
    [_KA_IOC_NR(DEVMM_SVM_MEM_SET_ACCESS)] = {devmm_ioctl_mem_set_access, DEVMM_CONVERT_ID | DEVMM_CMD_SURPORT_HOST_ID},
    [_KA_IOC_NR(DEVMM_SVM_MEM_GET_ACCESS)] = {devmm_ioctl_mem_get_access, DEVMM_CONVERT_ID | DEVMM_CMD_SURPORT_HOST_ID},
    [_KA_IOC_NR(DEVMM_SVM_MEM_EXPORT)] = {devmm_ioctl_mem_export, DEVMM_CONVERT_ID | DEVMM_CMD_NOT_SURPORT_HOST_AGENT |
        DEVMM_CMD_NOT_SURPORT_VDEV | DEVMM_CMD_SURPORT_HOST_ID},
    [_KA_IOC_NR(DEVMM_SVM_MEM_IMPORT)] = {devmm_ioctl_mem_import, DEVMM_CONVERT_ID | DEVMM_CMD_NOT_SURPORT_HOST_AGENT |
        DEVMM_CMD_NOT_SURPORT_VDEV | DEVMM_CMD_SURPORT_HOST_ID},
    [_KA_IOC_NR(DEVMM_SVM_MEM_SET_PID)] = {devmm_ioctl_mem_set_pid, DEVMM_CONVERT_ID | DEVMM_CMD_NOT_SURPORT_HOST_AGENT |
        DEVMM_CMD_NOT_SURPORT_VDEV | DEVMM_CMD_SURPORT_HOST_ID},
    [_KA_IOC_NR(DEVMM_SVM_MEM_SET_ATTR)] = {devmm_ioctl_mem_set_attr, DEVMM_CONVERT_ID | DEVMM_CMD_NOT_SURPORT_HOST_AGENT |
        DEVMM_CMD_NOT_SURPORT_VDEV | DEVMM_CMD_SURPORT_HOST_ID},
    [_KA_IOC_NR(DEVMM_SVM_MEM_GET_ATTR)] = {devmm_ioctl_mem_get_attr, DEVMM_CMD_NOT_SURPORT_HOST_AGENT |
        DEVMM_CMD_NOT_SURPORT_VDEV},
    [_KA_IOC_NR(DEVMM_SVM_MEM_GET_INFO)] = {devmm_ioctl_mem_get_info, DEVMM_CMD_NOT_SURPORT_HOST_AGENT |
        DEVMM_CMD_NOT_SURPORT_VDEV},
    [_KA_IOC_NR(DEVMM_SVM_REGISTER_DMA)] = {devmm_ioctl_register_dma, DEVMM_CONVERT_ID |
        DEVMM_CMD_NOT_SURPORT_VDEV | DEVMM_CMD_NOT_SURPORT_HOST_AGENT | DEVMM_ADD_SUB_REF},
    [_KA_IOC_NR(DEVMM_SVM_UNREGISTER_DMA)] = {devmm_ioctl_unregister_dma, DEVMM_CONVERT_ID |
    DEVMM_CMD_NOT_SURPORT_VDEV | DEVMM_CMD_NOT_SURPORT_HOST_AGENT},
    [_KA_IOC_NR(DEVMM_SVM_MEM_REPLAIR)] = {devmm_ioctl_mem_repair, DEVMM_CMD_NOT_SURPORT_VDEV |
    DEVMM_CMD_NOT_SURPORT_HOST_AGENT | DEVMM_CONVERT_ID | DEVMM_ADD_SUB_REF | DEVMM_CMD_WLOCK},
    [_KA_IOC_NR(DEVMM_SVM_RESERVE_ADDR_INFO_QUERY)] = {devmm_ioctl_resv_addr_info_query, DEVMM_ADD_SUB_REF},
    [_KA_IOC_NR(DEVMM_SVM_PREPARE_CLOSE_DEVICE)] = {devmm_ioctl_prepare_close_device, DEVMM_CONVERT_ID | DEVMM_CMD_WLOCK},
    [_KA_IOC_NR(DEVMM_SVM_MEMCPY_BATCH)] = {devmm_ioctl_memcpy_batch, DEVMM_CMD_NOT_SURPORT_VDEV | DEVMM_CMD_NOT_SURPORT_HOST_AGENT},
    [_KA_IOC_NR(DEVMM_SVM_MEM_MAP_CAP)] = {devmm_ioctl_mem_map_capability, DEVMM_CONVERT_ID | DEVMM_CMD_NOT_SURPORT_VDEV},

};

int devmm_ioctl_handler_register(int cmd, struct devmm_ioctl_handlers_st hander)
{
    if ((cmd < 0) || (cmd >= DEVMM_SVM_CMD_MAX_CMD)) {
        devmm_drv_err("Invalid cmd. (cmd=%d)\n", cmd);
        return -EINVAL;
    }
    devmm_ioctl_handlers[cmd] = hander;
    return 0;
}

static struct devmm_heap_ref *_devmm_find_first_page_ref(struct devmm_svm_heap *heap, u64 va, u32 ref_flag)
{
    u64 tmp_va = ka_base_round_down(va, heap->chunk_page_size);
    struct devmm_heap_ref *ref = NULL;
    u32 pfn;

    pfn = (u32)((tmp_va - heap->start) / heap->chunk_page_size);
    ref = (struct devmm_heap_ref *)&heap->ref[pfn];
    devmm_drv_debug("Show ref informatin. (count=%u; flag=%u; free=%u, lock=%u)\n",
                    ref->count, ref->flag, ref->free, ref->lock);

    if ((ref_flag & DEVMM_IS_MALLOC) == DEVMM_IS_MALLOC) {
        return ref;
    }

    if (!devmm_page_bitmap_is_page_alloced(&heap->page_bitmap[pfn])) {
        goto err_exit;
    }

    if (devmm_page_bitmap_is_first_page(&heap->page_bitmap[pfn])) {
        if (devmm_heap_ref_cnt_is_used_as_ref(ref)) { /* one page */
            return ref;
        }
        return (struct devmm_heap_ref *)&heap->ref[pfn + 1];
    } else {
        if (devmm_heap_ref_cnt_is_used_as_ref(ref)) { /* current va is second page */
            return ref;
        }
        return (struct devmm_heap_ref *)&heap->ref[pfn - ref->count + 1]; /* ref->count:offset */
    }

err_exit:
    /* The log cannot be modified, because in the failure mode library. */
    devmm_drv_err("Va error, please check input argument. (va=%llx; ref_flag=%x; page_bitmap=0x%x; ref=0x%x)\n",
        va, ref_flag, heap->page_bitmap[pfn], heap->ref[pfn]);
    return NULL;
}

struct devmm_heap_ref *devmm_find_first_page_ref(struct devmm_svm_process *svm_process, u64 va, u32 ref_flag)
{
    struct devmm_svm_heap *heap = NULL;

    heap = devmm_svm_get_heap(svm_process, va);
    if (devmm_check_heap_is_entity(heap) == false) {
        devmm_drv_err("Heap is idle, va isn't alloced. (va=0x%llx)\n", va);
        return NULL;
    }

    return _devmm_find_first_page_ref(heap, va, ref_flag);
}

static int devmm_addr_ref_inc(struct devmm_svm_heap *heap, struct devmm_heap_ref *ref, u64 va, u64 size)
{
    int ret;

    ret = devmm_add_page_ref(ref);
    if (ret != 0) {
        return ret;
    }

    if ((heap->heap_sub_type == SUB_RESERVE_TYPE) && (size != SVM_ADDR_REF_OPS_UNKNOWN_SIZE)) {
        ret = devmm_vmmas_occupy_inc(&heap->vmma_mng, va, size);
        if (ret != 0) {
            devmm_sub_page_ref(ref);
        }
    }

    return ret;
}

static void devmm_addr_ref_dec(struct devmm_svm_heap *heap, struct devmm_heap_ref *ref, u64 va, u64 size)
{
    if ((heap->heap_sub_type == SUB_RESERVE_TYPE) && (size != SVM_ADDR_REF_OPS_UNKNOWN_SIZE)) {
        devmm_vmmas_occupy_dec(&heap->vmma_mng, va, size);
    }
    devmm_sub_page_ref(ref);
}

static int devmm_set_page_ref_of_adivse(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap,
    struct devmm_heap_ref *ref, u64 va, u64 size)
{
    u32 *page_bitmap = NULL;
    int ret;

    if ((heap->heap_sub_type == SUB_DEVICE_TYPE) || (heap->heap_sub_type == SUB_DVPP_TYPE) ||
        (heap->heap_sub_type == SUB_RESERVE_TYPE)) {
        page_bitmap = devmm_get_page_bitmap(svm_proc, va);
        if (page_bitmap == NULL) {
            devmm_drv_err("Get page_bitmap failed. (heap_sub_type=%u)\n", heap->heap_sub_type);
            return -EADDRNOTAVAIL;
        }
        if (devmm_page_bitmap_is_dev_mapped(page_bitmap)) {
            ret = devmm_addr_ref_inc(heap, ref, va, size);
        } else {
            ret = devmm_set_page_ref_advise(ref);
        }
    } else {
        ret = devmm_set_page_ref_advise(ref);
    }

    return ret;
}

static void devmm_clear_page_ref_of_advise(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap,
    struct devmm_heap_ref *ref, u64 va, u64 size)
{
    u32 *page_bitmap = NULL;

    if (heap->heap_sub_type == SUB_RESERVE_TYPE) {
        page_bitmap = devmm_get_page_bitmap(svm_proc, va);
        if (page_bitmap == NULL) {
            devmm_drv_warn("Get page_bitmap failed. (heap_sub_type=%u)\n", heap->heap_sub_type);
            return;
        }
        if (devmm_page_bitmap_is_dev_mapped(page_bitmap)) {
            devmm_vmmas_occupy_dec(&heap->vmma_mng, va, size);
        }
    }

    devmm_sub_page_ref(ref);
}

static int _devmm_set_page_ref_before_ioctl(struct devmm_svm_process *svm_proc,
    u32 cmd_flag, struct devmm_ioctl_addr_info *info, u32 index)
{
    int ret;
    if ((cmd_flag & DEVMM_ADD_REF) == DEVMM_ADD_REF) {
        ret = devmm_addr_ref_inc(info->heap[index], info->ref[index], info->va[index], info->size[index]);
    } else if ((cmd_flag & DEVMM_IS_FREE) == DEVMM_IS_FREE) {
        ret = devmm_set_page_ref_free(info->ref[index]);
    } else if ((cmd_flag & DEVMM_IS_MALLOC) == DEVMM_IS_MALLOC) {
        ret = devmm_set_page_ref_malloc(info->ref[index]);
    } else if ((cmd_flag & DEVMM_IS_ADVISE) == DEVMM_IS_ADVISE) {
        ret = devmm_set_page_ref_of_adivse(svm_proc, info->heap[index],
            info->ref[index], info->va[index], info->size[index]);
    } else {
        ret = 0;
    }
    return ret;
}

static void _devmm_clear_page_ref_after_ioctl(struct devmm_svm_process *svm_proc, u32 cmd_flag, int ret,
    struct devmm_ioctl_addr_info *info, u32 index)
{
    if ((cmd_flag & DEVMM_SUB_REF) == DEVMM_SUB_REF) {
        devmm_addr_ref_dec(info->heap[index], info->ref[index], info->va[index], info->size[index]);
    } else if ((cmd_flag & DEVMM_IS_FREE) == DEVMM_IS_FREE) {
        devmm_clear_page_ref_free(info->ref[index], (ret == 0));
    } else if (((cmd_flag & DEVMM_IS_MALLOC) == DEVMM_IS_MALLOC) && (ret != 0)) {
        /* malloc err clear ref */
        devmm_clear_page_ref_malloc(info->ref[index]);
    } else if ((cmd_flag & DEVMM_IS_ADVISE) == DEVMM_IS_ADVISE) {
        devmm_clear_page_ref_of_advise(svm_proc, info->heap[index],
            info->ref[index], info->va[index], info->size[index]);
    } else if ((cmd_flag & DEVMM_ADD_REF) == DEVMM_ADD_REF) {
        if (ret != 0) { /* just add, not has sub flag, if return fail need to sub */
            devmm_addr_ref_dec(info->heap[index], info->ref[index], info->va[index], info->size[index]);
        }
    } else if ((cmd_flag & DEVMM_OPS_SUCCESS_SUB_REF) == DEVMM_OPS_SUCCESS_SUB_REF) {
        if (ret == 0) { /* ops success call sub */
            devmm_addr_ref_dec(info->heap[index], info->ref[index], info->va[index], info->size[index]);
        }
    }
}

static void devmm_oper_page_ref_err_dfx(struct devmm_svm_process *svm_proc, struct devmm_heap_ref *ref,
    u32 ref_flag, u64 va)
{
    bool convert, async;

    convert = devmm_check_va_is_convert(svm_proc, va);
    async = devmm_check_va_is_async_cpying(svm_proc, va);
    devmm_drv_err("Oper address failed. "
        "(va=0x%llx; ref_flag=0x%x; ref_lock=%d; ref_free:%d; ref_count=%d; convert=%d; async=%d)\n",
        va, ref_flag, ref->lock, ref->free, ref->count, convert, async);
}

static bool devmm_should_ops_page_ref(struct devmm_svm_process *svm_proc, u32 cmd_flag)
{
    return ((svm_proc != NULL) && ((cmd_flag & DEVMM_OPER_REF) == DEVMM_OPER_REF));
}

int devmm_set_page_ref_before_ioctl(struct devmm_svm_process *svm_proc, u32 cmd_flag,
    struct devmm_ioctl_addr_info *addr_info)
{
    u32 i, j;
    int ret;

    for (i = 0; i < addr_info->num; i++) {
        if (devmm_va_is_not_svm_process_addr(svm_proc, addr_info->va[i]) != 0) {
            continue;
        }

        addr_info->heap[i] = devmm_svm_get_heap(svm_proc, addr_info->va[i]);
        if (devmm_check_heap_is_entity(addr_info->heap[i]) == false) {
            devmm_drv_err("Heap is idle, va isn't alloced. (va=0x%llx)\n", addr_info->va[i]);
            ret = -EINVAL;
            goto err_out;
        }

        addr_info->ref[i] = _devmm_find_first_page_ref(addr_info->heap[i], addr_info->va[i], cmd_flag);
        if (addr_info->ref[i] == NULL) {
            devmm_drv_err("Va isn't alloced. i=%u(va=0x%llx; ref_flag=0x%x)\n", i, addr_info->va[i], cmd_flag);

            ret = -EINVAL;
            goto err_out;
        }

        ret = _devmm_set_page_ref_before_ioctl(svm_proc, cmd_flag, addr_info, i);
        if (ret != 0) {
            devmm_oper_page_ref_err_dfx(svm_proc, addr_info->ref[i], cmd_flag, addr_info->va[i]);
            goto err_out;
        }
    }

    return 0;
err_out:
    for (j = 0; j < i; j++) {
        if (addr_info->ref[j] == NULL) {
            continue;
        }
        /* just need to add ref, other will not has two addr */
        if ((cmd_flag & DEVMM_ADD_REF) == DEVMM_ADD_REF) {
            devmm_addr_ref_dec(addr_info->heap[j], addr_info->ref[j], addr_info->va[j], addr_info->size[j]);
        }
    }
    return ret;
}

void devmm_clear_page_ref_after_ioctl(struct devmm_svm_process *svm_proc,
    u32 cmd_flag, int ret, struct devmm_ioctl_addr_info *addr_info)
{
    u32 i;

    for (i = 0; i < addr_info->num; i++) {
        if (addr_info->ref[i] == NULL) {
            continue;
        }

        _devmm_clear_page_ref_after_ioctl(svm_proc, cmd_flag, ret, addr_info, i);
    }
}

static int _devmm_ioctl_dispatch(struct devmm_svm_process *svm_proc, u32 cmd_id, u32 cmd_flag,
    struct devmm_ioctl_arg *arg)
{
    struct devmm_ioctl_addr_info addr_info = {0};
    int ret;

    ret = devmm_get_ioctl_addr_info(arg, cmd_id, &addr_info);
    if (ret != 0) {
        return ret;
    }

    ret = devmm_set_page_ref_before_ioctl(svm_proc, cmd_flag, &addr_info);
    if (ret != 0) {
        return ret;
    }

    ret = devmm_ioctl_handlers[cmd_id].ioctl_handler(svm_proc, arg);
    devmm_clear_page_ref_after_ioctl(svm_proc, cmd_flag, ret, &addr_info);
    return ret;
}

int devmm_ioctl_dispatch(struct devmm_svm_process *svm_proc, u32 cmd_id, u32 cmd_flag,
    struct devmm_ioctl_arg *buffer)
{
    if (devmm_should_ops_page_ref(svm_proc, cmd_flag)) {
        return _devmm_ioctl_dispatch(svm_proc, cmd_id, cmd_flag, buffer);
    } else {
        return devmm_ioctl_handlers[cmd_id].ioctl_handler(svm_proc, buffer);
    }
}

void devmm_svm_ioctl_lock(struct devmm_svm_process *svm_proc, u32 lock_flag)
{
    if (svm_proc == NULL) {
        return;
    }

    if ((lock_flag & DEVMM_CMD_NOLOCK) == DEVMM_CMD_NOLOCK) {
        return;
    }

    if ((lock_flag & DEVMM_CMD_WLOCK) == DEVMM_CMD_WLOCK) {
        ka_task_down_write(&svm_proc->ioctl_rwsem);
    } else {
        if (ka_task_down_read_trylock(&svm_proc->ioctl_rwsem) == 0) {
            ka_task_down_read(&svm_proc->ioctl_rwsem);
        }
    }
}

void devmm_svm_ioctl_unlock(struct devmm_svm_process *svm_proc, u32 lock_flag)
{
    if (svm_proc == NULL) {
        return;
    }

    if ((lock_flag & DEVMM_CMD_NOLOCK) == DEVMM_CMD_NOLOCK) {
        return;
    }

    if ((lock_flag & DEVMM_CMD_WLOCK) == DEVMM_CMD_WLOCK) {
        ka_task_up_write(&svm_proc->ioctl_rwsem);
    } else {
        ka_task_up_read(&svm_proc->ioctl_rwsem);
    }
}

u32 devmm_get_logic_id_by_phy_id(struct devmm_svm_process *svm_proc, u32 devid, u32 vfid)
{
    u32 logical_devid;
    u32 phyid_from_svm_pro;
    u32 vfid_from_svm_pro;
    u32 i;

    if (svm_proc == NULL) {
        return DEVMM_INVALID_DEVICE_PHYID;
    }

    for (i = 0; i < (u32)SVM_MAX_AGENT_NUM; i++) {
        phyid_from_svm_pro = devmm_get_phyid_devid_from_svm_process(svm_proc, i);
        vfid_from_svm_pro = devmm_get_vfid_from_svm_process(svm_proc, i);
        if ((phyid_from_svm_pro == devid) && (vfid_from_svm_pro == vfid)) {
            logical_devid = i;
            break;
        }
    }
    if (i >= (u32)SVM_MAX_AGENT_NUM) {
        logical_devid = DEVMM_INVALID_DEVICE_PHYID;
    }

    return logical_devid;
}

int devmm_chan_update_msg_logic_id(struct devmm_svm_process *svm_proc, struct devmm_chan_msg_head *msg_head)
{
    u32 logical_devid;

    if (svm_proc == NULL) {
        msg_head->logical_devid = DEVMM_INVALID_DEVICE_PHYID;
        return 0;
    }

    logical_devid = devmm_get_logic_id_by_phy_id(svm_proc, msg_head->dev_id, msg_head->vfid);
    if (logical_devid >= SVM_MAX_AGENT_NUM) {
        devmm_drv_err("Get logic_id failed. (phyid=%u; vfid=%u; logicid=%u; msg_id=%u)\n",
            msg_head->dev_id, msg_head->vfid, logical_devid, msg_head->msg_id);
        return -ENODEV;
    }
    msg_head->logical_devid = (u16)logical_devid;

    return 0;
}

void devmm_get_svm_id(struct devmm_svm_process *svm_process, u32 *bitmap,
    u32 *logic_id, u32 *phy_id, u32 *vfid)
{
    *logic_id = devmm_page_bitmap_get_devid(bitmap);
    *phy_id = devmm_page_bitmap_get_phy_devid(svm_process, bitmap);
    *vfid = devmm_page_bitmap_get_vfid(svm_process, bitmap);
}

int devmm_fill_svm_id(struct devmm_devid *svm_id, u32 logic_id, u32 phy_id, u32 vfid)
{
    if (logic_id >= SVM_MAX_AGENT_NUM || phy_id >= SVM_MAX_AGENT_NUM || vfid >= DEVMM_MAX_VF_NUM) {
        devmm_drv_err("Input is invalid. (logic_id=%u; phy_id=%u; vfid=%u)\n", logic_id, phy_id, vfid);
        return -EINVAL;
    }
    svm_id->logical_devid = logic_id;
    svm_id->devid = phy_id;
    svm_id->vfid = vfid;
    return 0;
}

int devmm_get_real_phy_devid(u32 devid, u32 vfid, u32 *phy_devid)
{
    int ret = 0;

    *phy_devid = devid;
    if (!uda_is_phy_dev(devid)) {
#ifndef EMU_ST
        struct uda_mia_dev_para mia_para;
        ret = uda_udevid_to_mia_devid(devid, &mia_para);
        if (ret != 0) {
            devmm_drv_err("Udevid to mia devid fail. (ret=%d; devid=%u; vfid=%u)\n", ret, devid, vfid);
            return ret;
        }
        *phy_devid = mia_para.phy_devid;
#endif
    }
    return ret;
}

int devmm_container_vir_to_phs_devid(u32 virtual_devid, u32 *physical_devid, u32 *vfid)
{
    if (devmm_is_host_agent(virtual_devid) == true) {
#ifndef EMU_ST
        *physical_devid = 0;
        *vfid = 0;
        devmm_drv_err("Devid invalid. (devid=%u)\n", virtual_devid);
        return -EINVAL;
#endif
    } else {
        int ret;
        ret = devdrv_manager_container_logical_id_to_physical_id(virtual_devid, physical_devid, vfid);
        if ((ret != 0) || (*physical_devid >= DEVMM_MAX_DEVICE_NUM) || (*vfid >= DEVMM_MAX_VF_NUM)) {
            devmm_drv_err("Convert devid failed. (ret=%d; devid=%u; phyid=%u; vfid=%u)\n",
                ret, virtual_devid, *physical_devid, *vfid);
            return -EINVAL;
        }
    }
    return 0;
}

static void devmm_set_proc_real_phy_devid(struct devmm_svm_process *process, u32 logic_id, u32 devid, u32 vfid)
{
    if (!devmm_is_host_agent(logic_id)) {
        u32 real_phy_devid;
        int ret = devmm_get_real_phy_devid(devid, vfid, &real_phy_devid);
        if (ret != 0) {
            devmm_drv_err("Get read phy devid fail. (devid=%u; vfid=%u)\n", devid, vfid);
            return;
        }
        process->real_phy_devid[logic_id] = real_phy_devid;
    }
}

int devmm_convert_id_from_vir_to_phy(struct devmm_svm_process *process,
    struct devmm_ioctl_arg *buffer, u32 cmd_flag)
{
    u32 phyid_from_svm_pro, vfid_from_svm_pro;
    u32 phyid = DEVMM_INVALID_DEVICE_PHYID;
    u32 vfid = DEVMM_INVALID_DEVICE_PHYID;
    u32 logic_id;
    if ((cmd_flag & DEVMM_CONVERT_ID) == 0) {
        return 0;
    }
    logic_id = buffer->head.devid;
    if ((cmd_flag & DEVMM_CMD_SURPORT_HOST_ID) == 0) {
        if ((((cmd_flag & DEVMM_CMD_NOT_SURPORT_HOST_AGENT) != 0) && (logic_id >= DEVMM_MAX_DEVICE_NUM)) ||
            (((cmd_flag & DEVMM_CMD_NOT_SURPORT_HOST_AGENT) == 0) && (logic_id >= SVM_MAX_AGENT_NUM))) {
            devmm_drv_err("Logic_id is invalid or process is NULL. (logic_id=%d)\n", logic_id);
            return -ENODEV;
        }
    } else {
        if ((logic_id >= DEVMM_MAX_DEVICE_NUM) && (logic_id != uda_get_host_id())) {
            devmm_drv_err("Logic_id is invalid or process is NULL. (logic_id=%d)\n", logic_id);
            return -ENODEV;
        }
    }

    /* process is NULL need convert deviceid everytime */
    if (process == NULL) {
        if (logic_id == uda_get_host_id()) {
            phyid = logic_id;
            vfid = 0;
        } else {
            if (devmm_container_vir_to_phs_devid(logic_id, &phyid, &vfid) != 0) {
                return -EINVAL;
            }
        }

        buffer->head.logical_devid = logic_id;
        buffer->head.devid = phyid;
        buffer->head.vfid = vfid;
        return 0;
    }

    /* process is not NULL init once */
    phyid_from_svm_pro = devmm_get_phyid_devid_from_svm_process(process, logic_id);
    vfid_from_svm_pro = devmm_get_vfid_from_svm_process(process, logic_id);
    if (phyid_from_svm_pro == DEVMM_INVALID_DEVICE_PHYID || vfid_from_svm_pro == DEVMM_INVALID_DEVICE_PHYID) {
        if (logic_id == uda_get_host_id()) {
            phyid = logic_id;
            vfid = 0;
        } else {
            if (devmm_container_vir_to_phs_devid(logic_id, &phyid, &vfid) != 0) {
                return -EINVAL;
            }
        }
        devmm_set_proc_real_phy_devid(process, logic_id, phyid, vfid);
        devmm_set_phyid_devid_to_svm_process(process, logic_id, phyid);
        devmm_set_vfid_to_svm_process(process, logic_id, vfid);
        buffer->head.logical_devid = logic_id;
        buffer->head.devid = phyid;
        buffer->head.vfid = vfid;
    } else {
        buffer->head.logical_devid = logic_id;
        buffer->head.devid = phyid_from_svm_pro;
        buffer->head.vfid = vfid_from_svm_pro;
    }

    return 0;
}

int devmm_check_cmd_support(u32 cmd_flag)
{
    if ((cmd_flag & DEVMM_CMD_NOT_SURPORT_VDEV) != 0) {
        if (devmm_current_is_vdev()) {
            return -EOPNOTSUPP;
        }
    }

    return 0;
}


u32 devmm_get_vfid_from_svm_process(struct devmm_svm_process *svm_process, u32 logic_id)
{
    return svm_process->vfid[logic_id];
}

u32 devmm_get_phyid_devid_from_svm_process(struct devmm_svm_process *svm_process, u32 logic_id)
{
    return svm_process->phy_devid[logic_id];
}

void devmm_set_phyid_devid_to_svm_process(struct devmm_svm_process *svm_process, u32 logic_id, u32 phyid)
{
    svm_process->phy_devid[logic_id] = phyid;
}

void devmm_set_vfid_to_svm_process(struct devmm_svm_process *svm_process, u32 logic_id, u32 vfid)
{
    svm_process->vfid[logic_id] = vfid;
}

u32 devmm_page_bitmap_get_vfid(struct devmm_svm_process *svm_process, u32 *bitmap)
{
    u32 logic_id = devmm_page_bitmap_get_value(bitmap, DEVMM_PAGE_DEVID_SHIT, DEVMM_PAGE_DEVID_WID);
    return devmm_get_vfid_from_svm_process(svm_process, logic_id);
}

u32 devmm_page_bitmap_get_phy_devid(struct devmm_svm_process *svm_process, u32 *bitmap)
{
    u32 logic_id = devmm_page_bitmap_get_value(bitmap, DEVMM_PAGE_DEVID_SHIT, DEVMM_PAGE_DEVID_WID);
    return devmm_get_phyid_devid_from_svm_process(svm_process, logic_id);
}

void devmm_init_dev_private(struct devmm_svm_dev *dev, ka_file_operations_t *svm_fops)
{
    struct ka_sysinfo chuck_info = {0};

    ka_si_meminfo(&chuck_info);
    dev->device_info.host_ddr = (u64)chuck_info.totalram * KA_MM_PAGE_SIZE;
    dev->device_info.host_ddr = (1ul << (u64)ka_mm_get_order(dev->device_info.host_ddr)) * KA_MM_PAGE_SIZE;
    ka_base_atomic64_add((long long)dev->device_info.host_ddr, &dev->device_info.total_ddr);

    devmm_ipc_mem_init();
    devmm_get_sys_mem();
    devmm_init_dev_set_mmap_para(&dev->mmap_para);
    devmm_register_query_func();
    devmm_proc_fs_init(dev);
}

void devmm_uninit_dev_private(struct devmm_svm_dev *dev)
{
    devmm_ipc_mem_uninit();
    devmm_proc_fs_uninit();
    devmm_unregister_query_func();
    devmm_dev_res_mng_destroy_all();
}

u32 devmm_shm_get_page_size(struct devmm_svm_process_id *process_id, u64 va, u64 size)
{
    return 0;
}

int devmm_sdma_page_kernel_memzero(ka_page_t *pg, u64 page_size)
{
    return -EINVAL;
}

int devmm_inc_page_ref(struct devmm_svm_process *svm_proc, u64 va, u64 size)
{
    struct devmm_svm_heap *heap = NULL;
    struct devmm_heap_ref *ref = NULL;
    int ret;

    if (devmm_va_is_not_svm_process_addr(svm_proc, va)) {
        return 0;
    }

    heap = devmm_svm_heap_get(svm_proc, va);
    if (devmm_check_heap_is_entity(heap) == false) {
#ifndef EMU_ST
        devmm_svm_heap_put(heap);
#endif
        devmm_drv_err("Heap is idle, va isn't alloced. (va=0x%llx)\n", va);
        return -EINVAL;
    }

    ref = _devmm_find_first_page_ref(heap, va, 0);
    if (ref == NULL) {
#ifndef EMU_ST
        devmm_svm_heap_put(heap);
#endif
        devmm_drv_err("Va isn't alloced. (va=0x%llx)\n", va);
        return -EINVAL;
    }

    devmm_page_ref_lock(ref);
    ref->count++;
    devmm_page_ref_unlock(ref);

    if ((heap->heap_sub_type == SUB_RESERVE_TYPE) && (size != SVM_ADDR_REF_OPS_UNKNOWN_SIZE)) {
        ret = devmm_vmmas_occupy_inc(&heap->vmma_mng, va, size);
        if (ret != 0) {
            devmm_sub_page_ref(ref);
#ifndef EMU_ST
            devmm_svm_heap_put(heap);
#endif
            return ret;
        }
    }
    devmm_svm_heap_put(heap);

    return 0;
}

void devmm_dec_page_ref(struct devmm_svm_process *svm_proc, u64 va, u64 size)
{
    struct devmm_svm_heap *heap = NULL;
    struct devmm_heap_ref *ref = NULL;

    if (devmm_va_is_not_svm_process_addr(svm_proc, va)) {
        return;
    }

    heap = devmm_svm_heap_get(svm_proc, va);
    if (devmm_check_heap_is_entity(heap) == false) {
        devmm_svm_heap_put(heap);
        devmm_drv_err("Heap is idle, va isn't alloced. (va=0x%llx)\n", va);
        return;
    }

    ref = _devmm_find_first_page_ref(heap, va, 0);
    if (ref == NULL) {
        devmm_svm_heap_put(heap);
        devmm_drv_err("Va isn't alloced. (va=0x%llx)\n", va);
        return;
    }

    if ((heap->heap_sub_type == SUB_RESERVE_TYPE) && (size != SVM_ADDR_REF_OPS_UNKNOWN_SIZE)) {
        devmm_vmmas_occupy_dec(&heap->vmma_mng, va, size);
    }

    devmm_page_ref_lock(ref);
    ref->count--;
    devmm_page_ref_unlock(ref);
    devmm_svm_heap_put(heap);
}

int devmm_check_thread_valid(int hostpid, const char *sign, u32 len)
{
    return (hostpid == ka_task_get_current_tgid()) ? 0 : -EINVAL;
}
KA_EXPORT_SYMBOL_GPL(devmm_check_thread_valid);

int devmm_svm_davinci_module_init(const ka_file_operations_t *ops)
{
    int ret;

    ret = drv_davinci_register_sub_module(DAVINCI_SVM_SUB_MODULE_NAME, (ka_file_operations_t *)ops);
    if (ret != 0) {
        devmm_drv_err("Register sub module failed. (ret=%d)\n", ret);
        return -ENODEV;
    }
    return 0;
}

void devmm_svm_davinci_module_uninit(void)
{
    int ret;

    ret = drv_ascend_unregister_sub_module(DAVINCI_SVM_SUB_MODULE_NAME);
    if (ret != 0) {
        devmm_drv_err("Unregister sub module failed. (ret=%d)\n", ret);
        return;
    }
    return;
}

static ka_class_t *devmm_cdev_class = NULL;
/* create useless cdev to be compatible with container */
int devmm_svm_dev_init(ka_file_operations_t *ops)
{
    ka_device_t *class_cdev = NULL;
    const char *chrdev_name = NULL;
    ka_dev_t devno;
    u32 major;
    int ret;

    chrdev_name = devmm_get_chrdev_name();
    ret = ka_fs_alloc_chrdev_region(&devmm_svm->dev_no, 0, 1, chrdev_name);
    if (ret != 0) {
        devmm_drv_err("Alloc_chrdev_region error.\n");
        return ret;
    }

    /* init and add char device */
    major = KA_DRIVER_MAJOR(devmm_svm->dev_no);
    devno = KA_DRIVER_MKDEV(major, 0);
    ka_fs_cdev_init(&devmm_svm->char_dev, ops);
    devmm_svm->char_dev.owner = KA_THIS_MODULE;
    ret = ka_fs_cdev_add(&devmm_svm->char_dev, devno, 1);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_err("Cdevice_add error. (major=%u; devno=%d)\n", major, (int)devno);
        goto cdev_add_fail;
#endif
    }

    /* after the process followed, mmapi node in /dev setups automatically */
    devmm_cdev_class = ka_driver_class_create(KA_THIS_MODULE, chrdev_name);
    if (KA_IS_ERR_OR_NULL(devmm_cdev_class)) {
#ifndef EMU_ST
        devmm_drv_err("Class_create error. (major=%u; devno=%d)\n", major, (int)devno);
        ret = -EBUSY;
        goto class_create_fail;
#endif
    }

    class_cdev = ka_driver_device_create(devmm_cdev_class, NULL, devno, NULL, "%s", chrdev_name);
    if (KA_IS_ERR_OR_NULL(class_cdev)) {
#ifndef EMU_ST
        devmm_drv_err("Device_create error.\n");
        ret = -ENOTBLK;
        goto device_create_fail;
#endif
    }
    devmm_svm->dev = class_cdev;

    return 0;

#ifndef EMU_ST
device_create_fail:
    ka_driver_class_destroy(devmm_cdev_class);
class_create_fail:
    ka_fs_cdev_del(&devmm_svm->char_dev);
cdev_add_fail:
    ka_fs_unregister_chrdev_region(devmm_svm->dev_no, 1);

    return ret;
#endif
}

void devmm_svm_dev_destory(void)
{
    u32 major = KA_DRIVER_MAJOR(devmm_svm->dev_no);
    ka_dev_t devno = KA_DRIVER_MKDEV(major, 0);

    ka_driver_device_destroy(devmm_cdev_class, devno);
    ka_driver_class_destroy(devmm_cdev_class);
    ka_fs_cdev_del(&devmm_svm->char_dev);
    ka_fs_unregister_chrdev_region(devno, 1);
}
