/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
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
 * Create: 2023-08-21
 */
#include "devdrv_manager_comm.h"

#include "devmm_channel.h"
#include "devmm_common.h"
#include "svm_heap_mng.h"
#include "devmm_page_cache.h"
#include "svm_master_remote_map.h"
#include "svm_msg_client.h"
#include "svm_phy_addr_blk_mng.h"
#include "svm_vmma_mng.h"
#include "svm_master_query.h"
#include "svm_master_mem_map.h"
#include "svm_master_mem_create.h"
#include "svm_master_mem_share.h"

static bool devmm_is_d2h_access(u32 owner_udevid, u32 access_udevid)
{
    return ((owner_udevid == uda_get_host_id()) && (access_udevid != uda_get_host_id()));
}

static bool devmm_is_h2d_access(u32 owner_udevid, u32 access_udevid)
{
    return ((owner_udevid != uda_get_host_id()) && (access_udevid == uda_get_host_id()));
}

static u64 *devmm_h2d_access_mmap_src_pa_list_create(struct devmm_svm_process *svm_proc, struct devmm_devid *devids,
    struct devmm_mem_remote_map_para *map_para, struct devmm_memory_attributes *attr)
{
    u32 stamp = (u32)ka_jiffies;
    u64 page_num, tmp_va;
    u64 *palist = NULL;
    int ret;
    u64 i;

    page_num = map_para->size / attr->page_size;
    if (page_num == 0) {
        return NULL;
    }

    palist = devmm_kvzalloc_ex(sizeof(u64) * page_num, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (palist == NULL) {
        devmm_drv_err("Kvzalloc fail. (page_num=%llu)\n", page_num);
        return NULL;
    }

    for (i = 0, tmp_va = map_para->src_va; i < page_num; i++, tmp_va += attr->page_size) {
        ret = devmm_find_pa_cache(svm_proc, devids->logical_devid, tmp_va, attr->page_size, &palist[i]);
        if (ret != 0) {
            devmm_drv_err("Find agent_pa fail. (agent_va=0x%llx; page_size=%u; logical_devid=%u; ret=%d; i=%llu)\n",
                tmp_va, attr->page_size, devids->logical_devid, ret, i);
            devmm_kvfree_ex(palist);
            return NULL;
        }
        devmm_try_cond_resched(&stamp);
    }

    return palist;
}

static void devmm_h2d_access_mmap_src_pa_info_destroy(u64 *palist)
{
    if (palist != NULL) {
        devmm_kvfree_ex(palist);
        palist = NULL;
    }
}

static bool devmm_h2d_access_mmap_check_pa_is_cont(u64 *palist, struct devmm_mem_remote_map_para *map_para,
    struct devmm_memory_attributes *attr, struct devmm_vmma_struct *vmma)
{
    u64 page_num = map_para->size / attr->page_size;
    bool need_check = true;
    bool is_cont = false;

#ifndef EMU_ST
    need_check = (devmm_svm->device_page_size != devmm_svm->host_page_size) &&
        (vmma->info.pg_type == DEVMM_NORMAL_PAGE_TYPE);
#else
    need_check = (vmma->info.pg_type == DEVMM_NORMAL_PAGE_TYPE);
#endif
    if (need_check == false) {
        return true;
    }
    is_cont = devmm_palist_is_specify_continuous(palist, devmm_svm->device_page_size, page_num,
        devmm_device_page_adjust_num());
    devmm_drv_debug("Is continuous check. (agent_va=0x%llx; size==0x%llx; page_size=%u; page_num=%llu;is_cont=%d)\n",
        map_para->src_va, map_para->size, attr->page_size, page_num, is_cont);

    return is_cont;
}

static int devmm_access_remap_addrs(struct devmm_svm_process *svm_proc, struct devmm_devid *devids,
    u64 start_va, u64 *remap_palist, u64 remap_num, u64 per_remap_size)
{
    ka_pgprot_t page_prot = devmm_make_remote_pgprot(0);
    ka_vm_area_struct_t *vma = NULL;
    u32 stamp = (u32)ka_jiffies;
    u64 i, j, tmp_va, tmp_pa;
    int ret;

    vma = devmm_find_vma(svm_proc, start_va);
    if (vma == NULL) {
        devmm_drv_err("Can't find vma. (svm_addr=0x%llx)\n", start_va);
        return -EINVAL;
    }

    for (i = 0, tmp_va = start_va; i < remap_num; i++, tmp_va += per_remap_size) {
        ret = devmm_remote_pa_to_bar_pa(devids->devid, &remap_palist[i], 1, &tmp_pa);
        if (ret != 0) {
            devmm_drv_err("Remote_pa to bar_pa fail. (va=0x%llx; map_size=%llu; devid=%u; ret=%d)\n",
                tmp_va, per_remap_size, devids->devid, ret);
            goto clear_pfn_range;
        }

        ret = remap_pfn_range(vma, tmp_va, KA_MM_PFN_DOWN(tmp_pa), per_remap_size, page_prot);
        if (ret != 0) {
            devmm_drv_err("Remap_pfn_range fail. (i=%llu; va=0x%llx; per_remap_size=%u; ret=%d)\n",
                i, tmp_va, per_remap_size, ret);
            goto clear_pfn_range;
        }
        devmm_try_cond_resched(&stamp);
    }

    return 0;
clear_pfn_range:
#ifndef EMU_ST
    for (j = 0, tmp_va = start_va; j < i; j++, tmp_va += per_remap_size) {
        devmm_zap_vma_ptes(vma, tmp_va, per_remap_size);
        devmm_try_cond_resched(&stamp);
    }
#endif
    return ret;
}

static int _devmm_h2d_access_mmap(struct devmm_svm_process *svm_proc, struct devmm_devid *devids,
    struct devmm_mem_remote_map_para *map_para, struct devmm_memory_attributes *attr, struct devmm_vmma_struct *vmma)
{
    u64 *src_palist = NULL, *dst_palist = NULL;
    u64 src_page_num, dst_page_num, adjust_num;
    int ret;

    src_page_num = map_para->size / attr->page_size;
    map_para->dst_va = map_para->src_va;
    adjust_num = devmm_device_page_adjust_num();

    src_palist = devmm_h2d_access_mmap_src_pa_list_create(svm_proc, devids, map_para, attr);
    if (src_palist == NULL) {
        devmm_drv_err("Src palist create failed.\n");
        return -ENOMEM;
    }

    if (attr->page_size != devmm_svm->device_page_size) {
        ret = devmm_access_remap_addrs(svm_proc, devids, map_para->src_va, src_palist, src_page_num, attr->page_size);
        devmm_h2d_access_mmap_src_pa_info_destroy(src_palist);
    } else {
        dst_page_num = map_para->size / KA_MM_PAGE_SIZE;
        adjust_num = devmm_device_page_adjust_num();

        if (!devmm_h2d_access_mmap_check_pa_is_cont(src_palist, map_para, attr, vmma)) {
            devmm_h2d_access_mmap_src_pa_info_destroy(src_palist);
            devmm_drv_err("Pa need check, but is not continue.\n");
            return -EFAULT;
        }

        dst_palist = devmm_mem_map_adjust_pa_create(dst_page_num, KA_MM_PAGE_SIZE, src_palist, src_page_num, adjust_num);
        devmm_h2d_access_mmap_src_pa_info_destroy(src_palist);
        if (dst_palist == NULL) {
            devmm_drv_err("Dst palist create failed.\n");
            return -ENOMEM;
        }

        ret = devmm_access_remap_addrs(svm_proc, devids, map_para->src_va, dst_palist, dst_page_num, KA_MM_PAGE_SIZE);
        devmm_mem_map_adjust_pa_destroy(dst_palist);
    }

    return ret;
}

static int devmm_h2d_access_mmap(struct devmm_svm_process *svm_proc, struct devmm_vmma_struct *vmma,
    struct devmm_mem_set_access_para *para, u32 udevid)
{
    struct devmm_mem_remote_map_para map_para = {0};
    struct devmm_memory_attributes attr = {0};
    struct devmm_devid devids = {0};
    int ret;

    if (KA_DRIVER_IS_ALIGNED(para->va, KA_MM_PAGE_SIZE) == false) {
        devmm_drv_err("Va should be aligned by pg_size. (para->va=0x%llx)\n", para->va);
        return -EINVAL;
    }

    if (KA_DRIVER_IS_ALIGNED(para->size, KA_MM_PAGE_SIZE) == false) {
        devmm_drv_err("Size should be aligned by pg_size. (size=0x%llx)\n", para->size);
        return -EINVAL;
    }

    ret = devmm_get_memory_attributes(svm_proc, para->va, &attr);
    if (ret != 0) {
        devmm_drv_err("Get attributes fail. (src_va=0x%llx; ret=%d)\n", para->va, ret);
        return ret;
    }

    devmm_update_devids(&devids, attr.logical_devid, attr.devid, attr.vfid);

    map_para.src_va = para->va;
    map_para.dst_va = para->va;
    map_para.size = para->size;
    map_para.map_type = DEV_SVM_MAP_HOST;
    map_para.proc_type = DEVDRV_PROCESS_CP1;

    ret = _devmm_h2d_access_mmap(svm_proc, &devids, &map_para, &attr, vmma);
    if (ret != 0) {
        devmm_drv_err("Map fail. (src_va=0x%llx; ret=%d)\n", para->va, ret);
        return ret;
    }
    devmm_drv_debug("Map succ. (src_va=0x%llx)\n", para->va);
    return 0;
}

static int devmm_d2h_access_mmap(struct devmm_svm_process *svm_proc, struct devmm_vmma_struct *vmma,
    struct devmm_mem_set_access_para *para, u32 udevid)
{
    struct devmm_memory_attributes attr;
    struct devmm_mem_remote_map_para map_para = {0};
    struct devmm_devid devids = {0};
    int ret;

    if (!devmm_is_hccs_connect(udevid)) {
        devmm_drv_run_info("Only support hccs connect.\n");
#ifndef EMU_ST
        return -EOPNOTSUPP;
#endif
    }

    ret = devmm_get_memory_attributes(svm_proc, para->va, &attr);
    if (ret != 0) {
        devmm_drv_err("Get attributes fail. (src_va=0x%llx; ret=%d)\n", para->va, ret);
        return ret;
    }

    devids.devid = udevid;
    devids.logical_devid = para->logic_devid;
    devids.vfid = 0;

    map_para.src_va = para->va;
    map_para.dst_va = para->va;
    map_para.size = para->size;
    map_para.map_type = HOST_SVM_MAP_DEV;
    map_para.proc_type = DEVDRV_PROCESS_CP1;

    ret = devmm_map_svm_mem_to_agent(svm_proc, &devids, &map_para, &attr);
    if (ret != 0) {
        devmm_drv_err("Map fail. (src_va=0x%llx; ret=%d)\n", para->va, ret);
        return ret;
    }

    return 0;
}

static int devmm_access_mmap(struct devmm_svm_process *svm_proc, struct devmm_vmma_struct *vmma,
    struct devmm_mem_set_access_para *para, u32 udevid)
{
    if (devmm_is_d2h_access(vmma->info.devid, udevid)) {
        return devmm_d2h_access_mmap(svm_proc, vmma, para, udevid);
    } else if (devmm_is_h2d_access(vmma->info.devid, udevid)) {
        return devmm_h2d_access_mmap(svm_proc, vmma, para, udevid);
    } else {
        return 0;
    }
}

static void devmm_h2d_access_munmap(struct devmm_svm_process *svm_proc, struct devmm_vmma_struct *vmma, u32 udevid)
{
    if (udevid == uda_get_host_id()) {
        devmm_unmap_mem(svm_proc, vmma->info.va, vmma->info.size);
    }
}

static void devmm_d2h_access_munmap(struct devmm_svm_process *svm_proc, struct devmm_vmma_struct *vmma, u32 udevid)
{
    struct devmm_devid devids = {0};
    int ret;

    if (vmma->info.devid != uda_get_host_id()) {
        return;
    }

    devids.devid = udevid;
    devids.vfid = 0;

    ret = devmm_locked_host_unmap(svm_proc, &devids, vmma->info.va, vmma->info.size, vmma->info.va);
    if (ret != 0) {
        devmm_drv_err("Unmap fail. (src_va=0x%llx; ret=%d)\n", vmma->info.va, ret);
    }
}

static void devmm_access_munmap(struct devmm_svm_process *svm_proc, struct devmm_vmma_struct *vmma, u32 udevid)
{
    if (devmm_is_d2h_access(vmma->info.devid, udevid)) {
        devmm_d2h_access_munmap(svm_proc, vmma, udevid);
    } else if (devmm_is_h2d_access(vmma->info.devid, udevid)) {
        devmm_h2d_access_munmap(svm_proc, vmma, udevid);
    }
}

void devmm_access_munmap_all(struct devmm_svm_process *svm_proc, struct devmm_vmma_struct *vmma)
{
    u32 udevid, devid;

    for (devid = 0; devid < DEVMM_MAX_ACCESS_DEVICE_NUM; devid++) {
        if (vmma->info.device_access_type[devid] == MEM_ACCESS_TYPE_NONE) {
            continue;
        }

        if (uda_devid_to_udevid_ex(devid, &udevid) != 0) {
            devmm_drv_err("Get udevid failed. (loggic_devid=%u)\n", devid);
            continue;
        }

        devmm_access_munmap(svm_proc, vmma, udevid);
    }
}

/* Cannot be mapped to multiple locations. */
static int devmm_page_bitmap_mem_mapped_state_set(u32 *page_bitmap, u32 logic_devid, bool *is_first_map)
{
    devmm_page_bitmap_lock(page_bitmap);
    if (devmm_page_bitmap_is_mem_mapped(page_bitmap) == false) {
        devmm_page_bitmap_set_flag_without_lock(page_bitmap, DEVMM_PAGE_MEM_MAPPED_MASK);
        devmm_page_bitmap_set_value_nolock(page_bitmap, DEVMM_PAGE_DEVID_SHIT, DEVMM_PAGE_DEVID_WID, logic_devid);
        devmm_page_bitmap_unlock(page_bitmap);
        *is_first_map = true;
        return 0;
    }

    if (logic_devid != devmm_page_bitmap_get_devid(page_bitmap)) {
        devmm_drv_err("Could only mem mapped by one side. (devid=%u)\n", devmm_page_bitmap_get_devid(page_bitmap));
        devmm_page_bitmap_unlock(page_bitmap);
        return -EINVAL;
    }

    devmm_page_bitmap_unlock(page_bitmap);
    *is_first_map = false;
    return 0;
}

static int devmm_mem_map_pg_bitmap_state_set(struct devmm_svm_heap *heap,
    struct devmm_mem_map_para *para, u32 logic_devid, bool is_da_addr, bool *is_first_map)
{
    u32 *first_page_bitmap = NULL;
    u32 *page_bitmap = NULL;
    u64 pg_cnt = para->size / heap->chunk_page_size;
    u64 i, j;
    u32 flag, tmp_devid;
    int ret;

    flag = (para->side == MEM_HOST_SIDE) ?
        (DEVMM_PAGE_HOST_MAPPED_MASK | DEVMM_PAGE_LOCKED_HOST_MASK) :
        (DEVMM_PAGE_DEV_MAPPED_MASK | DEVMM_PAGE_LOCKED_DEVICE_MASK);

    tmp_devid = (para->side == MEM_HOST_SIDE) ? uda_get_host_id() : logic_devid;

    if (!is_da_addr) {
        first_page_bitmap = devmm_get_alloced_va_fst_page_bitmap_with_heap(heap, para->va);
        if (ka_unlikely(first_page_bitmap == NULL)) {
            devmm_drv_err("Unexpected, get first pg_bitmap failed. (va=0x%llx)\n", para->va);
            return -EINVAL;
        }
    }

    page_bitmap = devmm_get_page_bitmap_with_heap(heap, para->va);
    if (ka_unlikely(page_bitmap == NULL)) {
        devmm_drv_err("Unexpected, get pg_bitmap failed. (va=0x%llx)\n", para->va);
        return -EINVAL;
    }

    *is_first_map = false;
    if (!is_da_addr) {
        ret = devmm_page_bitmap_mem_mapped_state_set(first_page_bitmap, tmp_devid, is_first_map);
        if (ret != 0) {
            return ret;
        }
    }

    for (i = 0; i < pg_cnt; i++) {
        ret = devmm_page_bitmap_check_and_set_flag(page_bitmap + i, flag | DEVMM_PAGE_ADVISE_POPULATE_MASK);
        if (ret != 0) {
            devmm_drv_err("Already maped. (already_maped=%llu; va=0x%llx; page_cnt=%llu)\n",
                i, para->va, pg_cnt);
            ret = -EADDRINUSE;
            goto clear_bitmap;
        }
        devmm_page_bitmap_set_devid(page_bitmap + i, tmp_devid);
    }

    return 0;
clear_bitmap:
    for (j = 0; j < i; j++) {
        devmm_page_bitmap_clear_flag(page_bitmap + j, flag);
    }
    if (!is_da_addr) {
        if (*is_first_map) {
            devmm_page_bitmap_clear_flag(first_page_bitmap, DEVMM_PAGE_MEM_MAPPED_MASK);
        }
    }
    return ret;
}

static int devmm_mem_map_pg_bitmap_state_clear(struct devmm_svm_heap *heap,
    u64 va, u64 size, u32 side, bool is_da_addr, bool clear_first_bimap_state)
{
    u32 *first_page_bitmap = NULL;
    u32 *page_bitmap = NULL;
    u64 pg_cnt, i;
    u32 flag;

    pg_cnt = size / heap->chunk_page_size;
    flag = (side == MEM_HOST_SIDE) ?
        (DEVMM_PAGE_HOST_MAPPED_MASK | DEVMM_PAGE_LOCKED_HOST_MASK) :
        (DEVMM_PAGE_DEV_MAPPED_MASK | DEVMM_PAGE_LOCKED_DEVICE_MASK);

    if (!is_da_addr) {
        first_page_bitmap = devmm_get_alloced_va_fst_page_bitmap_with_heap(heap, va);
        if (ka_unlikely(first_page_bitmap == NULL)) {
            devmm_drv_err("Unexpected, get first pg_bitmap failed. (va=0x%llx)\n", va);
            return -EINVAL;
        }
    }

    page_bitmap = devmm_get_page_bitmap_with_heap(heap, va);
    if (ka_unlikely(page_bitmap == NULL)) {
        devmm_drv_err("Unexpected, get pg_bitmap failed. (va=0x%llx)\n", va);
        return -EINVAL;
    }

    for (i = 0; i < pg_cnt; i++) {
        devmm_page_bitmap_clear_flag(page_bitmap + i, flag | DEVMM_PAGE_ADVISE_POPULATE_MASK);
    }
    if (!is_da_addr) {
        if (clear_first_bimap_state) {
            devmm_page_bitmap_clear_flag(first_page_bitmap, DEVMM_PAGE_MEM_MAPPED_MASK);
        }
    }
    return 0;
}

int devmm_msg_to_agent_mem_unmap(struct devmm_svm_process *svm_proc, struct devmm_vmma_info *info)
{
    struct devmm_chan_mem_unmap msg = {{{0}}};
    u64 freed_num, tmp_num;
    int ret;

    msg.head.msg_id = DEVMM_CHAN_MEM_UNMAP_H2D_ID;
    msg.head.process_id.hostpid = svm_proc->process_id.hostpid;
    msg.head.process_id.vfid = (u16)info->vfid;
    msg.head.dev_id = (u16)info->devid;

    devmm_mem_free_preprocess_by_dev_and_va(svm_proc, info->devid, info->va, info->pg_num * info->pg_size);
    for (freed_num = 0; freed_num < info->pg_num; freed_num += tmp_num) {
        msg.va = info->va + freed_num * info->pg_size;
        ret = devmm_chan_msg_send(&msg, sizeof(struct devmm_chan_mem_unmap), sizeof(struct devmm_chan_mem_unmap));
        if (ret != 0) {
            devmm_drv_err("Send mem unmap msg failed. (ret=%d; va=0x%llx; freed_num=%llu)\n", ret, msg.va, freed_num);
            return ret;
        }
        /* Every agent map msg will create vmma, so map per page num should equal with unmap */
        tmp_num = ka_base_min((info->pg_num - freed_num), DEVMM_MEM_MAP_MAX_PAGE_NUM_PER_MSG);
        devmm_free_pages_cache(svm_proc, info->logic_devid, (u32)tmp_num, info->pg_size, msg.va, true);
    }

    return 0;
}

static int _devmm_ioctl_mem_unmap(struct devmm_svm_process *svm_proc, struct devmm_vmma_info *info)
{
    if (info->side == MEM_HOST_SIDE) {
        svm_set_da_status(svm_proc, info->va, info->size, DEVMM_DA_STATUS_UNMAPING);
        devmm_mem_unmap(svm_proc, info);
        svm_set_da_status(svm_proc, info->va, info->size, DEVMM_DA_STATUS_NONE);
        return 0;
    } else {
        return devmm_msg_to_agent_mem_unmap(svm_proc, info);
    }
}

static int devmm_mem_unmap_check_bitmap_state(struct devmm_svm_process *svm_proc,
    struct devmm_svm_heap *heap, u64 va, u64 size)
{
    u64 pg_cnt = size / heap->chunk_page_size;
    u32 *page_bitmap = NULL;
    bool is_translating;

    page_bitmap = devmm_get_page_bitmap_with_heap(heap, va);
    if (ka_unlikely(page_bitmap == NULL)) {
        devmm_drv_err("Unexpected, get pg_bitmap failed. (va=0x%llx)\n", va);
        return -EINVAL;
    }

    is_translating = devmm_check_is_translate(svm_proc, va, page_bitmap, heap->chunk_page_size, pg_cnt);
    return (is_translating) ? -EBUSY : 0;
}

int devmm_ioctl_mem_unmap(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_unmap_para *para = &arg->data.mem_unmap_para;
    struct devmm_vmma_struct *vmma = NULL;
    struct devmm_svm_heap *heap = NULL;
    u32 *page_bitmap = NULL;
    bool is_da_addr;
    int ret;

    heap = devmm_svm_get_heap(svm_proc, para->va);
    if (heap == NULL) {
        devmm_drv_err("Is idle addr. (va=0x%llx)\n", para->va);
        return -EINVAL;
    }

    page_bitmap = devmm_get_page_bitmap_with_heap(heap, para->va);
    if (ka_unlikely(page_bitmap == NULL)) {
        devmm_drv_err("Unexpected, get pg_bitmap failed. (va=0x%llx)\n", para->va);
        return -EINVAL;
    }

    vmma = devmm_vmma_get(&heap->vmma_mng, para->va);
    if (vmma == NULL) {
        /* The log cannot be modified, because in the failure mode library. */
        devmm_drv_err("Get vmma failed. (va=0x%llx)\n", para->va);
        return -EINVAL;
    }

    if (para->va != vmma->info.va) {
        devmm_drv_err("Va isn't head addr. (va=0x%llx; vmma->va=0x%llx)\n", para->va, vmma->info.va);
        devmm_vmma_put(vmma);
        return -EINVAL;
    }

    ret = devmm_vmma_exclusive_set(vmma);
    if (ret != 0) {
        devmm_vmma_put(vmma);
        return ret;
    }

    ret = devmm_mem_unmap_check_bitmap_state(svm_proc, heap, vmma->info.va, vmma->info.size);
    if (ret != 0) {
        goto exclusive_clear;
    }

    devmm_access_munmap_all(svm_proc, vmma);

    devmm_svm_free_share_page_msg(svm_proc, heap, vmma->info.va, vmma->info.size, page_bitmap);
    ret = _devmm_ioctl_mem_unmap(svm_proc, &vmma->info);
    if (ret != 0) {
        goto exclusive_clear;
    }

    is_da_addr = svm_is_da_addr(svm_proc, vmma->info.va, vmma->info.size);

    /* Don't clear the first bitmap status, that means the map_side is determined after the first success map. */
    devmm_mem_map_pg_bitmap_state_clear(heap, vmma->info.va, vmma->info.size, vmma->info.side, is_da_addr, false);

    para->logic_devid = vmma->info.logic_devid;
    para->side = vmma->info.side;
    para->unmap_size = vmma->info.size;

    devmm_vmma_destroy(&heap->vmma_mng, vmma);

exclusive_clear:
    devmm_vmma_exclusive_clear(vmma);
    devmm_vmma_put(vmma);
    return ret;
}

static int devmm_ioctl_mem_map_para_check(struct devmm_svm_process *svm_proc,
    struct devmm_svm_heap *heap, struct devmm_mem_map_para *para, u32 devid)
{
    u32 *page_bitmap = NULL;
    u64 alloced_size, pg_cnt, i;
    int ret;

    if (heap->heap_sub_type != SUB_RESERVE_TYPE) {
        devmm_drv_err("Heap sub type could only be reserve type. (va=0x%llx; heap_sub_type=%u)\n",
            para->va, heap->heap_sub_type);
        return -EINVAL;
    }

    if (para->size == 0) {
        devmm_drv_err("Size is zero.\n");
        return -EINVAL;
    }

    if (KA_DRIVER_IS_ALIGNED(para->va, heap->chunk_page_size) == false) {
        devmm_drv_err("Va should be aligned by pg_size. (pg_size=%u)\n", heap->chunk_page_size);
        return -EINVAL;
    }

    if (KA_DRIVER_IS_ALIGNED(para->size, heap->chunk_page_size) == false) {
        devmm_drv_err("Size should be aligned by pg_size. (size=%llu; pg_size=%u)\n",
            para->size, heap->chunk_page_size);
        return -EINVAL;
    }

    ret = devmm_check_va_add_size_by_heap(heap, para->va, para->size);
    if (ret != 0) {
        devmm_drv_err("Addr out of heap range. (va=0x%llx; size=%llu)\n", para->va, para->size);
        return ret;
    }

    if ((para->side != MEM_HOST_SIDE) && (para->side != MEM_DEV_SIDE)) {
        devmm_drv_err("Invalid side. (side=%u)\n", para->side);
        return -EINVAL;
    }

    if (para->pg_type >= MEM_MAX_PAGE_TYPE) {
        devmm_drv_err("Invalid pg_type. (pg_type=%u)\n", para->pg_type);
        return -EINVAL;
    }

    if ((para->side == MEM_HOST_SIDE) && (devid != uda_get_host_id())) {
        devmm_drv_err("Invalid side devid. (side=%u; devid=%u)\n", para->side, devid);
        return -EINVAL;
    }

    alloced_size = devmm_get_alloced_size_from_va(heap, para->va);
    if (para->size > alloced_size) {
        devmm_drv_err("Size out of range. (size=%llu; alloced_size=%llu)\n", para->size, alloced_size);
        return -EINVAL;
    }

    page_bitmap = devmm_get_page_bitmap_with_heap(heap, para->va);
    if (page_bitmap == NULL) {
        devmm_drv_err("Unexpected, get pg_bitmap failed. (va=0x%llx)\n", para->va);
        return -EINVAL;
    }

    pg_cnt = para->size / heap->chunk_page_size;
    for (i = 0; i < pg_cnt; i++) {
        if (devmm_page_bitmap_is_page_available(page_bitmap + i) == false) {
            devmm_drv_err("Addr hasn't been alloced. (va=0x%llx; size=%llu)\n", para->va, para->size);
            return -EINVAL;
        }
    }

    return 0;
}

static int devmm_master_mem_map(struct devmm_svm_process *svm_proc, struct devmm_vmma_info *info)
{
    return devmm_mem_map(svm_proc, info, true);
}

static int _devmm_ioctl_mem_map(struct devmm_svm_process *svm_proc, struct devmm_vmma_info *info)
{
    if (info->side == MEM_HOST_SIDE) {
        return devmm_master_mem_map(svm_proc, info);
    } else {
        return devmm_msg_to_agent_mem_map(svm_proc, info);
    }
}

static void devmm_ioctl_vmma_info_pack(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap,
    struct devmm_devid *devids, struct devmm_mem_map_para *para, struct devmm_vmma_info *info)
{
    u32 type = SVM_PYH_ADDR_BLK_NORMAL_TYPE;
    struct devmm_phy_addr_blk *blk = NULL;
    bool is_same_sys_share = true;
    int i;

   if (para->side == MEM_HOST_SIDE) {
       blk = devmm_phy_addr_blk_get(&svm_proc->phy_addr_blk_mng, para->id);
       if (blk != NULL) {
           is_same_sys_share = blk->is_same_sys_share;
           type = blk->type;
           devmm_drv_debug("blk info. (is_same_sys_share=%u; type=%u)\n", is_same_sys_share, blk->type);
           devmm_phy_addr_blk_put(blk);
       }
   }

    info->pg_type = para->pg_type;
    if (para->pg_type == MEM_GIANT_PAGE_TYPE) {
        info->pg_type = MEM_GIANT_PAGE_TYPE;
        info->pg_size = SVM_MASTER_GIANT_PAGE_SIZE;
    } else if ((para->side == MEM_HOST_SIDE) && !is_same_sys_share && (type == SVM_PYH_ADDR_BLK_IMPORT_TYPE)) {
        info->pg_type = MEM_NORMAL_PAGE_TYPE;
        info->pg_size = KA_MM_PAGE_SIZE;
    } else {
        info->pg_type = (heap->heap_type == DEVMM_HEAP_HUGE_PAGE) ? MEM_HUGE_PAGE_TYPE : MEM_NORMAL_PAGE_TYPE;
        if (info->pg_type == MEM_HUGE_PAGE_TYPE) {
            info->pg_size = (para->side == MEM_HOST_SIDE) ? SVM_MASTER_HUGE_PAGE_SIZE : devmm_svm->device_hpage_size;
        } else {
            info->pg_size = (para->side == MEM_HOST_SIDE) ? KA_MM_PAGE_SIZE : devmm_svm->device_page_size;
        }
    }

    info->va = para->va;
    info->size = para->size;
    info->pg_num = info->size / info->pg_size;

    info->side = para->side;
    info->logic_devid = devids->logical_devid;
    info->devid = devids->devid;
    info->vfid = devids->vfid;
    info->page_insert_dev_id = devids->logical_devid;
    info->local_handle_flag = 1;
    info->type = MEM_ACCESS_TYPE_READWRITE;
    for (i = 0; i < DEVMM_MAX_ACCESS_DEVICE_NUM; i++) {
        info->device_access_type[i] = MEM_ACCESS_TYPE_NONE;
    }

    info->module_id = para->module_id;
    info->phy_addr_blk_id = para->id;
    info->offset_pg_num = 0;
    info->phy_addr_blk_pg_num = para->pg_num;
}

int devmm_ioctl_mem_map(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_map_para *para = &arg->data.mem_map_para;
    struct devmm_share_id_map_node *map_node = NULL;
    struct devmm_vmma_info info = {0};
    struct devmm_svm_heap *heap = NULL;
    bool is_da_addr = svm_is_da_addr(svm_proc, para->va, para->size);
    bool is_first_map;
    int ret;

    heap = devmm_svm_get_heap(svm_proc, para->va);
    if (heap == NULL) {
        devmm_drv_err("Invalid addr, get heap failed. (va=0x%llx)\n", para->va);
        return -EINVAL;
    }

    ret = devmm_ioctl_mem_map_para_check(svm_proc, heap, para, arg->head.devid);
    if (ret != 0) {
        return ret;
    }

    ret = devmm_mem_map_pg_bitmap_state_set(heap, para, arg->head.logical_devid, is_da_addr, &is_first_map);
    if (ret != 0) {
        return ret;
    }

    devmm_ioctl_vmma_info_pack(svm_proc, heap, &arg->head, para, &info);
    ret = _devmm_ioctl_mem_map(svm_proc, &info);
    if (ret != 0) {
        devmm_mem_map_pg_bitmap_state_clear(heap, para->va, para->size, para->side, is_da_addr, is_first_map);
        return ret;
    }

    map_node = devmm_share_id_map_node_get(svm_proc, arg->head.devid, para->id);
    if (map_node != NULL) {
        info.local_handle_flag = 0;
        devmm_share_id_map_node_put(map_node);
    }

    ret = devmm_vmma_create(&heap->vmma_mng, &info);
    if (ret != 0) {
        _devmm_ioctl_mem_unmap(svm_proc, &info);
        devmm_mem_map_pg_bitmap_state_clear(heap, para->va, para->size, para->side, is_da_addr, is_first_map);
        return ret;
    }

    return 0;
}

int devmm_ioctl_resv_addr_info_query(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_resv_addr_info_query_para *para = &arg->data.resv_addr_info_query_para;
    struct devmm_vmma_struct *vmma = NULL;
    struct devmm_svm_heap *heap = NULL;

    heap = devmm_svm_get_heap(svm_proc, para->va);
    if (heap == NULL) {
        devmm_drv_info("Is idle addr. (va=0x%llx)\n", para->va);
        return -EINVAL;
    }

    vmma = devmm_vmma_get(&heap->vmma_mng, para->va);
    if (vmma == NULL) {
        devmm_drv_info("Not in range. (va=0x%llx)\n", para->va);
        return -EINVAL;
    }
    para->start = vmma->info.va;
    para->end = vmma->info.va + vmma->info.size - 1;
    para->module_id = vmma->info.module_id;
    para->devid = vmma->info.logic_devid;
    devmm_vmma_put(vmma);

    return 0;
}

void devmm_destroy_all_heap_vmmas_by_devid(struct devmm_svm_process *svm_proc, u32 devid)
{
    struct devmm_vmma_struct *vmma = NULL, *tmp = NULL;
    struct devmm_svm_heap *heap = NULL;
    u32 stamp = (u32)ka_jiffies;
    u64 heap_idx;

    for (heap_idx = 0; heap_idx < svm_proc->max_heap_use; heap_idx++) {
        devmm_try_cond_resched(&stamp);
        heap = devmm_get_heap_by_idx(svm_proc, heap_idx);
        if (devmm_check_heap_is_entity(heap) == false) {
            continue;
        }
        rbtree_postorder_for_each_entry_safe(vmma, tmp, &heap->vmma_mng.root, rbnode) {
            bool is_da_addr;

            devmm_try_cond_resched(&stamp);
            if ((vmma->info.side == DEVMM_SIDE_MASTER) || (vmma->info.devid != devid)) {
                continue;
            }
            is_da_addr = svm_is_da_addr(svm_proc, vmma->info.va, vmma->info.size);
            devmm_mem_map_pg_bitmap_state_clear(heap, vmma->info.va, vmma->info.size, vmma->info.side,
                is_da_addr, false);
            if (vmma->info.side == DEVMM_SIDE_TYPE) {
                devmm_mem_unmap(svm_proc, &vmma->info);
            }
            devmm_access_munmap_all(svm_proc, vmma);
            devmm_vmma_destroy(&heap->vmma_mng, vmma);
        }

        heap_idx += (heap->heap_size >= DEVMM_HEAP_SIZE) ? (heap->heap_size / DEVMM_HEAP_SIZE - 1) : 0;
    }
}

static struct devmm_vmma_struct *devmm_vmma_matched_get(struct devmm_svm_process *svm_proc, u64 va, u64 size)
{
    struct devmm_vmma_struct *vmma = NULL;
    struct devmm_svm_heap *heap = NULL;

    heap = devmm_svm_get_heap(svm_proc, va);
    if (heap == NULL) {
        devmm_drv_err("Is idle addr. (va=0x%llx)\n", va);
        return NULL;
    }

    vmma = devmm_vmma_get(&heap->vmma_mng, va);
    if (vmma == NULL) {
        devmm_drv_err("Get vmma failed. (va=0x%llx)\n", va);
        return NULL;
    }

    if ((va != vmma->info.va) || (size != vmma->info.size)) {
        devmm_drv_err("Not match para. (va=0x%llx; vmma->va=0x%llx; size=0x%llx; vmma->size=0x%llx)\n",
            va, vmma->info.va, size, vmma->info.size);
        devmm_vmma_put(vmma);
        return NULL;
    }

    return vmma;
}

int devmm_ioctl_mem_query_owner(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_query_owner_para *para = &arg->data.mem_query_owner_para;
    struct devmm_vmma_struct *vmma = NULL;

    vmma = devmm_vmma_matched_get(svm_proc, para->va, para->size);
    if (vmma == NULL) {
        devmm_drv_err("Get vmma failed. (va=0x%llx)\n", para->va);
        return -EINVAL;
    }

    para->devid = vmma->info.devid;
    para->local_handle_flag = vmma->info.local_handle_flag;
    devmm_vmma_put(vmma);

    return 0;
}

static bool devmm_is_mem_access_type_valid(drv_mem_access_type type)
{
    return ((type == MEM_ACCESS_TYPE_NONE) || (type == MEM_ACCESS_TYPE_READ) || (type == MEM_ACCESS_TYPE_READWRITE));
}

int devmm_ioctl_mem_set_access(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_set_access_para *para = &arg->data.mem_set_access_para;
    struct devmm_vmma_struct *vmma = NULL;
    u32 udevid;
    int ret;

    ret = uda_devid_to_udevid_ex(para->logic_devid, &udevid);
    if (ret != 0) {
        devmm_drv_err("Get udevid failed. (loggic_devid=%u)\n", para->logic_devid);
        return ret;
    }

    if ((para->logic_devid >= DEVMM_MAX_ACCESS_DEVICE_NUM) || (!devmm_is_mem_access_type_valid(para->type))) {
        devmm_drv_err("Invalid para. (loggic_devid=%u; type=%u)\n", para->logic_devid, (u8)para->type);
        return -EINVAL;
    }

    vmma = devmm_vmma_matched_get(svm_proc, para->va, para->size);
    if (vmma == NULL) {
        devmm_drv_err("Get vmma failed. (va=0x%llx)\n", para->va);
        return -EINVAL;
    }

    ret = 0;
    ka_task_mutex_lock(&vmma->mutex);

    if (udevid == vmma->info.devid) {
        vmma->info.type = para->type;
    } else {
        if (vmma->info.device_access_type[para->logic_devid] == MEM_ACCESS_TYPE_NONE) {
            ret = devmm_access_mmap(svm_proc, vmma, para, udevid);
            if (ret == 0) {
                vmma->info.device_access_type[para->logic_devid] = para->type;
            }
        } else {
            ret = -EEXIST;
            devmm_drv_err("Repeat set access. (va=0x%llx; logic_devid=%u)\n", para->va, para->logic_devid);
        }
    }

    ka_task_mutex_unlock(&vmma->mutex);

    devmm_vmma_put(vmma);

    return ret;
}

int devmm_ioctl_mem_get_access(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_get_access_para *para = &arg->data.mem_get_access_para;
    struct devmm_svm_heap *heap = NULL;
    struct devmm_vmma_struct *vmma = NULL;
    u32 udevid;
    int ret;

    ret = uda_devid_to_udevid_ex(para->logic_devid, &udevid);
    if (ret != 0) {
        devmm_drv_err("Get udevid failed. (logic_devid=%u)\n", para->logic_devid);
        return ret;
    }

    if (para->logic_devid >= DEVMM_MAX_ACCESS_DEVICE_NUM) {
        devmm_drv_err("Invalid para. (logic_devid=%u)\n", para->logic_devid);
        return -EINVAL;
    }

    heap = devmm_svm_get_heap(svm_proc, para->va);
    if (heap == NULL) {
        devmm_drv_err("Is idle addr. (va=0x%llx)\n", para->va);
        return -EINVAL;
    }

    vmma = devmm_vmma_get(&heap->vmma_mng, para->va);
    if (vmma == NULL) {
        devmm_drv_err("Get vmma failed. (va=0x%llx)\n", para->va);
        return -EINVAL;
    }

    if ((para->va + para->size) > (vmma->info.va + vmma->info.size)) {
        devmm_drv_err("Invalid para. (va=0x%llx; vmma->va=0x%llx; size=0x%llx; vmma->size=0x%llx)\n",
            para->va, vmma->info.va, para->size, vmma->info.size);
        devmm_vmma_put(vmma);
        return -EINVAL;
    }

    ka_task_mutex_lock(&vmma->mutex);

    para->size = vmma->info.size;
    if (udevid == vmma->info.devid) {
        para->type = vmma->info.type;
    } else {
        para->type = vmma->info.device_access_type[para->logic_devid];
    }

    ka_task_mutex_unlock(&vmma->mutex);

    devmm_vmma_put(vmma);

    return 0;
}

