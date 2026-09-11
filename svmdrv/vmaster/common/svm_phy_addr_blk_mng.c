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

#include "svm_proc_gfp.h"
#include "devmm_common.h"
#include "devmm_mem_alloc_interface.h"
#include "svm_phy_addr_blk_mng.h"

void devmm_phy_addr_blk_mng_init(struct devmm_phy_addr_blk_mng *mng)
{
    ka_base_idr_init(&mng->idr);
    mng->id_start = 0;
    mng->id_end = KA_INT_MAX;
    ka_task_init_rwsem(&mng->rw_sem);
}

static void _devmm_phy_addr_blk_get(struct devmm_phy_addr_blk *blk)
{
    ka_base_kref_get(&blk->ref);
}

static void devmm_phy_addr_blk_release(ka_kref_t *kref)
{
    struct devmm_phy_addr_blk *blk = ka_container_of(kref, struct devmm_phy_addr_blk, ref);

    devmm_kvfree_ex(blk->addr_info.target_addr);
    devmm_kvfree_ex(blk->dma_blk_info.dma_blks);
    devmm_kvfree_ex(blk->pg_info.pages);
    devmm_kvfree_ex(blk);
}

bool __attribute__((weak)) devmm_support_host_giant_page(void)
{
    return false;
}

void __attribute__((weak)) devmm_master_free_huge_pages(struct devmm_phy_addr_attr *attr, ka_page_t **pages, u64 pg_num)
{
}

int __attribute__((weak)) devmm_master_alloc_huge_pages(struct devmm_phy_addr_attr *attr, ka_page_t **pages, u64 pg_num)
{
    return -ENOMEM;
}

void __attribute__((weak)) devmm_master_free_giant_pages(struct devmm_phy_addr_attr *attr, ka_page_t **pages, u64 pg_num)
{
}

int __attribute__((weak)) devmm_master_alloc_giant_pages(struct devmm_phy_addr_attr *attr, ka_page_t **pages, u64 pg_num)
{
    return -ENOMEM;
}

static int devmm_phy_addr_blk_alloc_pages(struct devmm_svm_process *svm_proc,
        struct devmm_phy_addr_attr *attr, ka_page_t **pages, u64 pg_num)
{
    if ((attr->side == DEVMM_SIDE_MASTER) && (attr->pg_type != MEM_NORMAL_PAGE_TYPE)) {
        if (attr->pg_type == MEM_GIANT_PAGE_TYPE) {
            return devmm_master_alloc_giant_pages(attr, pages, pg_num);
        } else {
            return devmm_master_alloc_huge_pages(attr, pages, pg_num);
        }
    } else {
        return devmm_proc_alloc_pages(svm_proc, attr, pages, pg_num);
    }
}

static void devmm_phy_addr_blk_free_pages(struct devmm_svm_process *svm_proc,
    struct devmm_phy_addr_attr *attr, ka_page_t **pages, u64 pg_num)
{
    if (pg_num == 0 || pages == NULL) {
        devmm_drv_err("there is no mem that needs to free!\n");
        return;
    }

    if ((attr->side == DEVMM_SIDE_MASTER) && (attr->pg_type != MEM_NORMAL_PAGE_TYPE)) {
         if (attr->pg_type == MEM_GIANT_PAGE_TYPE) {
            devmm_master_free_giant_pages(attr, pages, pg_num);
        } else {
            devmm_master_free_huge_pages(attr, pages, pg_num);
        }
    } else {
        devmm_proc_free_pages(svm_proc, attr, pages, pg_num);
    }
}

struct devmm_phy_addr_blk *devmm_phy_addr_blk_get(struct devmm_phy_addr_blk_mng *mng, int id)
{
    struct devmm_phy_addr_blk *blk = NULL;

    ka_task_down_read(&mng->rw_sem);
    blk = (struct devmm_phy_addr_blk *)ka_base_idr_find(&mng->idr, (unsigned long)id);
    if (blk != NULL) {
        _devmm_phy_addr_blk_get(blk);
    }
    ka_task_up_read(&mng->rw_sem);

    return blk;
}

void devmm_phy_addr_blk_put(struct devmm_phy_addr_blk *blk)
{
    ka_base_kref_put(&blk->ref, devmm_phy_addr_blk_release);
}

static void devmm_phy_addr_blk_info_init(struct devmm_phy_addr_blk *blk,
    struct devmm_phy_addr_attr *attr, u64 pg_num, ka_page_t **pages, struct devmm_dma_blk *dma_blks)
{
    u64 pg_size = (attr->pg_type == DEVMM_HUGE_PAGE_TYPE) ? KA_HPAGE_SIZE : KA_MM_PAGE_SIZE;
    if (attr->pg_type == DEVMM_GIANT_PAGE_TYPE) {
        pg_size = SVM_MASTER_GIANT_PAGE_SIZE;
    }

    ka_base_kref_init(&blk->ref);
    ka_task_init_rwsem(&blk->rw_sem);
    ka_base_atomic64_set(&blk->occupied_num, 0);
    blk->type = SVM_PYH_ADDR_BLK_NORMAL_TYPE;
    blk->init_state = SVM_PHY_ADDR_BLK_IS_INITING;
    blk->attr = *attr;
    blk->pg_num = pg_num;
    blk->size = pg_num * pg_size;
    blk->pg_info.pages = pages;
    blk->pg_info.saved_num = 0;
    blk->pg_info.total_num = pg_num;
    blk->dma_blk_info.dma_blks = dma_blks;
    blk->dma_blk_info.saved_num = 0;
    blk->dma_blk_info.total_num = pg_num;
}

static void devmm_blk_target_addr_info_init(struct devmm_phy_addr_blk *blk, u64 pg_num, u64 *target_addr)
{
    blk->addr_info.total_num = pg_num;
    blk->addr_info.saved_num = 0;
    blk->addr_info.target_addr = target_addr;
}

struct devmm_phy_addr_blk *devmm_phy_addr_blk_create(struct devmm_phy_addr_blk_mng *mng,
    struct devmm_phy_addr_attr *attr, u64 pg_num, int *id)
{
    struct devmm_phy_addr_blk *blk = NULL;
    struct devmm_dma_blk *dma_blks = NULL;
    ka_page_t **pages = NULL;
    u64 *target_addr = NULL;

    blk = devmm_kvzalloc_ex(sizeof(struct devmm_phy_addr_blk), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (blk == NULL) {
        devmm_drv_err("Kvzalloc blk failed.\n");
        return NULL;
    }

    pages = devmm_kvzalloc_ex(sizeof(ka_page_t *) * pg_num, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (pages == NULL) {
        devmm_drv_err("Kvzalloc pages failed. (pg_num=%llu)\n", pg_num);
        goto free_blk;
    }

    dma_blks = devmm_kvzalloc_ex(sizeof(struct devmm_dma_blk) * pg_num, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (dma_blks == NULL) {
        devmm_drv_err("Kvzalloc dma_blks failed. (pg_num=%llu)\n", pg_num);
        goto free_pages;
    }

    target_addr = devmm_kvzalloc_ex(sizeof(u64) * pg_num, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (target_addr == NULL) {
        devmm_drv_err("Kvzalloc target_addr failed. (pg_num=%llu)\n", pg_num);
        goto free_dma_blks;
    }

    devmm_phy_addr_blk_info_init(blk, attr, pg_num, pages, dma_blks);
    devmm_blk_target_addr_info_init(blk, pg_num, target_addr);

    ka_task_down_write(&mng->rw_sem);
    blk->id = ka_base_idr_alloc_cyclic(&mng->idr, (void *)blk, mng->id_start, mng->id_end, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    *id = blk->id;
    ka_task_up_write(&mng->rw_sem);
    if (blk->id < 0) {
        devmm_drv_err("Idr alloc failed. (ret=%d)\n", blk->id);
        goto free_target_addr;
    }

    return blk;

free_target_addr:
    devmm_kvfree_ex(target_addr);
free_dma_blks:
    devmm_kvfree_ex(dma_blks);
free_pages:
    devmm_kvfree_ex(pages);
free_blk:
    devmm_kvfree_ex(blk);
    return NULL;
}

void devmm_phy_addr_blk_destroy(struct devmm_phy_addr_blk_mng *mng, struct devmm_phy_addr_blk *blk)
{
    ka_task_down_write(&mng->rw_sem);
    ka_base_idr_remove(&mng->idr, (unsigned long)blk->id);
    ka_task_up_write(&mng->rw_sem);

    devmm_phy_addr_blk_put(blk);
}

static void _devmm_phy_addr_blk_uninit(struct devmm_svm_process *svm_proc,
    struct devmm_phy_addr_blk *blk, u64 to_free_pg_num, bool *is_finish)
{
    struct devmm_dma_blk_info *dma_blk_info = &blk->dma_blk_info;
    struct devmm_page_info *pg_info = &blk->pg_info;
    struct devmm_dma_blk *dma_blks = NULL;
    ka_page_t **pages = NULL;
    u32 devid = blk->attr.devid;
    u64 freed_dma_blk_num, freed_pg_num;

    /* dma_blk_info->save_num must <= pg_info->saved_num */
    freed_pg_num = ka_base_min(to_free_pg_num, pg_info->saved_num);
    freed_dma_blk_num = ka_base_min(to_free_pg_num, dma_blk_info->saved_num);

    pages = &pg_info->pages[pg_info->saved_num - freed_pg_num];
    dma_blks = &dma_blk_info->dma_blks[dma_blk_info->saved_num - freed_dma_blk_num];

    devmm_phy_addr_blk_free_pages(svm_proc, &blk->attr, pages, freed_pg_num);
    pg_info->saved_num -= freed_pg_num;

    if (blk->attr.side == DEVMM_SIDE_DEVICE_AGENT) {
        devmm_dma_unmap_pages(devid, dma_blks, freed_dma_blk_num);
        dma_blk_info->saved_num -= freed_dma_blk_num;
    }

    *is_finish = (pg_info->saved_num == 0) ? true : false;
}

/* If not so much to_free_pg_num, return actual freed_pg_num. */
int devmm_phy_addr_blk_uninit(struct devmm_svm_process *svm_proc,
    struct devmm_phy_addr_blk *blk, u64 to_free_pg_num, u32 free_type, bool *is_finish)
{
    ka_task_down_write(&blk->rw_sem);
    if (ka_base_atomic64_read(&blk->occupied_num) != 0) {
        devmm_drv_err("Is mapped. (occupied_num=%llu; blk_id=%d)\n",
            (u64)ka_base_atomic64_read(&blk->occupied_num), blk->id);
        ka_task_up_write(&blk->rw_sem);
        return -EBUSY;
    }

    if (blk->init_state == SVM_PHY_ADDR_BLK_IS_UNINITED) {
        devmm_drv_err("Is already uninited. (blk_id=%d)\n", blk->id);
        ka_task_up_write(&blk->rw_sem);
        return -EBADRQC;
    }

    devmm_drv_debug("Blk uninit. (blk_type=%u; free_type=%u; id=%d)\n", blk->type, free_type, blk->id);
    if ((blk->type == SVM_PYH_ADDR_BLK_NORMAL_TYPE) || (free_type == SVM_PYH_ADDR_BLK_NORMAL_FREE)) {
        _devmm_phy_addr_blk_uninit(svm_proc, blk, to_free_pg_num, is_finish);
    } else {
        if ((blk->type == SVM_PYH_ADDR_BLK_EXPORT_TYPE) && (svm_proc != NULL)) {
            devmm_used_page_cnt_sub(&svm_proc->pg_cnt_stats, blk->attr.pg_type, blk->pg_info.pages, blk->pg_num);
        }
        *is_finish = true;
    }

    blk->init_state = (*is_finish) ? SVM_PHY_ADDR_BLK_IS_UNINITED : SVM_PHY_ADDR_BLK_IS_UNINITING;
    ka_task_up_write(&blk->rw_sem);
    return 0;
}

static int _devmm_phy_addr_blk_init(struct devmm_svm_process *svm_proc,
    struct devmm_phy_addr_blk *blk, u64 pg_num, bool *is_finish)
{
    struct devmm_dma_blk_info *dma_blk_info = &blk->dma_blk_info;
    struct devmm_page_info *pg_info = &blk->pg_info;
    struct devmm_dma_blk *dma_blks = &dma_blk_info->dma_blks[dma_blk_info->saved_num];
    ka_page_t **pages = &pg_info->pages[pg_info->saved_num];
    u32 pg_type = blk->attr.pg_type;
    u32 devid = blk->attr.devid;
    u64 mapped_dma_blk_num;
    int ret;

    /* dma_blk_info->saved_num must <= pg_info->saved_num, so just check pg_info->saved_num */
    if (pg_num > (pg_info->total_num - pg_info->saved_num)) {
        devmm_drv_err("Not enough idle pg space to save. (to_save_num=%llu; idle_num=%llu)\n",
            pg_num, (pg_info->total_num - pg_info->saved_num));
        return -ENOSPC;
    }

    ret = devmm_phy_addr_blk_alloc_pages(svm_proc, &blk->attr, pages, pg_num);
    if (ret != 0) {
        devmm_drv_no_err_if((ret == -ENOMEM), "Alloc pages failed. (ret=%d; pg_type=%u; pg_num=%llu)\n",
            ret, pg_type, pg_num);
        return ret;
    }

    if (blk->attr.side == DEVMM_SIDE_DEVICE_AGENT) {
        if (pg_type != DEVMM_NORMAL_PAGE_TYPE) {
            ret = devmm_dma_map_huge_pages(devid, pages, pg_num, dma_blks, &mapped_dma_blk_num);
        } else {
            ret = devmm_dma_map_normal_pages(devid, pages, pg_num, dma_blks, &mapped_dma_blk_num);
        }
        if (ret != 0) {
            devmm_drv_err("Dma map pages failed. (ret=%d; pg_type=%u; pg_num=%llu)\n", ret, pg_type, pg_num);
            devmm_phy_addr_blk_free_pages(svm_proc, &blk->attr, pages, pg_num);
            return ret;
        }
        dma_blk_info->saved_num += mapped_dma_blk_num;
    }

    pg_info->saved_num += pg_num;
    *is_finish = (pg_info->saved_num == pg_info->total_num) ? true : false;
    return 0;
}

int devmm_phy_addr_blk_init(struct devmm_svm_process *svm_proc,
    struct devmm_phy_addr_blk *blk, u64 pg_num)
{
    bool is_finish = false;
    int ret;

    ka_task_down_write(&blk->rw_sem);
    if (blk->init_state != SVM_PHY_ADDR_BLK_IS_INITING) {
        devmm_drv_err("Err state. (state=%u; blk_id=%d)\n", blk->init_state, blk->id);
        ka_task_up_write(&blk->rw_sem);
        return -EBUSY;
    }

    ret = _devmm_phy_addr_blk_init(svm_proc, blk, pg_num, &is_finish);
    blk->init_state = ((ret == 0) && is_finish) ? SVM_PHY_ADDR_BLK_IS_INITED : SVM_PHY_ADDR_BLK_IS_INITING;
    ka_task_up_write(&blk->rw_sem);
    return ret;
}

void _devmm_phy_addr_blks_destroy(struct devmm_svm_process *svm_proc, struct devmm_phy_addr_blk_mng *mng)
{
    struct devmm_phy_addr_blk *blk = NULL;
    u32 stamp = (u32)ka_jiffies;
    bool is_finish;
    int id;

    ka_task_down_write(&mng->rw_sem);
    ka_base_idr_for_each_entry(&mng->idr, blk, id) {
        ka_base_idr_remove(&mng->idr, (unsigned long)id);
        if ((blk->type == SVM_PYH_ADDR_BLK_NORMAL_TYPE) || (blk->type == SVM_PYH_ADDR_BLK_SHARE_TYPE)) {
            _devmm_phy_addr_blk_uninit(svm_proc, blk, blk->pg_num, &is_finish);
        }
        devmm_phy_addr_blk_put(blk);
        devmm_try_cond_resched(&stamp);
    }
    ka_task_up_write(&mng->rw_sem);
}

void devmm_phy_addr_blks_destroy(struct devmm_svm_process *svm_proc)
{
    struct devmm_phy_addr_blk_mng *mng = &svm_proc->phy_addr_blk_mng;

    _devmm_phy_addr_blks_destroy(svm_proc, mng);
}

ka_page_t **devmm_phy_addr_blk_get_pages(struct devmm_phy_addr_blk *blk, u64 offset, u64 pg_num)
{
    struct devmm_page_info *pg_info = &blk->pg_info;

    if (offset >= pg_info->saved_num) {
        devmm_drv_err("Not so much pages. (offset=%llu; to_pg_num=%llu; save_pg_num=%llu)\n",
            offset, pg_num, pg_info->saved_num);
        return NULL;
    }

    if (pg_num > (pg_info->saved_num - offset)) {
        devmm_drv_err("Not so much pages. (offset=%llu; to_pg_num=%llu; save_pg_num=%llu)\n",
            offset, pg_num, pg_info->saved_num);
        return NULL;
    }

    return &pg_info->pages[offset];
}

u64 *devmm_phy_addr_blk_get_target_addr(struct devmm_phy_addr_blk *blk, u64 offset, u64 pg_num)
{
    struct devmm_target_addr_info *addr_info = &blk->addr_info;

    if (offset >= addr_info->saved_num) {
        devmm_drv_err("Not so much pages. (offset=%llu; to_pg_num=%llu; save_pg_num=%llu)\n",
            offset, pg_num, addr_info->saved_num);
        return NULL;
    }

    if (pg_num > (addr_info->saved_num - offset)) {
        devmm_drv_err("Not so much pages. (offset=%llu; to_pg_num=%llu; save_pg_num=%llu)\n",
            offset, pg_num, addr_info->saved_num);
        return NULL;
    }

    return &addr_info->target_addr[offset];
}

int devmm_phy_addr_blk_occupy_inc(struct devmm_phy_addr_blk *blk)
{
    ka_task_down_read(&blk->rw_sem);
    if (blk->init_state != SVM_PHY_ADDR_BLK_IS_INITED) {
        devmm_drv_err("Blk hasn't been inited. (init_state=%u)\n", blk->init_state);
        ka_task_up_read(&blk->rw_sem);
        return -EBADRQC;
    }

    ka_base_atomic64_inc(&blk->occupied_num);
    ka_task_up_read(&blk->rw_sem);
    return 0;
}

void devmm_phy_addr_blk_occupy_dec(struct devmm_phy_addr_blk *blk)
{
    ka_base_atomic64_dec(&blk->occupied_num);
}

