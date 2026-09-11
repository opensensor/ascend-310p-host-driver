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
#include "devdrv_interface.h"
#include "davinci_interface.h"
#include "davinci_api.h"
#include "uda.h"

#include "devmm_adapt.h"
#include "svm_heap_mng.h"
#include "svm_log.h"
#include "devmm_channel.h"
#include "devmm_mem_alloc_interface.h"
#include "devmm_dev.h"
#include "devmm_common.h"
#include "svm_dynamic_addr.h"

bool g_devmm_true = 1;
bool g_devmm_false = 0;

#ifndef EMU_ST
u32 uda_get_host_id(void)
{
    return 65; /* stub host id 65 */
}

int uda_devid_to_udevid_ex(u32 devid, u32 *udevid)
{
    if (devid == uda_get_host_id()) {
	*udevid = devid;
	return 0;
    }

    return uda_devid_to_udevid(devid, udevid);
}
#endif

bool devmm_is_host_agent(u32 agent_id)
{
    return (agent_id == SVM_HOST_AGENT_ID);
}

void devmm_try_usleep_by_time(u32 *pre_stamp, u32 time)
{
    u32 timeinterval;

    timeinterval = ka_system_jiffies_to_msecs(ka_jiffies - *pre_stamp);
    if (timeinterval > time) {
        ka_system_usleep_range(1, 2); /* sleep 1~2us */
        *pre_stamp = (u32)ka_jiffies;
    }
}

void devmm_try_cond_resched_by_time(u32 *pre_stamp, u32 time)
{
    u32 timeinterval;

    timeinterval = ka_system_jiffies_to_msecs(ka_jiffies - *pre_stamp);
    if ((timeinterval > time) && !ka_base_in_atomic()) {
        ka_task_cond_resched();
        *pre_stamp = (u32)ka_jiffies;
    }
}

void devmm_try_cond_resched(u32 *pre_stamp)
{
    devmm_try_cond_resched_by_time(pre_stamp, DEVMM_WAKEUP_TIMEINTERVAL);
}

bool devmm_smmu_is_opening(void)
{
    return ((devmm_svm->smmu_status == DEVMM_SMMU_STATUS_OPENING) ? DEVMM_TRUE : DEVMM_FALSE);
}

static int devmm_ka_mm_pin_user_pages_fast(unsigned long start, int nr_pages, int write, ka_page_t **pages)
{
    u32 i, try_times = 5; /* try 5 times, maybe success */
    int tmp_num;

    for (i = 0; i < try_times; i++) {
        tmp_num = ka_mm_pin_user_pages_fast(start, nr_pages, (write != 0) ? KA_FOLL_WRITE : 0, pages);
        if (tmp_num == nr_pages) {
            return nr_pages;
        }
#ifndef EMU_ST
        if (tmp_num < 0) {
            return 0;
        }
        devmm_unpin_user_pages(pages, nr_pages, tmp_num);
#endif
    }
    return 0;
}

int devmm_pin_user_pages_fast(u64 va, u64 total_num, int write, ka_page_t **pages)
{
    u64 got_num, remained_num, tmp_va;
    int tmp_num, expected_num;
    u32 stamp;

    stamp = (u32)ka_jiffies;
    for (got_num = 0; got_num < total_num;) {
        tmp_va = va + got_num * KA_MM_PAGE_SIZE;
        remained_num = total_num - got_num;
        expected_num = (remained_num > DEVMM_GET_2M_PAGE_NUM) ? DEVMM_GET_2M_PAGE_NUM : (int)remained_num;
        tmp_num = devmm_ka_mm_pin_user_pages_fast(tmp_va, expected_num, write, &pages[got_num]);
        got_num += (tmp_num > 0) ? (u32)tmp_num : 0;
        if (tmp_num != expected_num) {
            devmm_drv_err("Get_user_pages_fast fail. (va=0x%llx; expected_page_num=%d; real_got_page_num=%d)\n",
                tmp_va, expected_num, tmp_num);
            goto page_err;
        }
        devmm_try_cond_resched(&stamp);
    }

    return 0;

page_err:
    devmm_unpin_user_pages(pages, total_num, got_num);

    return -EFAULT;
}

void devmm_pin_user_pages(ka_page_t **pages, u64 page_num)
{
    u32 stamp = (u32)ka_jiffies;
    u64 i;

    for (i = 0; i < page_num; i++) {
        ka_mm_get_page(pages[i]);
        devmm_try_cond_resched(&stamp);
    }
}

void devmm_put_user_pages(ka_page_t **pages, u64 page_num, u64 unpin_num)
{
    u32 stamp;
    u64 i;

    if (unpin_num > page_num || pages == NULL) {
        return;
    }

    stamp = (u32)ka_jiffies;
    for (i = 0; i < unpin_num; i++) {
        if (pages[i] != NULL) {
            ka_mm_put_page(pages[i]);
            pages[i] = NULL;
        }
        devmm_try_cond_resched(&stamp);
    }
}

void devmm_unpin_user_pages(ka_page_t **pages, u64 page_num, u64 unpin_num)
{
    u32 stamp;
    u64 i;

    if (unpin_num > page_num || pages == NULL) {
        return;
    }

    stamp = (u32)ka_jiffies;
    for (i = 0; i < unpin_num; i++) {
        if (pages[i] != NULL) {
            ka_mm_unpin_user_page(pages[i]);
            pages[i] = NULL;
        }
        devmm_try_cond_resched(&stamp);
    }
}

u64 devmm_get_pagecount_by_size(u64 vptr, u64 sizes, u32 page_size)
{
    u64 chunk_cnt = 0;
    u64 tem_byte;

    if ((page_size == 0) || ((vptr < (DEVMM_SVM_MEM_START + DEVMM_SVM_MEM_SIZE)) && (sizes > DEVMM_MAX_MAPPED_RANGE))) {
        devmm_drv_err("Chunk_page_size or byte_count is error. "
            "(chunk_page_size=%u; byte_count=%llu)", page_size, sizes);
        return chunk_cnt;
    }
    tem_byte = ((vptr & (page_size - 1)) + sizes);
    chunk_cnt = tem_byte / page_size;
    if ((tem_byte & (page_size - 1)) != 0) {
        chunk_cnt++;
    }
    return chunk_cnt;
}

ssize_t devmm_read_file(ka_file_t *fp, char *dst_addr, size_t fsize, loff_t *pos)
{
#ifndef EMU_ST
    return ka_fs_kernel_read(fp, dst_addr, fsize, pos);
#else
    return 0;
#endif
}

char *devmm_read_line(ka_file_t *fp, loff_t *offset, char *buf, u32 buf_len)
{
    u32 i = 0;
    long ret;

    ret = devmm_read_file(fp, buf, buf_len - 1, offset);
    if (ret <= 0) {
        devmm_drv_warn("File filestring not right. (buf_len=%u)\n", buf_len);
        return NULL;
    }

    while (g_devmm_true) {
        if (buf[i] == '\n') {
            i++;
            break;
        }

        i++;

        if (i >= ret) {
            break;
        }
    }

    if (i < ret) {
        *offset += i - ret;
    }

    if (i < buf_len) {
        buf[i] = 0;
    }

    return buf;
}

int devmm_check_va_add_size_by_heap(struct devmm_svm_heap *heap, u64 va, u64 size)
{
    u64 heap_total_cnt, heap_used_count, heap_free_count, va_need_count;

    if (size > heap->heap_size) {
        devmm_drv_err("Size is bad. (va=0x%llx; size=%llu; heap_size=%llu)\n",
                      va, size, heap->heap_size);
        return -EINVAL;
    }
    heap_used_count = (va - heap->start) / heap->chunk_page_size;
    heap_total_cnt = heap->heap_size / heap->chunk_page_size;
    heap_free_count = heap_total_cnt - heap_used_count;
    va_need_count = devmm_get_pagecount_by_size(va, size, heap->chunk_page_size);
    if ((va_need_count == 0) || (heap_free_count < va_need_count)) {
        devmm_drv_err("Va_need_count is zero, or heap_free_count < va_need_count. (heap_free_count=%llu; "
                      "va_need_count=%llu; va=0x%llx; size=%llu; heap_start_va=0x%llx; page_size=%u)\n",
                      heap_free_count, va_need_count, va, size, heap->start, heap->chunk_page_size);
        return -EINVAL;
    }

    return 0;
}

static int devmm_check_common_input_heap_id_and_size(struct devmm_svm_process *svm_proc, u32 heap_idx, u64 heap_size)
{
    u32 heap_num = (u32)(heap_size / DEVMM_HEAP_SIZE);

    if ((heap_size == 0) || ((heap_size % DEVMM_HEAP_SIZE) != 0)) {
        devmm_drv_err("Input para. (heap_idx=%u; heap_size=0x%llx)\n", heap_idx, heap_size);
        return -EINVAL;
    }

    if ((heap_idx >= DEVMM_TOTAL_MAX_HEAP_NUM) || ((heap_idx + heap_num) > DEVMM_TOTAL_MAX_HEAP_NUM)) {
        devmm_drv_err("Input para. (heap_idx=%u; heap_size=0x%llx; heap_num=%u; DEVMM_TOTAL_MAX_HEAP_NUM=%llu)\n",
            heap_idx, heap_size, heap_num, DEVMM_TOTAL_MAX_HEAP_NUM);
        return -EINVAL;
    }

    if ((heap_idx < DEVMM_MAX_HEAP_NUM) && ((heap_idx + heap_num) > DEVMM_MAX_HEAP_NUM)) {
        devmm_drv_err("Input para. (heap_idx=%u; heap_size=0x%llx; heap_num=%u; DEVMM_MAX_HEAP_NUM=%llu)\n",
            heap_idx, heap_size, heap_num, DEVMM_MAX_HEAP_NUM);
        return -EINVAL;
    }

    if (heap_idx < DEVMM_MAX_HEAP_NUM) {
        if (heap_size > (DEVMM_MAX_ALLOC_PAGE_NUM * KA_MM_PAGE_SIZE)) {
            devmm_drv_err("Input para. (heap_idx=%u; heap_size=0x%llx; max_heap_size=0x%llx)\n",
                heap_idx, heap_size, DEVMM_MAX_ALLOC_PAGE_NUM * KA_MM_PAGE_SIZE);
            return -EINVAL;
        }
    } else {
        /* rsv not with host, check svm_is_da_addr will failed */
    }

    return 0;
}

static bool devmm_check_common_input_heap_info(struct devmm_svm_process *svm_pro,
    struct devmm_update_heap_para *cmd, u32 devid)
{
    if ((cmd->op == DEVMM_HEAP_ENABLE) && (devmm_svm_mem_is_enable(svm_pro) == false) &&
        cmd->heap_sub_type == SUB_HOST_TYPE) {
        devmm_drv_err("Host svm mem is disable.\n");
        return false;
    }

    if ((cmd->op == DEVMM_HEAP_ENABLE)&& devmm_is_host_agent(devid) &&
        ((cmd->heap_sub_type != SUB_DEVICE_TYPE) || (cmd->heap_type != DEVMM_HEAP_CHUNK_PAGE))) {
        devmm_drv_err("Host agent only support heap SUB_DEVICE_TYPE DEVMM_HEAP_CHUNK_PAGE."
            " (heap_sub_type=%u; heap_type=%u).\n", cmd->heap_sub_type, cmd->heap_type);
        return false;
    }

    if (devmm_check_common_input_heap_id_and_size(svm_pro, cmd->heap_idx, cmd->heap_size) != 0) {
        return false;
    }

    /* heap type and page size must match */
    if (cmd->op == DEVMM_HEAP_ENABLE) {
        if ((cmd->heap_type < DEVMM_HEAP_PINNED_HOST) || (cmd->heap_type > DEVMM_HEAP_CHUNK_PAGE)) {
            return false;
        }
        if (cmd->heap_sub_type >= SUB_MAX_TYPE) {
            return false;
        }
        return DEVMM_TRUE;
    } else if (cmd->op == DEVMM_HEAP_DISABLE) {
        struct devmm_svm_heap *heap = devmm_get_heap_by_idx(svm_pro, cmd->heap_idx);
        if ((devmm_check_heap_is_entity(heap) == false) || (heap->heap_idx != cmd->heap_idx) ||
            (heap->heap_size != cmd->heap_size)) {
            devmm_drv_err("Disable input error. (hostpid=%d; devid=%d; vfid=%d; size=0x%llx)\n",
                svm_pro->process_id.hostpid, svm_pro->process_id.devid, svm_pro->process_id.vfid,
                cmd->heap_size);
            return false;
        }
        return true;
    }

    return false;
}

bool devmm_check_input_heap_info(struct devmm_svm_process *svm_pro,
    struct devmm_update_heap_para *cmd, u32 devid)
{
    struct devmm_svm_heap *heap_check = NULL;
    u32 heap_num, i;

    if (devmm_check_common_input_heap_info(svm_pro, cmd, devid) == false) {
        devmm_drv_err("Input error. (hostpid=%d; devid=%d; vfid=%d; size=0x%llx)\n",
            svm_pro->process_id.hostpid, svm_pro->process_id.devid, svm_pro->process_id.vfid, cmd->heap_size);
        return false;
    }

    heap_num = (u32)(cmd->heap_size / DEVMM_HEAP_SIZE);
    for (i = 0; i < heap_num; i++) {
        heap_check = devmm_get_heap_by_idx(svm_pro, i + cmd->heap_idx);
        if (cmd->op == DEVMM_HEAP_ENABLE) {
            if (devmm_check_heap_is_entity(heap_check) == true) {
                devmm_drv_info("The heap already exists. (hostpid=%d; devid=%d; vfid=%d; size=0x%llx)\n",
                    svm_pro->process_id.hostpid, svm_pro->process_id.devid, svm_pro->process_id.vfid,
                    cmd->heap_size);
                return false;
            }
        } else {
            if (devmm_check_heap_is_entity(heap_check) == false) {
                devmm_drv_err("Disable input error. (hostpid=%d; devid=%d; vfid=%d; size=0x%llx)\n",
                    svm_pro->process_id.hostpid, svm_pro->process_id.devid, svm_pro->process_id.vfid,
                    cmd->heap_size);
                return false;
            }
        }
    }

    return true;
}

/* heap Bitmap's granularity, not equal with pagesize */
static u32 devmm_get_chunk_page_size_by_heap_type(u32 heap_type, u32 heap_sub_type)
{
    if (heap_sub_type == SUB_RESERVE_TYPE) {
        return (heap_type == DEVMM_HEAP_CHUNK_PAGE) ? devmm_svm->host_page_size : devmm_svm->device_hpage_size;
    }

    if (heap_type == DEVMM_HEAP_HUGE_PAGE) {
        return devmm_svm->device_hpage_size;
    } else if (heap_type == DEVMM_HEAP_CHUNK_PAGE) {
        return ((heap_sub_type == SUB_DEVICE_TYPE) ? devmm_svm->device_page_size : devmm_svm->svm_page_size);
    } else if (heap_type == DEVMM_HEAP_PINNED_HOST) {
        return devmm_svm->device_hpage_size;
    }
    return 0;
}

#ifndef EMU_ST
/* used with the devmm_free, function as alloc */
void *devmm_kvalloc(u64 size, ka_gfp_t flags)
{
    void *ptr = devmm_kmalloc_ex(size, KA_GFP_KERNEL | __KA_GFP_NOWARN | __KA_GFP_ACCOUNT | flags);
    if (ptr == NULL) {
        ptr = __devmm_vmalloc_ex(size, KA_GFP_KERNEL | __KA_GFP_ACCOUNT | flags, KA_PAGE_KERNEL);
    }

    return ptr;
}

void *devmm_kvzalloc(u64 size)
{
    return devmm_kvalloc(size, __KA_GFP_ZERO);
}
#endif

/* used with the devmm_zalloc, function as free */
void devmm_kvfree(const void *ptr)
{
    if (ka_mm_is_vmalloc_addr(ptr)) {
        devmm_vfree_ex(ptr);
    } else {
        devmm_kfree_ex(ptr);
    }
}

ka_vm_area_struct_t *devmm_find_vma(struct devmm_svm_process *svm_proc, u64 vaddr)
{
    u64 double_pgtable_offset;

    if (devmm_va_is_in_svm_range(vaddr)) {
        return devmm_find_vma_proc(svm_proc->mm, svm_proc->vma, svm_proc->vma_num, vaddr);
    }

    double_pgtable_offset = devmm_get_double_pgtable_offset();
    if ((double_pgtable_offset != 0) && (vaddr >= double_pgtable_offset)
        && devmm_va_is_in_svm_range(vaddr - double_pgtable_offset)) {
        return devmm_find_vma_proc(svm_proc->mm, svm_proc->vma, svm_proc->vma_num, vaddr);
    }

    return svm_da_query_vma(svm_proc, vaddr);
}

struct devmm_svm_heap *devmm_get_heap_by_idx(struct devmm_svm_process *svm_proc, u32 heap_idx)
{
    if (heap_idx < DEVMM_MAX_HEAP_NUM) {
        return svm_proc->heaps[heap_idx];
    } else if (heap_idx < DEVMM_TOTAL_MAX_HEAP_NUM) {
        return svm_get_da_heap_by_idx(svm_proc, heap_idx);
    } else {
        return NULL;
    }
}

struct devmm_svm_heap *devmm_get_heap_by_va(struct devmm_svm_process *svm_proc, u64 vaddr)
{
    u64 heap_idx;

    heap_idx = (vaddr - DEVMM_SVM_MEM_START) / DEVMM_HEAP_SIZE;
    return devmm_get_heap_by_idx(svm_proc, heap_idx);
}

bool devmm_check_heap_is_entity(struct devmm_svm_heap *heap)
{
    if ((heap == NULL) || (heap->heap_type == DEVMM_HEAP_IDLE)) {
        return false;
    }
    return true;
}

/* Should check addr in heap range firstly. */
void devmm_set_heap_used_status(struct devmm_svm_heap *heap, u64 va, u64 size)
{
    u64 end_addr = ka_base_round_up(va + size, HEAP_USED_PER_MASK_SIZE);
    u64 addr = ka_base_round_down(va, HEAP_USED_PER_MASK_SIZE);
    u32 nr;

    for (; addr < end_addr; addr += HEAP_USED_PER_MASK_SIZE) {
        if ((addr < heap->start) || (addr >= (heap->start + heap->heap_size))) {
            break;
        }
        nr = (u32)((addr - heap->start) / HEAP_USED_PER_MASK_SIZE);
        ka_base_set_bit(nr, heap->used_mask);
    }
}

static void devmm_set_heap_info(struct devmm_svm_heap *heap, struct devmm_update_heap_para *cmd)
{
    heap->heap_type = cmd->heap_type;
    heap->heap_idx = cmd->heap_idx;
    heap->heap_sub_type = cmd->heap_sub_type;
    heap->chunk_page_size = devmm_get_chunk_page_size_by_heap_type(cmd->heap_type, cmd->heap_sub_type);
    heap->start = DEVMM_SVM_MEM_START + cmd->heap_idx * DEVMM_HEAP_SIZE;
    heap->heap_size = cmd->heap_size;
    ka_base_atomic64_set(&heap->occupy_cnt, 0);
    heap->is_invalid = false;
}

static void devmm_set_svm_heap_struct(struct devmm_svm_process *svm_process, struct devmm_svm_heap *heap,
    u32 heap_idx, u32 heap_num)
{
    if (heap_idx < DEVMM_MAX_HEAP_NUM) {
        u32 i;
        for (i = 0; i < heap_num; i++) {
            svm_process->heaps[heap_idx + i] = heap;
        }
    } else {
        svm_set_da_heap(svm_process, heap_idx, heap);
    }
}

void devmm_clear_svm_heap_struct(struct devmm_svm_process *svm_process, u32 heap_idx, u32 heap_num)
{
    if (heap_idx < DEVMM_MAX_HEAP_NUM) {
        u32 i;

        for (i = 0; i < heap_num; i++) {
            svm_process->heaps[heap_idx + i] = NULL;
        }
    } else {
        svm_set_da_heap(svm_process, heap_idx, NULL);
    }
}

#define SVM_WAIT_HEAP_IDEA_CNT 5000 /* 0.5s~1s */
bool devmm_wait_svm_heap_unoccupied(struct devmm_svm_process *svm_process, struct devmm_svm_heap *heap)
{
    u32 i;

    ka_task_down_write(&svm_process->heap_sem);
    heap->is_invalid = true;
    for (i = 0; i < SVM_WAIT_HEAP_IDEA_CNT; i++) {
        if (ka_base_atomic64_read(&heap->occupy_cnt) == 0) {
            ka_task_up_write(&svm_process->heap_sem);
            return true;
        }
        ka_system_usleep_range(100, 200); /* 100-200 us */
    }
    ka_task_up_write(&svm_process->heap_sem);
    return false;
}

void devmm_free_heap_struct(struct devmm_svm_process *svm_process, u32 heap_idx, u32 heap_num)
{
    struct devmm_svm_heap *heap = devmm_get_heap_by_idx(svm_process, heap_idx);

    devmm_clear_svm_heap_struct(svm_process, heap_idx, heap_num);
    if (heap != NULL) {
        devmm_kvfree(heap);
    }
}

int devmm_update_heap_info(struct devmm_svm_process *svm_process, struct devmm_update_heap_para *cmd,
    struct devmm_svm_heap *free_heap)
{
    struct devmm_svm_heap *heap = NULL;
    u32 heap_num;
    int ret;

    heap_num = (u32)(cmd->heap_size / DEVMM_HEAP_SIZE);

    if ((cmd->heap_idx < DEVMM_MAX_HEAP_NUM) && ((cmd->heap_idx + heap_num) > DEVMM_MAX_HEAP_NUM)) {
        devmm_drv_err("Invalid heap. (heap_idx=%u; heap_size=0x%llx)\n", cmd->heap_idx, cmd->heap_size);
        return -EINVAL;
    }

    if (cmd->op == DEVMM_HEAP_ENABLE) {
        u64 used_mask_size;

        heap = devmm_get_heap_by_idx(svm_process, cmd->heap_idx);
        if (heap != NULL) {
            /* smp device already updated or setup device more than once */
            return 0;
        }

        used_mask_size = sizeof(unsigned long) *
            KA_BASE_BITS_TO_LONGS((u64)(ka_base_round_up(cmd->heap_size, HEAP_USED_PER_MASK_SIZE) / HEAP_USED_PER_MASK_SIZE));

        heap = devmm_kvzalloc(sizeof(struct devmm_svm_heap) + used_mask_size);
        if (heap == NULL) {
            return -ENOMEM;
        }
        heap->used_mask = (unsigned long *)(void *)(heap + 1);

        devmm_set_heap_info(heap, cmd);
        ret = devmm_vmma_mng_init(&heap->vmma_mng, heap->start, heap->heap_size);
        if (ret != 0) {
            devmm_kvfree(heap);
            return ret;
        }

        if ((devmm_get_end_type() == DEVMM_END_HOST) && devmm_alloc_new_heap_pagebitmap(heap) != 0) {
            devmm_drv_err("Devmm_alloc_new_heap_pagebitmap error.\n");
            devmm_kvfree(heap);
            return -ENOMEM;
        }

        svm_process->alloced_heap_size += (cmd->heap_sub_type == SUB_SVM_TYPE) ? cmd->heap_size : 0;
        svm_process->max_heap_use = (svm_process->max_heap_use > (cmd->heap_idx + heap_num)) ?
            svm_process->max_heap_use : (cmd->heap_idx + heap_num);
        devmm_set_svm_heap_struct(svm_process, heap, cmd->heap_idx, heap_num);
    } else {
        if (free_heap == NULL) {
            return -EINVAL;
        }
        heap = free_heap;
        devmm_kvfree(heap);
        svm_process->alloced_heap_size -= (cmd->heap_sub_type == SUB_SVM_TYPE) ? cmd->heap_size : 0;
    }

    return 0;
}

void devmm_dev_fault_flag_set(u32 *flag, u32 shift, u32 wide, u32 value)
{
    u32 msk = ((1U << wide) - 1);
    u32 val = (msk & value);

    (*flag) &= (u32)(~(msk << shift));
    (*flag) |= (u32)(val << shift);
}

u32 devmm_dev_fault_flag_get(u32 flag, u32 shift, u32 wide)
{
    u32 msk = ((1U << wide) - 1);
    u32 val = flag >> shift;

    return (val & msk);
}

u32 devmm_get_max_page_num_of_per_msg(u32 *bitmap)
{
    u32 num;

    num = DEVMM_PAGE_NUM_PER_MSG;
#ifdef CFG_SOC_PLATFORM_ESL_FPGA
    if (devmm_page_bitmap_is_advise_continuty(bitmap) == true) {
        num = 1024; // 1024 max page_num per msg
    }
#endif
    return num;
}

const char *devmm_get_chrdev_name(void)
{
    return (const char *)DEVMM_CHRDEV_NAME;
}

#ifndef DEVMM_UT
bool devmm_is_pcie_connect(u32 dev_id)
{
    return (devdrv_get_connect_protocol(dev_id) == CONNECT_PROTOCOL_PCIE);
}

bool devmm_is_hccs_connect(u32 dev_id)
{
    return (devdrv_get_connect_protocol(dev_id) == CONNECT_PROTOCOL_HCCS);
}
#endif

bool devmm_is_dma_link_abnormal(u32 dev_id)
{
    struct ascend_intf_get_status_para status_para = {0};
    unsigned int status = 0;
    int ret;

    status_para.type = DAVINCI_STATUS_TYPE_DEVICE;
    status_para.para.device_id = dev_id;
#ifndef EMU_ST
    ret = ascend_intf_get_status(status_para, &status);
    if ((ret != 0) || ((status & DAVINCI_INTF_DEVICE_STATUS_LINK_ABNORMAL) == DAVINCI_INTF_DEVICE_STATUS_LINK_ABNORMAL)) {
        return true;
    }
#endif
    return false;
}

void devmm_svm_mem_enable(struct devmm_svm_process *svm_proc)
{
    svm_proc->is_enable_svm_mem = true;
}

void devmm_svm_mem_disable(struct devmm_svm_process *svm_proc)
{
    svm_proc->is_enable_svm_mem = false;
}

bool devmm_svm_mem_is_enable(struct devmm_svm_process *svm_proc)
{
    return svm_proc->is_enable_svm_mem;
}

void devmm_phy_addr_attr_pack(struct devmm_svm_process *svm_proc, u32 pg_type, u32 mem_type, bool is_continuous,
    struct devmm_phy_addr_attr *attr)
{
    attr->side = DEVMM_SIDE_TYPE;
    attr->devid = svm_proc->process_id.devid;
    attr->vfid = svm_proc->process_id.vfid;
    attr->module_id = 0;

    attr->pg_type = pg_type;
    attr->mem_type = mem_type;
    attr->is_continuous = (pg_type == DEVMM_NORMAL_PAGE_TYPE) ? is_continuous : false;
    attr->is_compound_page = ((DEVMM_SIDE_TYPE == DEVMM_SIDE_MASTER) || (DEVMM_SIDE_TYPE == DEVMM_SIDE_HOST_AGENT)) ?
        true : false;
}

u64 devmm_get_tgid_start_time(void)
{
    int vnr = task_tgid_vnr(ka_task_get_current());
    ka_task_struct_t *task = NULL;
    ka_struct_pid_t *pgrp = NULL;
    u64 start_time;

    /*
     * vnr: hostpid in container. Could not use ka_task_get_current_tgid(), because hostpid in kernel is not
     * the same as hostpid in container, will cause find pgrp fail.
     */
    pgrp = ka_task_find_get_pid(vnr);
    if (pgrp == NULL) {
        devmm_drv_err("Pgrp is NULL. (vnr=%d)\n", vnr);
        return KA_U64_MAX;
    }

    task = ka_task_get_pid_task(pgrp, KA_PIDTYPE_PID);
    if (task == NULL) {
        devmm_drv_err("Task is NULL.\n");
        ka_task_put_pid(pgrp);
        return KA_U64_MAX;
    }
    start_time = ka_task_get_starttime(task);
    ka_task_put_task_struct(task);
    ka_task_put_pid(pgrp);

    return start_time;
}

bool devmm_palist_is_continuous(u64 *pa, u32 num, u32 page_size)
{
    u64 pre_pa = 0;
    int i;

    for (i = 0; i < num; i++) {
        if ((i != 0) && (pre_pa + page_size != pa[i])) {
            return false;
        }
        pre_pa = pa[i];
    }
    return true;
}

bool devmm_palist_is_specify_continuous(u64 *pa, u32 page_size, u32 total_num, u32 min_con_num)
{
    u64 pre_pa = 0;
    u32 i, j;

    if ((total_num == 0) || (total_num % min_con_num != 0)) {
        return false;
    }

    for (i = 0; i < total_num;) {
        for (j = 0; j < min_con_num; j++) {
            if ((j != 0) && (pre_pa + page_size != pa[i + j])) {
                return false;
            }
            pre_pa = pa[i + j];
        }
        i = i + min_con_num;
    }
    return true;
}

void devmm_update_devids(struct devmm_devid *devids, u32 logical_devid, u32 devid, u32 vfid)
{
    devids->logical_devid = logical_devid;
    devids->devid = devid;
    devids->vfid = vfid;
}
