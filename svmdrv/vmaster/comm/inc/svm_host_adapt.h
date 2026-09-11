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

/* Should only be included by devmm_adapt.h */

#ifndef SVM_HOST_ADAPT_H
#define SVM_HOST_ADAPT_H

#include <linux/hugetlb.h>

#include <securec.h>

#include "ka_common_pub.h"
#include "ka_memory_pub.h"
#include "ka_system_pub.h"

#include "svm_cmd.h"

#ifndef HUGETLB_ALLOC_NONE
#define HUGETLB_ALLOC_NONE             0x00
#endif
#ifndef HUGETLB_ALLOC_NORMAL
#define HUGETLB_ALLOC_NORMAL           0x01
#endif
#ifndef HUGETLB_ALLOC_BUDDY
#define HUGETLB_ALLOC_BUDDY            0x02
#endif
#ifndef HUGETLB_ALLOC_NORECLAIM
#define HUGETLB_ALLOC_NORECLAIM        0x04
#endif
#ifndef HUGETLB_ALLOC_GIANT
#define HUGETLB_ALLOC_GIANT            0x08
#endif

#ifndef __KA_GFP_ACCOUNT
#ifdef __KA_GFP_KMEMCG
#define __KA_GFP_ACCOUNT __KA_GFP_KMEMCG /* for linux version 3.10 */
#endif
#ifdef __KA_GFP_NOACCOUNT
#define __KA_GFP_ACCOUNT 0            /* for linux version 4.1 */
#endif
#endif

static inline bool devmm_is_normal_node(int nid)
{
    return true;
}

static inline u32 devmm_get_alloc_mask(bool is_compound_page)
{
    /* arm host can not use __GFP_THISNODE flag */
    u32 alloc_mask = KA_GFP_KERNEL | __KA_GFP_NORETRY | __KA_GFP_NOWARN | __KA_GFP_ACCOUNT;

    if (is_compound_page) {
        alloc_mask |= __KA_GFP_COMP;
    }

    return alloc_mask;
}

static inline int devmm_get_nids(u32 devid, u32 vfid, u32 mem_type, int nids[], u32 *nid_num)
{
    *nid_num = 1;
    nids[0] = KA_NUMA_NO_NODE;
    return 0;
}

static inline u32 devmm_get_hugetlb_alloc_flag(ka_page_t *hpage)
{
    return 0;
}

static inline ka_page_t *_devmm_alloc_hpage(int nid, u32 flag)
{
    return NULL;
}

static inline u64 devmm_get_alloc_threshold(u32 devid, u32 vfid)
{
    return 0;
}

static inline u64 devmm_get_nid_free_size(int nid)
{
    return 0;
}

static inline u32 devmm_is_support_update_numa_order(void)
{
    return 0;
}

static inline u64 devmm_get_double_pgtable_offset(void)
{
    return 0;
}

static inline bool devmm_is_support_double_pgtable(void)
{
    return false;
}

static inline u64 devmm_double_pgtable_get_offset_addr(u64 normal_va)
{
    return 0;
}

static inline bool devmm_is_support_giant_page_feature(void)
{
    return false;
}
#endif /* SVM_HOST_ADAPT_H */
