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
 * Create: 2024-5-30
 */
#include <linux/types.h>

#include "devmm_proc_info.h"
#include "devmm_channel.h"
#include "svm_msg_client.h"
#include "devmm_page_cache.h"
#include "svm_shmem_node.h"
#include "svm_master_dev_capability.h"
#include "svm_master_mem_repair.h"
#include "svm_define.h"

static int devmm_mem_repair_para_check(struct devmm_mem_repair_para *para)
{
    u32 i;

    if ((para->count == 0) || (para->count > MEM_REPAIR_MAX_CNT)) {
        devmm_drv_err("Count is invalid. (count=%u)\n", para->count);
        return -EINVAL;
    }

    for (i = 0; i < para->count; i++) {
        if ((para->repair_addrs[i].ptr == 0) || ((para->repair_addrs[i].len != devmm_svm->device_page_size) &&
            (para->repair_addrs[i].len != devmm_svm->device_hpage_size) &&
            (para->repair_addrs[i].len != DEVMM_GIANT_PAGE_SIZE))) {
            devmm_drv_err("Repair addr info is invalid. (i=%u; cnt=%u; ptr=0x%llx; len=0x%llx)\n",
                i, para->count, para->repair_addrs[i].ptr, para->repair_addrs[i].len);
            return -EINVAL;
        }
    }

    return 0;
}

static void devmm_giant_page_mem_set_pages_cache(struct devmm_svm_process *svm_proc, u32 logical_devid,
    struct devmm_pages_cache_info *cache_info, struct devmm_chan_mem_repair *mem_repair)
{
    struct devmm_chan_query_phy_blk *blk = NULL;
    u64 blks_len, i;

    blks_len = sizeof(struct devmm_chan_query_phy_blk) * DEVMM_GIANT_TO_HUGE_PAGE_NUM;
    blk = devmm_kvzalloc_ex(blks_len, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (blk == NULL) {
        devmm_drv_err("Kvzalloc failed. (blks_len=%llu)\n", blks_len);
        return;
    }
    blk[0].dma_addr = mem_repair->blk.dma_addr;
    blk[0].phy_addr = mem_repair->blk.phy_addr;
    for (i = 1; i < DEVMM_GIANT_TO_HUGE_PAGE_NUM; i++) {
        blk[i].dma_addr = blk[0].dma_addr + i * devmm_svm->device_hpage_size;
        blk[i].phy_addr = blk[0].phy_addr + i * devmm_svm->device_hpage_size;
    }
    cache_info->pg_num = DEVMM_GIANT_TO_HUGE_PAGE_NUM;
    cache_info->blks = blk;
    devmm_pages_cache_set(svm_proc, logical_devid, cache_info);
    devmm_kvfree_ex(blk);
}

static void devmm_mem_repair_pages_cache_update(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg,
    struct devmm_chan_mem_repair *mem_repair)
{
    struct devmm_pages_cache_info cache_info = {0};
    struct devmm_svm_heap *heap = NULL;
    u32 page_size;

    heap = devmm_svm_get_heap(svm_proc, mem_repair->addr);
    if (heap == NULL) {
        devmm_drv_warn("Heap is null, no update page cache. (devid=%u; vifd=%u; repair_addr=0x%llx; len=%llx)\n",
            arg->head.devid, arg->head.vfid, mem_repair->addr, mem_repair->len);
        return;
    }

    page_size = (heap->heap_type == DEVMM_HEAP_HUGE_PAGE) ? devmm_svm->device_hpage_size : devmm_svm->device_page_size;
    cache_info.va = ka_base_round_down(mem_repair->addr, page_size);
    cache_info.pg_size = page_size;
    if (mem_repair->is_giant_page) {
        devmm_giant_page_mem_set_pages_cache(svm_proc, arg->head.logical_devid, &cache_info, mem_repair);
    } else {
        cache_info.pg_num = 1;
        cache_info.blks = &mem_repair->blk;
        devmm_pages_cache_set(svm_proc, arg->head.logical_devid, &cache_info);
    }
}

static int devmm_agent_mem_repair_post_process(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg,
    struct devmm_chan_mem_repair *mem_repair)
{
    u32 *bitmap = NULL;

    devmm_mem_repair_pages_cache_update(svm_proc, arg, mem_repair);
    devmm_drv_debug("Update mem cache.\n");

    bitmap = devmm_get_alloced_va_fst_page_bitmap(svm_proc, mem_repair->addr);
    if (bitmap == NULL) {
        devmm_drv_err("Find first page bitmap failed. (bitmap=%u; addr=0x%llx; len=0x%llx)\n",
            mem_repair->bitmap, mem_repair->addr, mem_repair->len);
        return -EINVAL;
    }
    if (devmm_page_bitmap_is_ipc_create_mem(bitmap)) {
        return devmm_ipc_create_mem_repair_post_process(svm_proc, mem_repair->addr, mem_repair->len);
    }
    return 0;
}

static int devmm_agent_mem_repair(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg,
    u64 addr, u64 len, bool svm_range)
{
    struct devmm_chan_mem_repair mem_repair;
    u32 *bitmap = NULL;
    int ret;

    if (svm_range) {
        bitmap = devmm_get_page_bitmap(svm_proc, addr);
        if (bitmap == NULL) {
            return -EINVAL;
        }
    }

    mem_repair.head.msg_id = svm_range ? DEVMM_CHAN_SVM_MEM_REPAIR_H2D_ID : DEVMM_CHAN_NO_SVM_MEM_REPAIR_H2D_ID;
    mem_repair.head.process_id.hostpid = svm_proc->process_id.hostpid;
    mem_repair.head.process_id.vfid = (u16)arg->head.vfid;
    mem_repair.head.dev_id = (u16)arg->head.devid;
    mem_repair.addr = addr;
    mem_repair.len = len;
    mem_repair.bitmap = ((bitmap != NULL) ? *bitmap : 0);
    mem_repair.need_cache_update = false;
    mem_repair.is_giant_page = false;

    ret = devmm_chan_msg_send(&mem_repair, sizeof(struct devmm_chan_mem_repair), sizeof(struct devmm_chan_mem_repair));
    if (ret != 0) {
        devmm_drv_err("Mem repair h2d failed. (devid=%u; vifd=%u; repair_addr=0x%llx; len=%llx; svm_range=%d)\n",
            arg->head.devid, arg->head.vfid, addr, len, svm_range);
        return ret;
    }
    devmm_drv_debug("Mem repair succ. (devid=%u; vifd=%u; addr=0x%llx; len=%llx; svm_range=%d; cache_update=%d; "
        "is_giant_page=%d)\n", arg->head.devid, arg->head.vfid, addr, len, svm_range, mem_repair.need_cache_update,
        mem_repair.is_giant_page);

    if (svm_range && mem_repair.need_cache_update) {
        return devmm_agent_mem_repair_post_process(svm_proc, arg, &mem_repair);
    }

    return ret;
}

static bool devmm_svm_range_repair_heap_and_bitmap_chk(struct devmm_svm_heap *heap, struct devmm_ioctl_arg *arg,
    u64 addr, u64 len, u64 *need_repair)
{
    bool support_shmem_repair = devmm_dev_capability_support_shmem_repair(arg->head.devid);
    u32 *fst_bitmap = NULL;
    u32 *bitmap = NULL;

    bitmap = devmm_get_page_bitmap_with_heap(heap, addr);
    if (bitmap == NULL) {
        devmm_drv_err("Get bitmap error. (devid=%u; vfid=%u; repair_addr=0x%llx; len=%llx)\n",
            arg->head.devid, arg->head.vfid, addr, len);
        return false;
    }
    if ((heap->heap_sub_type == SUB_HOST_TYPE) ||
        (!devmm_page_bitmap_is_ipc_open_mem(bitmap) && (heap->heap_sub_type == SUB_SVM_TYPE))) {
        devmm_drv_err("No support repair heap_sub_type. (devid=%u; vfid=%u; heap_sub_type=%u)\n",
            arg->head.devid, arg->head.vfid, heap->heap_sub_type);
        return false;
    }

    if (((heap->heap_type == DEVMM_HEAP_CHUNK_PAGE) && devmm_page_bitmap_is_advise_continuty(bitmap)) ||
        devmm_page_bitmap_advise_memory_shared(bitmap) || devmm_page_bitmap_is_remote_mapped(bitmap)) {
        devmm_drv_err("No support repair type. (devid=%u; vfid=%u; addr=%llx; len=0x%llx; bitmap=%u; heap_type=%u)\n",
            arg->head.devid, arg->head.vfid, addr, len, *bitmap, heap->heap_type);
        return false;
    }

    if (support_shmem_repair) {
        if (devmm_page_bitmap_is_ipc_open_mem(bitmap)) {
            *need_repair = false;
        }
    } else {
        fst_bitmap = devmm_get_alloced_va_fst_page_bitmap_with_heap(heap, addr);
        if ((fst_bitmap == NULL) || devmm_page_bitmap_is_ipc_create_mem(fst_bitmap) ||
            devmm_page_bitmap_is_ipc_open_mem(bitmap)) {
            devmm_drv_err("No support repair ipc shmem. (dev=%u; addr=%llx; len=%llu; bitmap=%u; fst_bitmap=%u)\n",
                arg->head.devid, addr, len, *bitmap, ((fst_bitmap == NULL) ? 0 : *fst_bitmap));
            return false;
        }
    }

    return true;
}

static bool devmm_svm_range_repair_attr_chk(struct devmm_svm_process *svm_proc, u64 addr)
{
    struct devmm_memory_attributes attr;
    int ret;

    ret = devmm_get_svm_mem_attrs(svm_proc, addr, &attr);
    if (ret != 0) {
        devmm_drv_err("Get mem_attr failed. (addr=0x%llx)\n", addr);
        return false;
    }

    if ((attr.is_mem_export == true) || (attr.is_mem_import == true)) {
        devmm_drv_err("No support repair type. (addr=0x%llx; is_mem_export=%d; is_mem_import=%d)\n",
            addr, attr.is_mem_export, attr.is_mem_import);
        return false;
    }

    return true;
}

static bool devmm_svm_range_addr_is_support_repair(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg,
    u64 addr, u64 len, u64 *need_repair)
{
    struct devmm_svm_heap *heap = NULL;

    heap = devmm_svm_get_heap(svm_proc, addr);
    if (heap == NULL) {
        devmm_drv_err("Heap is null. (devid=%u; vifd=%u; repair_addr=%llx; len=%llx)\n",
            arg->head.devid, arg->head.vfid, addr, len);
        return false;
    }

    if (devmm_svm_range_repair_heap_and_bitmap_chk(heap, arg, addr, len, need_repair) == false) {
        return false;
    }

    if (devmm_svm_range_repair_attr_chk(svm_proc, addr) == false) {
        return false;
    }

    return true;
}

static int devmm_repair_svm_range_addr(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg,
    u64 addr, u64 len)
{
    u64 need_repair = true;

    if (devmm_svm_range_addr_is_support_repair(svm_proc, arg, addr, len, &need_repair) == false) {
        return -EPERM;
    }
    if (need_repair == false) {
        return 0;
    }

    return devmm_agent_mem_repair(svm_proc, arg, addr, len, true);
}

static int devmm_repair_no_svm_range_addr(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg,
    u64 addr, u64 len)
{
    return devmm_agent_mem_repair(svm_proc, arg, addr, len, false);
}

int devmm_ioctl_mem_repair(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_repair_para *para = &arg->data.mem_repair_para;
    int ret;
    u32 i;

    ret = devmm_mem_repair_para_check(para);
    if (ret != 0) {
        return ret;
    }

    for (i = 0; i < para->count; i++) {
        if (devmm_va_is_in_svm_range(para->repair_addrs[i].ptr)) {
            ret = devmm_repair_svm_range_addr(svm_proc, arg, para->repair_addrs[i].ptr, para->repair_addrs[i].len);
        } else {
            ret = devmm_repair_no_svm_range_addr(svm_proc, arg, para->repair_addrs[i].ptr, para->repair_addrs[i].len);
        }

        if (ret != 0) {
            devmm_drv_err("Repair addr failed. (i=%u; count=%u; ptr=0x%llx; len=0x%llx; ret=%d)\n",
                i, para->count, para->repair_addrs[i].ptr, para->repair_addrs[i].len, ret);
            return ret;
        }
    }

    return 0;
}

