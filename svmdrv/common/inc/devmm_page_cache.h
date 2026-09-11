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

#ifndef DEVMM_PAGE_CACHE_H
#define DEVMM_PAGE_CACHE_H

#include "devmm_proc_info.h"
#include "devmm_channel.h"
#include "svm_proc_mng.h"

struct devmm_pages_cache_info {
    u64 va;
    u64 pg_num;
    u64 pg_size;
    struct devmm_chan_query_phy_blk *blks;
};

void devmm_init_dev_pages_cache(struct devmm_svm_process *svm_proc);
void devmm_destroy_dev_pages_cache(struct devmm_svm_process *svm_proc, u32 devid);
void devmm_destroy_pages_cache(struct devmm_svm_process *svm_proc);
void devmm_pages_cache_set(struct devmm_svm_process *svm_proc, u32 logical_devid, struct devmm_pages_cache_info *info);
void devmm_free_pages_cache(struct devmm_svm_process *svm_proc,
    u32 devid, u32 page_num, u32 page_size, u64 va, bool reuse);
void devmm_insert_pages_cache(struct devmm_svm_process *svm_process,
    struct devmm_chan_page_query_ack *query_pages, u32 devid);
bool devmm_find_pages_cache(struct devmm_svm_process *svm_process, struct devmm_page_query_arg query_arg,
    struct devmm_dma_block *blks, u32 *num);
int devmm_find_pa_cache(struct devmm_svm_process *svm_process, u32 logic_id, u64 va, u32 page_size, u64 *pa);

void devmm_init_dev_pages_cache_inner(struct devmm_svm_process *svm_pro);
void devmm_destroy_dev_pages_cache_inner(struct devmm_svm_process *svm_pro, u32 devid);
void devmm_destroy_pages_cache_inner(struct devmm_svm_process *svm_proc);
void devmm_free_pages_cache_inner(struct devmm_svm_process *svm_process,
    u32 devid, u32 page_num, u32 page_size, u64 va, bool reuse);
u64 devmm_get_continuty_len_after_dev_va(struct devmm_svm_process *svm_proc,
    u32 logic_id, u64 va, u32 page_size);

#endif /* __DEVMM_PAGE_CACHE_H__ */
