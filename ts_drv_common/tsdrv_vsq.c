/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
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
 * Create: 2022-7-20
 */
#include "drv_type.h"
#include "tsdrv_device.h"
#include "tsdrv_osal_mm.h"
#include "devdrv_cqsq.h"
#include "hvtsdrv_cqsq.h"
#define TSDRV_VM_INFO_MEM_SIZE REMAP_ALIGN(DEVDRV_SQ_INFO_OCCUPY_SIZE)
#define TSDRV_VM_INFO_ORDER (u32)ka_mm_get_order(DEVDRV_SQ_INFO_OCCUPY_SIZE)

int tsdrv_vsq_info_mem_create(u32 devid, u32 fid, u32 tsid)
{
#ifndef TSDRV_UT
    gfp_t gfp_flags = KA_GFP_KERNEL | __KA_GFP_COMP | __KA_GFP_ZERO;
    struct tsdrv_ts_resource *ts_res = NULL;
    struct tsdrv_vsq_info *vsq_info = NULL;
    u32 order = TSDRV_VM_INFO_ORDER;
    char *tmp = NULL;
    u32 i;

    ts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
    if (ts_res == NULL) {
        TSDRV_PRINT_ERR("dev_id=%u, fid=%u, tsid=%u.\n", devid, fid, tsid);
        return -EINVAL;
    }

    tmp = (char *)(uintptr_t)__get_free_pages(gfp_flags, order);
    if (tmp == NULL) {
        TSDRV_PRINT_ERR("Get free pages failed. (order=%u)\n", order);
        return -ENOMEM;
    }

    ts_res->mem_info[DEVDRV_VSQ_INFO_MEM].phy_addr = ka_mm_virt_to_phys(tmp);
    ts_res->mem_info[DEVDRV_VSQ_INFO_MEM].virt_addr = (phys_addr_t)(uintptr_t)tmp;
    ts_res->mem_info[DEVDRV_VSQ_INFO_MEM].size = TSDRV_VM_INFO_MEM_SIZE;

    for (i = 0; i < DEVDRV_MAX_SQ_NUM; i++) {
        vsq_info = tsdrv_calc_vsq_info((u64)(uintptr_t)tmp, i);
        vsq_info->index = i;
        vsq_info->head = 0;
        vsq_info->head_for_tsagent = 0;
        vsq_info->tail = 0;
    }

    ka_task_spin_lock_init(&ts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock);
#endif
    return 0;
}

void tsdrv_vsq_info_mem_destroy(u32 devid, u32 fid, u32 tsid)
{
#ifndef TSDRV_UT
    struct tsdrv_ts_resource *ts_res = NULL;
    u32 order = TSDRV_VM_INFO_ORDER;
    unsigned long addr, flags;

    ts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
    if (ts_res == NULL) {
        TSDRV_PRINT_ERR("dev_id=%u, fid=%u, tsid=%u.\n", devid, fid, tsid);
        return;
    }

    addr = (unsigned long)ts_res->mem_info[DEVDRV_VSQ_INFO_MEM].virt_addr;

    ka_task_spin_lock_irqsave(&ts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
    ts_res->mem_info[DEVDRV_VSQ_INFO_MEM].virt_addr = 0;
    ka_task_spin_unlock_irqrestore(&ts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);

    if (addr != 0) {
        ka_mm_free_pages(addr, order);
    }
#endif
}

int tsdrv_vsq_mem_create(struct tsdrv_ts_resource *ts_res, u32 vsq_id, u32 slot_size, u32 depth)
{
#ifndef TSDRV_UT
    struct tsdrv_device *tsdrv_dev = tsdrv_res_to_dev(ts_res);
    struct tsdrv_vsq_info *vsq_info = NULL;
    phys_addr_t info_mem;
    u32 size, chip_type;

    chip_type = devdrv_get_dev_chip_type(tsdrv_dev->devid);
    if (chip_type != HISI_CLOUD_V2) {
        depth = DEVDRV_MAX_SQ_DEPTH;
    }

    size = KA_MM_PAGE_ALIGN(depth * slot_size);
    info_mem = ts_res->mem_info[DEVDRV_VSQ_INFO_MEM].virt_addr;
    vsq_info = tsdrv_calc_vsq_info(info_mem, vsq_id);
    if (vsq_info->vsq_vaddr != 0) {
        TSDRV_PRINT_DEBUG("Repeat init. (devid=%u; tsid=%u; sqid=%u).\n", tsdrv_dev->devid, ts_res->tsid, vsq_id);
        return 0;
    }

    vsq_info->vsq_vaddr = (u64)(uintptr_t)tsdrv_alloc_pages_exact(size, KA_GFP_KERNEL);
    if (vsq_info->vsq_vaddr == 0) {
        TSDRV_PRINT_ERR("Alloc pages failed. (size=%u)\n", size);
        return -ENOMEM;
    }

    vsq_info->vsq_size = size;
    vsq_info->depth = depth;
    TSDRV_PRINT_DEBUG("Init vsq mem success. (devid=%u; tsid=%u; vsq_id=%u; depth=%u)\n",
        tsdrv_dev->devid, ts_res->tsid, vsq_id, depth);
#endif
    return 0;
}

void tsdrv_vsq_mem_destroy(struct tsdrv_ts_resource *ts_res, u32 vsq_id)
{
#ifndef TSDRV_UT
    struct tsdrv_vsq_info *vsq_info = NULL;
    phys_addr_t info_mem;

    info_mem = ts_res->mem_info[DEVDRV_VSQ_INFO_MEM].virt_addr;
    vsq_info = tsdrv_calc_vsq_info(info_mem, vsq_id);

    tsdrv_free_pages_exact((void *)(uintptr_t)vsq_info->vsq_vaddr, vsq_info->vsq_size);
    vsq_info->vsq_vaddr = 0;
    vsq_info->vsq_size = 0;
#endif
}

int tsdrv_get_vsq_phy_addr(struct tsdrv_ctx *ctx, u32 ts_id, u32 vsq_id, struct tsdrv_phy_addr_get *info)
{
#ifndef TSDRV_UT
    struct tsdrv_ts_resource *ts_res = tsdrv_ctx_to_ts_res(ctx, ts_id);
    struct devdrv_sq_sub_info *sq_sub = NULL;
    struct tsdrv_vsq_info *vsq_info = NULL;
    phys_addr_t paddr, info_mem;

    info_mem = ts_res->mem_info[DEVDRV_VSQ_INFO_MEM].virt_addr;
    vsq_info = tsdrv_calc_vsq_info(info_mem, vsq_id);
    sq_sub = tsdrv_get_sq_sub_info(ts_res, vsq_id);
    if ((info->offset + info->len) != (KA_MM_PAGE_ALIGN(sq_sub->size * sq_sub->depth))) {
        TSDRV_PRINT_ERR("Para is error. (sqId=%u; offset=%u; len=%u; sq_size=%lu)\n",
            vsq_id, info->offset, info->len, sq_sub->size * sq_sub->depth);
        return -EINVAL;
    }

    paddr = (phys_addr_t)ka_mm_virt_to_phys((void *)(uintptr_t)vsq_info->vsq_vaddr);
    info->paddr = paddr;
    info->paddr_len = info->len;
#endif
    return 0;
}

void tsdrv_clear_vsq_info(struct tsdrv_ts_resource *ts_res, u32 vsq_id)
{
#ifndef TSDRV_UT
    struct tsdrv_vsq_info *vsq_info = NULL;
    phys_addr_t info_mem;

    info_mem = ts_res->mem_info[DEVDRV_VSQ_INFO_MEM].virt_addr;
    vsq_info = tsdrv_calc_vsq_info(info_mem, vsq_id);

    vsq_info->head = 0;
    vsq_info->tail = 0;
    vsq_info->head_for_tsagent = 0;
#endif
}

u32 tsdrv_get_vsq_head(struct tsdrv_ts_resource *ts_res, u32 vsq_id)
{
    struct tsdrv_vsq_info *vsq_info = NULL;
    phys_addr_t info_mem;

    info_mem = ts_res->mem_info[DEVDRV_VSQ_INFO_MEM].virt_addr;
    vsq_info = tsdrv_calc_vsq_info(info_mem, vsq_id);

    return vsq_info->head;
}

void tsdrv_update_vsq_head(struct tsdrv_ts_resource *ts_res, u32 vsq_id, u32 vsq_head)
{
    struct tsdrv_vsq_info *vsq_info = NULL;
    phys_addr_t info_mem;

    info_mem = ts_res->mem_info[DEVDRV_VSQ_INFO_MEM].virt_addr;
    vsq_info = tsdrv_calc_vsq_info(info_mem, vsq_id);

    vsq_info->head = vsq_head;
}

void hvtsdrv_update_vsq_tail(struct tsdrv_ts_resource *ts_res, u32 vsq_id, u32 vsq_tail)
{
    struct tsdrv_vsq_info *vsq_info = NULL;
    unsigned long flags;

    ka_task_spin_lock_irqsave(&ts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
    vsq_info = hvtsdrv_get_vsq_info_inner(ts_res, vsq_id);
    if (vsq_info == NULL) {
        ka_task_spin_unlock_irqrestore(&ts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
        return;
    }
    vsq_info->tail = vsq_tail;
    ka_task_spin_unlock_irqrestore(&ts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
}

static int tsdrv_check_vsq_tail(struct tsdrv_ts_resource *ts_res, u32 vsq_id, u32 vsq_tail)
{
    u32 vsq_depth = DEVDRV_MAX_SQ_DEPTH;
#ifdef CFG_FEATURE_SUPPORT_VSQ_MNG
    struct tsdrv_vsq_info *vsq_info = NULL;

    vsq_info = hvtsdrv_get_vsq_info_inner(ts_res, vsq_id);
    if (vsq_info == NULL) {
        return -EINVAL;
    }
    vsq_depth = vsq_info->depth;
#endif
    if (vsq_tail >= vsq_depth) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("The vsq tail is invalid. (vsq_id=%ul vsq_tail=%u; depth=%u)\n",
            vsq_id, vsq_tail, vsq_depth);
        return -EINVAL;
#endif
    }

    return 0;
}

s32 tsdrv_vsq_msg_send(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    struct tsdrv_ts_resource *vts_res = NULL;
    struct tsdrv_id_inst id_inst;
    u32 vsqid, vsq_tail, sqe_num, chip_type, id_capacity;
    int tgid;
    int ret;

    id_inst.devid = tsdrv_get_devid_by_ctx(ctx);
    id_inst.fid = tsdrv_get_fid_by_ctx(ctx);
    id_inst.tsid = arg->tsid;
    tgid = ctx->tgid;
    vsqid = arg->normal_sq_send_para.sq_id;
    vsq_tail = arg->normal_sq_send_para.sq_tail;
    sqe_num = arg->normal_sq_send_para.sqe_num;

    TSDRV_PRINT_DEBUG("devid(%u), fid(%u), tsid(%u), tgid(%d), vsq_id(%u), vsq_tail(%u), sqe_num(%u).\n",
        id_inst.devid, id_inst.fid, id_inst.tsid, tgid, vsqid, vsq_tail, sqe_num);

    vts_res = tsdrv_ctx_to_ts_res(ctx, id_inst.tsid);
    chip_type = devdrv_get_dev_chip_type(id_inst.devid);
    if (chip_type == HISI_CLOUD_V2) {
#ifndef TSDRV_UT
        id_capacity = DEVDRV_MAX_SQ_NUM;
#endif
    } else {
        id_capacity = vts_res->id_res[TSDRV_SQ_ID].id_capacity;
    }
    if (vsqid >= id_capacity) {
        TSDRV_PRINT_ERR("Invalid para. (vsq_id=%u)\n", vsqid);
        return -EINVAL;
    }

    ret = tsdrv_sq_exist_check(vts_res, ctx, vsqid);
    if (ret != 0) {
        TSDRV_PRINT_ERR("vsq_id(%u) is not exist.\n", vsqid);
        return -EINVAL;
    }
    ret = tsdrv_check_vsq_tail(vts_res, vsqid, vsq_tail);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Failed to check vsq tail. (ret=%d; vsq_id=%ul vsq_tail=%u)\n",
            ret, vsqid, vsq_tail);
        return ret;
#endif
    }

    hvtsdrv_update_vsq_tail(vts_res, vsqid, vsq_tail);

#ifndef TSDRV_UT
    ret = hvtsdrv_notice_tsagent_vsq_proc(&id_inst, vsqid, NORMAL_VSQCQ_TYPE, sqe_num);
    if (ret != 0) {
        TSDRV_PRINT_ERR("notice tsagent faild, devid(%u), fid(%u), tsid(%u), vsqid(%u).\n",
            id_inst.devid, id_inst.fid, id_inst.tsid, vsqid);
        return -EINVAL;
    }
#endif
    TSDRV_PRINT_DEBUG("devid(%u), fid(%u), tsid(%u), tgid(%d), vsqid(%u), vsq_tail(%u).\n",
        id_inst.devid, id_inst.fid, id_inst.tsid, tgid,
        vsqid, vsq_tail);
    return 0;
}
