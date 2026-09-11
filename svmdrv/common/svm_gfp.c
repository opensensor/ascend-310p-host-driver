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
 * Create: 2023-07-25
 */

#include "devmm_adapt.h"
#include "devmm_common.h"
#include "svm_define.h"
#include "svm_mem_split.h"
#include "svm_cgroup_mng.h"
#include "devmm_mem_alloc_interface.h"
#include "svm_proc_gfp.h"
#include "ka_memory_pub.h"

#define DEVMM_ALLOC_CONT_PAGES_MAX_NUM      512ULL

static void devmm_update_alloc_nid_info(u32 devid, u32 vfid, int nids[], u32 *nid_num)
{
    if (devmm_is_support_update_numa_order() == 0) {
        return;
    }

    /* only support numa os, not support numa p2p&ts */
    if (devmm_is_normal_node(nids[0]) == false) {
        return;
    }

    /* numa info
     * devid    1p:0   2P:master,slave
     * numa id
     * OS DDR   0      0(master),1(slave)
     * P2P DDR  1      2(master),3(slave)
     * TS DDR   2      4(master),5(slave)
     */
    if (vfid == 0) {
        int before_os_numa = 0;
        int before_ts_numa = 2;

        /* in PM, update numa alloc order: os(0)->p2p(1)->os(2)->ts(3) */
        nids[3] = nids[before_ts_numa]; /* 3:ts numa */
        nids[2] = nids[before_os_numa]; /* 2:os numa */
        (*nid_num)++;
        devmm_alloc_numa_info_init(devid, vfid, nids, nid_num);
    } else {
        int temp;

        /* in calculation split scene, update numa alloc order: p2p(0)->os(1)->ts(2) */
        temp = nids[0];
        nids[0] = nids[1];
        nids[1] = temp;
    }
}

static void devmm_clear_single_page(ka_page_t *pg, u64 page_size)
{
    int ret = devmm_sdma_page_kernel_memzero(pg, page_size);
    if (ret == 0) {
        return;
    }
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 17, 0)
    if (ka_mm_PagePoisoned(pg) == 0) {
        (void)memset_s(ka_mm_page_address(pg), page_size, 0, page_size);
    }
#else
    (void)memset_s(ka_mm_page_address(pg), page_size, 0, page_size);
#endif
}

static void devmm_clear_normal_single_page(ka_page_t *pg)
{
    devmm_clear_single_page(pg, KA_MM_PAGE_SIZE);
}

static void devmm_clear_compound_page(ka_page_t *pg)
{
    ka_page_t *head_page = ka_mm_compound_head(pg);

    devmm_clear_single_page(head_page, devmm_kpg_size(head_page));
}

static void devmm_clear_huge_page(ka_page_t *pg)
{
    devmm_clear_compound_page(pg);
}

static void devmm_page_ref_dec(ka_page_t *pg, void (*clear_func)(ka_page_t *pg),
    void (*dec_func)(ka_page_t *pg), void (*free_func)(ka_page_t *pg), bool is_already_clear)
{
    int ref;

    ka_mm_lock_page(pg);
    ref = ka_mm_page_count(pg);
    if (ref > 1) {
        dec_func(pg);
        ka_mm_unlock_page(pg);
    } else {
        ka_mm_unlock_page(pg);
        /* Clear user data for security. */
        if (is_already_clear == false) {
            clear_func(pg);
        }
        free_func(pg);
    }
}

static void devmm_free_single_page(ka_page_t *pg)
{
    __ka_mm_free_page(pg);
}

static void devmm_put_page(ka_page_t *pg)
{
    ka_mm_put_page(pg);
}

static void devmm_normal_single_page_ref_dec(ka_page_t *pg, bool is_already_clear)
{
    devmm_page_ref_dec(pg, devmm_clear_normal_single_page, devmm_free_single_page, devmm_free_single_page,
        is_already_clear);
}

static void devmm_huge_page_ref_dec(ka_page_t *pg, bool is_already_clear)
{
    /* Keep the status quo, call put_page. */
    devmm_page_ref_dec(pg, devmm_clear_huge_page, devmm_put_page, devmm_hugetlb_free_hugepage_ex, is_already_clear);
}

static void devmm_compound_page_ref_dec(ka_page_t *pg, bool is_already_clear)
{
    devmm_page_ref_dec(pg, devmm_clear_compound_page, devmm_put_page, devmm_put_page, is_already_clear);
}

static void devmm_free_normal_page(struct devmm_phy_addr_attr *attr, ka_page_t *pg)
{
    int nid = ka_mm_page_to_nid(pg);

    if (PageCompound(pg) != 0) {
        devmm_compound_page_ref_dec(pg, attr->is_already_clear);
    } else {
        devmm_normal_single_page_ref_dec(pg, attr->is_already_clear);
    }
    devmm_normal_free_mem_size_add(attr->devid, attr->vfid, nid, 1);
}

static void devmm_free_normal_pages(struct devmm_phy_addr_attr *attr, ka_page_t **pages, u64 pg_num)
{
    u32 stamp = (u32)ka_jiffies;
    u64 i;

    for (i = 0; i < pg_num; i++) {
        if (pages[i] != NULL) {
            devmm_free_normal_page(attr, pages[i]);
            pages[i] = NULL;
        }
        devmm_try_cond_resched(&stamp);
    }
}

static void devmm_free_huge_page(struct devmm_phy_addr_attr *attr, ka_page_t *hpage, bool is_giant_page)
{
    int nid = ka_mm_page_to_nid(hpage);
    u32 flag = devmm_get_hugetlb_alloc_flag(hpage);
    u64 pg_num = (is_giant_page) ? DEVMM_GIANT_TO_HUGE_PAGE_NUM : 1;

    devmm_huge_page_ref_dec(hpage, attr->is_already_clear);
    devmm_huge_free_mem_size_add(attr->devid, attr->vfid, nid, pg_num, flag);
}

static void devmm_free_huge_pages(struct devmm_phy_addr_attr *attr, ka_page_t **pages, u64 pg_num)
{
    u32 stamp = (u32)ka_jiffies;
    bool is_giant_page = false;
    u64 i;

    is_giant_page = devmm_is_giant_page(pages, pg_num);
    for (i = 0; i < pg_num; i++) {
        if ((is_giant_page == false) || ((i % DEVMM_GIANT_TO_HUGE_PAGE_NUM) == 0)) {
            if (pages[i] != NULL) {
                devmm_free_huge_page(attr, pages[i], is_giant_page);
            }
            pages[i] = NULL;
        }
        devmm_try_cond_resched(&stamp);
    }
}

void devmm_free_pages(struct devmm_phy_addr_attr *attr, ka_page_t **pages, u64 pg_num)
{
    if (attr->pg_type == DEVMM_HUGE_PAGE_TYPE) {
        devmm_free_huge_pages(attr, pages, pg_num);
    } else {
        devmm_free_normal_pages(attr, pages, pg_num);
    }
}

static void devmm_get_sub_pages_from_normal_high_level_page(ka_page_t *page,
    u32 order, ka_page_t **pages, u64 pg_num)
{
    ka_page_t *pg = NULL;
    u64 alloced_page_num = 1ull << order;
    u64 i;

    if (order != 0) {
        ka_mm_split_page(page, order);
    }
    for (i = 0, pg = page; i < pg_num; pg++, i++) {
        pages[i] = pg;
    }
    for (; i < alloced_page_num; pg++, i++) {
        __ka_mm_free_page(pg);
    }
}

static void devmm_get_sub_pages_from_compound_page(ka_page_t *compound_page,
    u32 order, ka_page_t **pages, u64 pg_num)
{
    ka_page_t *pg = NULL;
    u64 i;

    for (i = 0, pg = compound_page; i < pg_num; pg++, i++) {
        pages[i] = pg;
    }

    for (i = 1; i < pg_num; i++) {
        ka_mm_get_page(pages[i]);
    }
}

static void devmm_get_sub_pages(struct devmm_phy_addr_attr *attr,
    ka_page_t *page, u32 order, ka_page_t **out_pages, u64 pg_num)
{
    if (PageCompound(page) != 0) {
        devmm_get_sub_pages_from_compound_page(page, order, out_pages, pg_num);
    } else {
        devmm_get_sub_pages_from_normal_high_level_page(page, order, out_pages, pg_num);
    }
}

static int _devmm_alloc_pages_node(struct devmm_phy_addr_attr *attr,
    int nid, ka_page_t **pages, u64 pg_num)
{
    ka_page_t *page = NULL;
    u32 order = (u32)ka_mm_get_order(pg_num << KA_MM_PAGE_SHIFT);
    u32 flag = devmm_get_alloc_mask(attr->is_compound_page);
    int ret;

    ret = devmm_normal_free_mem_size_sub(attr->devid, attr->vfid, nid, pg_num);
    if (ret != 0) {
        devmm_drv_debug("Not enough normal free mem. (devid=%u; vfid=%u; nid=%d; pg_num=%llu)\n",
            attr->devid, attr->vfid, nid, pg_num);
        return ret;
    }

    /* cannot use ka_alloc_pages_node, because ka mem stats not include normal page */
    page = __ka_alloc_pages_node(nid, flag, order);
    if (page == NULL) {
        devmm_normal_free_mem_size_add(attr->devid, attr->vfid, nid, pg_num);
        return -ENOMEM;
    }

    devmm_get_sub_pages(attr, page, order, pages, pg_num);
    return 0;
}

static ka_page_t *devmm_alloc_pages_node(struct devmm_phy_addr_attr *attr,
    int *latest_nid, int nids[], u32 nid_num)
{
    ka_page_t *pg = NULL;
    bool try_alloc = false;
    int ret, i;

    ret = _devmm_alloc_pages_node(attr, *latest_nid, &pg, 1);
    if (ret == 0) {
        return pg;
    }

    for (i = 0; i < nid_num; i++) {
        /* node os maybe alloc twice, controlled by try_alloc */
        if ((nids[i] == *latest_nid) && (try_alloc == false)) {
            try_alloc = true;
            continue;
        }
        ret = _devmm_alloc_pages_node(attr, nids[i], &pg, 1);
        if (ret == 0) {
            *latest_nid = nids[i];
            return pg;
        }
    }

    return NULL;
}

static int _devmm_alloc_normal_pages(struct devmm_phy_addr_attr *attr,
    int nids[], u32 nid_num, ka_page_t **pages, u64 pg_num)
{
    int latest_nid = nids[0];
    u32 stamp = (u32)ka_jiffies;
    u64 i;

    for (i = 0; i < pg_num; i++) {
        pages[i] = devmm_alloc_pages_node(attr, &latest_nid, nids, nid_num);
        if (pages[i] == NULL) {
            devmm_free_normal_pages(attr, pages, i);
            return -ENOMEM;
        }
        devmm_try_cond_resched(&stamp);
    }

    return 0;
}

static int devmm_alloc_continuous_pages(struct devmm_phy_addr_attr *attr,
    int nids[], u32 nid_num, ka_page_t **pages, u64 pg_num)
{
    int ret, i;

    /* To simplify the code, pg_num should be power of 2. */
    if (attr->is_compound_page && (is_power_of_2(pg_num) == false)) {
        return -EINVAL;
    }

    for (i = 0; i < nid_num; i++) {
        ret = _devmm_alloc_pages_node(attr, nids[i], pages, pg_num);
        if (ret == 0) {
            return 0;
        }
    }

    return -ENOMEM;
}

/* Alloc continuous pages every DEVMM_ALLOC_CONT_PAGES_MAX_NUM, and return got_num. */
static u64 _devmm_try_alloc_continuous_pages(struct devmm_phy_addr_attr *attr,
    int nids[], u32 nid_num, ka_page_t **pages, u64 pg_num)
{
    u32 stamp = (u32)ka_jiffies;
    u64 num, i = 0;
    int ret;

    for (i = 0; i < pg_num; i += num) {
        /* free_size < thres: disable threshold, the next alloc should enable threshold
         * to keep os->p2p->os->ts numa order
         */
        devmm_alloc_numa_enable_threshold(attr->devid, attr->vfid, nids[0]);
        num = ka_base_min(DEVMM_ALLOC_CONT_PAGES_MAX_NUM, (pg_num - i));
        ret = devmm_alloc_continuous_pages(attr, nids, nid_num, &pages[i], num);
        if (ret != 0) {
            return i;
        }
        devmm_try_cond_resched(&stamp);
    }

    return pg_num;
}

/* The returned pages is not necessarily continuous, but is as continuous as possible. */
static int devmm_try_alloc_continuous_pages(struct devmm_phy_addr_attr *attr,
    int nids[], u32 nid_num, ka_page_t **pages, u64 pg_num)
{
    u64 got_num = 0;
    int ret;

    got_num = _devmm_try_alloc_continuous_pages(attr, nids, nid_num, pages, pg_num);
    if (got_num == pg_num) {
        return 0;
    }

    ret = _devmm_alloc_normal_pages(attr, nids, nid_num, &pages[got_num], pg_num - got_num);
    if (ret != 0) {
        devmm_free_normal_pages(attr, pages, got_num);
    }

    return ret;
}

static int devmm_alloc_normal_pages(struct devmm_phy_addr_attr *attr,
    int nids[], u32 nid_num, ka_page_t **pages, u64 pg_num)
{
    if (attr->is_continuous) {
        return devmm_alloc_continuous_pages(attr, nids, nid_num, pages, pg_num);
    } else {
        return devmm_try_alloc_continuous_pages(attr, nids, nid_num, pages, pg_num);
    }
}

static ka_page_t *devmm_alloc_hpage(struct devmm_phy_addr_attr *attr, int nid, u32 flag)
{
    ka_page_t *hpage = NULL;
    u64 pg_num;
    int ret;

    pg_num = attr->is_giant_page ? DEVMM_GIANT_TO_HUGE_PAGE_NUM : 1;
    ret = devmm_huge_free_mem_size_sub(attr->devid, attr->vfid, nid, pg_num, flag);
    if (ret != 0) {
        devmm_drv_debug("Not enough huge free mem. (devid=%u; vfid=%u; nid=%d; flag=%u; pg_num=%llu)\n",
            attr->devid, attr->vfid, nid, flag, pg_num);
        return NULL;
    }

    hpage = _devmm_alloc_hpage(nid, flag);
    if (hpage == NULL) {
        devmm_huge_free_mem_size_add(attr->devid, attr->vfid, nid, pg_num, flag);
    }

    return hpage;
}

static ka_page_t *_devmm_alloc_huge_page(struct devmm_phy_addr_attr *attr,
    int *latest_nid, int nids[], u32 nid_num, u32 flag)
{
    ka_page_t *hpage = NULL;
    bool try_alloc = false;
    int i;

    hpage = devmm_alloc_hpage(attr, *latest_nid, flag);
    if (hpage != NULL) {
        return hpage;
    }

    for (i = 0; i < nid_num; i++) {
        /* node os maybe alloc twice, controlled by try_alloc */
        if ((nids[i] == *latest_nid) && (try_alloc == false)) {
            try_alloc = true;
            continue;
        }

        hpage = devmm_alloc_hpage(attr, nids[i], flag);
        if (hpage != NULL) {
            *latest_nid = nids[i];
            return hpage;
        }
    }

    return NULL;
}

static ka_page_t *devmm_alloc_huge_page(struct devmm_phy_addr_attr *attr, int *latest_nid, int nids[], u32 nid_num)
{
#ifndef EMU_ST
    ka_page_t *hpage = NULL;

    if (devmm_is_support_giant_page_feature() && attr->is_giant_page) {
        return _devmm_alloc_huge_page(attr, latest_nid, nids, nid_num, HUGETLB_ALLOC_GIANT);
    }

    hpage = _devmm_alloc_huge_page(attr, latest_nid, nids, nid_num, HUGETLB_ALLOC_NORMAL);
    if (hpage != NULL) {
        return hpage;
    }

    hpage = _devmm_alloc_huge_page(attr, latest_nid, nids, nid_num, HUGETLB_ALLOC_BUDDY | HUGETLB_ALLOC_NORECLAIM);
    if (hpage != NULL) {
        return hpage;
    }

    /* If alloc failed, allocing by buddy will reclaim mem which will takes a lot of time. */
    return _devmm_alloc_huge_page(attr, latest_nid, nids, nid_num, HUGETLB_ALLOC_BUDDY);
#else
    if (devmm_is_support_giant_page_feature() && attr->is_giant_page) {
        return _devmm_alloc_huge_page(attr, latest_nid, nids, nid_num, HUGETLB_ALLOC_GIANT);
    }
    return _devmm_alloc_huge_page(attr, latest_nid, nids, nid_num, HUGETLB_ALLOC_NORMAL);
#endif
}

static int devmm_alloc_huge_pages(struct devmm_phy_addr_attr *attr,
    int nids[], u32 nid_num, ka_page_t **hpages, u64 pg_num)
{
    int latest_nid = nids[0];    /* alloc from latest nid to approve performance */
    u32 stamp = (u32)ka_jiffies;
    u64 i;

    for (i = 0; i < pg_num; i++) {
        if ((attr->is_giant_page == false) || ((i % DEVMM_GIANT_TO_HUGE_PAGE_NUM) == 0)) {
            hpages[i] = devmm_alloc_huge_page(attr, &latest_nid, nids, nid_num);
            if (hpages[i] == NULL) {
                devmm_free_huge_pages(attr, hpages, i);
                return -ENOMEM;
            }
        } else {
            /* set page cache will use page_to_phys */
            hpages[i] = hpages[i - 1];
        }
        devmm_try_cond_resched(&stamp);
    }

    return 0;
}

int devmm_alloc_pages(struct devmm_phy_addr_attr *attr, ka_page_t **pages, u64 pg_num)
{
    int nids[DEVMM_MAX_NUMA_NUM_OF_PER_DEV] = {0};
    u32 nid_num = DEVMM_MAX_NUMA_NUM_OF_PER_DEV;
    int ret;

    ret = devmm_get_nids(attr->devid, attr->vfid, attr->mem_type, nids, &nid_num);
    if (ret != 0) {
        devmm_drv_err("Get nids failed. (ret=%d; devid=%u; vfid=%u; mem_type=%u)\n",
            ret, attr->devid, attr->vfid, attr->mem_type);
        return ret;
    }

    devmm_update_alloc_nid_info(attr->devid, attr->vfid, nids, &nid_num);
    if (attr->pg_type == DEVMM_HUGE_PAGE_TYPE) {
        ret = devmm_alloc_huge_pages(attr, nids, nid_num, pages, pg_num);
    } else {
        ret = devmm_alloc_normal_pages(attr, nids, nid_num, pages, pg_num);
    }
    if (ret != 0) {
        devmm_print_nodes_info(attr->devid, attr->vfid, attr->mem_type);
    }

    return ret;
}

void devmm_put_normal_page(ka_page_t *pg)
{
    devmm_page_ref_dec(pg, devmm_clear_normal_single_page, devmm_put_page, devmm_put_page, false);
}

void devmm_put_huge_page(ka_page_t *hpage)
{
    /* Keep the status quo, call put_page. */
    devmm_page_ref_dec(hpage, devmm_clear_huge_page, devmm_put_page, devmm_put_page, false);
}

