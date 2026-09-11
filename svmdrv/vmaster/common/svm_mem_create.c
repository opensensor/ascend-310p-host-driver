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

#include "svm_cgroup_mng.h"
#include "svm_phy_addr_blk_mng.h"
#include "svm_mem_create.h"

int devmm_mem_create_to_new_blk(struct devmm_svm_process *svm_proc,
    struct devmm_phy_addr_attr *attr, u64 total_pg_num, u64 to_create_pg_num, int *id)
{
    struct devmm_phy_addr_blk_mng *mng = &svm_proc->phy_addr_blk_mng;
    struct devmm_phy_addr_blk *blk = NULL;
    int tmp_id, ret;

    blk = devmm_phy_addr_blk_create(mng, attr, total_pg_num, &tmp_id);
    if (blk == NULL) {
        return -ENOMEM;
    }

    /*
     * Attention, the blk can be destroyed in security scenarios, when blk_state is set inited.
     * So call blk before blk_init.
     */
    ret = devmm_phy_addr_blk_init(svm_proc, blk, to_create_pg_num);
    if (ret != 0) {
        devmm_phy_addr_blk_destroy(mng, blk);
    } else {
        *id = tmp_id;
    }
    return ret;
}

int devmm_mem_create_to_old_blk(struct devmm_svm_process *svm_proc,
    struct devmm_phy_addr_attr *attr, u64 to_create_pg_num, int id)
{
    struct devmm_phy_addr_blk_mng *mng = &svm_proc->phy_addr_blk_mng;
    struct devmm_phy_addr_blk *blk = NULL;
    int ret;

    blk = devmm_phy_addr_blk_get(mng, id);
    if (blk == NULL) {
        devmm_drv_err("Is destroyed. (id=%d)\n", id);
        return -EINVAL;
    }
    ret = devmm_phy_addr_blk_init(svm_proc, blk, to_create_pg_num);
    devmm_phy_addr_blk_put(blk);
    return ret;
}

int _devmm_mem_release(struct devmm_svm_process *svm_proc, struct devmm_phy_addr_blk_mng *mng, int id,
    u64 to_free_pg_num, u32 free_type)
{
    struct devmm_phy_addr_blk *blk = NULL;
    bool is_finish = false;
    int ret;

    blk = devmm_phy_addr_blk_get(mng, id);
    if (blk == NULL) {
        devmm_drv_err("Invalid phy_addr_blk id. (id=%d)\n", id);
        return -EINVAL;
    }

    ret = devmm_phy_addr_blk_uninit(svm_proc, blk, to_free_pg_num, free_type, &is_finish);
    if ((ret == 0) && is_finish) {
        devmm_phy_addr_blk_destroy(mng, blk);
    }

    devmm_phy_addr_blk_put(blk);
    return ret;
}

int devmm_mem_release(struct devmm_svm_process *svm_proc, int id, u64 to_free_pg_num, u32 free_type)
{
    struct devmm_phy_addr_blk_mng *mng = &svm_proc->phy_addr_blk_mng;

    return _devmm_mem_release(svm_proc, mng, id, to_free_pg_num, free_type);
}

