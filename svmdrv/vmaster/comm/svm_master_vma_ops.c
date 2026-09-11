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
#include <linux/slab.h>

#include "devmm_proc_info.h"
#include "svm_proc_mng.h"
#include "devmm_common.h"
#include "devmm_channel.h"
#include "devmm_page_cache.h"
#include "svm_heap_mng.h"
#include "svm_proc_gfp.h"
#include "svm_dynamic_addr.h"

STATIC void devmm_vm_open(ka_vm_area_struct_t *vma)
{
    struct devmm_svm_process *svm_proc = devmm_get_svm_proc_by_mm(vma->vm_mm);
    if (svm_proc == NULL) {
        return;
    }

    devmm_drv_err("User improperly unmap, need to release all resources.\n");
    devmm_svm_ioctl_lock(svm_proc, DEVMM_CMD_WLOCK);
    devmm_svm_mmu_notifier_unreg(svm_proc);
    devmm_svm_ioctl_unlock(svm_proc, DEVMM_CMD_WLOCK);
    return;
}

STATIC int devmm_svm_vm_fault_host_sync_device_data(struct devmm_svm_process *svm_proc,
    struct devmm_svm_heap *heap, u64 start, ka_page_t **pages, u32 adjust_order)
{
    struct devmm_devid svm_id = {0};
    u32 dev_id, phy_id, vfid;
    u32 *page_bitmap = NULL;
    int ret;

    page_bitmap = devmm_get_page_bitmap_with_heap(heap, start);
    if ((page_bitmap == NULL)) {
        return -EINVAL;
    }
    /* dev_id: DEVMM_MAX_DEVICE_NUM means unmap in host and dev */
    dev_id = devmm_svm_va_to_devid(svm_proc, start);
    if (dev_id < DEVMM_MAX_DEVICE_NUM) {
        phy_id = devmm_get_phyid_devid_from_svm_process(svm_proc, dev_id);
        vfid = devmm_get_vfid_from_svm_process(svm_proc, dev_id);
        (void)devmm_fill_svm_id(&svm_id, dev_id, phy_id, vfid);
        devmm_free_pages_cache(svm_proc, dev_id, 1, heap->chunk_page_size, start, true);
        ret = devmm_page_fault_h2d_sync(svm_id, pages, start, adjust_order, heap);
        if (ret != 0) {
            devmm_drv_err("Devmm_page_fault_h2d_sync error. (ret=%d; dev_id=%u)\n", ret, dev_id);
            return ret;
        }
        devmm_svm_free_share_page_msg(svm_proc, heap, start, heap->chunk_page_size, page_bitmap);
        devmm_svm_clear_mapped_with_heap(svm_proc, start, heap->chunk_page_size, dev_id, heap);
    } else if (devmm_is_host_agent(dev_id)) {
        /* host agent not surport page fault */
        devmm_drv_err("Dev_id is error. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    return 0;
}

STATIC int devmm_svm_vm_fault_host_check_bitmap(u32 *page_bitmap)
{
    if ((devmm_page_bitmap_is_page_available(page_bitmap) == 0) ||
          devmm_page_bitmap_is_locked_device(page_bitmap)) {
        return -EINVAL;
    }

    if (devmm_page_bitmap_is_host_mapped(page_bitmap)) {
        return -EINVAL;
    }

    return 0;
}

/*
 * vm do fault: host process and device process
 */
STATIC int devmm_svm_vm_fault_host_proc(struct devmm_svm_process *svm_proc,
    struct devmm_svm_heap *heap, u64 start)
{
    struct devmm_phy_addr_attr attr = {0};
    ka_page_t **pages = NULL;
    u32 *page_bitmap = NULL;
    u32 adjust_order;
    u64 page_num;
    int ret;

    page_bitmap = devmm_get_page_bitmap_with_heap(heap, start);
    if (page_bitmap == NULL) {
        devmm_drv_err("Heap is error. (start=0x%llx; heap_idx=%u)\n", start, heap->heap_idx);
        return -EINVAL;
    }

    if (devmm_svm_vm_fault_host_check_bitmap(page_bitmap) != 0) {
        devmm_drv_err("Va is error, can not fault. (start=0x%llx, bitmap=0x%x)\n",
            start, devmm_page_read_bitmap(page_bitmap));
        devmm_print_pre_alloced_va(svm_proc, start);
        return -EINVAL;
    }

    adjust_order = (heap->heap_type == DEVMM_HEAP_CHUNK_PAGE) ?
        0 : devmm_host_hugepage_fault_adjust_order();
    page_num = 1ull << adjust_order;
    pages = devmm_kvzalloc(sizeof(ka_page_t *) * page_num);
    if (pages == NULL) {
        devmm_drv_err("Kvzalloc failed. (adjust_order=%u; start=0x%llx)\n", adjust_order, start);
        return -ENOMEM;
    }

    devmm_phy_addr_attr_pack(svm_proc, DEVMM_NORMAL_PAGE_TYPE, 0, false, &attr);
    ret = devmm_proc_alloc_pages(svm_proc, &attr, pages, page_num);
    if (ret != 0) {
        devmm_drv_err("Devmm_alloc_pages error. (ret=%d; start=0x%llx; adjust_order=%u)\n",
            ret, start, adjust_order);
        goto devmm_svm_vm_fault_host_free_page;
    }

    ret = devmm_svm_vm_fault_host_sync_device_data(svm_proc, heap, start, pages, adjust_order);
    if (ret != 0) {
        devmm_drv_err("Sync device data error. (ret=%d; start=0x%llx; page_num=%llu)\n",
            ret, start, page_num);
        devmm_proc_free_pages(svm_proc, &attr, pages, page_num);
        goto devmm_svm_vm_fault_host_free_page;
    }

    ret = devmm_pages_remap(svm_proc, start, page_num, pages, 0);
    if (ret != 0) {
        devmm_drv_err("Insert pages vma error. (ret=%d; start=0x%llx; adjust_order=%u)\n",
                      ret, start, adjust_order);
        devmm_proc_free_pages(svm_proc, &attr, pages, page_num);
        goto devmm_svm_vm_fault_host_free_page;
    }
    devmm_svm_set_mapped_with_heap(svm_proc, start, heap->chunk_page_size, DEVMM_INVALID_DEVICE_PHYID, heap);

devmm_svm_vm_fault_host_free_page:
    devmm_kvfree(pages);
    pages = NULL;

    return ret;
}

static int _devmm_svm_vm_fault_host(struct devmm_svm_process *svm_proc,
    ka_vm_area_struct_t *vma, u64 in_start)
{
    struct devmm_svm_heap *heap = NULL;
    u64 start = in_start;
    u64 pa_addr;
    int ret;

    heap = devmm_svm_get_heap(svm_proc, start);
    if (heap == NULL) {
        devmm_drv_err("Incorrect address. (start=0x%llx)\n", start);
        return DEVMM_FAULT_ERROR;
    }
    if (heap->heap_sub_type == SUB_RESERVE_TYPE) {
        devmm_drv_err("Reserve addr not support fault. (va=0x%llx)\n", start);
        return (int)DEVMM_FAULT_ERROR;
    }
    start = ka_base_round_down(start, heap->chunk_page_size);
    ka_task_down_write(&svm_proc->host_fault_sem);
    ret = devmm_va_to_pa(vma, start, &pa_addr);
    if (ret == 0) {
        ka_task_up_write(&svm_proc->host_fault_sem);
        return DEVMM_FAULT_OK;
    }
    ret = devmm_page_fault_get_va_ref(svm_proc, start);
    if (ret != 0) {
        ka_task_up_write(&svm_proc->host_fault_sem);
        devmm_drv_err("Va is in operation. (start=0x%llx; heap_idx=%u)\n",
                      start, heap->heap_idx);
        return DEVMM_FAULT_ERROR;
    }
    ret = devmm_svm_vm_fault_host_proc(svm_proc, heap, start);
    devmm_page_fault_put_va_ref(svm_proc, start);
    ka_task_up_write(&svm_proc->host_fault_sem);

     return (ret == 0) ? DEVMM_FAULT_OK : DEVMM_FAULT_ERROR;
}

STATIC int devmm_svm_vm_fault_host(ka_vm_area_struct_t *vma, ka_vm_fault_struct_t *vmf)
{
    u64 start = ka_mm_get_vm_start(vma) + (vmf->pgoff << KA_MM_PAGE_SHIFT);
    struct devmm_svm_process *svm_proc = NULL;
    int ret;

    devmm_drv_debug("Host enter vm fault. (start=0x%llx)\n", start);

    svm_proc = devmm_get_svm_proc_by_mm(vma->vm_mm);
    if (svm_proc == NULL) {
        devmm_drv_err("Can't find process by current pid.\n");
        return DEVMM_FAULT_ERROR;
    }

    if (devmm_svm_mem_is_enable(svm_proc) == false) {
        devmm_drv_err("Host mmap failed, can't fill host page.\n");
        return DEVMM_FAULT_ERROR;
    }

    ka_task_down_read(&svm_proc->ioctl_rwsem);
    ret = _devmm_svm_vm_fault_host(svm_proc, vma, start);
    ka_task_up_read(&svm_proc->ioctl_rwsem);
    return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
STATIC ka_vm_fault_t devmm_svm_vmf_fault_host(ka_vm_fault_struct_t *vmf)
{
    return (ka_vm_fault_t)devmm_svm_vm_fault_host(vmf->vma, vmf);
}
#endif

#ifndef EMU_ST
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 3, 0)
static int devmm_mremap(ka_vm_area_struct_t *area)
{
    return -EACCES;
}
#endif
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 17, 0)
static vm_fault_t devmm_mkf_write(struct vm_fault *vmf)
{
    u64 start = vmf->address;
    devmm_drv_debug("Host mem enter devmm_mkf_write. (start=0x%llx)\n", start);
 
    return VM_FAULT_SIGSEGV;
}
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
static int devmm_mkf_write(struct vm_fault *vmf)
{
    u64 start = vmf->address;
    devmm_drv_debug("Host mem enter devmm_mkf_write. (start=0x%llx)\n", start);
 
    return VM_FAULT_SIGSEGV;
}
#else
static int devmm_mk_write(struct vm_area_struct *vma, struct vm_fault *vmf)
{
    u64 start = vma->vm_start + (vmf->pgoff << PAGE_SHIFT);
    devmm_drv_debug("Host mem enter devmm_mk_write. (start=0x%llx)\n", start);
    
    return VM_FAULT_SIGSEGV;
}
#endif

static ka_vm_operations_struct_t svm_master_vma_ops = {
    .open = devmm_vm_open,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
    .fault = devmm_svm_vmf_fault_host,
#else
    .fault = devmm_svm_vm_fault_host,
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
    .pfn_mkwrite = devmm_mkf_write,
#else
    .pfn_mkwrite = devmm_mk_write,
#endif
#ifndef EMU_ST
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 3, 0)
    .mremap = devmm_mremap,
#endif
#endif
};

void devmm_svm_setup_vma_ops(ka_vm_area_struct_t *vma)
{
    ka_mm_set_vm_ops(vma, &svm_master_vma_ops);
}
