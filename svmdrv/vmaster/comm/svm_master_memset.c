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

#include "svm_master_memset.h"
#include "devmm_proc_info.h"
#include "svm_cmd.h"
#include "svm_heap_mng.h"
#include "devmm_common.h"
#include "devmm_channel.h"
#include "svm_shmem_interprocess.h"
#include "svm_msg_client.h"

STATIC int devmm_memset_fill_pages_process(struct devmm_svm_process *svm_pro,
    struct devmm_mem_memset_para *memset_para, size_t byte_count,
    struct devmm_memory_attributes *fst_attr)
{
    if (fst_attr->is_svm_non_page) {
        int ret;
        ret = devmm_insert_host_page_range(svm_pro, memset_para->dst, byte_count, fst_attr);
        if (ret != 0) {
            devmm_drv_err("Insert host range failed. (dst=0x%llx; byte_count=%lu; ret=%d)\n",
                          memset_para->dst, byte_count, ret);
            return ret;
        }
    }
    memset_para->hostmapped = 1;
    devmm_drv_debug("Fill host pages succeeded. (dst=0x%llx; byte_count=%lu)\n", memset_para->dst, byte_count);
    return 0;
}

STATIC int devmm_ioctl_memset_device_check(struct devmm_svm_process *svm_process, u64 va, size_t byte_count)
{
    struct devmm_svm_heap *heap = NULL;
    u32 *page_bitmap = NULL;

    heap = devmm_svm_get_heap(svm_process, va);
    if (heap == NULL) {
        devmm_drv_err("Can't find heap. (va=0x%llx; byte_count=%lu)\n", va, byte_count);
        return -EINVAL;
    }

    page_bitmap = devmm_get_page_bitmap_with_heap(heap, va);
    if ((page_bitmap == NULL) ||
        (devmm_check_va_add_size_by_heap(heap, va, byte_count) != 0)) {
        devmm_drv_err("Dev bitmap is NULL. (va=0x%llx; byte_count=%lu)\n", va, byte_count);
        return -EINVAL;
    }

    if (devmm_svm_check_bitmap_available(page_bitmap, byte_count, heap->chunk_page_size) != DEVMM_TRUE) {
        devmm_drv_err("Va isn't alloced. (va=0x%llx; byte_count=%lu)\n", va, byte_count);
        devmm_print_pre_alloced_va(svm_process, va);
        return -EINVAL;
    }

    return 0;
}

static int devmm_share_mem_memset(struct devmm_svm_process *svm_proc,
    struct devmm_mem_memset_para *memset_para, struct devmm_memory_attributes *attr, size_t byte_count)
{
    struct devmm_chan_share_mem_memset chan_memset = {{{0}}};
    struct devmm_svm_heap *heap = NULL;
    struct devmm_vmma_struct *vmma = NULL;
    size_t add_count, per_cnt;
    u64 va = memset_para->dst;
    u64 va_offset;

    heap = devmm_svm_get_heap(svm_proc, va);
    if (heap == NULL) {
        devmm_drv_err("Heap is NULL. (addr=0x%llx)\n", va);
        return -EADDRNOTAVAIL;
    }

    vmma = devmm_vmma_get(&heap->vmma_mng, va);
    if (vmma == NULL) {
        devmm_drv_err("Reserve addr hasn't been mapped. (addr=0x%llx)\n", va);
        return -EADDRNOTAVAIL;
    }
    va_offset = va - vmma->info.va;
    devmm_vmma_put(vmma);

    chan_memset.head.msg_id = DEVMM_CHAN_SHARE_MEM_MEMSET_H2D_ID;
    chan_memset.head.process_id.hostpid = svm_proc->process_id.hostpid;
    chan_memset.head.process_id.vfid = 0;
    chan_memset.head.dev_id = (u16)attr->mem_share_devid;
    chan_memset.value = memset_para->value;
    chan_memset.share_id = attr->mem_share_id;

    per_cnt = DEVMM_MEMSET8D_SIZE_PER_MSG;
    for (add_count = 0; add_count < byte_count; add_count += per_cnt) {
        int ret;
        chan_memset.va_offset = va_offset + add_count;
        chan_memset.count = (u32)ka_base_min(byte_count - add_count, per_cnt);
        ret = devmm_chan_msg_send(&chan_memset, sizeof(struct devmm_chan_share_mem_memset), sizeof(struct devmm_chan_msg_head));
        if (ret != 0) {
            devmm_drv_err("Device memset error. (ret=%d; va=0x%llx; value=0x%llx; count=%llu)\n", ret, va,
                          memset_para->value, memset_para->count);
            return ret;
        }
    }
    return 0;
}

STATIC int devmm_send_memset_device_msg(struct devmm_svm_process *svm_process,
    struct devmm_mem_memset_para *memset_para, struct devmm_memory_attributes *attr, size_t byte_count)
{
    struct devmm_chan_memset chan_memset = {{{0}}};
    struct devmm_svm_process_id proc_id;
    size_t add_count, per_cnt;
    u64 va;

    chan_memset.head.msg_id = DEVMM_CHAN_MEMSET8_H2D_ID;
    chan_memset.value = memset_para->value;
    if (attr->is_ipc_open) {
        struct devmm_svm_process *owner_proc = NULL;
        owner_proc = devmm_ipc_query_owner_info(svm_process, memset_para->dst, &va, &proc_id, NULL);
        if (owner_proc == NULL) {
            devmm_drv_err("Va query ipc owner info failed. (dst=%llx)\n", memset_para->dst);
            return -EFAULT;
        }
        chan_memset.head.process_id.hostpid = owner_proc->process_id.hostpid;
        chan_memset.head.process_id.vfid = proc_id.vfid;
        chan_memset.head.dev_id = proc_id.devid;
        devmm_drv_debug("Memset va created by ipc. (attr_devid=%d; dst=0x%llx; va=0x%llx; hostpid=%d; "
            "devid=%d)\n", attr->devid, memset_para->dst, va, owner_proc->process_id.hostpid, proc_id.devid);
    } else {
        chan_memset.head.process_id.hostpid = svm_process->process_id.hostpid;
        chan_memset.head.process_id.vfid = (u16)attr->vfid;
        chan_memset.head.dev_id = (u16)attr->devid;
        va = memset_para->dst;
    }

    per_cnt = DEVMM_MEMSET8D_SIZE_PER_MSG;
    for (add_count = 0; add_count < byte_count; add_count += per_cnt) {
        int ret;
        chan_memset.dst = va + add_count;
        chan_memset.count = (u32)ka_base_min(byte_count - add_count, per_cnt);
        ret = devmm_chan_msg_send(&chan_memset, sizeof(struct devmm_chan_memset), sizeof(struct devmm_chan_msg_head));
        if (ret != 0) {
            devmm_drv_err("Device memset error. (ret=%d; va=0x%llx; value=0x%llx; count=%llu)\n", ret, va,
                          memset_para->value, memset_para->count);
            return ret;
        }
    }

    return 0;
}

STATIC int devmm_ioctl_memset_device_process(struct devmm_svm_process *svm_process,
    struct devmm_mem_memset_para *memset_para, struct devmm_memory_attributes *attr, size_t byte_count)
{
    int ret;

    if (attr->is_svm_non_page) {
        devmm_drv_err("Address isn't mapped by device. (dst=0x%llx; byte_count=%lu)\n", memset_para->dst, byte_count);
        return -EINVAL;
    }

    if (attr->is_svm_readonly) {
        devmm_drv_err("Address is readonly. (dst=0x%llx; byte_count=%lu)\n", memset_para->dst, byte_count);
        return -EINVAL;
    }

    if (devmm_ioctl_memset_device_check(svm_process, memset_para->dst, byte_count) != 0) {
        devmm_drv_err("Va or count is error. (dst=0x%llx; count=%llu)\n", memset_para->dst, memset_para->count);
        return -EINVAL;
    }

    if (attr->is_mem_import) {
        ret = devmm_share_mem_memset(svm_process, memset_para, attr, byte_count);
    } else {
        ret = devmm_send_memset_device_msg(svm_process, memset_para, attr, byte_count);
    }
    if (ret != 0) {
        devmm_drv_err("Send memset device message failed. (dst=0x%llx; byte_count=%lu)\n",
                      memset_para->dst, byte_count);
        return ret;
    }
    devmm_drv_debug("Memset device succeeded. (dst=0x%llx; byte_count=%lu)\n", memset_para->dst, byte_count);

    return 0;
}

STATIC int devmm_ioctl_memset_process(struct devmm_svm_process *svm_pro, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_memset_para *memset_para = NULL;
    struct devmm_memory_attributes attr;
    size_t byte_count;
    int ret;

    memset_para = &arg->data.memset_para;
    devmm_drv_debug("Memset parameter. (dst=0x%llx; value=0x%llx; count=%llu)\n", memset_para->dst, memset_para->value,
        memset_para->count);

    ret = devmm_get_memory_attributes(svm_pro, memset_para->dst, &attr);
    if (ret != 0) {
        devmm_drv_err("Devmm_get_memory_attributes failed. (dst=0x%llx)\n", memset_para->dst);
        return ret;
    }
    if (memset_para->count > attr.heap_size) {
        devmm_drv_err("Count is bigger then heap_size. (count=%llu; heap_size=%llu; memset_para_dst=0x%llx)\n",
            memset_para->count, attr.heap_size, memset_para->dst);
        return -EINVAL;
    }
    byte_count = memset_para->count;

    if ((attr.is_svm_non_page && !attr.is_locked_device) || attr.is_svm_host) {
        return devmm_memset_fill_pages_process(svm_pro, memset_para, byte_count, &attr);
    } else if ((attr.is_svm_non_page && attr.is_locked_device) || attr.is_svm_device) {
        return devmm_ioctl_memset_device_process(svm_pro, memset_para, &attr, byte_count);
    } else {
        devmm_drv_err("Failed. "
            "(is_local_host=%d; is_host_pin=%d; is_svm=%d; devid=%u; dst=0x%llx; value=0x%llx; count=%llu)\n",
            attr.is_local_host, attr.is_host_pin, attr.is_svm, attr.devid, memset_para->dst,
            memset_para->value, memset_para->count);
        return -EINVAL;
    }
}

int devmm_ioctl_memset8(struct devmm_svm_process *svm_pro, struct devmm_ioctl_arg *arg)
{
    return devmm_ioctl_memset_process(svm_pro, arg);
}

