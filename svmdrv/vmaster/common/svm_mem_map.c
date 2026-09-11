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
 * Create: 2023-08-21
 */

#include "svm_phy_addr_blk_mng.h"
#include "svm_vmma_mng.h"
#include "devmm_common.h"
#include "svm_mem_map.h"

void devmm_mem_unmap(struct devmm_svm_process *svm_proc, struct devmm_vmma_info *info)
{
    struct devmm_phy_addr_blk *blk = NULL;

    blk = devmm_phy_addr_blk_get(&svm_proc->phy_addr_blk_mng, info->phy_addr_blk_id);
    if (ka_unlikely(blk == NULL)) {
        devmm_drv_warn("Get phy_addr_blk failed. (id=%d)\n", info->phy_addr_blk_id);
        return;
    }

    devmm_zap_pages(svm_proc, info->va, info->pg_num, info->pg_type);
    devmm_phy_addr_blk_occupy_dec(blk);
    devmm_phy_addr_blk_put(blk);
}

static int devmm_mem_map_no_need_page_adjust(struct devmm_svm_process *svm_proc,
    struct devmm_phy_addr_blk *blk, struct devmm_vmma_info *info)
{
    u64 *target_addr = NULL;

    target_addr = devmm_phy_addr_blk_get_target_addr(blk, info->offset_pg_num, info->pg_num);
    if (target_addr == NULL) {
        return -ENOMEM;
    }
    return _devmm_insert_virt_range(svm_proc, info->pg_type, info->va, target_addr, (u32)info->pg_num);
}

u64 *devmm_mem_map_adjust_pa_create(u64 dst_pg_num, u64 dst_pg_size, u64 *src_addr, u64 src_pg_num, u64 adjust_num)
{
    u64 *adjust_pa = NULL;
    u64 i, j;

    adjust_pa = devmm_kvzalloc_ex(sizeof(u64) * dst_pg_num, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (adjust_pa == NULL) {
        return NULL;
    }

    /* src_pg_size is larger than or equal to the dst_pg_size */
    if (dst_pg_num >= src_pg_num) {
        for (i = 0; i < src_pg_num; i++) {
            for (j = 0; j < adjust_num; j++) {
                adjust_pa[i * adjust_num + j] = src_addr[i] + dst_pg_size * j;
            }
        }
    } else { /* src_pg_size is less than dst_pg_size */
        for (i = 0; i < dst_pg_num; i++) {
            adjust_pa[i] = src_addr[i * adjust_num];
        }
    }

    return adjust_pa;
}

void devmm_mem_map_adjust_pa_destroy(u64 *adjust_pa)
{
    if (adjust_pa != NULL) {
        devmm_kvfree_ex(adjust_pa);
    }
}

static int devmm_mem_map_need_page_adjust(struct devmm_svm_process *svm_proc,
    struct devmm_phy_addr_blk *blk, struct devmm_vmma_info *info)
{
    u64 target_addr_num = info->pg_num;
    u64 *adjust_paddrs = NULL;
    u64 *target_addr = NULL;
    bool chk_pa_cont = false;
    u64 adjust_num;
    int ret;

    adjust_num = (blk->attr.pg_type == DEVMM_NORMAL_PAGE_TYPE) ?
        devmm_device_page_adjust_num() : devmm_host_to_dev_hpage_adjust_num();
    target_addr_num = (blk->attr.pg_type == DEVMM_NORMAL_PAGE_TYPE) ?
        (info->pg_num * adjust_num) : (info->pg_num / adjust_num);

    devmm_drv_debug("(pg_num=%llu; adjust_num=%llu; pg_type=%u)\n", info->pg_num, adjust_num, blk->attr.pg_type);

    if ((blk->attr.pg_type != DEVMM_NORMAL_PAGE_TYPE) && (info->pg_num % adjust_num != 0)) {
        devmm_drv_err("pg_num invalid. (pg_type=%u; pg_num=%llu; adjust_num=%llu)\n",
            blk->attr.pg_type, info->pg_num, (u64)adjust_num);
        return -EINVAL;
    }

    target_addr = devmm_phy_addr_blk_get_target_addr(blk, info->offset_pg_num, target_addr_num);
    if (target_addr == NULL) {
        return -ENOMEM;
    }

#ifndef EMU_ST
    chk_pa_cont = (devmm_svm->device_page_size != devmm_svm->host_page_size) && (blk->attr.pg_type == DEVMM_NORMAL_PAGE_TYPE);
#else
    chk_pa_cont = (blk->attr.pg_type == DEVMM_NORMAL_PAGE_TYPE);
#endif
    if (chk_pa_cont && !devmm_palist_is_specify_continuous(target_addr, devmm_svm->device_page_size, target_addr_num,
        devmm_device_page_adjust_num())) {
        devmm_drv_err("Check pa continue failed. (pg_num=%llu)\n", target_addr_num);
        return -EINVAL;
    }

    adjust_paddrs = devmm_mem_map_adjust_pa_create(info->pg_num, info->pg_size, target_addr, target_addr_num, adjust_num);
    if (adjust_paddrs == NULL) {
        devmm_drv_err("Adjust pa create failed.\n");
        return -ENOMEM;
    }

    ret = _devmm_insert_virt_range(svm_proc, info->pg_type, info->va, adjust_paddrs, info->pg_num);
    devmm_mem_map_adjust_pa_destroy(adjust_paddrs);

    return ret;
}

static int _devmm_mem_map(struct devmm_svm_process *svm_proc,
    struct devmm_phy_addr_blk *blk, struct devmm_vmma_info *info, bool need_page_adjust)
{
    ka_page_t **pages = NULL;

    if ((blk->type == SVM_PYH_ADDR_BLK_IMPORT_TYPE) && (blk->is_same_sys_share == false)) {
        if (need_page_adjust) {
            return devmm_mem_map_need_page_adjust(svm_proc, blk, info);
        } else {
            return devmm_mem_map_no_need_page_adjust(svm_proc, blk, info);
        }
    }

    pages = devmm_phy_addr_blk_get_pages(blk, info->offset_pg_num, info->pg_num);
    if (pages == NULL) {
        return -ENOMEM;
    }
    return devmm_remap_pages(svm_proc, info->va, pages, info->pg_num, info->pg_type);
}

static int devmm_mem_map_info_check(struct devmm_phy_addr_blk *blk, struct devmm_vmma_info *info)
{
    bool no_chk_pg_type = false;

    /* dev mem map host */
    no_chk_pg_type = (info->side == MEM_HOST_SIDE) && !(blk->is_same_sys_share) &&
        (blk->type == SVM_PYH_ADDR_BLK_IMPORT_TYPE) && (blk->attr.devid != uda_get_host_id());
    /* check va's pg_type */
    if ((!no_chk_pg_type) && info->pg_type != blk->attr.pg_type) {
        devmm_drv_err("Va's pg_type doesn't match with pa's pg_type. (va_pg_type=%u; pa_pg_type=%u)\n",
            info->pg_type, blk->attr.pg_type);
        return -EINVAL;
    }

    /* check handle's pg_num */
    if (info->phy_addr_blk_pg_num != blk->pg_num) {
        devmm_drv_err("Handle's pg_num is not match. (handle->pg_num=%llu; blk->pg_num=%llu)\n",
            info->phy_addr_blk_pg_num, blk->pg_num);
        return -EINVAL;
    }

    /* check handle's module_id */
    if (info->module_id != blk->attr.module_id) {
        devmm_drv_err("Handle's module_id is not match. (handle->module_id=%u; blk->module_id=%u)\n",
            info->module_id, blk->attr.module_id);
        return -EINVAL;
    }

    if (blk->attr.is_giant_page) {
        if (KA_DRIVER_IS_ALIGNED(info->va, DEVMM_GIANT_PAGE_SIZE) == false) {
            devmm_drv_err("Va is not giant page align. (va=0x%llx)\n", info->va);
            return -EINVAL;
        }
        if (KA_DRIVER_IS_ALIGNED(info->size, DEVMM_GIANT_PAGE_SIZE) == false) {
            devmm_drv_err("Size is not giant page align. (size=%llu)\n", info->size);
            return -EINVAL;
        }
    }

    return 0;
}

int devmm_mem_map(struct devmm_svm_process *svm_proc, struct devmm_vmma_info *info, bool need_page_adjust)
{
    struct devmm_phy_addr_blk *blk = NULL;
    int ret = -EINVAL;

    blk = devmm_phy_addr_blk_get(&svm_proc->phy_addr_blk_mng, info->phy_addr_blk_id);
    if (blk == NULL) {
        devmm_drv_err("Get phy_addr_blk failed. (id=%d)\n", info->phy_addr_blk_id);
        return -EINVAL;
    }

    ret = devmm_mem_map_info_check(blk, info);
    if (ret != 0) {
        devmm_phy_addr_blk_put(blk);
        return ret;
    }
    if (blk->attr.is_giant_page) {
        devmm_free_ptes_in_range(svm_proc, info->va, info->size);
    }

    ret = devmm_phy_addr_blk_occupy_inc(blk);
    if (ka_unlikely(ret != 0)) {
        devmm_phy_addr_blk_put(blk);
        return ret;
    }

    ret = _devmm_mem_map(svm_proc, blk, info, need_page_adjust);
    if (ret != 0) {
        devmm_phy_addr_blk_occupy_dec(blk);
    }

    devmm_phy_addr_blk_put(blk);
    return ret;
}
