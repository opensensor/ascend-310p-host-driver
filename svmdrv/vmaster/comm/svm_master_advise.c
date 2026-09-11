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

#include "devmm_proc_mem_copy.h"
#include "devmm_common.h"
#include "svm_master_remote_map.h"
#include "svm_heap_mng.h"
#include "svm_msg_client.h"
#include "svm_master_memcpy.h"
#include "devmm_channel.h"
#include "svm_mem_stats.h"
#include "svm_master_dev_capability.h"
#include "svm_master_advise.h"

#define DEVMM_PRINT_STR_MAX 8

STATIC u32 *devmm_get_alloced_page_bitmap(struct devmm_svm_process *svm_process, u64 p)
{
    u32 *page_bitmap = NULL;

    page_bitmap = devmm_get_page_bitmap(svm_process, p);
    if (page_bitmap == NULL) {
        devmm_drv_err("Can't find page_bitmap. (va=0x%llx)\n", p);
        return page_bitmap;
    }

    if (!devmm_page_bitmap_is_page_available(page_bitmap)) {
        devmm_drv_err("Va isn't alloced. (va=0x%llx; page_bitmap=0x%x)\n", p, devmm_page_read_bitmap(page_bitmap));

        return NULL;
    }

    return page_bitmap;
}

STATIC void devmm_clear_hostmapped_prefetch(struct devmm_svm_process *svm_pro, u64 devPtr, size_t count,
    u32 dev_id, struct devmm_memory_attributes *attr)
{
    u32 adjust_order, page_size, num, i;
    u32 stamp = (u32)ka_jiffies;
    u32 *page_bitmap = NULL;

    page_bitmap = devmm_get_alloced_page_bitmap(svm_pro, devPtr);
    if (page_bitmap == NULL) {
        devmm_drv_err("Can't find bitmap. (devPtr=0x%llx; count=%ld; devid=%u)\n",
                      devPtr, count, dev_id);
        return;
    }

    if (attr->is_svm_huge) {
        adjust_order = devmm_host_hugepage_fault_adjust_order();
        page_size = devmm_svm->device_hpage_size;
    } else {
        adjust_order = 0;
        page_size = KA_MM_PAGE_SIZE;
    }
    num = (u32)(ka_base_round_up(count, page_size) / page_size);
    for (i = 0; i < num; i++) {
        if (devmm_page_bitmap_is_host_mapped(page_bitmap + i)) {
            devmm_unmap_pages(svm_pro, devPtr + i * (unsigned long)page_size, 1ull << adjust_order);
            devmm_page_bitmap_clear_flag(page_bitmap + i, DEVMM_PAGE_HOST_MAPPED_MASK);
        }
        devmm_try_cond_resched(&stamp);
    }
}

STATIC void devmm_set_device_maped(struct devmm_svm_process *svm_proc, u64 devPtr, size_t count,
    u32 logical_devid, u32 page_size)
{
    u32 stamp = (u32)ka_jiffies;
    u32 *page_bitmap = NULL;
    u64 page_num;
    int i;

    page_bitmap = devmm_get_alloced_page_bitmap(svm_proc, devPtr);
    if (page_bitmap == NULL) {
        devmm_drv_err("Can't find bitmap. (va=0x%llx; logical_devid=%u)\n", devPtr, logical_devid);
        return;
    }

    page_num = devmm_get_pagecount_by_size(devPtr, count, page_size);
    for (i = 0; i < page_num; i++) {
        devmm_page_bitmap_set_flag(page_bitmap + i, DEVMM_PAGE_ADVISE_POPULATE_MASK);
        devmm_page_bitmap_set_flag(page_bitmap + i, DEVMM_PAGE_DEV_MAPPED_MASK);
        devmm_page_bitmap_set_devid(page_bitmap + i, logical_devid);
        devmm_try_cond_resched(&stamp);
    }
}

STATIC void devmm_fill_attr_after_prefetch(struct devmm_svm_process *svm_proc,
    struct devmm_page_query_arg *query_arg,
    struct devmm_memory_attributes *old_attr,
    struct devmm_memory_attributes *new_attr)
{
    new_attr->is_local_host = false;
    new_attr->is_host_pin = false;
    new_attr->is_svm = true;
    new_attr->is_svm_huge = old_attr->is_svm_huge;
    new_attr->is_svm_host = false;
    new_attr->is_svm_device = true;
    new_attr->is_svm_non_page = false;
    new_attr->is_locked_host = false;
    new_attr->is_locked_device = false;
    new_attr->is_ipc_open = false;
    new_attr->is_svm_remote_maped = false;
    if ((!devmm_is_host_agent(query_arg->dev_id)) && (devmm_dev_capability_support_pcie_dma_sva(query_arg->dev_id))) {
        new_attr->copy_use_va = true;
    } else {
        new_attr->copy_use_va = false;
    }
    new_attr->bitmap = old_attr->bitmap;
    new_attr->page_size = old_attr->page_size;
    new_attr->host_page_size = old_attr->host_page_size;
    new_attr->heap_size = old_attr->heap_size;
    new_attr->va = old_attr->va;
    new_attr->ssid = svm_proc->deviceinfo[query_arg->logical_devid].ssid;
    new_attr->logical_devid = query_arg->logical_devid;
    new_attr->devid = query_arg->dev_id;
    new_attr->vfid = query_arg->process_id.vfid;
}

STATIC int devmm_prefetch_to_device(struct devmm_svm_process *svm_proc, u64 dev_ptr,
    u64 byte_cnt, struct devmm_page_query_arg *query_arg, struct devmm_memory_attributes *attr)
{
    struct devmm_memory_attributes device_attr = {0};
    struct devmm_mem_copy_convrt_para para;
    u64 aligned_down_addr, aligned_cnt;
    u32 dev_id = query_arg->dev_id;
    int ret;

    devmm_fill_attr_after_prefetch(svm_proc, query_arg, attr, &device_attr);

    if (!devmm_acquire_aligned_addr_and_cnt(dev_ptr, byte_cnt, attr->is_svm_huge, &aligned_down_addr, &aligned_cnt)) {
        devmm_drv_err("Acquire aligned addr and cnt failed. (dev_ptr=0x%llx; byte_count=%llx\n", dev_ptr, byte_cnt);
        return -EINVAL;
    }
    devmm_set_device_maped(svm_proc, aligned_down_addr, aligned_cnt, device_attr.logical_devid, attr->page_size);

    para.dst = aligned_down_addr;
    para.src = aligned_down_addr;
    para.count = aligned_cnt;
    para.direction = DEVMM_COPY_HOST_TO_DEVICE;
    para.blk_size = ka_base_min((u32)KA_MM_PAGE_SIZE, devmm_svm->device_page_size);
    devmm_init_task_para(&para, true, true, false, DEVMM_CPY_SYNC_MODE);

    ret = devmm_ioctl_memcpy_process_res(svm_proc, &para, attr, &device_attr);
    if (ret != 0) {
        devmm_drv_err_if((ret != -EOPNOTSUPP), "Memcpy failed. (ret=%d; dev_ptr=0x%llx; byte_count=%llx; "
            "aligned_down_addr=0x%llx; aligned_count=%llx; dev_id=%u)\n",
            ret, dev_ptr, byte_cnt, aligned_down_addr, aligned_cnt, dev_id);
        return ret;
    }

    devmm_clear_hostmapped_prefetch(svm_proc, aligned_down_addr, aligned_cnt, dev_id, attr);

    devmm_drv_debug("Prefetch to device succeeded. "
        "(dev_ptr=0x%llx; byte_count=0x%llx; aligned_down_addr=0x%llx; aligned_count=0x%llx; dev_id=%u)\n",
        dev_ptr, byte_cnt, aligned_down_addr, aligned_cnt, dev_id);
    return 0;
}

STATIC int devmm_prefetch_to_device_proc(struct devmm_svm_process *svm_pro,
    u64 dev_ptr, u64 byte_count, struct devmm_page_query_arg *query_arg)
{
    u64 aligned_down_addr, aligned_count;
    struct devmm_memory_attributes attr;
    u64 byte_offset, cnt;
    int ret = -EINVAL;
    u64 ptr = dev_ptr;
    u64 per_max_cnt;
    u32 stamp = (u32)ka_jiffies;
    int tmp_ret;

    tmp_ret = devmm_get_memory_attributes(svm_pro, dev_ptr, &attr);
    if (tmp_ret != 0) {
        devmm_drv_err("Devmm_get_memory_attributes failed. (dev_ptr=0x%llx)\n", dev_ptr);
        return tmp_ret;
    }
    if (!devmm_acquire_aligned_addr_and_cnt(ptr, byte_count, attr.is_svm_huge, &aligned_down_addr, &aligned_count)) {
        devmm_drv_err("Acquire aligned addr and cnt failed. (dev_ptr=0x%llx; byte_count=%llx\n", ptr, byte_count);
        return -EINVAL;
    }
    per_max_cnt = DEVMM_PREFETCH_COPY_NUM * KA_MM_PAGE_SIZE;
    for (byte_offset = 0; byte_offset < aligned_count; byte_offset += cnt, aligned_down_addr += cnt) {
        cnt = ka_base_min((aligned_count - byte_offset), per_max_cnt);

        ret = devmm_prefetch_to_device(svm_pro, aligned_down_addr, cnt, query_arg, &attr);
        if (ret != 0) {
            devmm_drv_err_if((ret != -EOPNOTSUPP), "Prefetch to device error. (ret=%d; ptr=0x%llx; cnt=%llu)\n",
                ret, aligned_down_addr, cnt);
            return ret;
        }
        devmm_drv_debug("Prefetch information. (ret=%d; ptr=0x%llx; cnt=%llu)\n", ret, aligned_down_addr, cnt);
        devmm_try_cond_resched(&stamp);
    }

    return ret;
}

STATIC int devmm_prefetch_to_device_frame(struct devmm_svm_process *svm_pro,
    struct devmm_page_query_arg query_arg, u32 *page_bitmap, u64 *num)
{
    u64 i, page_cnt, prefetch_cnt, ptr;
    int ret;

    page_cnt = *num;
    /* va not continued in host */
    for (i = 0; i < page_cnt; i++) {
        if (!devmm_page_bitmap_is_host_mapped(page_bitmap + i)) {
            /* host and devcie not mapped, goto advise, return i for pretched num */
            break;
        }
    }
    /* last take */
    prefetch_cnt = i;
    if (prefetch_cnt == 0) {
        *num = i;
        return 0;
    }
    prefetch_cnt *= query_arg.page_size;
    ptr = query_arg.va;
    ret = devmm_prefetch_to_device_proc(svm_pro, ptr, prefetch_cnt, &query_arg);
    if (ret != 0) {
        devmm_drv_err_if((ret != -EOPNOTSUPP), "Prefetch to device proc error. (ret=%d; ptr=0x%llx; prefetch_cnt=%llu)\n",
            ret, ptr, prefetch_cnt);
        return ret;
    }
    *num = i;
    devmm_drv_debug("Prefetch information. (num=%llu; query_arg_va=0x%llx; query_arg_size=%llu; prefetch_cnt=%llu)\n",
        i, query_arg.va, query_arg.size, prefetch_cnt);
    return 0;
}

STATIC int devmm_populate_to_device_frame(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap,
    struct devmm_page_query_arg query_arg, u32 *page_bitmap, u64 *num)
{
    u32 page_cnt, i, populate_num;
    u32 stamp = (u32)ka_jiffies;
    int ret;

    page_cnt = (u32)(*num);
    for (i = 0, populate_num = 0; i < page_cnt; i++) {
        if (devmm_page_bitmap_is_host_mapped(page_bitmap + i) ||
            devmm_page_bitmap_is_dev_mapped(page_bitmap + i)) {
            /* is host or device maped goto prefetch */
            break;
        }
        devmm_page_bitmap_set_devid(page_bitmap + i, query_arg.logical_devid);
        devmm_page_bitmap_set_flag(page_bitmap + i, DEVMM_PAGE_DEV_MAPPED_MASK);
        devmm_page_bitmap_set_flag(page_bitmap + i, DEVMM_PAGE_ADVISE_POPULATE_MASK);
        populate_num++;
        devmm_try_cond_resched(&stamp);
    }
    page_cnt = (((heap->heap_type == DEVMM_HEAP_CHUNK_PAGE) && (heap->heap_sub_type == SUB_SVM_TYPE)) ?
        (populate_num * (1UL << devmm_device_page_adjust_order())) : populate_num);
    query_arg.size = query_arg.page_size * (unsigned long)populate_num;  /* just create size of page cnt */
    query_arg.msg_id = DEVMM_CHAN_PAGE_CREATE_H2D_ID;
    query_arg.page_insert_dev_id = query_arg.logical_devid;
    query_arg.addr_type = DEVMM_ADDR_TYPE_DMA;
    /* populate_num will not eq 0 */
    ret = devmm_query_page_by_msg(svm_proc, query_arg, NULL, &page_cnt);
    if (ret != 0) {
        devmm_drv_debug("Can not memory populate. (ret=%d; va=0x%llx; size=%llu; page_cnt=%u; num=%llu)\n",
            ret, query_arg.va, query_arg.size, page_cnt, *num);
        for (i = 0; i < populate_num; i++) {
            devmm_page_bitmap_clear_flag(page_bitmap + i, DEVMM_PAGE_DEV_MAPPED_MASK);
            devmm_page_bitmap_clear_flag(page_bitmap + i, DEVMM_PAGE_ADVISE_POPULATE_MASK);
        }
        return ret;
    }
    *num = (u64)populate_num;
    devmm_drv_debug("Argument. (populate_num=%u; va=0x%llx; size=%llu)\n",
                    page_cnt, query_arg.va, query_arg.size);
    return 0;
}

STATIC int devmm_advise_populate_arg_check(struct devmm_svm_heap *heap,
    u32 *page_bitmap, struct devmm_ioctl_arg *arg, u64 page_cnt)
{
    u64 i;

    /* if locked device, device id must eq device get form bitmap */
    if (devmm_page_bitmap_is_locked_device(page_bitmap) &&
        (devmm_page_bitmap_get_devid(page_bitmap) != arg->head.logical_devid)) {
        devmm_drv_err("Devid error. (va=0x%llx; locked_did=%u; advise_did=%u)\n",
            arg->data.advise_para.ptr, devmm_page_bitmap_get_devid(page_bitmap), arg->head.logical_devid);
        return -EINVAL;
    }
    for (i = 0; i < page_cnt; i++) {
        if (devmm_page_bitmap_is_dev_mapped(page_bitmap + i) &&
            (devmm_page_bitmap_get_devid(page_bitmap + i) != arg->head.logical_devid)) {
            /* mapped by device, but deivce id is not same */
            devmm_drv_err("Mapped by device, but advise devid error. (logical_devid=%u; va=0x%llx; num=%llu)\n",
                arg->head.logical_devid, arg->data.advise_para.ptr, i);
            return -EINVAL;
        }
    }
    return 0;
}

STATIC int devmm_advise_populate_process(struct devmm_svm_process *svm_pro, struct devmm_svm_heap *heap,
    u32 *page_bitmap, struct devmm_ioctl_arg *arg, u64 page_cnt)
{
    struct devmm_page_query_arg query_arg = {{0}};
    u32 stamp = (u32)ka_jiffies;
    u64 i, num;
    int ret;

    if (devmm_advise_populate_arg_check(heap, page_bitmap, arg, page_cnt) != 0) {
        devmm_drv_err("Devmm populate_arg_check error. (va=0x%llx; devid=%u)\n",
                      arg->data.advise_para.ptr, arg->head.devid);
        return -EINVAL;
    }

    query_arg.page_size = heap->chunk_page_size;
    query_arg.process_id.hostpid = svm_pro->process_id.hostpid;
    query_arg.process_id.vfid = (u16)arg->head.vfid;
    query_arg.dev_id = arg->head.devid;
    query_arg.logical_devid = arg->head.logical_devid;
    query_arg.offset = 0;
    query_arg.bitmap = devmm_page_read_bitmap(page_bitmap);
    query_arg.is_giant_page = ((arg->data.advise_para.advise & DV_ADVISE_GIANTPAGE) != 0);
    for (i = 0; i < page_cnt; i += num) {
        num = page_cnt - i;
        query_arg.va = arg->data.advise_para.ptr + i * heap->chunk_page_size;
        query_arg.size = arg->data.advise_para.count - i * heap->chunk_page_size;
        if (devmm_page_bitmap_is_host_mapped(page_bitmap + i)) {
            /* mapped by host prefetch data to device */
            ret = devmm_prefetch_to_device_frame(svm_pro, query_arg, (page_bitmap + i), &num);
        } else if (!devmm_page_bitmap_is_dev_mapped(page_bitmap + i)) {
            /* not maped by host and device populate page on device */
            ret = devmm_populate_to_device_frame(svm_pro, heap, query_arg, (page_bitmap + i), &num);
        } else {
            /* mapped by device, next page */
            ret = 0;
            num = 1;
        }
        if ((ret != 0) || num == 0) {
            return ret;
        }
        devmm_try_cond_resched(&stamp);
        devmm_drv_debug("Devmm populated. (va=0x%llx; count=%llu; devid=%u; num=%llu; i=%llu)\n",
            query_arg.va, query_arg.size, arg->head.devid, num, i);
    }

    return 0;
}

int devmm_ipc_page_table_create_process(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap,
    u32 *page_bitmap, struct devmm_ioctl_arg *arg, void *n_attr)
{
    struct devmm_page_query_arg query_arg = {{0}};
    struct devmm_ipc_owner_attr owner_attr = {0};
    u64 va = arg->data.advise_para.ptr;
    u64 page_cnt;
    u32 num;
    int ret;
    u64 i;

    page_cnt = devmm_get_pagecount_by_size(va, arg->data.advise_para.count, heap->chunk_page_size);
    ret = devmm_ipc_query_owner_attr_by_va(svm_proc, va, n_attr, &owner_attr);
    if (ret != 0) {
        devmm_drv_err("Query owner attr by va fail. (ret=%d; va=0x%llx)\n", ret, va);
        return ret;
    }
    num = (u32)((heap->heap_type == DEVMM_HEAP_HUGE_PAGE) ?
        page_cnt : (page_cnt * (1UL << devmm_device_page_adjust_order())));

    query_arg.p2p_owner_va = owner_attr.va;
    query_arg.p2p_owner_process_id.hostpid = owner_attr.pid;
    query_arg.p2p_owner_process_id.devid = (uint16_t)owner_attr.devid;
    query_arg.p2p_owner_process_id.vfid = (uint16_t)owner_attr.vfid;
    query_arg.bitmap = devmm_page_read_bitmap(page_bitmap);
    query_arg.dev_id = arg->head.devid;
    query_arg.process_id.hostpid = svm_proc->process_id.hostpid;
    query_arg.process_id.vfid = (u16)arg->head.vfid;
    query_arg.va = va;
    query_arg.size = arg->data.advise_para.count;
    query_arg.offset = (u64)(arg->data.advise_para.ptr & (heap->chunk_page_size - 1));
    query_arg.page_size = heap->chunk_page_size;
    query_arg.addr_type = DEVMM_ADDR_TYPE_DMA;
    query_arg.msg_id = DEVMM_CHAN_PAGE_P2P_CREATE_H2D_ID;
    query_arg.page_insert_dev_id = arg->head.logical_devid;
    query_arg.p2p_owner_sdid = owner_attr.sdid;
    query_arg.mem_map_route = owner_attr.mem_map_route;
    ret = devmm_p2p_page_create_msg(svm_proc, query_arg, NULL, &num);
    if ((ret != 0) || (num == 0)) {
        devmm_drv_err("Advise_page error. (ret=%d; num=%u; va=0x%llx; count=%lu; page_cnt=%llu; page_size=%u)\n",
            ret, num, arg->data.advise_para.ptr, arg->data.advise_para.count, page_cnt, heap->chunk_page_size);

        return ((ret != 0) ? ret : -EINVAL);
    }

    for (i = 0; i < page_cnt; i++) {
        devmm_page_bitmap_set_flag(page_bitmap + i, DEVMM_PAGE_ADVISE_POPULATE_MASK);
        devmm_page_bitmap_set_flag(page_bitmap + i, DEVMM_PAGE_DEV_MAPPED_MASK);
        devmm_page_bitmap_set_flag(page_bitmap + i, DEVMM_PAGE_LOCKED_DEVICE_MASK);
        devmm_page_bitmap_set_devid(page_bitmap + i, arg->head.logical_devid);
    }

    devmm_drv_debug("Show details. "
        "(dev_id=%d; page_num=%u; va=0x%llx; count=%lu; page_cnt=%llu; page_size=%u; owner_va=0x%llx; src_dev=%d)\n",
        query_arg.dev_id, num, arg->data.advise_para.ptr, arg->data.advise_para.count, page_cnt,
        heap->chunk_page_size, query_arg.p2p_owner_va, query_arg.p2p_owner_process_id.devid);

    return 0;
}

static bool devmm_page_bitmaps_is_dev_mapped(u32 *page_bitmap, u64 page_bitmap_cnt)
{
    u64 i;

    for (i = 0; i < page_bitmap_cnt; i++) {
        if (devmm_page_bitmap_is_dev_mapped(page_bitmap + i) == false) {
            return false;
        }
    }

    return true;
}

STATIC int devmm_advise_d2d_populate_process(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap,
    u32 *page_bitmap, struct devmm_ioctl_arg *arg, u64 page_bitmap_cnt)
{
    struct devmm_page_query_arg query_arg = {{0}};
    u32 phy_devid = devmm_page_bitmap_get_phy_devid(svm_proc, page_bitmap);
    u32 num;
    u64 i;
    int ret;

    if (devmm_is_same_dev(phy_devid, arg->head.devid) &&
        devmm_page_bitmaps_is_dev_mapped(page_bitmap, page_bitmap_cnt)) {
        return 0;
    }

    if (heap->heap_sub_type == SUB_RESERVE_TYPE) {
        query_arg.page_size = (heap->heap_type == DEVMM_HEAP_HUGE_PAGE) ?
            devmm_svm->device_hpage_size : devmm_svm->device_page_size;
        num = (u32)(page_bitmap_cnt * heap->chunk_page_size / query_arg.page_size);
    } else {
        query_arg.page_size = heap->chunk_page_size;
        num = (u32)(((heap->heap_type == DEVMM_HEAP_CHUNK_PAGE) && (heap->heap_sub_type == SUB_SVM_TYPE)) ?
            (page_bitmap_cnt * (1UL << devmm_device_page_adjust_order())) : page_bitmap_cnt);
    }

    query_arg.p2p_owner_va = arg->data.advise_para.ptr;
    query_arg.p2p_owner_process_id.hostpid = svm_proc->process_id.hostpid;
    query_arg.p2p_owner_process_id.devid = (u16)phy_devid;
    query_arg.p2p_owner_process_id.vfid = (u16)devmm_page_bitmap_get_vfid(svm_proc, page_bitmap);

    query_arg.bitmap = devmm_page_read_bitmap(page_bitmap);
    query_arg.dev_id = arg->head.devid;
    query_arg.process_id.hostpid = svm_proc->process_id.hostpid;
    query_arg.process_id.vfid = (u16)arg->head.vfid;
    query_arg.va = arg->data.advise_para.ptr;
    query_arg.size = arg->data.advise_para.count;
    query_arg.offset = (u64)(arg->data.advise_para.ptr & (heap->chunk_page_size - 1));
    query_arg.addr_type = DEVMM_ADDR_TYPE_DMA;
    query_arg.msg_id = DEVMM_CHAN_PAGE_P2P_CREATE_H2D_ID;
    query_arg.page_insert_dev_id = arg->head.logical_devid;
#ifndef EMU_ST
    /* sdid == KA_UINT_MAX means it's on the same pod */
    query_arg.p2p_owner_sdid = KA_UINT_MAX;
#endif
    for (i = 0; i < page_bitmap_cnt; i++) {
        devmm_page_bitmap_set_flag(page_bitmap + i, DEVMM_PAGE_ADVISE_MEMORY_SHARED_MASK);
    }
    devmm_page_bitmap_set_value_nolock(&query_arg.bitmap, DEVMM_PAGE_DEVID_SHIT, DEVMM_PAGE_DEVID_WID, phy_devid);
    ret = devmm_p2p_page_create_msg(svm_proc, query_arg, NULL, &num);
    if ((ret != 0) || (num == 0)) {
        devmm_drv_err("Advise_page error or num is 0. "
            "(ret=%d; num=%u; va=0x%llx; count=%lu; page_bitmap_cnt=%llu; page_size=%u)\n",
            ret, num, arg->data.advise_para.ptr, arg->data.advise_para.count, page_bitmap_cnt, heap->chunk_page_size);
        return ((ret != 0) ? ret : -EINVAL);
    }

    return 0;
}

STATIC int devmm_prefetch_host_agent_process(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap,
    u32 *page_bitmap, struct devmm_ioctl_arg *arg, u64 page_cnt)
{
    struct devmm_mem_advise_para *prefetch_para = &arg->data.prefetch_para;
    struct devmm_mem_remote_map_para map_para = {0};
    struct devmm_devid *devids = &arg->head;
    struct devmm_memory_attributes attr;
    int ret, i;

    ret = devmm_get_memory_attributes(svm_proc, prefetch_para->ptr, &attr);
    if (ret != 0) {
        devmm_drv_err("Get attributes failed. (src_va=0x%llx; ret=%d)\n", map_para.src_va, ret);
        return ret;
    }

    if (!devmm_is_host_agent(devids->devid) && devmm_is_pcie_connect(devids->devid)) {
        ret = devmm_shm_config_txatu(svm_proc, devids->devid);
        if (ret != 0) {
            devmm_drv_err("Config txatu failed. (ret=%d)\n", ret);
            return ret;
        }
    }

    if (devmm_is_host_agent(devids->devid) && (attr.is_locked_device == false)) {
#ifndef EMU_ST
        devmm_drv_run_info("Don't support prefetch no_locked dev_ptr to host_agent.\n");
#endif
        return -EOPNOTSUPP;
    }

    map_para.src_va = prefetch_para->ptr;
    map_para.dst_va = prefetch_para->ptr;
    map_para.size = prefetch_para->count;
    ret = devmm_map_svm_mem_to_agent(svm_proc, devids, &map_para, &attr);
    if (ret != 0) {
        devmm_drv_err("Remote_map failed. (src_va=0x%llx; size=%llu)\n", map_para.src_va, map_para.size);
        return ret;
    }

    for (i = 0; i < page_cnt; i++) {
        devmm_page_bitmap_set_flag(page_bitmap + i, DEVMM_PAGE_ADVISE_MEMORY_SHARED_MASK);
    }
    return 0;
}

static int devmm_ioctl_advise_populate(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap,
    u32 *page_bitmap, struct devmm_ioctl_arg *arg, u64 page_cnt)
{
    u32 devid_from_bitmap;
    int ret;

    if (devmm_page_bitmap_is_ipc_open_mem(page_bitmap)) {
        if (devmm_page_bitmap_is_dev_mapped(page_bitmap)) {
            if (devmm_page_bitmap_get_devid(page_bitmap) == arg->head.logical_devid) {
                return 0;
            } else {
                devmm_drv_err("Va has allready locked by other device. (va=0x%llx)\n", arg->data.advise_para.ptr);
                return -EINVAL;
            }
        }

        /* 1. proc1 create, 2. proc2 open, 3. proc2 prefetch */
        ret = devmm_ipc_page_table_create_process(svm_proc, heap, page_bitmap, arg, NULL);
    } else {
        devid_from_bitmap = devmm_page_bitmap_get_phy_devid(svm_proc, page_bitmap);
        if (devmm_is_host_agent(devid_from_bitmap) && devmm_is_host_agent(arg->head.devid)) {
#ifndef EMU_ST
            devmm_drv_run_info("Don't support prefetch host_agent to host_agent.\n");
#endif
            return -EOPNOTSUPP;
        }

        if (devmm_is_host_agent(devid_from_bitmap) || devmm_is_host_agent(arg->head.devid)) {
            ret = devmm_prefetch_host_agent_process(svm_proc, heap, page_bitmap, arg, page_cnt);
        } else if (devmm_page_bitmap_is_dev_mapped(page_bitmap)) {
            ret = devmm_advise_d2d_populate_process(svm_proc, heap, page_bitmap, arg, page_cnt);
        } else {
            /* device not mapped or device mapped and advise to the same device */
            ret = devmm_advise_populate_process(svm_proc, heap, page_bitmap, arg, page_cnt);
        }
    }

    return ret;
}

static int devmm_advise_svm_check(struct devmm_svm_process *svm_proc, struct devmm_mem_advise_para *advise_para,
    u32 dev_id, u32 logical_devid)
{
    u32 *fst_page_bitmap = NULL;
    u32 devid_from_bitmap;

    if (devmm_svm_mem_is_enable(svm_proc) == false) {
        devmm_drv_err("Host mmap failed, can't fill host page.\n");
        return -EINVAL;
    }

    fst_page_bitmap = devmm_get_alloced_va_fst_page_bitmap(svm_proc, advise_para->ptr);
    if (fst_page_bitmap == NULL) {
        devmm_drv_err("Get first bitmap error. (va=0x%llx)\n", advise_para->ptr);
        return -EINVAL;
    }
    if (((advise_para->advise & DV_ADVISE_LOCK_DEV) != 0) &&
        (devmm_page_bitmap_is_locked_device(fst_page_bitmap) ==0)) {
        devmm_page_bitmap_set_flag(fst_page_bitmap, DEVMM_PAGE_LOCKED_DEVICE_MASK);
        devmm_page_bitmap_set_devid(fst_page_bitmap, logical_devid);
    }
    devid_from_bitmap = devmm_page_bitmap_get_phy_devid(svm_proc, fst_page_bitmap);
    if (devmm_dev_is_same_system(dev_id, devid_from_bitmap) == DEVMM_FALSE) {
#ifndef EMU_ST
        devmm_drv_run_info("Don't support advise to diffenent os. (devid=%u; devid_from_first_bitmap=%u)\n",
            dev_id, devid_from_bitmap);
#endif
        return -EOPNOTSUPP;
    }

    return 0;
}

static int devmm_advise_check(struct devmm_svm_heap *heap, struct devmm_svm_process *svm_pro,
    u32 *page_bitmap, u64 chunk_cnt, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_advise_para *advise_para = &arg->data.advise_para;
    u32 dev_id = arg->head.devid;
    u64 i;

    if (devmm_is_host_agent(dev_id) &&
        ((heap->heap_sub_type != SUB_DEVICE_TYPE) || heap->heap_type != DEVMM_HEAP_CHUNK_PAGE)) {
#ifndef EMU_ST
        devmm_drv_run_info("Host agent only support heap SUB_DEVICE_TYPE DEVMM_HEAP_CHUNK_PAGE."
            " (heap_sub_type=%u; heap_type=%u)\n", heap->heap_sub_type, heap->heap_type);
#endif
        return -EOPNOTSUPP;
    }

    if ((heap->heap_sub_type == SUB_READ_ONLY_TYPE) &&
        ((advise_para->advise & DV_ADVISE_READONLY) == 0)) {
        devmm_drv_err("Advise input error, need readonly flag. (heap_sub_type=%u; advise=%x)\n",
            heap->heap_sub_type, advise_para->advise);
        return -EINVAL;
    }

    for (i = 0; i < chunk_cnt; i++) {
        if (!devmm_page_bitmap_is_page_available(page_bitmap + i)) {
            devmm_drv_err("Advise_populate error, not alloc. (devPtr=0x%llx; count=%lu)\n",
                          advise_para->ptr, advise_para->count);
            return -EINVAL;
        }
    }

    /* if addr is not svm addr, and advise is not DV_ADVISE_PERSISTENT, not permission to advise again */
    if (devmm_page_bitmap_is_advise_populate(page_bitmap) && (heap->heap_sub_type != SUB_SVM_TYPE) &&
        ((advise_para->advise & DV_ADVISE_PERSISTENT) == 0)) {
        if (devmm_dev_is_same_system(dev_id, devmm_page_bitmap_get_phy_devid(svm_pro, page_bitmap))) {
            devmm_drv_err("Don't support advise again. (advise=0x%x; dev_id=%d)\n", advise_para->advise, dev_id);
            return -EINVAL;
        }
    }

    /* if addr is svm type and advise populate, check first bitmap */
    if (((advise_para->advise & DV_ADVISE_POPULATE) != 0) && (heap->heap_sub_type == SUB_SVM_TYPE)) {
        int ret = devmm_advise_svm_check(svm_pro, advise_para, dev_id, arg->head.logical_devid);
        if (ret != 0) {
            devmm_drv_err("Advise svm memory check failed. (devid=%u; locig_id=%u; ret=%d)\n",
                dev_id, arg->head.logical_devid, ret);
            return ret;
        }
    }

    return 0;
}

static void devmm_advise_set_bitmap(struct devmm_ioctl_arg *arg, u32 *page_bitmap, u64 chunk_cnt)
{
    struct devmm_mem_advise_para *advise_para = &arg->data.advise_para;
    u32 stamp = (u32)ka_jiffies;
    u64 i;

    for (i = 0; i < chunk_cnt; i++) {
        /* if aready populate donot change mem attribute */
        if (devmm_page_bitmap_is_advise_populate(page_bitmap + i)) {
            continue;
        }
        if ((advise_para->advise & DV_ADVISE_DDR) != 0) {
            /* ddr and hbm is mutual exclusion */
            devmm_page_bitmap_set_flag(page_bitmap + i, DEVMM_PAGE_ADVISE_DDR_MASK);
            devmm_page_bitmap_clear_flag(page_bitmap + i, DEVMM_PAGE_ADVISE_P2P_HBM_MASK);
        }
        if ((advise_para->advise & DV_ADVISE_CONTINUTY) != 0) {
            devmm_page_bitmap_set_flag(page_bitmap + i, DEVMM_PAGE_CONTINUTY_MASK);
        }
        if ((advise_para->advise & DV_ADVISE_TS_DDR) != 0) {
            devmm_page_bitmap_set_flag(page_bitmap + i, DEVMM_PAGE_ADVISE_TS_MASK);
        }
        if ((advise_para->advise & DV_ADVISE_P2P_HBM) != 0) {
            /* ddr and hbm is mutual exclusion */
            devmm_page_bitmap_set_flag(page_bitmap + i, DEVMM_PAGE_ADVISE_P2P_HBM_MASK);
            devmm_page_bitmap_clear_flag(page_bitmap + i, DEVMM_PAGE_ADVISE_DDR_MASK);
        }
        if ((advise_para->advise & DV_ADVISE_P2P_DDR) != 0) {
            /* p2p ddr and hbm is mutual exclusion */
            devmm_page_bitmap_set_flag(page_bitmap + i, DEVMM_PAGE_ADVISE_P2P_DDR_MASK);
            devmm_page_bitmap_clear_flag(page_bitmap + i, DEVMM_PAGE_ADVISE_P2P_HBM_MASK);
        }
        if ((advise_para->advise & DV_ADVISE_READONLY) != 0) {
            devmm_page_bitmap_set_flag(page_bitmap + i, DEVMM_PAGE_READONLY_MASK);
        }
        if ((advise_para->advise & DV_ADVISE_LOCK_DEV) != 0) {
            /* lock device */
            devmm_page_bitmap_set_flag(page_bitmap + i, DEVMM_PAGE_LOCKED_DEVICE_MASK);
            devmm_page_bitmap_set_devid(page_bitmap + i, arg->head.logical_devid);
        }
        devmm_try_cond_resched(&stamp);
    }
}

STATIC INLINE int devmm_advise_check_type(struct devmm_svm_heap *heap, u64 advise)
{
    if ((heap->heap_type != DEVMM_HEAP_HUGE_PAGE) && (heap->heap_type != DEVMM_HEAP_PINNED_HOST)
        && ((advise & DV_ADVISE_HUGEPAGE) != 0)) {
        devmm_drv_err("Heap type is not huge but advise huge page . "
            "(heap_type=%x; advise=0x%llx)\n", heap->heap_type, advise);
        return -EINVAL;
    }
    return 0;
}

static int devmm_ioctl_advise_master(struct devmm_svm_process *svm_pro, struct devmm_svm_heap *heap,
    u32 *page_bitmap, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_advise_para *advise_para = &arg->data.advise_para;
    u64 i, set_num, chunk_page_cnt, page_cnt, byte_count, ptr;
    int ret;
    u32 page_size;
    enum devmm_page_type page_type;

    /* host advise
     * va must alloc fst va
     * size must alloc size
     */
    ptr = advise_para->ptr;
    byte_count = advise_para->count;
    if (((ptr % heap->chunk_page_size) != 0) || !devmm_page_bitmap_is_first_page(page_bitmap)) {
        devmm_drv_err("Va isn't fst. (va=0x%llx; byte_count=%llu)\n", ptr, byte_count);
        return -EINVAL;
    }

    chunk_page_cnt = devmm_get_page_num_from_va(heap, ptr);
    if (chunk_page_cnt != devmm_get_pagecount_by_size(ptr, byte_count, heap->chunk_page_size)) {
        devmm_drv_err("Byte_count error. (va=0x%llx; byte_count=%llu)\n", ptr, byte_count);
        return -EINVAL;
    }

    for (i = 0; i < chunk_page_cnt; i++) {
        if (devmm_page_bitmap_check_and_set_flag(page_bitmap + i,
            DEVMM_PAGE_HOST_MAPPED_MASK | DEVMM_PAGE_LOCKED_HOST_MASK) != 0) {
            devmm_drv_err("Already maped. (already_maped=%llu; va=0x%llx; page_cnt=%llu)\n", i, ptr, chunk_page_cnt);
            ret = -EADDRINUSE;
            goto alloc_fail_handle;
        }
    }

    if ((advise_para->advise & DV_ADVISE_HUGEPAGE) != 0) {
        page_size = DEVMM_HUGE_PAGE_SIZE;
        page_type = DEVMM_HUGE_PAGE_TYPE;
    } else {
        page_size = KA_MM_PAGE_SIZE;
        page_type = DEVMM_NORMAL_PAGE_TYPE;
    }

    page_cnt = devmm_get_pagecount_by_size(ptr, byte_count, page_size);
    ret = devmm_alloc_host_range(svm_pro, ptr, page_cnt, page_type);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_run_info("Can not alloc host mem. (ret=%d; va=0x%llx; page_cnt=%llu)\n", ret, ptr, page_cnt);
#endif
        goto alloc_fail_handle;
    }

    return 0;

alloc_fail_handle:
    set_num = i;
    for (i = 0; i < set_num; i++) {
        devmm_page_bitmap_clear_flag(page_bitmap + i,
            DEVMM_PAGE_HOST_MAPPED_MASK | DEVMM_PAGE_LOCKED_HOST_MASK);
    }

    return ret;
}

STATIC int devmm_advise_cache_persist(struct devmm_svm_process *svm_pro, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_advise_para *advise_para = &arg->data.advise_para;
    struct devmm_chan_advise_cache_persist channel_set_para = {{{0}}};
    int ret;

    channel_set_para.head.msg_id = DEVMM_CHAN_ADVISE_CACHE_PERSIST_H2D_ID;
    channel_set_para.head.process_id.hostpid = svm_pro->process_id.hostpid;
    channel_set_para.head.dev_id = (u16)arg->head.devid;
    channel_set_para.head.process_id.vfid = (u16)arg->head.vfid;
    channel_set_para.va = advise_para->ptr;
    channel_set_para.count = advise_para->count;

    ret = devmm_chan_msg_send(&channel_set_para, sizeof(struct devmm_chan_advise_cache_persist), sizeof(struct devmm_chan_msg_head));
    if (ret != 0) {
        devmm_drv_err("Send cache persist message failed. (ret=%d; va=0x%llx; size=0x%lx; devid=%u; vfid=%u)\n",
            ret, advise_para->ptr, advise_para->count, arg->head.devid, arg->head.vfid);
        return ret;
    }

    return 0;
}

STATIC int devmm_ioctl_advise_agent(struct devmm_svm_process *svm_pro, struct devmm_svm_heap *heap,
    u32 *page_bitmap, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_advise_para *advise_para = &arg->data.advise_para;
    u64 byte_count, page_cnt, ptr;
    int ret;

    byte_count = advise_para->count;
    ptr = advise_para->ptr;
    page_cnt = devmm_get_pagecount_by_size(ptr, byte_count, heap->chunk_page_size);
    ret = devmm_advise_check(heap, svm_pro, page_bitmap, page_cnt, arg);
    if (ret != 0) {
        devmm_drv_err("Bitmap check error. "
            "(devPtr=0x%llx; byte_count=%llu; advise=0x%x; devid=%u; page_bitmap=0x%x; ret=%d)\n",
            ptr, byte_count, advise_para->advise, arg->head.devid, devmm_page_read_bitmap(page_bitmap), ret);
        devmm_print_pre_alloced_va(svm_pro, ptr);
        return ret;
    }

    if ((advise_para->advise & DV_ADVISE_PERSISTENT) != 0) {
        return devmm_advise_cache_persist(svm_pro, arg);
    } else {
        devmm_advise_set_bitmap(arg, page_bitmap, page_cnt);
        if ((advise_para->advise & DV_ADVISE_POPULATE) != 0) {
            return devmm_advise_populate_process(svm_pro, heap, page_bitmap, arg, page_cnt);
        }
    }
    return 0;
}

static int devmm_ioctl_advise_reserve(struct devmm_svm_process *svm_pro, struct devmm_svm_heap *heap,
    u32 *page_bitmap, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_advise_para *advise_para = &arg->data.advise_para;
    u64 byte_count, page_cnt, ptr, i;

    byte_count = advise_para->count;
    ptr = advise_para->ptr;
    page_cnt = devmm_get_pagecount_by_size(ptr, byte_count, heap->chunk_page_size);

    for (i = 0; i < page_cnt; i++) {
        if (devmm_page_bitmap_is_page_available(page_bitmap + i) == false) {
            devmm_drv_err("Advise_populate error, not alloc. (devPtr=0x%llx; count=%lu)\n",
                advise_para->ptr, advise_para->count);
            return -EINVAL;
        }
    }

    if ((advise_para->advise & DV_ADVISE_PERSISTENT) != 0) {
        return devmm_advise_cache_persist(svm_pro, arg);
    } else if ((advise_para->advise & DV_ADVISE_POPULATE) != 0) {
        return -EOPNOTSUPP;
    }

    return 0;
}

#define DEVMM_MAX_TMP_MEM_INFO_LEN      128
#define DEVMM_MAX_DEV_MEM_DFX_LEN       DEVMM_MAX_TMP_MEM_INFO_LEN * DEVMM_MAX_NUMA_NUM_OF_PER_DEV
void devmm_get_dev_mem_dfx(struct devmm_svm_process *svm_pro, u32 mem_type, struct devmm_ioctl_arg *arg)
{
    struct devmm_chan_query_mem_dfx *dfx_msg = NULL;
    u64 len = sizeof(struct devmm_chan_query_mem_dfx);
    char *dfx_msg_print = NULL;
    int ret;
    u32 i;

    dfx_msg = devmm_kvzalloc(len);
    if (dfx_msg == NULL) {
        return;
    }
    dfx_msg_print = devmm_kvzalloc(DEVMM_MAX_DEV_MEM_DFX_LEN);
#ifndef EMU_ST
    if (dfx_msg_print == NULL) {
        devmm_kvfree(dfx_msg);
        return;
    }
#endif

    dfx_msg->head.msg_id = DEVMM_CHAN_MEM_DFX_QUERY_H2D_ID;
    dfx_msg->head.process_id.hostpid = svm_pro->process_id.hostpid;
    dfx_msg->head.dev_id = (u16)arg->head.devid;
    dfx_msg->head.process_id.vfid = (u16)arg->head.vfid;
    dfx_msg->mem_type = mem_type;

    ret = devmm_chan_msg_send(dfx_msg, (u32)len, (u32)len);
    if (ret != 0) {
        devmm_kvfree(dfx_msg);
#ifndef EMU_ST
        devmm_kvfree(dfx_msg_print);
#endif
        return;
    }
    devmm_drv_info("Can not alloc dev mem. (hostpid=%d; devid=%u; vfid=%u; used_page_cnt=%llu; used_hpage_cnt=%llu)\n",
        svm_pro->process_id.hostpid, arg->head.devid, arg->head.vfid, dfx_msg->used_page_cnt, dfx_msg->used_hpage_cnt);

    if (dfx_msg->node_num >= DEVMM_MAX_NUMA_NUM_OF_PER_DEV) {
        devmm_kvfree(dfx_msg);
#ifndef EMU_ST
        devmm_kvfree(dfx_msg_print);
#endif
        return;
    }

    for (i = 0; i < dfx_msg->node_num; i++) {
        char tmp[DEVMM_MAX_TMP_MEM_INFO_LEN] = {0};
        (void)sprintf_s(tmp, DEVMM_MAX_TMP_MEM_INFO_LEN, " [%u: %lluK, %lluK, %lluK, %lluK]", dfx_msg->node_index[i],
            dfx_msg->node_info[i].total_normal_size / BYTES_PER_KB,
            dfx_msg->node_info[i].free_normal_size / BYTES_PER_KB,
            dfx_msg->node_info[i].total_huge_size / BYTES_PER_KB,
            dfx_msg->node_info[i].free_huge_size / BYTES_PER_KB);
        (void)strcat_s(dfx_msg_print, DEVMM_MAX_DEV_MEM_DFX_LEN - 1, tmp);
    }

    devmm_drv_info("Node info. %s\n", dfx_msg_print);
    devmm_kvfree(dfx_msg);
    devmm_kvfree(dfx_msg_print);
}

static void devmm_get_memtype_str(u32 bitmap, char *str)
{
    int ret;

    if ((bitmap & DEVMM_PAGE_ADVISE_P2P_HBM_MASK) != 0) {
        char p2p_hbm[DEVMM_PRINT_STR_MAX] = "p2p_hbm";
        ret = memcpy_s(str, DEVMM_PRINT_STR_MAX, p2p_hbm, ka_base_strlen(p2p_hbm));
    } else if ((bitmap & DEVMM_PAGE_ADVISE_P2P_DDR_MASK) != 0) {
        char p2p_ddr[DEVMM_PRINT_STR_MAX] = "p2p_ddr";
        ret = memcpy_s(str, DEVMM_PRINT_STR_MAX, p2p_ddr, ka_base_strlen(p2p_ddr));
    } else if ((bitmap & DEVMM_PAGE_ADVISE_TS_MASK) != 0) {
        char ts_ddr[DEVMM_PRINT_STR_MAX] = "ts_ddr";
        ret = memcpy_s(str, DEVMM_PRINT_STR_MAX, ts_ddr, ka_base_strlen(ts_ddr));
    } else {
        char normal[DEVMM_PRINT_STR_MAX] = "normal";
        ret = memcpy_s(str, DEVMM_PRINT_STR_MAX, normal, ka_base_strlen(normal));
    }
    if (ret != 0) {
        devmm_drv_warn("Memcpy_s not sucess. (ret=%d)\n", ret);
    }
}

static void devmm_advise_error_print(struct devmm_svm_process *svm_pro, struct devmm_ioctl_arg *arg,
    u32 heap_type, u32 *page_bitmap)
{
    struct devmm_mem_advise_para *advise_para = &arg->data.advise_para;
    u32 bitmap = devmm_page_read_bitmap(page_bitmap);
    u32 alloc_pagesize = advise_para->advise & DV_ADVISE_HUGEPAGE;
    u32 hbm_ddr = bitmap & DEVMM_PAGE_ADVISE_DDR_MASK;
    char p2p_ts[DEVMM_PRINT_STR_MAX] = {0};
    u64 byte_count, ptr;

    byte_count = advise_para->count;
    ptr = advise_para->ptr;
    devmm_get_memtype_str(bitmap, p2p_ts);
    devmm_drv_info("Can not advise. (ptr=0x%llx; byte_count=%llu; advise=0x%x; devid=%u; bitmap=0x%x; "
        "host_dev=%s; pagesize=%s; hbm_ddr=%s; p2p_ts=%s)\n", ptr, byte_count, advise_para->advise,
        arg->head.devid, bitmap, heap_type == DEVMM_HEAP_PINNED_HOST ? "host" : "dev",
        (alloc_pagesize != 0) ? "huge" : "normal", (hbm_ddr != 0) ? "ddr" : "hbm", p2p_ts);
    if (heap_type != DEVMM_HEAP_PINNED_HOST) {
        u32 mem_type = ((bitmap & DEVMM_PAGE_ADVISE_DDR_MASK) != 0) ?
            MEM_INFO_TYPE_DDR_SIZE : MEM_INFO_TYPE_HBM_SIZE;

        devmm_get_dev_mem_dfx(svm_pro, mem_type, arg);
        devmm_dev_mem_stats_log_show(arg->head.logical_devid);
    }
}

int devmm_ioctl_advise(struct devmm_svm_process *svm_pro, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_advise_para *advise_para = &arg->data.advise_para;
    struct devmm_svm_heap *heap = NULL;
    u32 *page_bitmap = NULL;
    u64 byte_count, ptr;
    int ret;

    byte_count = advise_para->count;
    ptr = advise_para->ptr;
    devmm_drv_debug("Advise info. (ptr=0x%llx; count=%llu; advise=0x%x; dev=%u)\n",
        ptr, byte_count, advise_para->advise, arg->head.devid);
    heap = devmm_svm_get_heap(svm_pro, ptr);
    if ((heap == NULL) || (devmm_advise_check_type(heap, advise_para->advise) != 0)) {
        devmm_drv_err("Heap is NULL or error. (heap_is_null=%d; devPtr=0x%llx; device=%u)\n",
            (heap == NULL), ptr, arg->head.devid);
        return -EADDRNOTAVAIL;
    }

    page_bitmap = devmm_get_page_bitmap_with_heap(heap, ptr);
    if ((page_bitmap == NULL) || (devmm_check_va_add_size_by_heap(heap, ptr, byte_count) != 0)) {
        devmm_drv_err("Page_bitmap is NULL. (devPtr=0x%llx; byte_count=%llu)\n", ptr, byte_count);
        return -EINVAL;
    }

    if (heap->heap_sub_type == SUB_RESERVE_TYPE) {
        ret = devmm_ioctl_advise_reserve(svm_pro, heap, page_bitmap, arg);
    } else if (heap->heap_type == DEVMM_HEAP_PINNED_HOST) {
        ret = devmm_ioctl_advise_master(svm_pro, heap, page_bitmap, arg);
    } else {
        ret = devmm_ioctl_advise_agent(svm_pro, heap, page_bitmap, arg);
    }

    if (ret != 0) {
        devmm_advise_error_print(svm_pro, arg, heap->heap_type, page_bitmap);
    }
    return ret;
}

static int devmm_prefetch_para_check(struct devmm_svm_process *svm_proc, u64 dev_ptr, u32 *page_bitmap,
    u64 count, u64 page_cnt)
{
    struct devmm_memory_attributes attr;
    int ret;
    u64 i;

    /* locked host,lock cmd lock all alloc page ,so just judge frist page */
    if (devmm_page_bitmap_is_locked_host(page_bitmap)) {
        devmm_drv_err("Locked host, but attempt to prefetch to device. (devPtr=0x%llx; count=%llu; "
            "page_bitmap=0x%x)\n", dev_ptr, count, devmm_page_read_bitmap(page_bitmap));
        return -EINVAL;
    }

    if (devmm_page_bitmap_is_advise_readonly(page_bitmap)) {
        devmm_drv_err("Readonly mem, but attempt to prefetch to device. (devPtr=0x%llx; count=%llu; "
            "page_bitmap=0x%x)\n", dev_ptr, count, devmm_page_read_bitmap(page_bitmap));
        return -EINVAL;
    }

    for (i = 0; i < page_cnt; i++) {
        if (!devmm_page_bitmap_is_page_available(page_bitmap + i)) {
            devmm_drv_err("Prefetch error, not alloc. (devPtr=0x%llx; i=%llu; count=%llu)\n",
                dev_ptr, i, count);
            return -EINVAL;
        }
    }

    ret = devmm_get_memory_attributes(svm_proc, dev_ptr, &attr);
    if (ret != 0) {
        devmm_drv_err("Get attributes failed. (dev_ptr=0x%llx; ret=%d)\n", dev_ptr, ret);
        return ret;
    }
    if (attr.is_mem_import) {
        devmm_drv_err("Not support import mem.\n");
        return -EINVAL;
    }
    return 0;
}

int devmm_ioctl_prefetch(struct devmm_svm_process *svm_pro, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_advise_para *prefetch_para = &arg->data.prefetch_para;
    struct devmm_svm_heap *heap = NULL;
    u64 count, dev_ptr, page_cnt;
    u32 *page_bitmap = NULL;
    int ret;

    dev_ptr = prefetch_para->ptr;
    count = (u64)prefetch_para->count;

    devmm_drv_debug("Prefetch information. (devPtr=0x%llx; count=%llu; devid=%u)\n", dev_ptr, count, arg->head.devid);
    heap = devmm_svm_get_heap(svm_pro, dev_ptr);
    if (heap == NULL) {
        devmm_drv_err("Heap is NULL. (devPtr=0x%llx; cnt=%llu)\n", dev_ptr, count);
        return -EADDRNOTAVAIL;
    }
    page_bitmap = devmm_get_page_bitmap_with_heap(heap, dev_ptr);
    if ((page_bitmap == NULL) || (devmm_check_va_add_size_by_heap(heap, dev_ptr, count) != 0)) {
        devmm_drv_err("Page_bitmap is NULL or check_va_add_size_by_heap failed. "
                      "(devPtr=0x%llx; count=%llu)\n", dev_ptr, count);
        return -EINVAL;
    }
    page_cnt = devmm_get_pagecount_by_size(dev_ptr, count, heap->chunk_page_size);
    ret = devmm_prefetch_para_check(svm_pro, dev_ptr, page_bitmap, count, page_cnt);
    if (ret != 0) {
        return ret;
    }

    ret = devmm_ioctl_advise_populate(svm_pro, heap, page_bitmap, arg, page_cnt);
    if (ret != 0) {
        devmm_drv_err_if((ret != -EOPNOTSUPP), "Prefetch failed. (ptr=0x%llx; count=%llu; device=%u)\n",
            dev_ptr, count, arg->head.devid);
#ifndef EMU_ST
        if (ret != -EOPNOTSUPP) {
            devmm_print_pre_alloced_va(svm_pro, dev_ptr);
        }
#endif
        return ret;
    }

    return 0;
}

