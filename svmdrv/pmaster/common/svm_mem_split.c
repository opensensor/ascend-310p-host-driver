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

#include "devmm_adapt.h"
#include "devmm_proc_info.h"
#include "svm_mem_split.h"

static u32 g_support_memory_split_feature = 0;

void devmm_alloc_numa_info_init(u32 devid, u32 vfid, int nids[], u32 *nid_num)
{
    struct devmm_alloc_numa_info *info = NULL;
    u32 udevid = devid; /* pm scene, udevid == devid; */
    u32 i;

    for (i = 1; i < *nid_num; i++) {
        info = &devmm_svm->device_info.alloc_numa_info[udevid][nids[i]];
        info->free_size = 0;
        info->threshold = 0;
        info->enable_threshold = false;
    }

    info = &devmm_svm->device_info.alloc_numa_info[udevid][nids[0]];
    info->free_size = devmm_get_nid_free_size(nids[0]);
    info->threshold = devmm_get_alloc_threshold(devid, vfid);
    info->enable_threshold = true;
    devmm_drv_debug("Alloc_numa_info_init. (devid=%u; vfid=%u; nid=%d; threshold=%llu; free_size=%llu)\n",
        udevid, vfid, nids[0], info->threshold, info->free_size);
}

void devmm_alloc_numa_enable_threshold(u32 devid, u32 vfid, int nid)
{
#ifndef EMU_ST
    u32 udevid = devid; /* pm scene, udevid == devid; */

    if ((devmm_is_support_update_numa_order() == 0) || (devmm_is_normal_node(nid) == false) || (vfid != 0)) {
        return;
    }

    devmm_svm->device_info.alloc_numa_info[udevid][nid].enable_threshold = true;
#endif
}

static int devmm_alloc_numa_free_size_sub(u32 devid, u32 vfid, int nid, u64 page_num, u64 page_size)
{
    struct devmm_alloc_numa_info *info = NULL;
    u32 udevid = devid; /* pm scene, udevid == devid; */

    info = &devmm_svm->device_info.alloc_numa_info[udevid][nid];
    if (info->enable_threshold == false) {
        return 0;
    }

    if (info->free_size <= info->threshold) {
        info->enable_threshold = false;
        return -ENOMEM;
    }

    info->free_size -= (page_num * page_size);
    return 0;
}

static int devmm_numa_normal_free_size_sub(u32 devid, u32 vfid, int nid, u64 page_num)
{
    if ((devmm_is_support_update_numa_order() == 0) || (devmm_is_normal_node(nid) == false) || (vfid != 0)) {
        return 0;
    }

    return devmm_alloc_numa_free_size_sub(devid, vfid, nid, page_num, KA_MM_PAGE_SIZE);
}

static int devmm_numa_huge_free_size_sub(u32 devid, u32 vfid, int nid, u64 page_num, u32 hugetlb_alloc_flag)
{
    if ((devmm_is_support_update_numa_order() == 0) || (devmm_is_normal_node(nid) == false) || (vfid != 0)) {
        return 0;
    }

    if (hugetlb_alloc_flag == HUGETLB_ALLOC_NORMAL) {
        return 0;
    }
    return devmm_alloc_numa_free_size_sub(devid, vfid, nid, page_num, KA_HPAGE_SIZE);
}

void devmm_set_memory_split_feature(u32 flag)
{
    g_support_memory_split_feature = flag;

    return;
}

static bool devmm_is_support_memory_split(u32 vfid, int nid)
{
    if ((g_support_memory_split_feature == 0) || (vfid == 0) || devmm_is_normal_node(nid)) {
        return false;
    }
    return true;
}

int devmm_normal_free_mem_size_sub(u32 devid, u32 vfid, int nid, u64 page_num)
{
    u64 mem_size;
    int ret;

    ret = devmm_numa_normal_free_size_sub(devid, vfid, nid, page_num);
    if (ret != 0) {
        return ret;
    }

    /* Only memory split feature need statistic. */
    if (devmm_is_support_memory_split(vfid, nid) == false) {
        return 0;
    }

    mem_size = page_num * KA_MM_PAGE_SIZE;
    if ((u64)ka_base_atomic64_read(&devmm_svm->device_info.free_mem_size[devid][nid][vfid]) < mem_size) {
        return -ENOMEM;
    }
    ka_base_atomic64_sub((long)mem_size, &devmm_svm->device_info.free_mem_size[devid][nid][vfid]);
    return 0;
}

void devmm_normal_free_mem_size_add(u32 devid, u32 vfid, int nid, u64 page_num)
{
    u64 mem_size;

    if (devmm_is_support_memory_split(vfid, nid) == false) {
        return;
    }

    mem_size = page_num * KA_MM_PAGE_SIZE;
    ka_base_atomic64_add((long)mem_size, &devmm_svm->device_info.free_mem_size[devid][nid][vfid]);
}

int devmm_huge_free_mem_size_sub(u32 devid, u32 vfid, int nid, u64 page_num, u32 hugetlb_alloc_flag)
{
    u64 mem_size;
    int ret;

    ret = devmm_numa_huge_free_size_sub(devid, vfid, nid, page_num, hugetlb_alloc_flag);
    if (ret != 0) {
        return ret;
    }

    if (devmm_is_support_memory_split(vfid, nid) == false) {
        return 0;
    }

    mem_size = page_num * KA_HPAGE_SIZE;
    if (hugetlb_alloc_flag == HUGETLB_ALLOC_NORMAL) {
        if ((u64)ka_base_atomic64_read(&devmm_svm->device_info.free_mem_hugepage_size[devid][nid][vfid]) < mem_size) {
            return -ENOMEM;
        }
        ka_base_atomic64_sub((long)mem_size, &devmm_svm->device_info.free_mem_hugepage_size[devid][nid][vfid]);
    } else {
        if ((u64)ka_base_atomic64_read(&devmm_svm->device_info.free_mem_size[devid][nid][vfid]) < mem_size) {
            return -ENOMEM;
        }
        ka_base_atomic64_sub((long)mem_size, &devmm_svm->device_info.free_mem_size[devid][nid][vfid]);
    }

    return 0;
}

void devmm_huge_free_mem_size_add(u32 devid, u32 vfid, int nid, u64 page_num, u32 hugetlb_alloc_flag)
{
    u64 mem_size;

    if (devmm_is_support_memory_split(vfid, nid) == false) {
        return;
    }

    mem_size = page_num * KA_HPAGE_SIZE;
    if (hugetlb_alloc_flag == HUGETLB_ALLOC_NORMAL) {
        ka_base_atomic64_add((long)mem_size, &devmm_svm->device_info.free_mem_hugepage_size[devid][nid][vfid]);
    } else {
        ka_base_atomic64_add((long)mem_size, &devmm_svm->device_info.free_mem_size[devid][nid][vfid]);
    }
}
