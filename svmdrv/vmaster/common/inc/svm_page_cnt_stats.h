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
 * Create: 2023-03-04
 */

#ifndef SVM_PAGE_CNT_STATS_H
#define SVM_PAGE_CNT_STATS_H

#include "ka_memory_pub.h"
#include "ka_common_pub.h"
#include "ka_base_pub.h"

#include "svm_define.h"

struct devmm_page_cnt_stats {
    ka_atomic64_t cgroup_pg_cnt[DEVMM_PAGE_TYPE_MAX];
    ka_atomic64_t cdm_pg_cnt[DEVMM_PAGE_TYPE_MAX];
    ka_atomic64_t peak_pg_cnt[DEVMM_PAGE_TYPE_MAX];
};

void devmm_page_cnt_stats_init(struct devmm_page_cnt_stats *stats);
void devmm_used_page_cnt_add(struct devmm_page_cnt_stats *stats, u32 page_type, ka_page_t **pages, u64 page_num);
void devmm_used_page_cnt_sub(struct devmm_page_cnt_stats *stats, u32 page_type, ka_page_t **pages, u64 page_num);
u64 devmm_get_cgroup_used_page_cnt(struct devmm_page_cnt_stats *stats, u32 page_type);
u64 devmm_get_cdm_used_page_cnt(struct devmm_page_cnt_stats *stats, u32 page_type);
void devmm_page_cnt_stats_show(struct devmm_page_cnt_stats *stats);
u64 devmm_get_peak_page_cnt(struct devmm_page_cnt_stats *stats, u32 page_type);

#endif
