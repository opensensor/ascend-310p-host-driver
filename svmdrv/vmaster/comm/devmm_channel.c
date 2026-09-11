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
#include "devmm_proc_info.h"
#include "devdrv_interface.h"
#include "devdrv_manager_comm.h"
#include "svm_msg_client.h"
#include "devmm_common.h"
#include "devmm_page_cache.h"
#include "svm_dma.h"
#include "svm_master_remote_map.h"
#include "svm_master_mem_share.h"
#include "svm_heap_mng.h"
#include "svm_proc_mng.h"
#include "devmm_mem_alloc_interface.h"
#include "devmm_channel.h"

/*
 * this function is use in  loop by idx. there're 3 
 * (1)init pa_idx and merg_idx
 * (2)call this function
 * (3)restore merg idx
 */
static void devmm_merg_pa(u64 *palist, u32 pa_idx, u32 pgsz, u32 *merg_szlist, u32 *merg_idx)
{
    u64 *merg_palist = palist;
    u32 i = pa_idx;
    u32 j = *merg_idx;

    if ((i >= 1) && (palist[i - 1] + pgsz == palist[i])) {
        merg_szlist[j - 1] += pgsz;
    } else {
        merg_palist[j] = palist[i];
        merg_szlist[j] = pgsz;
        j++;
    }
    *merg_idx = j;
}

void devmm_merg_pa_by_num(u64 *pas, u32 num, u32 pgsz, u32 *merg_szlist, u32 *merg_num)
{
    u32 stamp = (u32)ka_jiffies;
    u32 i, j;

    for (i = 0, j = 0; i < num; i++) {
        devmm_merg_pa(pas, i, pgsz, merg_szlist, &j);
        devmm_try_cond_resched(&stamp);
    }
    *merg_num = j;
}

void devmm_merg_blk(struct devmm_dma_block *blks, u32 idx, u32 *merg_idx)
{
    struct devmm_dma_block *merg_blks = blks;
    u32 j = *merg_idx;
    u32 i = idx;

    if ((i >= 1) && (blks[i - 1].pa + blks[i - 1].sz == blks[i].pa)) {
        merg_blks[j - 1].sz += blks[i].sz;
    } else {
        merg_blks[j].pa = blks[i].pa;
        merg_blks[j].dma = blks[i].dma;
        merg_blks[j].sz = blks[i].sz;
        j++;
    }
    *merg_idx = j;
}

void devmm_merg_phy_blk(struct devmm_chan_phy_block *blks, u32 blks_idx, u32 *merg_idx)
{
    struct devmm_chan_phy_block *merg_blks = (struct devmm_chan_phy_block *)blks;
    u32 j = *merg_idx;
    u32 i = blks_idx;

    if ((i >= 1) && (blks[i - 1].pa + blks[i - 1].sz == blks[i].pa)) {
        merg_blks[j - 1].sz += blks[i].sz;
    } else {
        merg_blks[j].pa = blks[i].pa;
        merg_blks[j].sz = blks[i].sz;
        j++;
    }
    *merg_idx = j;
}

/*lint -e629*/
STATIC int devmm_chan_query_vaflgs_d2h_process(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap,
    void *msg, u32 *ack_len)
{
#ifndef EMU_ST
    struct devmm_chan_page_query *flg_msg = (struct devmm_chan_page_query *)msg;
    struct devmm_svm_process_id *process_id = &flg_msg->head.process_id;
    struct devmm_svm_heap *heap_tmp = NULL;
    int ret;

    devmm_drv_debug("Host receive query_valfg message. (hostpid=%d; devid=%u; vfid=%u; va=0x%llx)\n",
        process_id->hostpid, process_id->devid, process_id->vfid, flg_msg->va);

    ka_task_down_read(&svm_proc->heap_sem);
    heap_tmp = devmm_svm_get_heap(svm_proc, flg_msg->va);
    if (heap_tmp == NULL) {
        ka_task_up_read(&svm_proc->heap_sem);
        devmm_drv_err_if((svm_proc->device_fault_printf != 0), "Device fault error. (hostpid=%d; va=0x%llx)\n",
            process_id->hostpid, flg_msg->va);
        return -EFAULT;
    }
    ret = devmm_dev_page_fault_get_vaflgs(svm_proc, heap_tmp, flg_msg);
    ka_task_up_read(&svm_proc->heap_sem);
    if (ret != 0) {
        devmm_drv_err_if((svm_proc->device_fault_printf != 0), "Device fault error. (hostpid=%d; va=0x%llx; ret=%d)\n",
            process_id->hostpid, flg_msg->va, ret);
        svm_proc->device_fault_printf = 0;
        return ret;
    }
    svm_proc->device_fault_printf = 1;
    *ack_len = sizeof(struct devmm_chan_page_query);
#endif
    return 0;
}

STATIC int devmm_chan_page_fault_process_copy(struct devmm_chan_page_fault *fault_msg,
    struct devmm_svm_process *svm_process, struct devmm_svm_heap *heap)
{
    struct devmm_svm_process_id *process_id = &fault_msg->head.process_id;
    u32 num = DEVMM_PAGE_NUM_PER_FAULT;
    ka_vm_area_struct_t *vma = NULL;
    ka_page_t **pages = NULL;
    ka_device_t *dev = NULL;
    u64 *pas = NULL;
    u32 *szs = NULL;
    u32 i, j, pa_len;
    u32 stamp;
    int ret;

    pa_len = sizeof(unsigned long) * num + sizeof(u64) * num + sizeof(u32) * num;
    pas = (u64 *)devmm_kzalloc_ex(pa_len, KA_GFP_KERNEL);
    if (pas == NULL) {
        devmm_drv_err("Kzalloc pas failed. (va=0x%llx; num=%d)\n", fault_msg->va, fault_msg->num);
        return -ENOMEM;
    }
    pages = (ka_page_t **)((unsigned long)(uintptr_t)pas + (u32)(sizeof(u64) * num));
    szs = (u32 *)(uintptr_t)((unsigned long)(uintptr_t)pas +
        (u32)(sizeof(u64) * num) + (u32)(sizeof(unsigned long) * num));
    vma = devmm_find_vma(svm_process, fault_msg->va);
    if (vma == NULL) {
        devmm_drv_err("Find vma failed, check fault_msg. (va=0x%llx; hostpid=%d; devid=%u; vfid=%u)\n",
            fault_msg->va, process_id->hostpid, process_id->devid, process_id->vfid);
        ret = -EADDRNOTAVAIL;
        goto page_fault_d2h_copy_free_pas;
    }

    ret = devmm_va_to_palist(vma, fault_msg->va, heap->chunk_page_size, pas, &num);
    if (ret != 0) {
        devmm_drv_err("Va to palist failed. (hostpid=%d; devid=%u; vfid=%u; ret=%d; num=%d; va=0x%llx)\n",
            process_id->hostpid, process_id->devid, process_id->vfid, ret, num, fault_msg->va);
        goto page_fault_d2h_copy_free_pas;
    }
    dev = devmm_device_get_by_devid(fault_msg->head.dev_id);
    if (dev == NULL) {
        devmm_drv_err("Dev is NULL. (dev_id=%d)\n", fault_msg->head.dev_id);
        ret = -ENODEV;
        goto page_fault_d2h_copy_free_pas;
    }

    stamp = (u32)ka_jiffies;
    for (i = 0; i < num; i++) {
        szs[i] = KA_MM_PAGE_SIZE;
        pages[i] = devmm_pa_to_page(pas[i]);
        ka_mm_get_page(pages[i]);
        pas[i] = devdrv_dma_map_page(dev, pages[i], 0, szs[i], KA_DMA_BIDIRECTIONAL);
        ret = ka_mm_dma_mapping_error(dev, pas[i]);
        if (ret != 0) {
            devmm_drv_err("Dma map page failed. (hostpid=%d; devid=%u; vfid=%u; va=0x%llx; num=%d; ret=%d)\n",
                          process_id->hostpid, process_id->devid, process_id->vfid, fault_msg->va, fault_msg->num, ret);
            ka_mm_put_page(pages[i]);
            goto page_fault_d2h_copy_dma_free;
        }
        devmm_try_cond_resched(&stamp);
    }
    ret = devmm_chan_page_fault_d2h_process_dma_copy(fault_msg, pas, szs, num);
    if (ret != 0) {
        devmm_drv_err("Dma failed. (hostpid=%d; devid=%u; vfid=%u; va=0x%llx; num=%d; fault=%d; ret=%d)\n",
            process_id->hostpid, process_id->devid, process_id->vfid, fault_msg->va, num, fault_msg->num, ret);
    }

page_fault_d2h_copy_dma_free:
    stamp = (u32)ka_jiffies;
    for (j = 0; j < i; j++) {
        ka_mm_put_page(pages[j]);
        devdrv_dma_unmap_page(dev, pas[j], szs[j], KA_DMA_BIDIRECTIONAL);
        devmm_try_cond_resched(&stamp);
    }
    devmm_device_put_by_devid(fault_msg->head.dev_id);
page_fault_d2h_copy_free_pas:
    devmm_kfree_ex(pas);
    pas = NULL;

    return ret;
}

static int devmm_chan_page_fault_check_va(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap, u64 va)
{
    u32 *page_bitmap = devmm_get_page_bitmap_with_heap(heap, va);
    int ret;

    page_bitmap = devmm_get_page_bitmap_with_heap(heap, va);
    if (page_bitmap == NULL) {
        return -EINVAL;
    }
    ret = ((devmm_page_bitmap_is_page_available(page_bitmap) == 0) ||
        devmm_page_bitmap_is_locked_host(page_bitmap) ||
        devmm_page_bitmap_is_ipc_open_mem(page_bitmap) ||
        devmm_page_bitmap_is_dev_mapped(page_bitmap));
    if (ret != 0) {
        devmm_drv_err("Devmm fault address check error. (hostpid=%d; devid=%u; vfid=%u; va=0x%llx; bitmap=0x%x)\n",
            svm_proc->process_id.hostpid, svm_proc->process_id.devid, svm_proc->process_id.vfid, va,
            devmm_page_read_bitmap(page_bitmap));
        return ret;
    }
    return 0;
}

STATIC int devmm_chan_page_fault_copy_data(struct devmm_svm_process *svm_process, struct devmm_svm_heap *heap,
    void *msg, u32 *ack_len)
{
    struct devmm_chan_page_fault *fault_msg = (struct devmm_chan_page_fault *)msg;
    struct devmm_svm_process_id *process_id = &fault_msg->head.process_id;
    int ret;

    ret = devmm_chan_page_fault_check_va(svm_process, heap, fault_msg->va);
    if (ret != 0) {
        devmm_drv_err("Devmm fault address check error. (hostpid=%d; devid=%u; vfid=%u; va=0x%llx; num=%d; ret=%d)\n",
            process_id->hostpid, process_id->devid, process_id->vfid, fault_msg->va, fault_msg->num, ret);
        return ret;
    }
    devmm_svm_set_mapped_with_heap(svm_process, fault_msg->va, heap->chunk_page_size,
        fault_msg->head.logical_devid, heap);
    ret = devmm_chan_page_fault_process_copy(fault_msg, svm_process, heap);
    if (ret != 0) {
        devmm_drv_err("Devmm fault d2h copy failed. (hostpid=%d; devid=%u; vfid=%u; va=0x%llx; num=%d; ret=%d)\n",
            process_id->hostpid, process_id->devid, process_id->vfid, fault_msg->va, fault_msg->num, ret);
        return ret;
    }
    devmm_unmap_pages(svm_process, fault_msg->va, heap->chunk_page_size / KA_MM_PAGE_SIZE);

    devmm_svm_clear_mapped_with_heap(svm_process, fault_msg->va, heap->chunk_page_size,
        DEVMM_INVALID_DEVICE_PHYID, heap);

    return 0;
}

STATIC int devmm_chan_page_fault_d2h_process(struct devmm_svm_process *svm_process, struct devmm_svm_heap *heap,
    void *msg, u32 *ack_len)
{
#ifndef EMU_ST
    struct devmm_chan_page_fault *fault_msg = (struct devmm_chan_page_fault *)msg;
    struct devmm_svm_process_id *process_id = &fault_msg->head.process_id;
    struct devmm_svm_heap *heap_tmp = NULL;
    int ret;

    devmm_drv_debug("Enter host receive page_fault. (hostpid=%d; devid=%u; vfid=%u; va=0x%llx; num=%d)\n",
        process_id->hostpid, process_id->devid, process_id->vfid, fault_msg->va, fault_msg->num);

    ka_task_down_read(&svm_process->heap_sem);
    heap_tmp = devmm_svm_get_heap(svm_process, fault_msg->va);
    if (heap_tmp == NULL) {
        ka_task_up_read(&svm_process->heap_sem);
        devmm_drv_err("Va is not alloced. (hostpid=%d; va=0x%llx; devid=%u)\n",
            svm_process->process_id.hostpid, fault_msg->va, fault_msg->head.dev_id);
        return -EFAULT;
    }

    if (devmm_page_fault_get_va_ref(svm_process, fault_msg->va) != 0) {
        ka_task_up_read(&svm_process->heap_sem);
        devmm_drv_err("Va don't allow fault by device, va is in operation. (hostpid=%d; va=0x%llx; devid=%u)\n",
            svm_process->process_id.hostpid, fault_msg->va, fault_msg->head.dev_id);
        return -EINVAL;
    }
    ret = devmm_chan_page_fault_copy_data(svm_process, heap_tmp, msg, ack_len);
    devmm_page_fault_put_va_ref(svm_process, fault_msg->va);
    ka_task_up_read(&svm_process->heap_sem);
    if (ret != 0) {
        devmm_drv_err("Devmm_chan_page_fault_d2h_process_copy failed. "
            "(hostpid=%d; devid=%u; vfid=%u; va=0x%llx; num=%d; ret=%d)\n",
            process_id->hostpid, process_id->devid, process_id->vfid, fault_msg->va, fault_msg->num, ret);
        return ret;
    }
    devmm_drv_debug("Exit. (hostpid=%d; devid=%u; vfid=%u; va=0x%llx; num=%d; ret=%d)\n",
        process_id->hostpid, process_id->devid, process_id->vfid, fault_msg->va, fault_msg->num, ret);
#endif
    return 0;
}

STATIC int devmm_chan_check_va_d2h_process(struct devmm_svm_process *svm_pro, struct devmm_svm_heap *heap,
    void *msg, u32 *ack_len)
{
    struct devmm_chan_check_va *check_va_msg = (struct devmm_chan_check_va *)msg;
    struct devmm_svm_heap *heap_tmp = NULL;
    u32 *page_bitmap = NULL;
    u64 pre_start_va = 0x0;
    u64 pre_end_va = 0x0;
    u64 post_start_va = 0x0;
    u64 post_end_va = 0x0;
    u64 check_va;

    *ack_len = sizeof(struct devmm_chan_check_va);
    check_va = check_va_msg->check_va;

    ka_task_down_read(&svm_pro->heap_sem);
    heap_tmp = devmm_svm_get_heap(svm_pro, check_va);
    if (heap_tmp == NULL) {
        ka_task_up_read(&svm_pro->heap_sem);
        devmm_drv_err("Va is not alloced. (hostpid=%d; va=0x%llx)\n", svm_pro->process_id.hostpid, check_va);
        return -EFAULT;
    }

    page_bitmap = devmm_get_page_bitmap_with_heap(heap_tmp, check_va);
    if ((page_bitmap == NULL) || devmm_page_bitmap_is_locked_host(page_bitmap)) {
        ka_task_up_read(&svm_pro->heap_sem);
        devmm_drv_err("Va isn't alloced. (va=0x%llx; hostpid=%d; bitmap=0x%x)\n",
                      check_va, svm_pro->process_id.hostpid,
                      page_bitmap ? devmm_page_read_bitmap(page_bitmap) : 0);
        return -EINVAL;
    }
    check_va_msg->bitmap = *page_bitmap;

    (void)devmm_check_alloced_va(svm_pro, check_va, &pre_start_va, &pre_end_va, DEVMM_PRE_ALLOCED_FLAG);
    (void)devmm_check_alloced_va(svm_pro, check_va, &post_start_va, &post_end_va, DEVMM_POST_ALLOCED_FLAG);
    ka_task_up_read(&svm_pro->heap_sem);
    check_va_msg->pre_start_va = pre_start_va;
    check_va_msg->pre_end_va = pre_end_va;
    check_va_msg->post_start_va = post_start_va;
    check_va_msg->post_end_va = post_end_va;
    return 0;
}

static int devmm_shm_node_ref_inc(struct devmm_chan_shm_getput_pages_d2h *get_pages_msg,
    struct devmm_svm_process *svm_proc, u32 map_type)
{
    struct devmm_shm_pro_node *shm_pro_node = NULL;
    struct devmm_shm_node *node = NULL;

    shm_pro_node = devmm_get_shm_pro_node(svm_proc, map_type);
    if (shm_pro_node == NULL) {
        devmm_drv_err("Process exited. (dev_id=%u; hostpid=%u; dev_va=0x%llx)\n", get_pages_msg->head.dev_id,
            get_pages_msg->head.process_id.hostpid, get_pages_msg->dev_va);
        return -ESRCH;
    }
    node = devmm_get_shm_node_by_dva(&shm_pro_node->shm_head, get_pages_msg->dev_va,
        get_pages_msg->size, get_pages_msg->head.dev_id, get_pages_msg->head.vfid);
    if (node == NULL) {
        return -EINVAL;
    }
    node->ref++;
    return 0;
}

#define DEVMM_LOCAL_HOST_MAP_TYPE_MAX 2
static int devmm_chan_shm_get_pages_d2h_process(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap,
    void *msg, u32 *ack_len)
{
    struct devmm_chan_shm_getput_pages_d2h *get_pages_msg = (struct devmm_chan_shm_getput_pages_d2h *)msg;
    u32 map_type[DEVMM_LOCAL_HOST_MAP_TYPE_MAX] = {HOST_MEM_MAP_DEV, HOST_MEM_MAP_DEV_PCIE_TH};
    ka_semaphore_t *shm_sem = NULL;
    int ret;
    u32 i;

    for (i = 0; i < DEVMM_LOCAL_HOST_MAP_TYPE_MAX; i++) {
        shm_sem = devmm_get_shm_sem(svm_proc, map_type[i]);
        ka_task_down(shm_sem);
        ret = devmm_shm_node_ref_inc(get_pages_msg, svm_proc, map_type[i]);
        ka_task_up(shm_sem);
        if ((ret == -ESRCH) || (ret == 0)) {
            return ret;
        }
    }
    devmm_drv_err("Get share memory failed. (dev_va=0x%llx; size=%llu; devid=%u; vfid=%u)\n",
        get_pages_msg->dev_va, get_pages_msg->size, get_pages_msg->head.dev_id, get_pages_msg->head.vfid);
    return -EINVAL;
}

STATIC struct devmm_shm_pro_node *devmm_get_shm_pro_node_by_hostpid(u32 hostpid,
    struct devmm_shm_process_head *shm_process_head)
{
    struct devmm_shm_pro_node *shm_pro_node = NULL;
    ka_list_head_t *head = NULL;
    ka_list_head_t *n = NULL;
    ka_list_head_t *pos = NULL;

    head = &shm_process_head->head;

    ka_list_for_each_safe(pos, n, head) {
        shm_pro_node = ka_list_entry(pos, struct devmm_shm_pro_node, list);
        if (shm_pro_node->hostpid == hostpid) {
            devmm_drv_debug("Get shm_pro_node succeeded. (hostpid=%u)\n", hostpid);
            return shm_pro_node;
        }
    }

    return NULL;
}

static int devmm_shm_node_ref_dec_after_proc_exit(struct devmm_chan_shm_getput_pages_d2h *put_pages_msg,
    struct devmm_shm_process_head *shm_process_head)
{
    struct devmm_shm_pro_node *shm_pro_node = NULL;
    struct devmm_shm_node *node = NULL;

    shm_pro_node = devmm_get_shm_pro_node_by_hostpid((u32)put_pages_msg->head.process_id.hostpid, shm_process_head);
    if (shm_pro_node == NULL) {
        return -ENOENT;
    }
    node = devmm_get_shm_node_by_dva(&shm_pro_node->shm_head, put_pages_msg->dev_va,
        put_pages_msg->size, put_pages_msg->head.dev_id, put_pages_msg->head.vfid);
    if (node == NULL) {
        return -EINVAL;
    }

    if (node->ref > 0) {
        node->ref--;
    }
    if (node->ref == 0) {
        devmm_remote_unmap_and_delete_node(NULL, node);
        node = NULL;
        devmm_delete_shm_pro_node(shm_pro_node);
    }
    return 0;
}

static int devmm_shm_node_ref_dec(struct devmm_chan_shm_getput_pages_d2h *put_pages_msg,
    struct devmm_svm_process *svm_proc, u32 map_type)
{
    struct devmm_shm_pro_node *shm_pro_node = NULL;
    struct devmm_shm_node *node = NULL;

    shm_pro_node = devmm_get_shm_pro_node(svm_proc, map_type);
    if (shm_pro_node == NULL) {
        return -ESRCH;
    }

    node = devmm_get_shm_node_by_dva(&shm_pro_node->shm_head, put_pages_msg->dev_va,
        put_pages_msg->size, put_pages_msg->head.dev_id, put_pages_msg->head.vfid);
    if (node == NULL) {
        return -EINVAL;
    }
    if (node->ref > 0) {
        node->ref--;
    }
    return 0;
}

static int devmm_chan_shm_put_pages_d2h_process(struct devmm_svm_process *svm_process, struct devmm_svm_heap *heap,
    void *msg, u32 *ack_len)
{
    struct devmm_chan_shm_getput_pages_d2h *put_pages_msg = (struct devmm_chan_shm_getput_pages_d2h *)msg;
    u32 map_type[DEVMM_LOCAL_HOST_MAP_TYPE_MAX] = {HOST_MEM_MAP_DEV, HOST_MEM_MAP_DEV_PCIE_TH};
    struct devmm_svm_process *svm_proc = NULL;
    ka_semaphore_t *shm_sem = NULL;
    struct devmm_shm_process_head *shm_process_head = NULL;
    int ret;
    u32 i;

    svm_proc = devmm_svm_proc_get_by_process_id_ex(&put_pages_msg->head.process_id);
    if (svm_proc == NULL) {
        goto svm_pro_exit;
    }

    for (i = 0; i < DEVMM_LOCAL_HOST_MAP_TYPE_MAX; i++) {
        shm_sem = devmm_get_shm_sem(svm_proc, map_type[i]);
        ka_task_down(shm_sem);
        /* Message processing and process exit may occur at the same time,
         * and svm_proc destroys after bottom-release when the process exits,
         * so shm_pro_node may have been removed from svm_proc while it still exists,
         * it need to search the shm node in devmm_svm->shm_pro_head.
         */
        ret = devmm_shm_node_ref_dec(put_pages_msg, svm_proc, map_type[i]);
        ka_task_up(shm_sem);
        if (ret == -ESRCH) {
            devmm_svm_proc_put(svm_proc);
            goto svm_pro_exit;
        } else if (ret == 0) {
            devmm_svm_proc_put(svm_proc);
            return 0;
        }
    }
    devmm_svm_proc_put(svm_proc);
    devmm_drv_err("Get share memory failed. (dev_va=0x%llx; devid=%u; ret=%d)\n",
        put_pages_msg->dev_va, put_pages_msg->head.dev_id, ret);
    return ret;

svm_pro_exit:
    for (i = 0; i < DEVMM_LOCAL_HOST_MAP_TYPE_MAX; i++) {
        shm_process_head = devmm_get_shm_pro_head(map_type[i]);
        ka_task_mutex_lock(&shm_process_head->node_lock);
        ret = devmm_shm_node_ref_dec_after_proc_exit(put_pages_msg, shm_process_head);
        ka_task_mutex_unlock(&shm_process_head->node_lock);
        if (ret != 0) {
            devmm_drv_warn("Shm_node has been released. (dev_va=0x%llx; devid=%u; ret=%d; map_type=%u)\n",
                put_pages_msg->dev_va, put_pages_msg->head.dev_id, ret, map_type[i]);
        }
    }

    return 0;
}

struct devmm_chan_handlers_st devmm_channel_msg_processes[DEVMM_CHAN_MAX_ID] = {
    [DEVMM_CHAN_PAGE_FAULT_D2H_ID] =
        {
            devmm_chan_page_fault_d2h_process,
            sizeof(struct devmm_chan_page_fault),
            0,
            0
        },
    [DEVMM_CHAN_QUERY_VAFLGS_D2H_ID] =
        {
            devmm_chan_query_vaflgs_d2h_process,
            sizeof(struct devmm_chan_page_query),
            0,
            0
        },
    [DEVMM_CHAN_CHECK_VA_D2H_ID] =
        {
            devmm_chan_check_va_d2h_process,
            sizeof(struct devmm_chan_check_va),
            0,
            0
        },
    [DEVMM_CHAN_SHM_GET_PAGES_D2H_ID] =
        {
            devmm_chan_shm_get_pages_d2h_process,
            sizeof(struct devmm_chan_shm_getput_pages_d2h),
            0,
            0
        },
    [DEVMM_CHAN_SHM_PUT_PAGES_D2H_ID] =
        {
            devmm_chan_shm_put_pages_d2h_process,
            sizeof(struct devmm_chan_shm_getput_pages_d2h),
            0,
            DEVMM_MSG_NOT_NEED_PROC_MASK
        },
    [DEVMM_CHAN_PROCESS_STATUS_REPORT_D2H_ID] =
        {
            devmm_chan_report_process_status_d2h,
            sizeof(struct devmm_chan_process_status),
            0,
            0
        },
    [DEVMM_CHAN_TARGET_ADDR_P2P_ID] =
        {
            devmm_chan_target_blk_query_pa_process,
            sizeof(struct devmm_chan_target_blk_query),
            sizeof(struct devmm_target_blk),
            DEVMM_MSG_NOT_NEED_PROC_MASK
        }
};

int devmm_notify_device_close_process(struct devmm_svm_process *svm_pro,
    u32 logical_devid, u32 phy_devid, u32 vfid)
{
    struct devmm_chan_close_device chan_close = {{{0}}};
    int ret;

    chan_close.head.process_id.vfid = (u16)vfid;
    chan_close.head.process_id.hostpid = svm_pro->process_id.hostpid;
    chan_close.head.msg_id = DEVMM_CHAN_CLOSE_DEVICE_H2D;
    chan_close.head.dev_id = (u16)phy_devid;
    chan_close.devpid = svm_pro->deviceinfo[logical_devid].devpid;
    ret = devmm_chan_msg_send(&chan_close, sizeof(struct devmm_chan_close_device),
        sizeof(struct devmm_chan_close_device));
    if ((ret != 0) && (ret != -ENOSYS)) {
        /* -ENOSYS: pcie worker is not invoked and the message needs to be sent again.
         * other error codes: device thread may already quited.
         */
        return 0;
    }
    if (chan_close.devpid != DEVMM_SVM_INVALID_PID) {
        return 1;
    }
    return 0;
}

void devmm_svm_free_share_page_msg(struct devmm_svm_process *svm_process, struct devmm_svm_heap *heap,
                                   unsigned long start, u64 real_size, u32 *page_bitmap)
{
    struct devmm_chan_free_pages free_info;
    int share_flag = 0;
    u64 tem_cnt, i;

    tem_cnt = devmm_get_pagecount_by_size(start, real_size, heap->chunk_page_size);
    for (i = 0; i < tem_cnt; i++) {
        if (devmm_page_bitmap_advise_memory_shared(page_bitmap + i)) {
            share_flag = 1;
            break;
        }
    }

    if (share_flag != 0) {
        u32 logic_id, phy_id, vfid;
        free_info.va = start;
        free_info.real_size = real_size;
        free_info.head.msg_id = DEVMM_CHAN_FREE_PAGES_H2D_ID;
        free_info.head.process_id.hostpid = svm_process->process_id.hostpid;
        devmm_get_svm_id(svm_process, page_bitmap, &logic_id, &phy_id, &vfid);
        free_info.head.logical_devid = (u16)logic_id;
        free_info.head.dev_id = (u16)phy_id;
        free_info.head.vfid = (u16)vfid;
        (void)devmm_chan_send_msg_free_pages(&free_info, heap, svm_process, share_flag, DEVMM_FALSE);

        for (i = 0; i < tem_cnt; i++) {
            devmm_page_bitmap_clear_flag(page_bitmap + i, DEVMM_PAGE_ADVISE_MEMORY_SHARED_MASK);
        }
    }
}    /*lint +e629*/
