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

#ifndef QUEUE_UT
#if !defined(EMU_ST)
#ifndef DRV_HOST
#include "linux/share_pool.h"
#endif
#endif
#include "ka_memory_pub.h"
#include "ka_base_pub.h"
#include "ka_common_pub.h"

#include "svm_interface.h"
#include "queue_module.h"
#include "queue_fops.h"
#include "queue_channel.h"
#include "kernel_version_adapt.h"
#include "queue_dma.h"

#define QUEUE_WAKEUP_TIMEINTERVAL 5000 /* 5s */

#define QUEUE_GET_2M_PAGE_NUM   512

#define QUEUE_DMA_RETRY_CNT     (1000 * 50)
#define QUEUE_DMA_WAIT_MIN_TIME 100
#define QUEUE_DMA_WAIT_MAX_TIME 200
#ifdef CFG_SOC_PLATFORM_MINI_DMA
#define QUEUE_DMA_MAX_NODE_CNT  (0x8000 - 0x200)
#else
#define QUEUE_DMA_MAX_NODE_CNT  32768
#endif


void *queue_kvalloc(u64 size, unsigned int flags)
{
    void *ptr = queue_drv_kmalloc(size, KA_GFP_ATOMIC | __KA_GFP_NOWARN | __KA_GFP_ACCOUNT | flags);
    if (ptr == NULL) {
        ptr = queue_drv_vmalloc(size, KA_GFP_KERNEL | __KA_GFP_ACCOUNT | flags, KA_PAGE_KERNEL);
    }

    return ptr;
}

void queue_kvfree(const void *ptr)
{
    if (ka_mm_is_vmalloc_addr(ptr)) {
        queue_drv_vfree(ptr);
    } else {
        queue_drv_kfree(ptr);
    }
}

/* Only use in agentSmmu scene */
STATIC int queue_pa_to_pm_pa(u32 devid, u64 *paddr, u64 num, u64 *out_paddr)
{
#ifdef DRV_HOST
    u64 i;
    int ret;

    for (i = 0; i < num;) {
        u64 real_num  = ka_base_min_t(u64, num - i, DEVDRV_AGENT_SMMU_SUPPORT_MAX_NUM);
        ret = devdrv_smmu_iova_to_phys(devid, (ka_dma_addr_t *)(uintptr_t)&paddr[i],
            real_num, (phys_addr_t *)(uintptr_t)&out_paddr[i]);
        if (ret != 0) {
            queue_err("Can not transfer iova to phys. (devid=%u; i=%llu; num=%llu)\n", devid, i, num);
            return ret;
        }
        i += real_num;
    }
#endif
    return 0;
}

STATIC int queue_pa_blks_to_pm_pa_blks(u32 devid, struct queue_dma_block *blks, u64 num, struct queue_dma_block *out_blks)
{
    u64 *iova_list = (u64 *)queue_kvalloc(DEVDRV_AGENT_SMMU_SUPPORT_MAX_NUM * sizeof(u64), 0);
    u64 i, j;
    int ret;

    if (iova_list == NULL) {
        queue_err("kmalloc failed.\n");
        return -ENOMEM;
    }

    for (i = 0; i < num;) {
        u64 real_num = ka_base_min_t(u64, num - i, DEVDRV_AGENT_SMMU_SUPPORT_MAX_NUM);

        for (j = 0; j < real_num; ++j) {
            iova_list[j] = (u64)blks[i + j].dma;
        }

        ret = queue_pa_to_pm_pa(devid, iova_list, real_num, iova_list);
        if (ret != 0) {
            queue_kvfree(iova_list);
            return ret;
        }

        for (j = 0; j < real_num; ++j) {
            out_blks[i + j].dma = (ka_dma_addr_t)iova_list[j];
        }
        i += real_num;
    }
    queue_kvfree(iova_list);
    return 0;
}

STATIC u64 queue_get_page_num(u64 addr, u64 addr_len)
{
    u64 align_addr_len, page_num;
    align_addr_len = ((addr & (KA_MM_PAGE_SIZE - 1)) + addr_len);
    page_num = align_addr_len / KA_MM_PAGE_SIZE;
    if ((align_addr_len & (KA_MM_PAGE_SIZE - 1)) != 0) {
        page_num++;
    }
    return page_num;
}

STATIC int queue_alloc_dma_blks(struct queue_dma_list *dma_list, bool dma_sva_enable)
{
    u64 page_num;

    if (dma_sva_enable) {
        page_num = 1; // Only one blk when using virtual address copy.
    } else {
        page_num = queue_get_page_num(dma_list->va, dma_list->len);
    }
    dma_list->page = (ka_page_t **)queue_kvalloc(page_num * sizeof(ka_page_t *), 0);
    if (dma_list->page == NULL) {
        queue_err("kmalloc %llu failed.\n", page_num);
        return -ENOMEM;
    }
    dma_list->page_num = page_num;

    dma_list->blks = (struct queue_dma_block *)queue_kvalloc(page_num * sizeof(struct queue_dma_block), 0);
    if (dma_list->blks == NULL) {
        queue_err("kmalloc %llu failed.\n", page_num);
        dma_list->page_num = 0;
        queue_kvfree(dma_list->page);
        dma_list->page = NULL;
        return -ENOMEM;
    }
    dma_list->blks_num = page_num;

    return 0;
}

STATIC void queue_free_dma_blks(struct queue_dma_list *dma_list)
{
    queue_kvfree(dma_list->page);
    dma_list->page = NULL;
    queue_kvfree(dma_list->blks);
    dma_list->blks = NULL;
}

void queue_try_cond_resched(unsigned long *pre_stamp)
{
    unsigned long timeinterval = ka_system_jiffies_to_msecs(ka_jiffies - *pre_stamp);

    if (timeinterval > QUEUE_WAKEUP_TIMEINTERVAL) {
        ka_task_cond_resched();
        *pre_stamp = ka_jiffies;
    }
}

STATIC void queue_fill_dma_blks_sva(struct queue_dma_list *dma_list)
{
#if !defined(EMU_ST)
    // Only one blk when using virtual address copy.
    u64 aligned_va, aligned_size;
    aligned_va = ka_base_round_down(dma_list->va, KA_MM_PAGE_SIZE);
    aligned_size = ka_base_round_up(dma_list->len + (dma_list->va - aligned_va), KA_MM_PAGE_SIZE);
    dma_list->blks[0].dma = aligned_va; /* tmp store pa */
    dma_list->blks[0].sz = aligned_size;
#endif
    return;
}

STATIC void queue_put_user_pages(ka_page_t **pages, u64 page_num, u64 unpin_num)
{
    unsigned long stamp;
    u64 i;

    if ((unpin_num == 0) || (unpin_num > page_num)) {
        return;
    }

    stamp = ka_jiffies;
    for (i = 0; i < unpin_num; i++) {
        if (pages[i] != NULL) {
            ka_mm_put_page(pages[i]);
            pages[i] = NULL;
        }
        queue_try_cond_resched(&stamp);
    }
}

STATIC int queue_get_user_pages_fast(u64 va, u64 page_num, ka_page_t **pages)
{
    u64 got_num, remained_num, tmp_va;
    unsigned long stamp = ka_jiffies;
    int expected_num, tmp_num;

    for (got_num = 0; got_num < page_num;) {
        tmp_va = va + got_num * KA_MM_PAGE_SIZE;
        remained_num = page_num - got_num;
        expected_num = (int)((remained_num > QUEUE_GET_2M_PAGE_NUM) ? QUEUE_GET_2M_PAGE_NUM : remained_num);
        tmp_num = ka_mm_get_user_pages_fast(tmp_va, expected_num, KA_FOLL_WRITE, &pages[got_num]);
        got_num += (u64)((tmp_num > 0) ? (u32)tmp_num : 0);
        if (tmp_num != expected_num) {
            queue_err("Get_user_pages_fast fail. (bufPtr=0x%pK; already_got_num=%llu; get_va=0x%pK; "
                "expected_num=%d; get_num_or_ret=%d)\n",
                (void *)(uintptr_t)va, got_num, (void *)(uintptr_t)tmp_va, expected_num, tmp_num);
            goto err_exit;
        }
        queue_try_cond_resched(&stamp);
    }

    return 0;

err_exit:
    queue_put_user_pages(pages, page_num, got_num);
    return -EFBIG;
}

STATIC bool is_svm_addr(ka_vm_area_struct_t *vma, u64 addr)
{
#if !defined(EMU_ST)
#ifndef DRV_HOST
    if (mg_is_sharepool_addr(addr)) {
        return false;
    }
#endif
#endif
    return ((ka_mm_get_vm_flags(vma) & KA_VM_PFNMAP) != 0);
}

STATIC int queue_get_user_pages(struct queue_dma_list *dma_list)
{
    ka_vm_area_struct_t *vma = NULL;
    bool svm_flag;
    int ret;

    ka_task_down_read(ka_mm_get_mmap_sem(QUEUE_CURRENT_MM));
    vma = ka_mm_find_vma(ka_task_get_current_mm(), dma_list->va);
    if ((vma == NULL) || (dma_list->va < ka_mm_get_vm_start(vma))) {
        ka_task_up_read(ka_mm_get_mmap_sem(QUEUE_CURRENT_MM));
        queue_err("Get vma failed. (va=0x%pK; len=0x%llx; page_num=%llu)\n",
            (void *)(uintptr_t)dma_list->va, dma_list->len, dma_list->page_num);
        return -EFBIG;
    }
    
    svm_flag = is_svm_addr(vma, dma_list->va);
    ka_task_up_read(ka_mm_get_mmap_sem(QUEUE_CURRENT_MM));

    /* memory remap by remap_pfn_rang, get user page fast can not get page addr */
    if (svm_flag == true) {
        ret = devmm_get_pages_list(QUEUE_CURRENT_MM, dma_list->va, dma_list->page_num, dma_list->page);
    } else {
        ret = queue_get_user_pages_fast(dma_list->va, dma_list->page_num, dma_list->page);
    }

    return ret;
}

STATIC int queue_fill_dma_blks(struct queue_dma_list *dma_list, bool dma_sva_enable)
{
    int ret;
    u64 i;
    if (dma_sva_enable) {
        queue_fill_dma_blks_sva(dma_list);
        return 0;
    }
    ret = queue_get_user_pages(dma_list);
    if (ret != 0) {
        queue_err("Get_user_pages failed. (va=0x%pK; len=0x%llx; page_num=%llu; ret=%d)\n",
            (void *)(uintptr_t)dma_list->va, dma_list->len, dma_list->page_num, ret);
        return -EFBIG;
    }

    for (i = 0; i < dma_list->page_num; i++) {
        dma_list->blks[i].dma = (ka_dma_addr_t)ka_mm_page_to_phys(dma_list->page[i]); /* tmp store pa */
        dma_list->blks[i].sz = KA_MM_PAGE_SIZE;
    }

    return 0;
}

STATIC int queue_map_dma_blks(ka_device_t *dev, struct queue_dma_list *dma_list)
{
    unsigned long stamp = ka_jiffies;
    ka_page_t *page = NULL;
    u64 i, j;

    for (i = 0; i < dma_list->blks_num; i++) {
        page = ka_mm_pfn_to_page(KA_MM_PFN_DOWN(dma_list->blks[i].dma));
        dma_list->blks[i].dma = devdrv_dma_map_page(dev, page, 0, dma_list->blks[i].sz, KA_DMA_BIDIRECTIONAL);
        if (ka_mm_dma_mapping_error(dev, dma_list->blks[i].dma) != 0) {
            queue_err("Dma mapping error. (dma_idx=%llu; ret=%d)\n", i, ka_mm_dma_mapping_error(dev, dma_list->blks[i].dma));
            goto map_dma_blks_err;
        }
        queue_try_cond_resched(&stamp);
    }

    return 0;
map_dma_blks_err:
    stamp = ka_jiffies;
    for (j = 0; j < i; j++) {
        devdrv_dma_unmap_page(dev, dma_list->blks[j].dma, dma_list->blks[j].sz, KA_DMA_BIDIRECTIONAL);
        queue_try_cond_resched(&stamp);
    }

    return -EIO;
}

STATIC void queue_clear_dma_blks(struct queue_dma_list *dma_list)
{
#ifndef CFG_FEATURE_SURPORT_PCIE_DMA_SVA
    unsigned long stamp = ka_jiffies;
    u64 i;

    for (i = 0; i < dma_list->page_num; i++) {
        ka_mm_put_page(dma_list->page[i]);
        queue_try_cond_resched(&stamp);
    }
#endif
    return;
}

STATIC void queue_merg_dma_blks(struct queue_dma_block *blks, u64 idx, u64 *merg_idx)
{
    struct queue_dma_block *merg_blks = blks;
    u64 j = *merg_idx;
    u64 i = idx;

    if ((i >= 1) && (blks[i - 1].dma + blks[i - 1].sz == blks[i].dma)) {
        merg_blks[j - 1].sz += blks[i].sz;
    } else {
        merg_blks[j].sz = blks[i].sz;
        merg_blks[j].dma = blks[i].dma;
        j++;
    }
    *merg_idx = j;
}

STATIC void queue_unmap_dma_blks(ka_device_t *dev, struct queue_dma_list *dma_list)
{
    unsigned long stamp = ka_jiffies;
    u64 i;

    for (i = 0; i < dma_list->blks_num; i++) {
#ifndef CFG_FEATURE_SURPORT_PCIE_DMA_SVA
        devdrv_dma_unmap_page(dev, dma_list->blks[i].dma, dma_list->blks[i].sz, KA_DMA_BIDIRECTIONAL);
#endif
        queue_try_cond_resched(&stamp);
    }
}
bool queue_get_dma_sva_enable(void)
{
#ifdef CFG_FEATURE_SURPORT_PCIE_DMA_SVA
    return true;
#else
    return false;
#endif
}
int queue_make_dma_list(ka_device_t *dev, bool hccs_vm_flag, u32 dev_id, struct queue_dma_list *dma_list)
{
    u64 i, merg_idx;
    int ret;
    bool dma_sva_enable = queue_get_dma_sva_enable();

    if (dma_list->va == 0 || dma_list->dma_flag == false) {
        dma_list->blks_num = 0;
        return 0;
    }
    ret = queue_alloc_dma_blks(dma_list, dma_sva_enable);
    if (ret != 0) {
        queue_err("alloc dma blks failed, ret=%d.\n", ret);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    ret = queue_fill_dma_blks(dma_list, dma_sva_enable);
    if (ret != 0) {
        queue_err("fill dma blks failed, ret=%d.\n", ret);
        goto free_dma_list;
    }

    if (hccs_vm_flag) {
         ret = queue_pa_blks_to_pm_pa_blks(dev_id, dma_list->blks, dma_list->page_num, dma_list->blks);
         if (ret != 0) {
             queue_err("dma blks pa failed, ret=%d.\n", ret);
             goto clear_dma_blks;
         }
    }

    for (merg_idx = 0, i = 0; i < dma_list->page_num; i++) {
        queue_merg_dma_blks(dma_list->blks, i, &merg_idx);
    }
    dma_list->blks_num = merg_idx;
    if ((dma_sva_enable == false) && (hccs_vm_flag == false)) {
        ret = queue_map_dma_blks(dev, dma_list);
        if (ret != 0) {
            queue_err("map dma blks failed, ret=%d.\n", ret);
            goto clear_dma_blks;
        }
    }

    return 0;
clear_dma_blks:
    queue_clear_dma_blks(dma_list);
free_dma_list:
    queue_free_dma_blks(dma_list);
    return DRV_ERROR_MEMORY_OPT_FAIL;
}

void queue_clear_dma_list(ka_device_t *dev, bool hccs_vm_flag, struct queue_dma_list *dma_list)
{
    if (dma_list->blks_num == 0) {
        return;
    }
    if (hccs_vm_flag == false) {
        queue_unmap_dma_blks(dev, dma_list);
    }
    queue_clear_dma_blks(dma_list);
    queue_free_dma_blks(dma_list);
}

int queue_dma_sync_link_copy(u32 dev_id, struct devdrv_dma_node *dma_node, u64 dma_node_num)
{
    struct devdrv_dma_node *copy_node = dma_node;
    u64 already_copy_num, left_node_num, max_per_num;
    u32 copy_num;
    int retry_cnt = 0;
    int ret = 0;
    unsigned long stamp = ka_jiffies;
    unsigned long timeinterval;

    max_per_num = QUEUE_DMA_MAX_NODE_CNT;
    for (already_copy_num = 0; already_copy_num < dma_node_num;) {
        left_node_num = dma_node_num - already_copy_num;
        copy_num = (u32)ka_base_min(left_node_num, max_per_num);
        ret = devdrv_dma_sync_link_copy(dev_id, DEVDRV_DMA_DATA_TRAFFIC, DEVDRV_DMA_WAIT_INTR,
            copy_node, copy_num);
        /* dma queue is full, delay resubmit */
        if ((ret == -ENOSPC) && (retry_cnt < QUEUE_DMA_RETRY_CNT)) {
            ka_system_usleep_range(QUEUE_DMA_WAIT_MIN_TIME, QUEUE_DMA_WAIT_MAX_TIME);
            retry_cnt++;
            continue;
        }

        if (ret != 0) {
            queue_err("Devdrv_dma_sync_link_copy fail. (dev_id=%u; copy_num=%d, finish_num=%llu; node_cnt=%llu; ret=%d)\n",
                dev_id, copy_num, already_copy_num, dma_node_num, ret);
            return ret;
        }
        already_copy_num += copy_num;
        copy_node = copy_node + copy_num;
    }
    timeinterval= ka_system_jiffies_to_msecs(ka_jiffies - stamp);
    if (timeinterval > QUEUE_WAKEUP_TIMEINTERVAL) {
        queue_warn("Devdrv_dma_sync_link_copy too long. (dev_id=%u; timeinterval=%lu, retry_cnt=%d; dma_node_num=%llu)\n",
            dev_id, timeinterval, retry_cnt, dma_node_num);
    }

    return ret;
}
#else
int queue_dma_ut(void)
{
    return 0;
}
#endif
