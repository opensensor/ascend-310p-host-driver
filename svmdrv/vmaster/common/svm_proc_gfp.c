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

#include "svm_cgroup_mng.h"
#include "svm_proc_gfp.h"
#include "svm_define.h"

int devmm_proc_alloc_pages(struct devmm_svm_process *svm_proc,
    struct devmm_phy_addr_attr *attr, ka_page_t **pages, u64 pg_num)
{
    ka_mem_cgroup_t *memcg = NULL;
    ka_mem_cgroup_t *old_memcg = NULL;
    ka_pid_t pid = (attr->side == DEVMM_SIDE_MASTER) ? svm_proc->process_id.hostpid : svm_proc->devpid;
    u32 pg_type;
    int ret;

    old_memcg = devmm_enable_cgroup(&memcg, pid);
    if ((attr->side == DEVMM_SIDE_DEVICE_AGENT) && (memcg == NULL)) {
#ifndef EMU_ST
        return -ESRCH;
#endif
    }

    ret = devmm_alloc_pages(attr, pages, pg_num);
    devmm_disable_cgroup(memcg, old_memcg);
    if (ret != 0) {
        devmm_page_cnt_stats_show(&svm_proc->pg_cnt_stats);
        return ret;
    }

    pg_type = attr->is_giant_page ? DEVMM_GIANT_PAGE_TYPE : attr->pg_type;
    devmm_used_page_cnt_add(&svm_proc->pg_cnt_stats, pg_type, pages, pg_num);
    return ret;
}

void devmm_proc_free_pages(struct devmm_svm_process *svm_proc,
    struct devmm_phy_addr_attr *attr, ka_page_t **pages, u64 pg_num)
{
    if (svm_proc != NULL) {
        u32 pg_type;

        pg_type = devmm_is_giant_page(pages, pg_num) ? DEVMM_GIANT_PAGE_TYPE : attr->pg_type;
        devmm_used_page_cnt_sub(&svm_proc->pg_cnt_stats, pg_type, pages, pg_num);
    }
    devmm_free_pages(attr, pages, pg_num);
}
