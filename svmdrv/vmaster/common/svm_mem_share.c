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

#include "spod_info.h"
#include "svm_cmd.h"
#include "devmm_common.h"
#include "devmm_adapt.h"
#include "svm_gfp.h"
#include "svm_mem_create.h"
#include "svm_cgroup_mng.h"
#include "svm_phy_addr_blk_mng.h"
#include "devmm_mem_alloc_interface.h"
#include "devmm_proc_mem_copy.h"
#include "svm_proc_mng.h"
#include "devmm_channel.h"
#include "svm_mem_share.h"

void devmm_share_phy_addr_blk_put(struct devmm_phy_addr_blk *share_blk)
{
    devmm_phy_addr_blk_put(share_blk);
}

struct devmm_phy_addr_blk *devmm_share_phy_addr_blk_get(u32 devid, int share_id)
{
    struct devmm_phy_addr_blk_mng *share_mng = &devmm_svm->share_phy_addr_blk_mng[devid];

    return devmm_phy_addr_blk_get(share_mng, share_id);
}

#ifdef DRV_HOST
static int _devmm_target_blk_query_pa_vm_pa_convert(u32 devid, struct devmm_target_blk blk[], u32 num)
{
    u64 *pa_blk = NULL;
    u32 i;
    int ret;

    pa_blk = devmm_kvzalloc(sizeof(u64) * num);
    if (pa_blk == NULL) {
        devmm_drv_err("Kvzalloc pa blk failed. (devid=%u; num=%u)\n", devid, num);
        return -ENOMEM;
    }

    for (i = 0; i < num; i++) {
        pa_blk[i] = blk[i].target_addr;
    }

    ret = devmm_vm_pa_to_pm_pa(devid, pa_blk, num, pa_blk, num);
    if (ret != 0) {
        devmm_drv_err("Vm pa to pm pa failed. (devid=%u; num=%u; ret=%d)\n", devid, num, ret);
        devmm_kvfree(pa_blk);
        return ret;
    }

    for (i = 0; i < num; i++) {
        blk[i].target_addr = pa_blk[i];
        blk[i].dma_blk.dma_addr = (ka_dma_addr_t)pa_blk[i];
    }

    devmm_kvfree(pa_blk);
    return 0;
}

static int devmm_target_blk_query_pa_vm_pa_convert(u32 devid, struct devmm_target_blk blk[], u32 num)
{
    u32 host_flag;
    int ret;

    ret = devmm_get_host_phy_mach_flag(devid, &host_flag);
    if (ret != 0) {
        devmm_drv_err("Get host flag failed. (devid=%u; ret=%d)\n", devid, ret);
        return ret;
    }

    if (devmm_is_hccs_vm_scene(devid, host_flag)) {
        return _devmm_target_blk_query_pa_vm_pa_convert(devid, blk, num);
    }

    return 0;
}
#endif

static int _devmm_target_blk_query_pa_process(struct devmm_chan_target_blk_query_msg *msg,
    struct devmm_phy_addr_blk *share_blk, u32 devid, int side)
{
    u64 page_size = (share_blk->attr.pg_type == MEM_NORMAL_PAGE_TYPE) ? KA_MM_PAGE_SIZE : SVM_MASTER_HUGE_PAGE_SIZE;
    u32 stamp = (u32)ka_jiffies;
    u32 i;

    if (share_blk->attr.pg_type == MEM_GIANT_PAGE_TYPE) {
        page_size = SVM_MASTER_GIANT_PAGE_SIZE;
    }

    if ((msg->num == 0) || (msg->offset > share_blk->addr_info.total_num) ||
        (msg->num > (share_blk->addr_info.total_num - msg->offset))) {
        devmm_drv_err("Invalid num. (num=%u; offset=%u; total_num=%llu)\n",
            msg->num, msg->offset, share_blk->addr_info.total_num);
        return -ERANGE;
    }

    for (i = 0; i < msg->num; i++) {
        u32 offset;

        offset = msg->offset + i;
        if (offset >= share_blk->pg_info.saved_num) {
            break;
        }

        msg->blk[i].target_addr = ka_mm_page_to_phys(share_blk->pg_info.pages[offset]);
        if (side == MEM_HOST_SIDE) {
            msg->blk[i].dma_blk.dma_addr = (ka_dma_addr_t)msg->blk[i].target_addr;
            msg->blk[i].dma_blk.size = page_size;
        } else {
            msg->blk[i].dma_blk= msg->blk[i].dma_blk;
        }

        devmm_try_cond_resched(&stamp);
    }
#ifdef DRV_HOST
    if ((side == MEM_HOST_SIDE) && (i > 0)) {
        int ret = devmm_target_blk_query_pa_vm_pa_convert(devid, msg->blk, i);
        if (ret != 0) {
            return ret;
        }
    }
#endif
    msg->dma_saved = share_blk->pg_info.saved_num;
    return 0;
}

int devmm_target_blk_query_pa_process(u32 devid, struct devmm_chan_target_blk_query_msg *msg, int side)
{
    struct devmm_phy_addr_blk *share_blk = NULL;
    int ret;

    if (side == MEM_HOST_SIDE) {
        share_blk = devmm_share_phy_addr_blk_get(0, msg->share_id); /* master use devid 0 */
    } else {
        share_blk = devmm_share_phy_addr_blk_get(devid, msg->share_id);
    }
    if (share_blk == NULL) {
        devmm_drv_err("Share id doesn't exist. (devid=%u; share_id=%d;side=%d)\n", devid, msg->share_id, side);
        return -EBADR;
    }

    ret = _devmm_target_blk_query_pa_process(msg, share_blk, devid, side);
    devmm_share_phy_addr_blk_put(share_blk);
    return ret;
}

static void devmm_pg_dma_info_init(struct devmm_phy_addr_blk *to_blk, struct devmm_phy_addr_blk *from_blk,
    u32 to_create_pg_num)
{
    u32 dma_copy_num, offset;
    u64 size;

    size = to_create_pg_num * sizeof(ka_page_t*);
    offset = to_blk->pg_info.saved_num;
    (void)memcpy_s(&to_blk->pg_info.pages[offset], size, &from_blk->pg_info.pages[offset], size);
    to_blk->pg_info.saved_num += to_create_pg_num;

    dma_copy_num = ka_base_min(to_create_pg_num, (u32)(from_blk->dma_blk_info.saved_num - to_blk->dma_blk_info.saved_num));
    if (dma_copy_num == 0) {
        return;
    }
    size = dma_copy_num * sizeof(struct devmm_dma_blk);
    offset = to_blk->dma_blk_info.saved_num;
    (void)memcpy_s(&to_blk->dma_blk_info.dma_blks[offset], size, &from_blk->dma_blk_info.dma_blks[offset], size);
    to_blk->dma_blk_info.saved_num += dma_copy_num;
}

int devmm_share_phy_addr_blk_init(struct devmm_phy_addr_blk *to_blk,
    struct devmm_phy_addr_blk *from_blk, u32 to_create_pg_num, u32 blk_type)
{
    bool is_finish = false;

    ka_task_down_write(&to_blk->rw_sem);
    if (to_blk->init_state != SVM_PHY_ADDR_BLK_IS_INITING) {
        devmm_drv_err("Err state. (state=%u; blk_id=%d)\n", to_blk->init_state, to_blk->id);
        ka_task_up_write(&to_blk->rw_sem);
        return -EBUSY;
    }

    if (from_blk != NULL) {
        devmm_pg_dma_info_init(to_blk, from_blk, to_create_pg_num);
        to_blk->is_same_sys_share = true;
        is_finish = (to_blk->pg_info.saved_num == to_blk->pg_info.total_num);
    } else {
        to_blk->is_same_sys_share = false;
        is_finish = (to_blk->addr_info.saved_num == to_blk->addr_info.total_num);
    }
    to_blk->type = blk_type;
    to_blk->init_state = is_finish ? SVM_PHY_ADDR_BLK_IS_INITED : SVM_PHY_ADDR_BLK_IS_INITING;
    ka_task_up_write(&to_blk->rw_sem);
    return 0;
}

int devmm_share_phy_addr_blk_create(struct devmm_phy_addr_blk *blk, u32 devid, int *share_id)
{
    struct devmm_phy_addr_blk_mng *share_mng = &devmm_svm->share_phy_addr_blk_mng[devid];
    struct devmm_phy_addr_blk *share_blk = NULL;
    int ret, tmp_share_id;

    share_blk = devmm_phy_addr_blk_create(share_mng, &blk->attr, blk->pg_num, &tmp_share_id);
    if (share_blk == NULL) {
        return -ENOMEM;
    }

    ret = devmm_share_phy_addr_blk_init(share_blk, blk, blk->pg_num, SVM_PYH_ADDR_BLK_SHARE_TYPE);
    if (ret != 0) {
        devmm_phy_addr_blk_destroy(share_mng, share_blk);
        return ret;
    }
    *share_id = tmp_share_id;
    return 0;
}

void devmm_share_phy_addr_blks_destroy(u32 devid)
{
    struct devmm_phy_addr_blk_mng *share_mng = &devmm_svm->share_phy_addr_blk_mng[devid];

    _devmm_phy_addr_blks_destroy(NULL, share_mng);
}

int devmm_phy_addr_blk_init_in_same_os(struct devmm_phy_addr_blk *blk, u32 share_devid, int share_id,
    u32 to_create_pg_num)
{
    struct devmm_phy_addr_blk *share_blk = NULL;
    int ret;

    devmm_drv_debug("In same os. (share_devid=%u; share_id=%d; to_create_pg_num=%u)\n",
        share_devid, share_id, to_create_pg_num);

    share_blk = devmm_share_phy_addr_blk_get(share_devid, share_id);
    if (share_blk == NULL) {
        devmm_drv_err("Get share blk fail. (share_devid=%u; share_id=%d)\n",
            share_devid, share_id);
        return -EBADR;
    }
    if (share_blk->pg_num != blk->pg_num) {
        devmm_drv_err("Page num is invalid. (blk_pg_num=%llu; share_blk_pg_num=%llu)\n",
            blk->pg_num, share_blk->pg_num);
        devmm_share_phy_addr_blk_put(share_blk);
        return -ERANGE;
    }
    if (share_blk->init_state != SVM_PHY_ADDR_BLK_IS_INITED) {
        devmm_drv_err("Share blk state is invalid. (init_state=%u)\n", share_blk->init_state);
        devmm_share_phy_addr_blk_put(share_blk);
        return -EINVAL;
    }

    ret = devmm_share_phy_addr_blk_init(blk, share_blk, to_create_pg_num, SVM_PYH_ADDR_BLK_IMPORT_TYPE);
    devmm_share_phy_addr_blk_put(share_blk);
    return ret;
}

