/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
 * Create: 2025-11-21
 */
#include <linux/version.h>
#include <linux/mm_types.h>
#include <linux/mm.h>
#include <linux/numa.h>

#include "devmm_common.h"
#include "svm_phy_addr_blk_mng.h"
#include "svm_proc_mng.h"
#include "dbl/feature_loader.h"

#if (defined CFG_SOC_PLATFORM_CLOUD_V2) && (defined __aarch64__)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
struct devmm_host_obmm_info {
    ka_mutex_t numa_lock[SVM_MASTER_NUMA_MAX];
};
struct devmm_host_obmm_info *g_obmm_info = NULL;

/* This func is time-consuming operation, may cause performance problem. */
bool devmm_pa_is_local_mem(u64 pa)
{
    return page_is_ram(KA_MM_PFN_DOWN(pa));
}

int devmm_obmm_init(void)
{
    int i;
    g_obmm_info = (struct devmm_host_obmm_info *)devmm_vzalloc_ex(sizeof(struct devmm_host_obmm_info));
    if (g_obmm_info == NULL) {
        devmm_drv_err("Vzalloc obmm_info fail.\n");
        return -ENOMEM;
    }

    for (i = 0; i < SVM_MASTER_NUMA_MAX; i++) {
        ka_task_mutex_init(&g_obmm_info->numa_lock[i]);
    }

    return 0;
}
DECLAER_FEATURE_AUTO_INIT(devmm_obmm_init, FEATURE_LOADER_STAGE_5);

void devmm_obmm_uninit(void)
{
    if (g_obmm_info != NULL) {
        devmm_vfree_ex(g_obmm_info);
        g_obmm_info = NULL;
    }
}
DECLAER_FEATURE_AUTO_UNINIT(devmm_obmm_uninit, FEATURE_LOADER_STAGE_5);

static bool devmm_is_support_fabric_page(void)
{
    static int record_val = -1;
    u32 dev_id = 0; // use 0 check
    u32 host_flag;
    int ret;

    if (record_val >= 0) {
        return (record_val > 0);
    }

    ret = devmm_get_host_phy_mach_flag(dev_id, &host_flag);
    if (ret != 0) {
        devmm_drv_err("get host flag failed!\n");
        return false;
    }
    record_val = (devmm_is_hccs_connect(dev_id) && devmm_is_hccs_vm_scene(dev_id, host_flag) == false);
    return (record_val > 0);
}

bool devmm_support_host_giant_page(void)
{
    return ((g_obmm_info != NULL) && devmm_is_support_fabric_page());
}

static ka_mutex_t *devmm_master_get_lock_by_numa(int numa_id)
{
    if (g_obmm_info == NULL || numa_id < 0 || numa_id >= SVM_MASTER_NUMA_MAX) {
        return NULL;
    }

    return &g_obmm_info->numa_lock[numa_id];
}

static int devmm_master_get_next_numa(int numa)
{
    int next_numa;
    next_numa = next_online_node(numa);
    if (next_numa >= SVM_MASTER_NUMA_MAX) {
        next_numa = first_online_node;
    }
    return next_numa;
}

static void devmm_master_get_pfn_range_for_nid(int nid, u64 *start_pfn, u64 *end_pfn)
{
    if (nid < 0 || nid >= num_possible_nodes() || !node_online(nid)) {
        *start_pfn = *end_pfn = 0U;
        return;
    }

    if (NODE_DATA(nid) == NULL) {
        *start_pfn = *end_pfn = 0U;
        return;
    }

    *start_pfn = NODE_DATA(nid)->node_start_pfn;
    *end_pfn = NODE_DATA(nid)->node_start_pfn + NODE_DATA(nid)->node_spanned_pages;
}

static u64 devmm_master_alloc_interleaving_large_pages(ka_page_t **pages, u64 pg_num, u64 pg_size, u32 gfp_mask)
{
    u64 alloced;
    u64 pa;
    u64 node_offset[DEVMM_S2S_HOST_NODE_NUM] = { 0 };
    u32 stamp = (u32)ka_jiffies;
    u32 i;
    int ret;

    for (alloced = 0; alloced < pg_num;) {
        for (i = 0; i < DEVMM_S2S_HOST_NODE_NUM && alloced < pg_num; i++) {
            u32 node_id = (i + alloced) % DEVMM_S2S_HOST_NODE_NUM;
            u64 hccs_start = devmm_get_host_node_local_addr(node_id);
            u64 hccs_end = hccs_start + DEVMM_S2S_HOST_NODE_MEM_SIZE;

            hccs_start += node_offset[node_id];
            hccs_start = KA_DRIVER_ALIGN(hccs_start, pg_size);
            hccs_end = ALIGN_DOWN(hccs_end, pg_size);
            for (pa = hccs_start; pa < hccs_end; pa += pg_size) {
                u64 start_pfn = __phys_to_pfn(pa);
                u64 end_pfn = __phys_to_pfn(pa + pg_size);
                ka_mutex_t *nlock = NULL;

                devmm_try_cond_resched(&stamp);
                if (!devmm_pa_is_local_mem(pa)) {
                    continue;
                }

                nlock = devmm_master_get_lock_by_numa(pfn_to_nid(start_pfn));
                if (ka_unlikely(nlock == NULL)) {
                    devmm_drv_err("invalid numa id. (numa:%d; max:%d)\n", (int)pfn_to_nid(start_pfn), (int)SVM_MASTER_NUMA_MAX);
                    return alloced;
                }

                ka_task_mutex_lock(nlock);
                ret = alloc_contig_range(start_pfn, end_pfn, MIGRATE_MOVABLE, gfp_mask);
                ka_task_mutex_unlock(nlock);
                if (ret == 0) {
                    pages[alloced] = ka_mm_pfn_to_page(start_pfn);
                    (void)memset_s(page_address(pages[alloced++]), pg_size, 0, pg_size);
                    devmm_drv_debug("alloc interleaving pages. (cpu_id:%u; pg_size:0x%llx pa:%pk)\n", node_id, pg_size, (void *)pa);
                    // set DEVMM_S2S_HOST_NODE_NUM + 1U, indicates that 'alloced' has changed
                    i = DEVMM_S2S_HOST_NODE_NUM + 1U;
                    break;
                }
            }
            node_offset[node_id] = pa - devmm_get_host_node_local_addr(node_id);
        }

        if (i == DEVMM_S2S_HOST_NODE_NUM) { // break if 'alloced' is not changed
            break;
        }
    }

    return alloced;
}

static u64 devmm_master_alloc_numa_large_pages(u32 numa_id, ka_page_t **pages, u64 pg_num, u64 pg_size, u32 gfp_mask)
{
    u64 alloced = 0;
    u64 pa;
    u32 stamp = (u32)ka_jiffies;
    u32 node_id;
    int numa_total, cur_numa;
    int i;
    u64 numa_start_pfn, numa_end_pfn;

    cur_numa = (numa_id == 0) ? ka_system_numa_node_id() : (numa_id - 1U);
    numa_total = num_online_nodes();
    if (cur_numa < 0 || cur_numa >= SVM_MASTER_NUMA_MAX || !node_online(cur_numa)) {
        devmm_drv_err("invalid numa id. (numa:%d; max:%d)\n", cur_numa, (int)SVM_MASTER_NUMA_MAX);
        return 0;
    }

    for (i = 0; i < numa_total && alloced < pg_num; i++) {
        ka_mutex_t *nlock = devmm_master_get_lock_by_numa(cur_numa);
        if (ka_unlikely(nlock == NULL)) {
            devmm_drv_err("invalid numa id or not inited. (numa:%d; max:%d)\n", cur_numa, (int)SVM_MASTER_NUMA_MAX);
            return alloced;
        }
        
        devmm_master_get_pfn_range_for_nid(cur_numa, &numa_start_pfn, &numa_end_pfn);
        for (node_id = 0; node_id < DEVMM_S2S_HOST_NODE_NUM; node_id++) {
            u64 numa_start = __pfn_to_phys(numa_start_pfn);
            u64 numa_end = __pfn_to_phys(numa_end_pfn);
            u64 hccs_start = devmm_get_host_node_local_addr(node_id);
            u64 hccs_end = hccs_start + DEVMM_S2S_HOST_NODE_MEM_SIZE;

            hccs_start = max(numa_start, hccs_start);
            hccs_end = ka_base_min(numa_end, hccs_end);
            hccs_start = KA_DRIVER_ALIGN(hccs_start, pg_size);
            hccs_end = ALIGN_DOWN(hccs_end, pg_size);
            if (hccs_start >= hccs_end) {
                continue;
            }

            ka_task_mutex_lock(nlock);
            for (pa = hccs_start; pa < hccs_end && alloced < pg_num; pa += pg_size) {
                u64 start_pfn = __phys_to_pfn(pa);
                u64 end_pfn = __phys_to_pfn(pa + pg_size);
                devmm_try_cond_resched(&stamp);
                if (!devmm_pa_is_local_mem(pa)) {
                    continue;
                }

                if (alloc_contig_range(start_pfn, end_pfn, MIGRATE_MOVABLE, gfp_mask) == 0) {
                    pages[alloced] = ka_mm_pfn_to_page(start_pfn);
                    (void)memset_s(page_address(pages[alloced++]), pg_size, 0, pg_size);
                    devmm_drv_debug("alloc normal pages. (numa:%u; pg_size:0x%llx pa:%pk)\n", cur_numa, pg_size, (void *)pa);
                }
            }
            ka_task_mutex_unlock(nlock);
        }

        if (numa_id > 0) { // alloc in this numa only, if numa is designated
            break;
        }
        cur_numa = devmm_master_get_next_numa(cur_numa);
    }
    return alloced;
}

static u64 devmm_master_alloc_huge_page_by_cma(u32 numa_id, ka_page_t **pages, u64 pg_num, u32 gfp_mask)
{
    if (numa_id == KA_U32_MAX) {
        return devmm_master_alloc_interleaving_large_pages(pages, pg_num, SVM_MASTER_HUGE_PAGE_SIZE, gfp_mask);
    } else {
        return devmm_master_alloc_numa_large_pages(numa_id, pages, pg_num, SVM_MASTER_HUGE_PAGE_SIZE, gfp_mask);
    }
}

static void devmm_master_free_one_page_by_size(u64 pa, u64 size)
{
#ifndef EMU_ST
    unsigned long start_pfn = __phys_to_pfn(pa);
    unsigned long end_pfn = __phys_to_pfn(pa + size);
    unsigned long nr_pages = end_pfn - start_pfn;
    ka_mutex_t *nlock = devmm_master_get_lock_by_numa(pfn_to_nid(start_pfn));
    if (ka_unlikely(nlock == NULL)) {
        devmm_drv_err("invalid pfn. (pfn:0x%lx)\n", start_pfn);
        free_contig_range(start_pfn, nr_pages);
        return;
    }

    ka_task_mutex_lock(nlock);
    free_contig_range(start_pfn, nr_pages);
    ka_task_mutex_unlock(nlock);
#endif
}

static u64 devmm_master_alloc_giant_page_by_cma(u32 numa_id, ka_page_t **pages, u64 pg_num, u32 gfp_mask)
{
    if (numa_id == KA_U32_MAX) {
        return devmm_master_alloc_interleaving_large_pages(pages, pg_num, SVM_MASTER_GIANT_PAGE_SIZE, gfp_mask);
    } else {
        return devmm_master_alloc_numa_large_pages(numa_id, pages, pg_num, SVM_MASTER_GIANT_PAGE_SIZE, gfp_mask);
    }
}
#else
static u64 devmm_master_alloc_huge_page_by_cma(u32 numa_id, ka_page_t **pages, u64 pg_num, u32 gfp_mask)
{
    return 0;
}

static void devmm_master_free_one_page_by_size(u64 pa, u64 size)
{
}

static u64 devmm_master_alloc_giant_page_by_cma(u32 numa_id, ka_page_t **pages, u64 pg_num, u32 gfp_mask)
{
    return 0;
}

static bool devmm_is_support_fabric_page(void)
{
    return false;
}

#endif
#endif

static void devmm_master_put_one_huge_pages(ka_page_t *page)
{
#ifndef EMU_ST
    ka_mm_put_page(page);
#else
    __ka_mm_free_pages(page, ka_mm_get_order(SVM_MASTER_HUGE_PAGE_SIZE));
#endif
}

static void devmm_master_free_one_huge_pages(struct devmm_phy_addr_attr *attr, ka_page_t *page)
{
#if (defined CFG_SOC_PLATFORM_CLOUD_V2) && (defined __aarch64__)
    u64 pa = ka_mm_page_to_phys(page);
    if (attr->mem_type == MEM_P2P_DDR_TYPE && devmm_is_support_fabric_page()) {
        devmm_master_free_one_page_by_size(pa, SVM_MASTER_HUGE_PAGE_SIZE);
    } else {
        devmm_master_put_one_huge_pages(page);
    }
#else
    devmm_master_put_one_huge_pages(page);
#endif
}

void devmm_master_free_huge_pages(struct devmm_phy_addr_attr *attr, ka_page_t **pages, u64 pg_num)
{
    u32 stamp = (u32)ka_jiffies;
    u64 i;

    for (i = 0; i < pg_num; i++) {
        devmm_master_free_one_huge_pages(attr, pages[i]);
        pages[i] = NULL;
        devmm_try_cond_resched(&stamp);
    }
}

static bool devmm_is_specified_numa(u32 numa_id)
{
    return !((numa_id == -1) || (numa_id == 0));
}

static u64 devmm_master_alloc_normal_large_pages(u32 numa, ka_page_t **pages, u64 pg_num, u64 pg_size, u32 gfp_mask)
{
    u32 stamp = (u32)ka_jiffies;
    u32 numa_id;
    u64 i;

    numa_id = (devmm_is_specified_numa(numa)) ? (numa - 1) : KA_NUMA_NO_NODE;
    for (i = 0; i < pg_num; i++) {
        pages[i] = __ka_alloc_pages_node(numa_id, gfp_mask, ka_mm_get_order(pg_size));
        if (pages[i] == NULL) {
            return i;
        }
#ifndef EMU_ST
        (void)memset_s(ka_mm_page_address(pages[i]), pg_size, 0, pg_size);
#endif
        devmm_try_cond_resched(&stamp);
    }
    return i;
}

int devmm_master_alloc_huge_pages(struct devmm_phy_addr_attr *attr, ka_page_t **pages, u64 pg_num)
{
    u64 alloced = 0;
    u32 gfp_mask = devmm_get_alloc_mask(true);
    gfp_mask |= (devmm_is_specified_numa(attr->numa_id) ? __KA_GFP_THISNODE : 0);
#if (defined CFG_SOC_PLATFORM_CLOUD_V2) && (defined __aarch64__)
    if (attr->mem_type == MEM_P2P_DDR_TYPE && devmm_is_support_fabric_page()) {
        alloced = devmm_master_alloc_huge_page_by_cma(attr->numa_id, pages, pg_num, gfp_mask);
    } else {
        alloced = devmm_master_alloc_normal_large_pages(attr->numa_id, pages, pg_num,
            SVM_MASTER_HUGE_PAGE_SIZE, gfp_mask);
    }
#else
    alloced = devmm_master_alloc_normal_large_pages(attr->numa_id, pages, pg_num,
        SVM_MASTER_HUGE_PAGE_SIZE, gfp_mask);
#endif
    if (alloced != pg_num) {
        devmm_master_free_huge_pages(attr, pages, alloced);
        return -ENOMEM;
    }
    return 0;
}

static void devmm_master_free_one_giant_pages(struct devmm_phy_addr_attr *attr, ka_page_t *page)
{
#if (defined CFG_SOC_PLATFORM_CLOUD_V2) && (defined __aarch64__)
    u64 pa = ka_mm_page_to_phys(page);
    if (attr->mem_type == MEM_P2P_DDR_TYPE && devmm_is_support_fabric_page()) {
        devmm_master_free_one_page_by_size(pa, SVM_MASTER_GIANT_PAGE_SIZE);
    }
#else
#ifdef EMU_ST
        __ka_mm_free_pages(page, ka_mm_get_order(SVM_MASTER_GIANT_PAGE_SIZE));
#endif
#endif
}

void devmm_master_free_giant_pages(struct devmm_phy_addr_attr *attr, ka_page_t **pages, u64 pg_num)
{
    u32 stamp = (u32)ka_jiffies;
    u64 i;

    for (i = 0; i < pg_num; i++) {
        devmm_master_free_one_giant_pages(attr, pages[i]);
        pages[i] = NULL;
        devmm_try_cond_resched(&stamp);
    }
}

int devmm_master_alloc_giant_pages(struct devmm_phy_addr_attr *attr, ka_page_t **pages, u64 pg_num)
{
    u64 alloced = 0;
#if (defined CFG_SOC_PLATFORM_CLOUD_V2) && (defined __aarch64__)
    if (attr->mem_type == MEM_P2P_DDR_TYPE && devmm_is_support_fabric_page()) {
        alloced = devmm_master_alloc_giant_page_by_cma(attr->numa_id, pages, pg_num, devmm_get_alloc_mask(true));
    } else {
        alloced = 0;
    }
#else
#ifdef EMU_ST
    alloced = devmm_master_alloc_normal_large_pages(attr->numa_id, pages, pg_num, SVM_MASTER_GIANT_PAGE_SIZE,
        devmm_get_alloc_mask(true));
#endif
#endif
    if (alloced != pg_num) {
        devmm_master_free_giant_pages(attr, pages, alloced);
        return -ENOMEM;
    }
    return 0;
}