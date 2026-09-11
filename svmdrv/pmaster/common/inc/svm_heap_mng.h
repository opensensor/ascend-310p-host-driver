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

#ifndef SVM_HEAP_MNG_H
#define SVM_HEAP_MNG_H

#include <asm/processor.h>

#include "svm_log.h"
#include "svm_vmma_mng.h"
#include "devmm_common.h"

/*
 * one va page has a byte to mantain va status
 *    bit21~31 dev id
 *    bit20: mem mapped
 *    bit19: p2p ddr
 *    bit18: phy mem with continuty
 *    bit17: bitmap_locked_flag
 *    bit16: first remote map flag
 *    bit15: readonly mem
 *    bit14: ts ddr
 *    bit13: p2p hbm
 *    bit12: memory shared, it is not related to IPC, when user call prefetch or dev1 to access the page table
 *           created by dev0 due to page fault. The physical address is at the peer end.
 *    bit11: locked device
 *    bit10: locked host
 *    bit9: alloced
 *    bit8: is tranlate pa by rts
 *    bit7: advise:populate (no fault memory)
 *    bit6: advise:1-ddr,0-hbm(default)
 *    bit5: is first page of a va?
 *    bit4: host mapped
 *    bit3: dev mapped
 *    bit2: remote maped, when va is locked host or device, remote side can map and access it
 *    bit2: multiplexing to device fault get sync_data flag(DEVMM_PAGE_NOSYNC_FLG)
 *    bit1: ipc create flag, set in first page
 *    bit0: ipc open flag, set in every page
 */

#define DEVMM_PAGE_IPC_MEM_OPEN_BIT 0
#define DEVMM_PAGE_IPC_MEM_CREATE_BIT 1
#define DEVMM_PAGE_REMOTE_MAPPED_BIT 2
#define DEVMM_PAGE_DEV_MAPPED_BIT 3
#define DEVMM_PAGE_HOST_MAPPED_BIT 4
#define DEVMM_PAGE_IS_FIRST_PAGE_BIT 5
#define DEVMM_PAGE_ADVISE_DDR_BIT 6
#define DEVMM_PAGE_ADVISE_POPULATE_BIT 7
#define DEVMM_PAGE_IS_TRANSLATE_BIT 8
#define DEVMM_PAGE_ALLOCED_BIT 9
#define DEVMM_PAGE_LOCKED_HOST_BIT 10
#define DEVMM_PAGE_LOCKED_DEVICE_BIT 11
#define DEVMM_PAGE_ADVISE_MEMORY_SHARED_BIT 12
#define DEVMM_PAGE_ADVISE_P2P_HBM_BIT 13
#define DEVMM_PAGE_ADVISE_TS_BIT 14
#define DEVMM_PAGE_READONLY_BIT 15
#define DEVMM_PAGE_REMOTE_MAPPED_FIRST_BIT 16
#define DEVMM_PAGE_BITMAP_LOCKED_BIT 17
#define DEVMM_PAGE_ADVISE_CONTINUTY_BIT 18
#define DEVMM_PAGE_ADVISE_P2P_DDR_BIT 19
#define DEVMM_PAGE_MEM_MAPPED_BIT 20
#define DEVMM_PAGE_DEVID_SHIT 21
#define DEVMM_PAGE_DEVID_WID 11

#define DEVMM_PAGE_INVAILD_BIT 32

/* MEROS FOR BITMAP* */
#define DEVMM_PAGE_REMOTE_MAPPED_MASK (1UL << DEVMM_PAGE_REMOTE_MAPPED_BIT)
#define DEVMM_PAGE_REMOTE_MAPPED_FIRST_MASK (1UL << DEVMM_PAGE_REMOTE_MAPPED_FIRST_BIT)
#define DEVMM_PAGE_DEV_MAPPED_MASK (1UL << DEVMM_PAGE_DEV_MAPPED_BIT)
#define DEVMM_PAGE_HOST_MAPPED_MASK (1UL << DEVMM_PAGE_HOST_MAPPED_BIT)
#define DEVMM_PAGE_IS_FIRST_PAGE_MASK (1UL << DEVMM_PAGE_IS_FIRST_PAGE_BIT)
#define DEVMM_PAGE_ADVISE_DDR_MASK (1UL << DEVMM_PAGE_ADVISE_DDR_BIT)
#define DEVMM_PAGE_ADVISE_POPULATE_MASK (1UL << DEVMM_PAGE_ADVISE_POPULATE_BIT)
#define DEVMM_PAGE_IS_TRANSLATE_MASK (1UL << DEVMM_PAGE_IS_TRANSLATE_BIT)
#define DEVMM_PAGE_ALLOCED_MASK (1UL << DEVMM_PAGE_ALLOCED_BIT)
#define DEVMM_PAGE_LOCKED_HOST_MASK (1UL << DEVMM_PAGE_LOCKED_HOST_BIT)
#define DEVMM_PAGE_LOCKED_DEVICE_MASK (1UL << DEVMM_PAGE_LOCKED_DEVICE_BIT)
#define DEVMM_PAGE_ADVISE_MEMORY_SHARED_MASK (1ULL << DEVMM_PAGE_ADVISE_MEMORY_SHARED_BIT)
#define DEVMM_PAGE_ADVISE_P2P_HBM_MASK (1UL << DEVMM_PAGE_ADVISE_P2P_HBM_BIT)
#define DEVMM_PAGE_ADVISE_P2P_DDR_MASK (1UL << DEVMM_PAGE_ADVISE_P2P_DDR_BIT)
#define DEVMM_PAGE_ADVISE_TS_MASK (1UL << DEVMM_PAGE_ADVISE_TS_BIT)
#define DEVMM_PAGE_DEVID_MASK (((1UL << DEVMM_PAGE_DEVID_WID) - 1) << DEVMM_PAGE_DEVID_SHIT)
#define DEVMM_PAGE_BITMAP_LOCKED_MASK (1U << DEVMM_PAGE_BITMAP_LOCKED_BIT)
#define DEVMM_PAGE_IPC_MEM_OPEN_MASK (1ULL << DEVMM_PAGE_IPC_MEM_OPEN_BIT)
#define DEVMM_PAGE_IPC_MEM_CREATE_MASK (1ULL << DEVMM_PAGE_IPC_MEM_CREATE_BIT)
#define DEVMM_PAGE_CONTINUTY_MASK (1ULL << DEVMM_PAGE_ADVISE_CONTINUTY_BIT)
#define DEVMM_PAGE_READONLY_MASK (1UL << DEVMM_PAGE_READONLY_BIT)
#define DEVMM_PAGE_MEM_MAPPED_MASK (1UL << DEVMM_PAGE_MEM_MAPPED_BIT)

/* MEROS FOR PAGE REF */
#define DEVMM_HEAP_PAGE_LOCK_FLAG_BIT 31
#define DEVMM_HEAP_PAGE_LOCK_FLAG_MASK (1u << DEVMM_HEAP_PAGE_LOCK_FLAG_BIT)

/* in the case of multi pages, first page's ref.count indicates pages count, second page's ref.count used as ref count
 * cases by bitmap and heap_ref as follow:
 * -----------------------------------------------------------------------------------------------------------------
 *             |  bitmap.first_page:1                 |  bitmap.first_page:0
 * -----------------------------------------------------------------------------------------------------------------
 * ref.flag:0  |  ref.count:pages count of va         |  ref.count:offset from current va to first va
 * -----------------------------------------------------------------------------------------------------------------
 * ref.flag:1  |  ref.count:(only one page)ref of va  |  ref.count:current va is second page, its count used as ref.
 * -----------------------------------------------------------------------------------------------------------------
 */
struct devmm_heap_ref {
    u32 count : 29;  /* pages count(max:2TB) / offset / ref count */
    u32 flag : 1;    /* switch flag to express different meanings of count */

    u32 free : 1;    /* free flag */
    u32 lock : 1;    /* lock ref */
};

#define DEVMM_HEAP_USED_BITS_NUM_MAX 128U
#define DEVMM_DYN_HEAP_USED_BITS_NUM_MAX (128 * 1024U) /* 128T */
struct devmm_svm_heap {
    u32 heap_type;
    u32 heap_sub_type;
    u32 heap_idx;     /* point to the first addr heap */
    u32 chunk_page_size;       /* Bitmap's granularity, not equal with pagesize */
    u64 start;
    unsigned long *used_mask;    /* accelerate proc exit process */
    u64 heap_size;
    /*
     * page_bitmap define see svm_heap_mng.h define
     */
    u32 *page_bitmap;          /* A page_bitmap manages a chunk_page_size. */
    /*
     * bit31:lock ref
     * bit30:free ref
     * bit29:flag
     * bit0-28:page count/offset/ref
     */
    u32 *ref;
    ka_atomic64_t occupy_cnt;
    bool is_invalid;
    struct devmm_vmma_mng vmma_mng;
};

int devmm_set_page_ref_free(struct devmm_heap_ref *ref);
void devmm_clear_page_ref_free(struct devmm_heap_ref *ref, u32 clear_flag);
int devmm_get_virt_pfn_by_heap(const struct devmm_svm_heap *heap, u64 va, unsigned long *pfn);
int devmm_get_alloced_va_fst_pfn(struct devmm_svm_heap *heap, u64 va, unsigned long *fst_pfn);
void devmm_svm_set_bitmap_mapped(u32 *page_bitmap, size_t size, size_t page_size, unsigned int devid);
void devmm_svm_clear_bitmap_mapped(u32 *page_bitmap, size_t size, size_t page_size, u32 devid);
u64 devmm_get_page_num_by_pfn(struct devmm_svm_heap *heap, u64 pfn);
u64 devmm_get_page_num_from_va(struct devmm_svm_heap *heap, u64 va);
u64 devmm_get_alloced_size_from_va(struct devmm_svm_heap *heap, u64 va);
int devmm_set_page_ref(struct devmm_svm_heap *heap, u64 fst_va, u64 chunk_cnt);
int devmm_svm_check_bitmap_available(u32 *page_bitmap, size_t size, size_t page_size);
struct devmm_heap_ref *devmm_get_page_ref(struct devmm_svm_heap *heap, u64 va);
void devmm_clean_page_ref(struct devmm_heap_ref *ref);
int devmm_alloc_new_heap_pagebitmap(struct devmm_svm_heap *heap);
void devmm_free_heap_pagebitmap_ref(struct devmm_svm_heap *heap);
void devmm_destroy_reserve_heap_mem(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap);

static inline bool devmm_test_and_set_bit(u32 mask_bit, unsigned int *p)
{
    int old;
    int mask = (int)mask_bit;

    if ((KA_READ_ONCE(*p) & mask) != 0) {
        return 1;
    }
    old = ka_base_atomic_fetch_or(mask, (ka_atomic_t *)p);
    return ((old & mask) != 0);
}

/*
 * bitmap加解锁操作：
 * devmm_page_bitmap_lock()/devmm_page_bitmap_unlock()必须配套使用
 */
static inline void devmm_page_bitmap_lock(u32 *bitmap)
{
    while (devmm_test_and_set_bit(DEVMM_PAGE_BITMAP_LOCKED_MASK, bitmap) != 0) {
        cpu_relax();
    }
}

static inline void devmm_page_bitmap_unlock(u32 *bitmap)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 3, 0)
    ka_base_atomic_and((int)(~DEVMM_PAGE_BITMAP_LOCKED_MASK), (ka_atomic_t *)bitmap);
#else
    atomic_clear_mask(DEVMM_PAGE_BITMAP_LOCKED_MASK, (ka_atomic_t *)bitmap);
#endif
}

static inline void devmm_page_bitmap_set_value_nolock(u32 *bitmap, u32 shift, u32 wide, u32 value)
{
    u32 msk = ((1U << wide) - 1);
    u32 val = (msk & value);

    (*bitmap) &= (u32)(~(msk << shift));
    (*bitmap) |= (u32)(val << shift);
}

static inline void devmm_page_bitmap_set_value(u32 *bitmap, u32 shift, u32 wide, u32 value)
{
    devmm_page_bitmap_lock(bitmap);
    devmm_page_bitmap_set_value_nolock(bitmap, shift, wide, value);
    devmm_page_bitmap_unlock(bitmap);
}

static inline u32 devmm_page_bitmap_get_value(u32 *bitmap, u32 shift, u32 wide)
{
    u32 msk = (u32)((1UL << wide) - 1);
    u32 val = (*bitmap) >> shift;

    return (val & msk);
}

static inline void devmm_page_clean_bitmap(u32 *bitmap)
{
    devmm_page_bitmap_lock(bitmap);
    (*bitmap) &= DEVMM_PAGE_BITMAP_LOCKED_MASK;
    devmm_page_bitmap_unlock(bitmap);
}

static inline u32 devmm_page_read_bitmap(u32 *bitmap)
{
    return ((*bitmap) & (~DEVMM_PAGE_BITMAP_LOCKED_MASK));
}

static inline void devmm_page_bitmap_set_devid(u32 *bitmap, u32 devid)
{
    devmm_page_bitmap_set_value(bitmap, DEVMM_PAGE_DEVID_SHIT, DEVMM_PAGE_DEVID_WID, devid);
}

static inline u32 devmm_page_bitmap_get_devid(u32 *bitmap)
{
    return devmm_page_bitmap_get_value(bitmap, DEVMM_PAGE_DEVID_SHIT, DEVMM_PAGE_DEVID_WID);
}

static inline void devmm_page_bitmap_set_flag_without_lock(u32 *bitmap, u32 flag)
{
    (*bitmap) |= flag;
}

static inline void devmm_page_bitmap_set_flag(u32 *bitmap, u32 flag)
{
    devmm_page_bitmap_lock(bitmap);
    devmm_page_bitmap_set_flag_without_lock(bitmap, flag);
    devmm_page_bitmap_unlock(bitmap);
}

static inline int devmm_page_bitmap_check_and_set_flag(u32 *bitmap, u32 flag)
{
    devmm_page_bitmap_lock(bitmap);
    if (((*bitmap) & flag) != 0) {
        devmm_page_bitmap_unlock(bitmap);
        return 1;
    }
    (*bitmap) |= flag;
    devmm_page_bitmap_unlock(bitmap);
    return 0;
}

static inline void devmm_page_bitmap_clear_flag(u32 *bitmap, u32 flag)
{
    devmm_page_bitmap_lock(bitmap);
    (*bitmap) &= (~flag);
    devmm_page_bitmap_unlock(bitmap);
}

static inline bool devmm_page_bitmap_is_mem_mapped(u32 *bitmap)
{
    return (bool)devmm_page_bitmap_get_value(bitmap, DEVMM_PAGE_MEM_MAPPED_BIT, 1);
}

static inline bool devmm_page_bitmap_is_remote_mapped(u32 *bitmap)
{
    return (bool)devmm_page_bitmap_get_value(bitmap, DEVMM_PAGE_REMOTE_MAPPED_BIT, 1);
}

static inline bool devmm_page_bitmap_is_remote_mapped_first(u32 *bitmap)
{
    return (bool)devmm_page_bitmap_get_value(bitmap, DEVMM_PAGE_REMOTE_MAPPED_FIRST_BIT, 1);
}

static inline bool devmm_page_bitmap_is_dev_mapped(u32 *bitmap)
{
    return (bool)devmm_page_bitmap_get_value(bitmap, DEVMM_PAGE_DEV_MAPPED_BIT, 1);
}

static inline bool devmm_page_bitmap_is_host_mapped(u32 *bitmap)
{
    return (bool)devmm_page_bitmap_get_value(bitmap, DEVMM_PAGE_HOST_MAPPED_BIT, 1);
}

static inline bool devmm_page_bitmap_is_first_page(u32 *bitmap)
{
    return (bool)devmm_page_bitmap_get_value(bitmap, DEVMM_PAGE_IS_FIRST_PAGE_BIT, 1);
}

static inline bool devmm_page_bitmap_is_advise_ddr(u32 *bitmap)
{
    return (bool)devmm_page_bitmap_get_value(bitmap, DEVMM_PAGE_ADVISE_DDR_BIT, 1);
}

static inline bool devmm_page_bitmap_is_advise_populate(u32 *bitmap)
{
    return (bool)devmm_page_bitmap_get_value(bitmap, DEVMM_PAGE_ADVISE_POPULATE_BIT, 1);
}

static inline bool devmm_page_bitmap_is_page_alloced(u32 *bitmap)
{
    return (bool)devmm_page_bitmap_get_value(bitmap, DEVMM_PAGE_ALLOCED_BIT, 1);
}

static inline bool devmm_page_bitmap_is_page_available(u32 *bitmap)
{
    return (bool)((u32)DEVMM_PAGE_ALLOCED_MASK & (*bitmap));
}

static inline bool devmm_page_bitmap_is_locked_host(u32 *bitmap)
{
    return (bool)devmm_page_bitmap_get_value(bitmap, DEVMM_PAGE_LOCKED_HOST_BIT, 1);
}

static inline bool devmm_page_bitmap_is_locked_device(u32 *bitmap)
{
    return (bool)devmm_page_bitmap_get_value(bitmap, DEVMM_PAGE_LOCKED_DEVICE_BIT, 1);
}

static inline bool devmm_page_bitmap_advise_memory_shared(u32 *bitmap)
{
    return (bool)devmm_page_bitmap_get_value(bitmap, DEVMM_PAGE_ADVISE_MEMORY_SHARED_BIT, 1);
}

static inline bool devmm_page_bitmap_is_advise_p2p_hbm(u32 *bitmap)
{
    return (bool)devmm_page_bitmap_get_value(bitmap, DEVMM_PAGE_ADVISE_P2P_HBM_BIT, 1);
}

static inline bool devmm_page_bitmap_is_advise_p2p_ddr(u32 *bitmap)
{
    return (bool)devmm_page_bitmap_get_value(bitmap, DEVMM_PAGE_ADVISE_P2P_DDR_BIT, 1);
}

static inline bool devmm_page_bitmap_is_ipc_open_mem(u32 *bitmap)
{
    return (bool)devmm_page_bitmap_get_value(bitmap, DEVMM_PAGE_IPC_MEM_OPEN_BIT, 1);
}

static inline bool devmm_page_bitmap_is_ipc_create_mem(u32 *bitmap)
{
    return (bool)devmm_page_bitmap_get_value(bitmap, DEVMM_PAGE_IPC_MEM_CREATE_BIT, 1);
}

static inline bool devmm_page_bitmap_is_advise_ts(u32 *bitmap)
{
    return (bool)devmm_page_bitmap_get_value(bitmap, DEVMM_PAGE_ADVISE_TS_BIT, 1);
}

static inline bool devmm_page_bitmap_is_advise_continuty(u32 *bitmap)
{
    return (bool)devmm_page_bitmap_get_value(bitmap, DEVMM_PAGE_ADVISE_CONTINUTY_BIT, 1);
}

static inline bool devmm_page_bitmap_is_advise_readonly(u32 *bitmap)
{
    return (bool)devmm_page_bitmap_get_value(bitmap, DEVMM_PAGE_READONLY_BIT, 1);
}

static inline bool devmm_page_bitmap_is_translate(u32 *bitmap)
{
    return (bool)devmm_page_bitmap_get_value(bitmap, DEVMM_PAGE_IS_TRANSLATE_BIT, 1);
}

static inline void devmm_heap_ref_set_flag(struct devmm_heap_ref *ref, u32 flag)
{
    ref->flag = flag;
}

static inline bool devmm_heap_ref_cnt_is_used_as_ref(struct devmm_heap_ref *ref)
{
    return ref->flag == 1;
}

static inline void devmm_heap_ref_set_cnt(struct devmm_heap_ref *ref, u32 cnt)
{
    ref->count = cnt;
}

static inline void devmm_page_ref_lock(struct devmm_heap_ref *ref)
{
    u32 stamp = (u32)ka_jiffies;
    while (devmm_test_and_set_bit(DEVMM_HEAP_PAGE_LOCK_FLAG_MASK, (u32 *)ref) != 0) {
        cpu_relax();
        devmm_try_cond_resched(&stamp);
    }
}

static inline void devmm_page_ref_unlock(struct devmm_heap_ref *ref)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 3, 0)
    ka_base_atomic_and((int)(~DEVMM_HEAP_PAGE_LOCK_FLAG_MASK), (ka_atomic_t *)ref);
#else
    atomic_clear_mask(DEVMM_HEAP_PAGE_LOCK_FLAG_MASK, (ka_atomic_t *)ref);
#endif
}

static inline int devmm_set_page_ref_malloc(struct devmm_heap_ref *ref)
{
    devmm_page_ref_lock(ref);
    if ((ref->count != 0) || (ref->free == 1)) {
        devmm_drv_err("Set malloc error. (ref_lock=%d; ref_free=%d; ref_count=%d)\n",
            ref->lock, ref->free, ref->count);
        devmm_page_ref_unlock(ref);
        return -EBUSY;
    }
    ref->count = 1;
    devmm_page_ref_unlock(ref);
    return 0;
}

static inline void devmm_clear_page_ref_malloc(struct devmm_heap_ref *ref)
{
    devmm_page_ref_lock(ref);
    ref->count = 0;
    devmm_page_ref_unlock(ref);
}

static inline int devmm_set_page_ref_advise(struct devmm_heap_ref *ref)
{
    devmm_page_ref_lock(ref);
    /* donot act with other action */
    if (ref->count != 1 || ref->free == 1) {
        devmm_drv_err("Adivse is acting. (ref_lock=%d; ref_free=%d; ref_count=%d)\n",
            ref->lock, ref->free, ref->count);
        devmm_page_ref_unlock(ref);
        return -EBUSY;
    }
    ref->count++;
    devmm_page_ref_unlock(ref);
    return 0;
}

static inline void devmm_clear_page_ref_advise(struct devmm_heap_ref *ref)
{
    devmm_page_ref_lock(ref);
    ref->count--;
    devmm_page_ref_unlock(ref);
}

static inline int devmm_add_page_ref(struct devmm_heap_ref *ref)
{
    devmm_page_ref_lock(ref);
    if ((ref->count == 0) || (ref->free == 1)) {
        devmm_drv_err("Add ref error. (ref_lock=%d; ref_free=%d; ref_count=%d)\n",
            ref->lock, ref->free, ref->count);
        devmm_page_ref_unlock(ref);
        return -EBUSY;
    }
    ref->count++;
    devmm_page_ref_unlock(ref);
    return 0;
}

static inline void devmm_sub_page_ref(struct devmm_heap_ref *ref)
{
    devmm_page_ref_lock(ref);
    ref->count--;
    devmm_page_ref_unlock(ref);
}

#endif /* __SVM_HEAP_MNG_H__ */
