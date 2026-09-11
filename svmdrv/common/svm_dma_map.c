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
 * Create: 2023-08-11
 */

#include "devmm_proc_info.h"
#include "devmm_common.h"
#include "svm_dma_map.h"

struct devmm_dma_map_page_info {
    ka_page_t **pages;
    u64 pg_num;
    u32 pg_type;
};

static void _devmm_dma_unmap_pages(ka_device_t *dev, struct devmm_dma_blk *dma_blks, u64 blk_num)
{
    u64 i;
    u32 stamp = (u32)ka_jiffies;

    for (i = 0; i < blk_num; i++) {
        devdrv_dma_unmap_page(dev, dma_blks[i].dma_addr, dma_blks[i].size, KA_DMA_BIDIRECTIONAL);
        devmm_try_cond_resched(&stamp);
    }
}

void devmm_dma_unmap_pages(u32 devid, struct devmm_dma_blk *dma_blks, u64 blk_num)
{
    ka_device_t *dev = NULL;

    if (dma_blks->dma_addr != 0) {
        dev = devmm_device_get_by_devid(devid);
        if (dev != NULL) {
            _devmm_dma_unmap_pages(dev, dma_blks, blk_num);
            dma_blks->dma_addr = 0;
            devmm_device_put_by_devid(devid);
        }
    }
}

static u64 devmm_get_continuous_pg_num_from_begin(u32 pg_type, ka_page_t **pages, u64 pg_num)
{
    u64 i;

    /* Hpage no need to merge, dma perf is ok */
    if (pg_type == DEVMM_HUGE_PAGE_TYPE) {
        if (devmm_is_giant_page(pages, pg_num)) {
            return DEVMM_GIANT_TO_HUGE_PAGE_NUM;
        } else {
            return 1;
        }
    }

    for (i = 1; i < pg_num; i++) {
        if (devmm_pages_is_continue(pages[i - 1], pages[i]) == false) {
            return i;
        }
    }

    return pg_num;
}

static int _devmm_dma_map_pages(u32 devid, struct devmm_dma_map_page_info *pg_info,
    struct devmm_dma_blk *dma_blks, u64 *blk_num)
{
    ka_device_t *dev = NULL;
    u64 i, j, cont_num, pg_shift;
    u32 stamp = (u32)ka_jiffies;
    int ret = 0;

    *blk_num = 0;
    dev = devmm_device_get_by_devid(devid);
    if (dev == NULL) {
        return -ENODEV;
    }

    pg_shift = (pg_info->pg_type == DEVMM_NORMAL_PAGE_TYPE) ? KA_MM_PAGE_SHIFT : KA_MM_HPAGE_SHIFT;
    for (i = 0, j = 0; i < pg_info->pg_num; j++, i += cont_num) {
        cont_num = devmm_get_continuous_pg_num_from_begin(pg_info->pg_type, &pg_info->pages[i], pg_info->pg_num - i);
        dma_blks[j].size = cont_num << pg_shift;
        dma_blks[j].dma_addr = devdrv_dma_map_page(dev, pg_info->pages[i], 0, dma_blks[j].size, KA_DMA_BIDIRECTIONAL);
        ret = ka_mm_dma_mapping_error(dev, dma_blks[j].dma_addr);
        if (ret != 0) {
            devmm_drv_err("Dma map page failed. (ret=%d; i=%llu; size=%llu)\n", ret, i, dma_blks[j].size);
            _devmm_dma_unmap_pages(dev, dma_blks, j);
            devmm_device_put_by_devid(devid);
            return ret;
        }
        devmm_try_cond_resched(&stamp);
    }
    *blk_num = j;
    devmm_device_put_by_devid(devid);

    return 0;
}

int devmm_dma_map_normal_pages(u32 devid, ka_page_t **pages, u64 pg_num,
    struct devmm_dma_blk *dma_blks, u64 *blk_num)
{
    struct devmm_dma_map_page_info pg_info = {.pages = pages, .pg_num = pg_num, .pg_type = DEVMM_NORMAL_PAGE_TYPE};
    return _devmm_dma_map_pages(devid, &pg_info, dma_blks, blk_num);
}

int devmm_dma_map_huge_pages(u32 devid, ka_page_t **pages, u64 pg_num,
    struct devmm_dma_blk *dma_blks, u64 *blk_num)
{
    struct devmm_dma_map_page_info pg_info = {.pages = pages, .pg_num = pg_num, .pg_type = DEVMM_HUGE_PAGE_TYPE};
    return _devmm_dma_map_pages(devid, &pg_info, dma_blks, blk_num);
}

