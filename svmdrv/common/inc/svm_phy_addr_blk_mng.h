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
#ifndef SVM_PHY_ADDR_BLK_MNG_H
#define SVM_PHY_ADDR_BLK_MNG_H

#include "devmm_proc_info.h"
#include "svm_dma_map.h"
#include "svm_gfp.h"

enum devmm_phy_addr_blk_init_states {
    SVM_PHY_ADDR_BLK_IS_UNINITED = 0,
    SVM_PHY_ADDR_BLK_IS_INITING,
    SVM_PHY_ADDR_BLK_IS_INITED,
    SVM_PHY_ADDR_BLK_IS_UNINITING,
    SVM_PHY_ADDR_BLK_MAX_STATE
};

struct devmm_page_info {
    u64 total_num;
    u64 saved_num;
    ka_page_t **pages;
};

struct devmm_dma_blk_info {
    u64 total_num;
    u64 saved_num;
    struct devmm_dma_blk *dma_blks;
};

struct devmm_target_addr_info {
    u64 total_num;
    u64 saved_num;
    u64 *target_addr;
};

struct devmm_phy_addr_blk {
    ka_kref_t ref;

    int id;
    u32 type;
    u64 size;
    u64 pg_num;
    bool is_same_sys_share;
    struct devmm_phy_addr_attr attr;

    ka_rw_semaphore_t rw_sem;
    u32 init_state;
    ka_atomic64_t occupied_num;
    struct devmm_page_info pg_info;
    struct devmm_dma_blk_info dma_blk_info;
    struct devmm_target_addr_info addr_info;
};

#define SVM_PYH_ADDR_BLK_NORMAL_TYPE SVM_MEM_HANDLE_NORMAL_TYPE
#define SVM_PYH_ADDR_BLK_EXPORT_TYPE SVM_MEM_HANDLE_EXPORT_TYPE
#define SVM_PYH_ADDR_BLK_IMPORT_TYPE SVM_MEM_HANDLE_IMPORT_TYPE
#define SVM_PYH_ADDR_BLK_SHARE_TYPE  SVM_MEM_HANDLE_SHARE_TYPE

#define SVM_PYH_ADDR_BLK_NORMAL_FREE   0x0
#define SVM_PYH_ADDR_BLK_FREE_NO_PAGE  0x1

void devmm_phy_addr_blk_mng_init(struct devmm_phy_addr_blk_mng *mng);
struct devmm_phy_addr_blk *devmm_phy_addr_blk_create(struct devmm_phy_addr_blk_mng *mng,
    struct devmm_phy_addr_attr *attr, u64 pg_num, int *id);
void devmm_phy_addr_blk_destroy(struct devmm_phy_addr_blk_mng *mng, struct devmm_phy_addr_blk *blk);
struct devmm_phy_addr_blk *devmm_phy_addr_blk_get(struct devmm_phy_addr_blk_mng *mng, int id);
void devmm_phy_addr_blk_put(struct devmm_phy_addr_blk *blk);

int devmm_phy_addr_blk_init(struct devmm_svm_process *svm_proc,
    struct devmm_phy_addr_blk *blk, u64 pg_num);
int devmm_phy_addr_blk_uninit(struct devmm_svm_process *svm_proc,
    struct devmm_phy_addr_blk *blk, u64 to_free_pg_num, u32 free_type, bool *is_finish);
void devmm_phy_addr_blks_destroy(struct devmm_svm_process *svm_proc);

int devmm_phy_addr_blk_occupy_inc(struct devmm_phy_addr_blk *blk);
void devmm_phy_addr_blk_occupy_dec(struct devmm_phy_addr_blk *blk);

ka_page_t **devmm_phy_addr_blk_get_pages(struct devmm_phy_addr_blk *blk, u64 offset, u64 pg_num);
u64 *devmm_phy_addr_blk_get_target_addr(struct devmm_phy_addr_blk *blk, u64 offset, u64 pg_num);
void _devmm_phy_addr_blks_destroy(struct devmm_svm_process *svm_proc, struct devmm_phy_addr_blk_mng *mng);

void devmm_master_free_huge_pages(struct devmm_phy_addr_attr *attr, ka_page_t **pages, u64 pg_num);
int devmm_master_alloc_huge_pages(struct devmm_phy_addr_attr *attr, ka_page_t **pages, u64 pg_num);
void devmm_master_free_giant_pages(struct devmm_phy_addr_attr *attr, ka_page_t **pages, u64 pg_num);
int devmm_master_alloc_giant_pages(struct devmm_phy_addr_attr *attr, ka_page_t **pages, u64 pg_num);
bool devmm_support_host_giant_page(void);

#endif /* SVM_PHY_ADDR_BLK_MNG_H */

