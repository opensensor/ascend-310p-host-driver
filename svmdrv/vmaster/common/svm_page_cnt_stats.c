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

#include "svm_log.h"
#include "devmm_adapt.h"
#include "devmm_proc_info.h"
#include "svm_page_cnt_stats.h"
#include "svm_define.h"

void devmm_page_cnt_stats_init(struct devmm_page_cnt_stats *stats)
{
    u32 page_type;

    for (page_type = 0; page_type < DEVMM_PAGE_TYPE_MAX; page_type++) {
        ka_base_atomic64_set(&stats->cgroup_pg_cnt[page_type], 0);
    }

    for (page_type = 0; page_type < DEVMM_PAGE_TYPE_MAX; page_type++) {
        ka_base_atomic64_set(&stats->cdm_pg_cnt[page_type], 0);
    }
}

#define RETRRY_MAX_CNT 3
static void devmm_update_peak_page_cnt(ka_atomic64_t *peak_pg_cnt, u64 new_cnt)
{
    u32 i;

    for (i = 0; i < RETRRY_MAX_CNT; i++) {
        u64 cur_cnt = (u64)ka_base_atomic64_read(peak_pg_cnt);
        if (new_cnt <= cur_cnt) {
            break;
        }
        if ((u64)ka_base_atomic64_cmpxchg(peak_pg_cnt, cur_cnt, new_cnt) == cur_cnt) {
            break;
        }
    }
}

void devmm_used_page_cnt_add(struct devmm_page_cnt_stats *stats, u32 page_type, ka_page_t **pages, u64 page_num)
{
    u64 tmp_cdm_pg_cnt, tmp_cgroup_pg_cnt;
    u64 cgroup_pg_cnt = 0;
    u64 cdm_pg_cnt = 0;
    u64 i, page_num_offset;

    page_num_offset = (page_type == DEVMM_GIANT_PAGE_TYPE) ? DEVMM_GIANT_TO_HUGE_PAGE_NUM : 1;
    for (i = 0; i < page_num; i += page_num_offset) {
        if (devmm_is_normal_node(ka_mm_page_to_nid(pages[i])) == false) {
            cdm_pg_cnt++;
        } else {
            cgroup_pg_cnt++;
        }
    }

    tmp_cdm_pg_cnt = (u64)ka_base_atomic64_add_return((long)cdm_pg_cnt, &stats->cdm_pg_cnt[page_type]);
    tmp_cgroup_pg_cnt = (u64)ka_base_atomic64_add_return((long)cgroup_pg_cnt, &stats->cgroup_pg_cnt[page_type]);
    devmm_update_peak_page_cnt(&stats->peak_pg_cnt[page_type], tmp_cdm_pg_cnt + tmp_cgroup_pg_cnt);
}

void devmm_used_page_cnt_sub(struct devmm_page_cnt_stats *stats, u32 page_type, ka_page_t **pages, u64 page_num)
{
    u64 cgroup_pg_cnt = 0;
    u64 cdm_pg_cnt = 0;
    u64 tmp_pg_cnt;
    u64 i, page_num_offset;

    page_num_offset = (page_type == DEVMM_GIANT_PAGE_TYPE) ? DEVMM_GIANT_TO_HUGE_PAGE_NUM : 1;
    for (i = 0; i < page_num; i += page_num_offset) {
        if (pages[i] != NULL) {
            if (devmm_is_normal_node(ka_mm_page_to_nid(pages[i])) == false) {
                cdm_pg_cnt++;
            } else {
                cgroup_pg_cnt++;
            }
        }
    }

    tmp_pg_cnt = (u64)ka_base_atomic64_sub_return((long)cdm_pg_cnt, &stats->cdm_pg_cnt[page_type]);
    if (tmp_pg_cnt >= KA_UINT_MAX) {
        ka_base_atomic64_set(&stats->cdm_pg_cnt[page_type], 0);
    }

    tmp_pg_cnt = (u64)ka_base_atomic64_sub_return((long)cgroup_pg_cnt, &stats->cgroup_pg_cnt[page_type]);
    if (tmp_pg_cnt >= KA_UINT_MAX) {
        ka_base_atomic64_set(&stats->cgroup_pg_cnt[page_type], 0);
    }
}

u64 devmm_get_cgroup_used_page_cnt(struct devmm_page_cnt_stats *stats, u32 page_type)
{
    return (u64)ka_base_atomic64_read(&stats->cgroup_pg_cnt[page_type]);
}

u64 devmm_get_cdm_used_page_cnt(struct devmm_page_cnt_stats *stats, u32 page_type)
{
    return (u64)ka_base_atomic64_read(&stats->cdm_pg_cnt[page_type]);
}

u64 devmm_get_peak_page_cnt(struct devmm_page_cnt_stats *stats, u32 page_type)
{
    return (u64)ka_base_atomic64_read(&stats->peak_pg_cnt[page_type]);
}

void devmm_page_cnt_stats_show(struct devmm_page_cnt_stats *stats)
{
    u64 cgroup_page_cnt, cgroup_hpage_cnt, cgroup_giant_cnt, cdm_page_cnt, cdm_hpage_cnt, cdm_giant_cnt;
    u64 peak_hpage_cnt, peak_page_cnt, peak_giant_cnt;

    cgroup_page_cnt = (u64)ka_base_atomic64_read(&stats->cgroup_pg_cnt[DEVMM_NORMAL_PAGE_TYPE]);
    cgroup_hpage_cnt = (u64)ka_base_atomic64_read(&stats->cgroup_pg_cnt[DEVMM_HUGE_PAGE_TYPE]);
    cgroup_giant_cnt = (u64)ka_base_atomic64_read(&stats->cgroup_pg_cnt[DEVMM_GIANT_PAGE_TYPE]);

    cdm_page_cnt = (u64)ka_base_atomic64_read(&stats->cdm_pg_cnt[DEVMM_NORMAL_PAGE_TYPE]);
    cdm_hpage_cnt = (u64)ka_base_atomic64_read(&stats->cdm_pg_cnt[DEVMM_HUGE_PAGE_TYPE]);
    cdm_giant_cnt = (u64)ka_base_atomic64_read(&stats->cdm_pg_cnt[DEVMM_GIANT_PAGE_TYPE]);

    peak_page_cnt = (u64)ka_base_atomic64_read(&stats->peak_pg_cnt[DEVMM_NORMAL_PAGE_TYPE]);
    peak_hpage_cnt = (u64)ka_base_atomic64_read(&stats->peak_pg_cnt[DEVMM_HUGE_PAGE_TYPE]);
    peak_giant_cnt = (u64)ka_base_atomic64_read(&stats->peak_pg_cnt[DEVMM_GIANT_PAGE_TYPE]);

    if ((cgroup_page_cnt != 0) || (cgroup_hpage_cnt != 0) || (cgroup_giant_cnt != 0) || (cdm_page_cnt != 0) ||
        (cdm_hpage_cnt != 0) || (cdm_giant_cnt != 0)) {
        devmm_drv_info("Page cnt. (cgroup: normal=%llu huge=%llu giant=%llu; "
            "cdm: normal=%llu huge=%llu giant=%llu; peak: normal=%llu huge=%llu giant=%llu)\n",
            cgroup_page_cnt, cgroup_hpage_cnt, cgroup_giant_cnt, cdm_page_cnt, cdm_hpage_cnt,
            cdm_giant_cnt, peak_page_cnt, peak_hpage_cnt, peak_giant_cnt);
    }
}
