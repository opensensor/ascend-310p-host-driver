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

#include "devmm_common.h"
#include "devmm_proc_info.h"
#include "svm_proc_mng.h"
#include "devmm_dev.h"
#include "svm_proc_fs.h"
#include "devmm_mem_alloc_interface.h"
#include "svm_mmu_notifier.h"
#include "svm_dynamic_addr.h"

void devmm_mmu_notifier_unregister_no_release(struct devmm_svm_process *svm_proc)
{
#ifndef ADAPT_KP_OS_FOR_EMU_TEST
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
    if (svm_proc->notifier != NULL) {
        ka_mm_mmu_notifier_put(svm_proc->notifier);
    }
#else
    ka_mm_mmu_notifier_unregister_no_release(&svm_proc->notifier, svm_proc->mm);
#endif
#endif
}

void devmm_svm_mmu_notifier_unreg(struct devmm_svm_process *svm_proc)
{
    bool notifier_release_flag = false;

    devmm_drv_info("Devmm notifier unreg."
        "(hostpid=%d; devpid=%d; devid=%d; vfid=%d; status=0x%x; proc_idx=0x%x; msg=%u; other_proc=%u)\n",
        svm_proc->process_id.hostpid, svm_proc->devpid, svm_proc->process_id.devid, svm_proc->process_id.vfid,
        svm_proc->proc_status, svm_proc->proc_idx, svm_proc->msg_processing, svm_proc->other_proc_occupying);
    /*
    * with user unmap, trace: do_munmap->devmm_notifier_start->devmm_svm_mmu_notifier_unreg
    * without user unmap, trace: exit->devmm_notifier_release->devmm_svm_mmu_notifier_unreg
    */
    devmm_wait_exit_and_del_from_hashtable_lock(svm_proc);

    ka_task_mutex_lock(&svm_proc->proc_lock);
    if (svm_proc->notifier_reg_flag == DEVMM_SVM_INITED_FLAG) {
        devmm_proc_fs_del_task(svm_proc);
        svm_proc->notifier_reg_flag = DEVMM_SVM_UNMAP_FLAG;
        svm_proc->tsk = NULL;
        svm_proc->proc_status = DEVMM_SVM_THREAD_EXITING;
        svm_proc->inited = DEVMM_SVM_UNMAP_FLAG;
        notifier_release_flag = true;
    }
    ka_task_mutex_unlock(&svm_proc->proc_lock);

    if (notifier_release_flag == true) {
        devmm_mmu_notifier_unregister_no_release(svm_proc);
        devmm_notifier_release_private(svm_proc);
        devmm_isb();
        svm_proc->notifier_reg_flag = DEVMM_SVM_UNINITED_FLAG;
    }
}

#ifndef ADAPT_KP_OS_FOR_EMU_TEST
#ifndef EMU_ST
static inline bool devmm_mem_is_in_dvpp_vma_range(u64 start, u64 end)
{
    u64 devid, start_addr, end_addr;
    for (devid = 0; devid < DEVMM_MAX_DEVICE_NUM; devid++) {
        start_addr = DEVMM_SVM_MEM_START + devid * DEVMM_DVPP_HEAP_RESERVATION_SIZE;
        end_addr = start_addr + DEVMM_MAX_HEAP_MEM_FOR_DVPP_16G;
        if (start == start_addr && end == end_addr) {
            return true;
        }
    }
    return false;
}

static bool devmm_mem_is_in_readonly_vma_range(u64 start, u64 end)
{
    u64 size = end - start;

    if (size == DEVMM_READ_ONLY_HEAP_TOTAL_SIZE) {
        if ((start == DEVMM_READ_ONLY_ADDR_START) &&
            (end == (DEVMM_READ_ONLY_ADDR_START + DEVMM_READ_ONLY_HEAP_TOTAL_SIZE))) {
            return true;
        }
    } else if (size == DEVMM_DEV_READ_ONLY_HEAP_TOTAL_SIZE) {
        if ((start == DEVMM_DEV_READ_ONLY_ADDR_START) &&
            (end == (DEVMM_DEV_READ_ONLY_ADDR_START + DEVMM_DEV_READ_ONLY_HEAP_TOTAL_SIZE))) {
            return true;
        }
    }
    return false;
}

static bool _devmm_mem_is_in_vma_range(ka_vm_area_struct_t *vma[], u32 vma_num, u64 start, u64 end)
{
    u64 size = end - start;

    if (size == DEVMM_SVM_MEM_SIZE) {
        if ((start == DEVMM_SVM_MEM_START) && (end == (DEVMM_SVM_MEM_START + DEVMM_SVM_MEM_SIZE))) {
            return true;
        }
    } else if (size == DEVMM_MAX_HEAP_MEM_FOR_DVPP_16G) {
        if (devmm_mem_is_in_dvpp_vma_range(start, end)) {
            return true;
        }
    } else if (size == DEVMM_DVPP_HEAP_TOTAL_SIZE) {
        if ((start == DEVMM_SVM_MEM_START) && (end == (DEVMM_SVM_MEM_START + DEVMM_DVPP_HEAP_TOTAL_SIZE))) {
            return true;
        }
    } else if ((size == DEVMM_READ_ONLY_HEAP_TOTAL_SIZE) || (size == DEVMM_DEV_READ_ONLY_HEAP_TOTAL_SIZE)) {
        if (devmm_mem_is_in_readonly_vma_range(start, end)) {
            return true;
        }
    } else if (size == DEVMM_SVM_MEM_SIZE - DEVMM_DVPP_HEAP_TOTAL_SIZE - DEVMM_READ_ONLY_HEAP_TOTAL_SIZE -
        DEVMM_DEV_READ_ONLY_HEAP_TOTAL_SIZE) {
        if ((start == DEVMM_NON_RESERVATION_HEAP_ADDR_START) &&
            (end == DEVMM_SVM_MEM_START + DEVMM_SVM_MEM_SIZE)) {
            return true;
        }
    } else if (size == DEVMM_HOST_PIN_SIZE) {
        if ((start == DEVMM_HOST_PIN_START) && (end == DEVMM_HOST_PIN_END)) {
            ka_vm_area_struct_t *tmp_vma = NULL;
            tmp_vma = _devmm_find_vma_proc(vma, vma_num, DEVMM_HOST_PIN_START);
            return (tmp_vma != NULL);
        }
    }
    return false;
}
#endif

bool devmm_mem_is_in_vma_range(ka_vm_area_struct_t *vma[], u32 vma_num, u64 start, u64 end)
{
#ifndef EMU_ST
    bool is_in_range = false;

    is_in_range = _devmm_mem_is_in_vma_range(vma, vma_num, start, end);
    if (is_in_range) {
        return true;
    }
    if (devmm_is_support_double_pgtable() && (start >= (DEVMM_SVM_MEM_START + devmm_get_double_pgtable_offset())) &&
        (end <= (DEVMM_SVM_MEM_START + devmm_get_double_pgtable_offset() + DEVMM_SVM_MEM_SIZE))) {
        u64 check_start = start - devmm_get_double_pgtable_offset();
        u64 check_end = end - devmm_get_double_pgtable_offset();

        return _devmm_mem_is_in_vma_range(vma, vma_num, check_start, check_end);
    }
#endif
    return false;
}

/*
   os call this ops(mmu_notifier_ops.invalidate_range_start) when destroy page table:
     1. call munmap partial/full in user
     2. call ka_mm_zap_vma_ptes partial/full in kernel
     3. call mmu_notifier_invalidate_range_start in kernel when needed, for example: free huge table devmm_zap_vma_pmds
   call munmap partial in user illegal, os will call ops devmm_vm_open(vm_operations_struct.open) also,
       use this to check illegal call
*/
STATIC int _devmm_notifier_start(ka_mmu_notifier_t *mn, ka_mm_struct_t *mm,
    unsigned long start, unsigned long end, bool blockable)
{
#ifndef EMU_ST
    struct devmm_svm_process *svm_proc = NULL;
    bool unexpected_munmap = false;

    svm_proc = devmm_get_svm_proc_by_mm(mm);
    if (svm_proc == NULL) {
        devmm_drv_info("Unlikely.\n");
        /*
         * If blockable argument is set to false then the callback cannot
         * sleep and has to return with -EAGAIN. 0 should be returned
         * otherwise
         */
        return (blockable == false) ? -EAGAIN : 0;
    }

    if (svm_is_da_match(svm_proc, start, end - start)) {
        if (blockable == false) {
            return -EAGAIN;
        }

        /* ioctl unmap should do nothing, only munmap should enter */
        if (svm_is_da_unmap(svm_proc, start, end - start)) {
            return 0;
        }

        /*
         * If the heap is null, it indicates a normal dynamic address release (munmap), and the da should be deleted.
         * Otherwise, it indicates an abnormal munmap, and the da should be deleted after the heap and bitmap have been destroyed.
         */
        svm_occupy_da(svm_proc); /* Mutually exclusive with ioctl heap destroy. */
        unexpected_munmap = (devmm_svm_get_heap(svm_proc, start) != NULL);
        if (!unexpected_munmap) {
            (void)svm_da_del_addr(svm_proc, start, end - start);
        }
        svm_release_da(svm_proc);
    } else if (devmm_mem_is_in_vma_range(svm_proc->vma, svm_proc->vma_num, start, end)) {
        if (blockable == false) {
            return -EAGAIN;
        }
        unexpected_munmap = true;
    }

    if (unexpected_munmap) {
        devmm_drv_info("User abnormal unmap, need to release all resources.\n");
        devmm_svm_ioctl_lock(svm_proc, DEVMM_CMD_WLOCK);
        devmm_svm_mmu_notifier_unreg(svm_proc);
        devmm_svm_ioctl_unlock(svm_proc, DEVMM_CMD_WLOCK);
    }
#endif
    return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0))
STATIC int devmm_notifier_start(ka_mmu_notifier_t *mn, const ka_mmu_notifier_range_t *range)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0))
    bool blockable = ka_mm_mmu_notifier_range_blockable(range);
#else
    bool blockable = range->blockable;
#endif

    return _devmm_notifier_start(mn, range->mm, range->start, range->end, blockable);
}
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)
STATIC int devmm_notifier_start(ka_mmu_notifier_t *mn, ka_mm_struct_t *mm, unsigned long start, unsigned long end,
    bool blockable)
{
    return _devmm_notifier_start(mn, mm, start, end, blockable);
}
#else
STATIC void devmm_notifier_start(ka_mmu_notifier_t *mn, ka_mm_struct_t *mm, unsigned long start, unsigned long end)
{
    (void)_devmm_notifier_start(mn, mm, start, end, true);
}
#endif

STATIC void devmm_notifier_release(ka_mmu_notifier_t *mn, ka_mm_struct_t *mm)
{
    struct devmm_svm_process *svm_proc = NULL;

    svm_proc = devmm_get_svm_proc_by_mm(mm);
    if (svm_proc == NULL) {
        devmm_drv_err("Find svm_proc fail.\n");
        return;
    }

    devmm_svm_mmu_notifier_unreg(svm_proc);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
static ka_mmu_notifier_t *devmm_alloc_notifier(ka_mm_struct_t *mm)
{
    ka_mmu_notifier_t *notifier = NULL;

    notifier = (ka_mmu_notifier_t *)devmm_kmalloc_ex(sizeof(ka_mmu_notifier_t),
        KA_GFP_ATOMIC | __KA_GFP_NOWARN | __KA_GFP_ACCOUNT | __KA_GFP_ZERO);
    if (notifier == NULL) {
        devmm_drv_err("Kmalloc mmu_notifier fail.\n");
        return KA_ERR_PTR(-ENOMEM);
    }

    return notifier;
}

static void devmm_free_notifier(ka_mmu_notifier_t *mn)
{
    devmm_kfree_ex(mn);
    return;
}
#endif

ka_mmu_notifier_ops_t devmm_process_mmu_notifier = {
    .invalidate_range_start = devmm_notifier_start,
    .release = devmm_notifier_release,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
    .alloc_notifier = devmm_alloc_notifier,
    .free_notifier = devmm_free_notifier,
#endif
};
#endif

int devmm_mmu_notifier_register(struct devmm_svm_process *svm_proc)
{
    int ret = 0;
#ifndef ADAPT_KP_OS_FOR_EMU_TEST
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
    {
        ka_mmu_notifier_t *mn = NULL;
        mn = mmu_notifier_get(&devmm_process_mmu_notifier, svm_proc->mm);
        if (KA_IS_ERR(mn)) {
            ret = (int)KA_PTR_ERR(mn);
            svm_proc->notifier = NULL;
        } else {
            svm_proc->notifier = mn;
        }
    }
#else
    svm_proc->notifier.ops = &devmm_process_mmu_notifier;
    ret = ka_mm_mmu_notifier_register(&svm_proc->notifier, svm_proc->mm);
#endif
    if (ret == 0) {
        /*
         * Fd release will judge this value, if mmu notifier register failed
         * but not UNINITED set, fd release will fail
         */
        svm_proc->notifier_reg_flag = DEVMM_SVM_INITED_FLAG;
    }
#endif
    return ret;
}
