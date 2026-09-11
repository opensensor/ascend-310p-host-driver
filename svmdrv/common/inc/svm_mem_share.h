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
 */
#ifndef SVM_MEM_SHARE_H
#define SVM_MEM_SHARE_H

#include "svm_gfp.h"
#include "devmm_proc_info.h"

int devmm_share_phy_addr_blk_create(struct devmm_phy_addr_blk *blk, u32 devid, int *share_id);
void devmm_share_phy_addr_blks_destroy(u32 devid);
void devmm_share_phy_addr_blk_put(struct devmm_phy_addr_blk *share_blk);
struct devmm_phy_addr_blk *devmm_share_phy_addr_blk_get(u32 devid, int share_id);
int devmm_phy_addr_blk_init_in_same_os(struct devmm_phy_addr_blk *blk, u32 share_devid, int share_id,
    u32 to_create_pg_num);
int devmm_share_phy_addr_blk_init(struct devmm_phy_addr_blk *to_blk,
    struct devmm_phy_addr_blk *from_blk, u32 to_create_pg_num, u32 blk_type);
int devmm_target_blk_query_pa_process(u32 devid, struct devmm_chan_target_blk_query_msg *msg, int side);

#endif
