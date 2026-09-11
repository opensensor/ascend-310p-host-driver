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
#include "kernel_version_adapt.h"

#include "devmm_proc_info.h"
#include "devmm_common.h"
#include "svm_vmma_mng.h"
#include "devmm_mem_alloc_interface.h"
#include "svm_heap_mng.h"

#define DEVMM_THIRD_PAGE 2

int devmm_get_virt_pfn_by_heap(const struct devmm_svm_heap *heap, u64 va, unsigned long *pfn)
{
    if (heap == NULL) {
        devmm_drv_err("Heap is NULL.\n");
#ifndef EMU_ST
        return -EINVAL;
#endif
    }
    if (heap->heap_type == DEVMM_HEAP_IDLE) {
        devmm_drv_err("Heap->heap_type is idle. (va=0x%llx)\n", va);
        return -EINVAL;
    }

    if ((va < heap->start) || (va >= (heap->start + heap->heap_size))) {
        devmm_drv_err("Vaddress overflow. (va=0x%llx; start=0x%llx; size=%llu)\n", va, heap->start,  heap->heap_size);
        return -EINVAL;
    }

    *pfn = (va - heap->start) / heap->chunk_page_size;
    return 0;
}

struct devmm_heap_ref *devmm_get_page_ref(struct devmm_svm_heap *heap, u64 va)
{
    unsigned long pfn;
    int ret;

    ret = devmm_get_virt_pfn_by_heap(heap, va, &pfn);
    if (ret != 0) {
        devmm_drv_err("Fail to get virt pfn of va. (va=0x%llx)\n", va);
        return NULL;
    }

    return (struct devmm_heap_ref *)&heap->ref[pfn];
}

u64 devmm_get_page_num_by_pfn(struct devmm_svm_heap *heap, u64 pfn)
{
    struct devmm_heap_ref *ref = (struct devmm_heap_ref *)&heap->ref[pfn];
    u32 *bitmap = &heap->page_bitmap[pfn];
    struct devmm_heap_ref *tmp_ref = NULL;

    if (devmm_page_bitmap_is_first_page(bitmap)) {
        if (devmm_heap_ref_cnt_is_used_as_ref(ref)) {
            return 1;
        }
        return ref->count;
    }

    /* pfn is not first va's pfn */
    tmp_ref = devmm_heap_ref_cnt_is_used_as_ref(ref) ? (struct devmm_heap_ref *)&heap->ref[pfn - 1]
                                                     : (struct devmm_heap_ref *)&heap->ref[pfn - ref->count];
    return tmp_ref->count;
}

u64 devmm_get_page_num_from_va(struct devmm_svm_heap *heap, u64 va)
{
    unsigned long pfn;
    int ret;

    ret = devmm_get_virt_pfn_by_heap(heap, va, &pfn);
    if (ret != 0) {
        devmm_drv_err("Get vaddress pfn fail. (va=0x%llx)\n", va);
        return 0;
    }
    return devmm_get_page_num_by_pfn(heap, pfn);
}

u64 devmm_get_alloced_size_from_va(struct devmm_svm_heap *heap, u64 va)
{
    u64 page_num;
    u64 size;

    page_num = devmm_get_page_num_from_va(heap, va);
    size = page_num * heap->chunk_page_size;

    return size;
}

int devmm_get_alloced_va_fst_pfn(struct devmm_svm_heap *heap, u64 va, unsigned long *fst_pfn)
{
    struct devmm_heap_ref *ref = NULL;
    unsigned long pfn;

    if (devmm_get_virt_pfn_by_heap(heap, va, &pfn) != 0) {
        devmm_drv_err("Vaddress get pfn failed. (va=0x%llx)\n", va);
        return -EINVAL;
    }

    if (!devmm_page_bitmap_is_page_alloced(&heap->page_bitmap[pfn])) {
        devmm_drv_err("Vaddress is not alloced. (va=0x%llx)\n", va);
        return -EINVAL;
    }

    if (devmm_page_bitmap_is_first_page(&heap->page_bitmap[pfn])) {
        *fst_pfn = pfn;
    } else {
        ref = (struct devmm_heap_ref *)&heap->ref[pfn];
        if (devmm_heap_ref_cnt_is_used_as_ref(ref)) {
            *fst_pfn = pfn - 1;
        } else {
            *fst_pfn = pfn - ref->count;
        }
    }

    return 0;
}

int devmm_svm_check_bitmap_available(u32 *page_bitmap, size_t size, size_t page_size)
{
    u32 page_num, i;
    u64 tmp_page_size;

    tmp_page_size = (page_size != 0) ? page_size : KA_MM_PAGE_SIZE;
    page_num = (u32)(size / tmp_page_size);
    if (size % tmp_page_size != 0) {
        page_num++;
    }

    for (i = 0; i < page_num; i++) {
        if (!devmm_page_bitmap_is_page_available(page_bitmap + i)) {
            devmm_drv_err("Bit map none alloc. (va_offset=0x%llx; size=%lu)\n", i * tmp_page_size, size);
            return DEVMM_FALSE;
        }
    }
    return DEVMM_TRUE;
}

void devmm_svm_set_bitmap_mapped(u32 *page_bitmap, size_t size, size_t page_size, unsigned int devid)
{
    u32 mapped_flag, page_num, i;
    u64 tmp_page_size;

    tmp_page_size = (page_size != 0) ? page_size : KA_MM_PAGE_SIZE;
    page_num = (u32)(size / tmp_page_size);
    if (size % tmp_page_size != 0) {
        page_num++;
    }
    if (devid < SVM_MAX_AGENT_NUM) {
        mapped_flag = DEVMM_PAGE_DEV_MAPPED_MASK;
    } else {
        mapped_flag = DEVMM_PAGE_HOST_MAPPED_MASK;
    }
    for (i = 0; i < page_num; i++) {
        if (!devmm_page_bitmap_is_page_available(page_bitmap + i)) {
            devmm_drv_err("Bit map none alloc. (va_offset=0x%llx; size=%lu; devid=%u)\n",
                (u64)i * tmp_page_size, size, devid);
            return;
        }
        devmm_page_bitmap_set_flag(page_bitmap + i, mapped_flag);
        if (devid < SVM_MAX_AGENT_NUM) {
            devmm_page_bitmap_set_devid(page_bitmap + i, devid);
        }
    }
}

void devmm_svm_clear_bitmap_mapped(u32 *page_bitmap, size_t size, size_t page_size, u32 devid)
{
    u32 mapped_flag, page_num, i;
    u64 tmp_page_size;

    tmp_page_size = (page_size != 0) ? page_size : KA_MM_PAGE_SIZE;
    page_num = (u32)(size / tmp_page_size);
    if (size % tmp_page_size != 0) {
        page_num++;
    }
    if (devid < SVM_MAX_AGENT_NUM) {
        mapped_flag = DEVMM_PAGE_DEV_MAPPED_MASK;
    } else {
        mapped_flag = DEVMM_PAGE_HOST_MAPPED_MASK;
    }
    for (i = 0; i < page_num; i++) {
        if (!devmm_page_bitmap_is_page_available(page_bitmap + i)) {
            devmm_drv_warn("Bit map none alloc. (va_offset=0x%llx; size=%lu; devid=%u)\n",
                           i * tmp_page_size, size, devid);
            return;
        }
        devmm_page_bitmap_clear_flag(page_bitmap + i, mapped_flag);
        if (!devmm_page_bitmap_is_dev_mapped(page_bitmap + i)) {
            devmm_page_bitmap_clear_flag(page_bitmap + i, DEVMM_PAGE_ADVISE_POPULATE_MASK);
        }
    }
}

/*
 * Description: handle of new heap/del heap of host pin memory
 *   or svm memory
 * Params:
 *   @arg: arg of user state
 * Return:
 *   @ret:ok or fail
 */
int devmm_alloc_new_heap_pagebitmap(struct devmm_svm_heap *heap)
{
    unsigned long page_cnt;

    if (heap == NULL) {
        return -ENOMEM;
    }
    page_cnt = heap->heap_size / heap->chunk_page_size;
    heap->page_bitmap = (u32 *)__devmm_vmalloc_ex(page_cnt * sizeof(u32),
        KA_GFP_KERNEL | __KA_GFP_ZERO | __KA_GFP_NOWARN | __KA_GFP_ACCOUNT, KA_PAGE_KERNEL);
    devmm_drv_debug("Vmalloc page_bitmap heap. (start=%llx; heap_size=%llu; page_cnt=%lu)\n",
        heap->start, heap->heap_size, page_cnt);
    if (heap->page_bitmap == NULL) {
        devmm_drv_err("Vmalloc page_bitmap fail. (page_cnt=%lu)\n", page_cnt);
        return -ENOMEM;
    }
    heap->ref = (u32 *)__devmm_vmalloc_ex(page_cnt * sizeof(u32), KA_GFP_KERNEL | __KA_GFP_ZERO | __KA_GFP_NOWARN | __KA_GFP_ACCOUNT,
        KA_PAGE_KERNEL);
    if (heap->ref == NULL) {
        devmm_vfree_ex(heap->page_bitmap);
        heap->page_bitmap = NULL;
        devmm_drv_err("Vmalloc page_ref fail. (page_cnt=%lu)\n", page_cnt);
        return -ENOMEM;
    }

    return 0;
}

void devmm_free_heap_pagebitmap_ref(struct devmm_svm_heap *heap)
{
    if (heap->page_bitmap != NULL) {
        devmm_vfree_ex(heap->page_bitmap);
        heap->page_bitmap = NULL;
    }
    if (heap->ref != NULL) {
        devmm_vfree_ex(heap->ref);
        heap->ref = NULL;
    }
}

int devmm_set_page_ref_free(struct devmm_heap_ref *ref)
{
    devmm_page_ref_lock(ref);
    if ((ref->count != 1) || (ref->free != 0)) {
        /* The log cannot be modified, because in the failure mode library. */
        devmm_drv_err("Set free error. (ref_lock=%d; ref_free=%d; ref_count=%d)\n",
            ref->lock, ref->free, ref->count);
        devmm_page_ref_unlock(ref);
        return -EBUSY;
    }
    ref->free = 1;
    devmm_page_ref_unlock(ref);
    return 0;
}

void devmm_clear_page_ref_free(struct devmm_heap_ref *ref, u32 clear_flag)
{
    devmm_page_ref_lock(ref);
    ref->free = 0;
    ref->count = (clear_flag == 1) ? (u32)0 : ref->count;
    devmm_page_ref_unlock(ref);
}

int devmm_set_page_ref(struct devmm_svm_heap *heap, u64 fst_va, u64 chunk_cnt)
{
    struct devmm_heap_ref *ref = NULL;
    u32 stamp = (u32)ka_jiffies;
    u64 i;

    devmm_drv_debug("Page_ref details. (fst_va=0x%llx; page_num=%llu)\n", fst_va, chunk_cnt);
    ref = devmm_get_page_ref(heap, fst_va);
    if (ref == NULL) {
        devmm_drv_err("Can not find heap_ref. (va=0x%llx)\n", fst_va);
        return -EINVAL;
    }

    devmm_page_ref_lock(ref);
    /* just one page of va */
    if (chunk_cnt == 1) {
        devmm_heap_ref_set_flag(ref, 1);
        devmm_heap_ref_set_cnt(ref, 1); /* first page's ref.count used as ref */
        devmm_page_ref_unlock(ref);
        return 0;
    }

    /* set first page */
    devmm_heap_ref_set_flag(ref, 0);
    devmm_heap_ref_set_cnt(ref, (u32)chunk_cnt);

    /* set second page */
    devmm_heap_ref_set_flag(ref + 1, 1);
    devmm_heap_ref_set_cnt(ref + 1, 1); /* second page's ref.count used as ref */

    for (i = DEVMM_THIRD_PAGE; i < chunk_cnt; i++) {
        devmm_heap_ref_set_flag(ref + i, 0);
        devmm_heap_ref_set_cnt(ref + i, (u32)i);
        devmm_try_cond_resched(&stamp);
    }
    devmm_page_ref_unlock(ref);

    return 0;
}

void devmm_clean_page_ref(struct devmm_heap_ref *ref)
{
    devmm_page_ref_lock(ref);
    ref->count = 0;
    ref->flag = 0;
    devmm_page_ref_unlock(ref);
}

void devmm_destroy_reserve_heap_mem(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap)
{
    u64 page_cnt = heap->heap_size / heap->chunk_page_size;
    u32 *page_bitmap = heap->page_bitmap;
    u32 stamp = (u32)ka_jiffies;
    u64 i;

    devmm_vmmas_destroy(svm_proc, &heap->vmma_mng);
    for (i = 0; i < page_cnt; i++) {
        devmm_page_clean_bitmap(page_bitmap + i);
        devmm_try_cond_resched(&stamp);
    }
}
