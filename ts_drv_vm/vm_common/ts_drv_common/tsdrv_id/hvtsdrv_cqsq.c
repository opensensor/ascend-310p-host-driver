/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2023. All rights reserved.
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
 * Create: 2019-10-15
 */
#include "devdrv_cqsq.h"
#include "devdrv_devinit.h"
#include "devdrv_id.h"
#include "devdrv_cb.h"
#include "tsdrv_capacity.h"

#include "hvtsdrv_cqsq.h"
#include "hvtsdrv_id.h"
#include "hvtsdrv_proc.h"
#include "hvtsdrv_cb.h"
#include "shm_sqcq.h"
#include "logic_cq.h"
#include "tsdrv_logic_cq.h"
#include "tsdrv_common.h"

STATIC int hvtsdrv_cq_init(u32 devid, u32 fid, u32 tsid, struct tsdrv_ts_resource *v_ts_res,
    struct tsdrv_ts_resource *p_ts_res)
{
    struct tsdrv_vcq_info *vcq_info = NULL;
    struct tsdrv_id_info *id_info = NULL;
    struct res_id_move move_info;
    u32 id_cnt, j;

    id_cnt = v_ts_res->id_res[TSDRV_CQ_ID].id_capacity;
    if (id_cnt == TSDRV_INVALIC_CAPACITY) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("tsid=%u.\n", tsid);
        return -EINVAL;
#endif
    }

    v_ts_res->id_res[TSDRV_CQ_ID].pm_id = tsdrv_kzalloc((id_cnt + 1) * sizeof(u32), KA_GFP_KERNEL);
    if (v_ts_res->id_res[TSDRV_CQ_ID].pm_id == NULL) {
#ifndef TSDRV_UT
       TSDRV_PRINT_ERR("kzalloc failed, dev-%u ts-%u.\n", devid, tsid);
        return -ENOMEM;
#endif
    }

    for (j = 0; j <= id_cnt; j++) {
        if (j != id_cnt) {
            move_info.sync_flag = NEED_SYNC_FROM_DEVICE;
            move_info.id_type = TSDRV_CQ_ID;
            move_info.src_id = &p_ts_res->id_res[TSDRV_CQ_ID];
            move_info.des_id = &v_ts_res->id_res[TSDRV_CQ_ID];
            id_info = tsdrv_dev_res_id_move(devid, fid, tsid, &move_info);
            if (id_info == NULL) {
#ifndef TSDRV_UT
                tsdrv_kfree(v_ts_res->id_res[TSDRV_CQ_ID].pm_id);
                v_ts_res->id_res[TSDRV_CQ_ID].pm_id = NULL;
                TSDRV_PRINT_ERR("dev-%u ts-%u cnt-%u resouce move fail.\n", devid, tsid, j);
                return -EPERM;
#endif
            }
            id_info->virt_id = j;
            id_info->id = id_info->virt_id;
            v_ts_res->id_res[TSDRV_CQ_ID].pm_id[j] = id_info->phy_id;
        }

        vcq_info = hvtsdrv_get_vcq_info(v_ts_res, j);
        if (vcq_info == NULL) {
#ifndef TSDRV_UT
            tsdrv_kfree(v_ts_res->id_res[TSDRV_CQ_ID].pm_id);
            v_ts_res->id_res[TSDRV_CQ_ID].pm_id = NULL;
            return -EINVAL;
#endif
        }
        vcq_info->devid = devid;
        vcq_info->fid = fid;
        vcq_info->tsid = tsid;

        vcq_info->index = j;
        vcq_info->head = 0;
        vcq_info->tail = 0;
        vcq_info->slot_size = DEVDRV_CQ_SLOT_SIZE;
        vcq_info->alloc_status = SQCQ_INACTIVE;
        ka_task_spin_lock_init(&vcq_info->spinlock);

        vcq_info->top_half_tick = 0;

        /* the wq free in sqcq uninit function */
        vcq_info->update_cq_wq = ka_task_create_singlethread_workqueue("update_cq_report");
        if (vcq_info->update_cq_wq == NULL) {
#ifndef TSDRV_UT
            tsdrv_kfree(v_ts_res->id_res[TSDRV_CQ_ID].pm_id);
            v_ts_res->id_res[TSDRV_CQ_ID].pm_id = NULL;
            TSDRV_PRINT_ERR("create cq wq failed.\n");
            return -EPERM;
#endif
        }
        if (j < id_cnt) {
            KA_TASK_INIT_WORK(&vcq_info->update_cq_work, hvtsdrv_update_cq_report);
        } else {
            KA_TASK_INIT_WORK(&vcq_info->update_cq_work, hvtsdrv_cb_update_cq_report);
            vcq_info->slot_size = DEVDRV_MAX_CBCQ_SIZE;
            TSDRV_PRINT_INFO("cbcq virtid is-%u\n", j);
        }
    }
    v_ts_res->id_res[TSDRV_CQ_ID].id_addr = p_ts_res->id_res[TSDRV_CQ_ID].id_addr;

    /* callback cq */
    v_ts_res->id_res[TSDRV_CQ_ID].pm_id[id_cnt] = GET_CALLBACK_CQ_ID(fid);

    return 0;
}

STATIC int hvtsdrv_sq_init(u32 devid, u32 fid, u32 tsid, struct tsdrv_ts_resource *v_ts_res,
    struct tsdrv_ts_resource *p_ts_res)
{
    struct tsdrv_vsq_info *vsq_info = NULL;
    struct tsdrv_id_info *id_info = NULL;
    struct res_id_move move_info;
    phys_addr_t vsq_info_mem_addr;
    u32 id_cnt, j;

    vsq_info_mem_addr = v_ts_res->mem_info[DEVDRV_VSQ_INFO_MEM].virt_addr;

    id_cnt = v_ts_res->id_res[TSDRV_SQ_ID].id_capacity;
    if (id_cnt == TSDRV_INVALIC_CAPACITY) {
#ifndef TSDRV_UT
        TSDRV_PRINT_INFO("tsid=%u.\n", tsid);
        return 0;
#endif
    }

    v_ts_res->id_res[TSDRV_SQ_ID].pm_id = tsdrv_kzalloc(id_cnt * sizeof(u32), KA_GFP_KERNEL);
    if (v_ts_res->id_res[TSDRV_SQ_ID].pm_id == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("kzalloc failed, dev-%u ts-%u.\n", devid, tsid);
        return -ENOMEM;
#endif
    }

    for (j = 0; j < id_cnt; j++) {
        move_info.sync_flag = NEED_SYNC_FROM_DEVICE;
        move_info.id_type = TSDRV_SQ_ID;
        move_info.src_id = &p_ts_res->id_res[TSDRV_SQ_ID];
        move_info.des_id = &v_ts_res->id_res[TSDRV_SQ_ID];
        id_info = tsdrv_dev_res_id_move(devid, fid, tsid, &move_info);
        if (id_info == NULL) {
#ifndef TSDRV_UT
            tsdrv_kfree(v_ts_res->id_res[TSDRV_SQ_ID].pm_id);
            v_ts_res->id_res[TSDRV_SQ_ID].pm_id = NULL;
            TSDRV_PRINT_ERR("dev-%u ts-%u cnt-%u resouce move fail.\n", devid, tsid, j);
            return -EPERM;
#endif
        }
        id_info->virt_id = j;
        id_info->id = id_info->virt_id;
        v_ts_res->id_res[TSDRV_SQ_ID].pm_id[j] = id_info->phy_id;

        vsq_info = tsdrv_calc_vsq_info(vsq_info_mem_addr, j);
        vsq_info->index = j;
        vsq_info->head = 0;
        vsq_info->head_for_tsagent = 0;
        vsq_info->tail = 0;
    }
    v_ts_res->id_res[TSDRV_SQ_ID].id_addr = p_ts_res->id_res[TSDRV_SQ_ID].id_addr;

    return 0;
}

int hvtsdrv_cqsq_init(u32 devid, u32 fid)
{
    struct tsdrv_dev_resource *pm_dev_res = NULL;
    struct tsdrv_dev_resource *vm_dev_res = NULL;
    u32 tsnum, tsid;
    int ret;

    vm_dev_res = tsdrv_get_dev_resource(devid, fid);
    pm_dev_res = tsdrv_get_dev_resource(devid, TSDRV_PM_FID);
    if (vm_dev_res == NULL || pm_dev_res == NULL) {
        TSDRV_PRINT_ERR("get dev resource failed, devid(%u).\n", devid);
        return -EINVAL;
    }

    tsnum = tsdrv_get_dev_tsnum(devid);
    for (tsid = 0; tsid < tsnum; tsid++) {
        ret = hvtsdrv_sq_init(devid, fid, tsid, &vm_dev_res->ts_resource[tsid], &pm_dev_res->ts_resource[tsid]);
        if (ret != 0) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("dev-%u fid-%u ts-%u sq_int fail.\n", devid, fid, tsid);
            goto release_resouce;
#endif
        }

        ret = hvtsdrv_cq_init(devid, fid, tsid, &vm_dev_res->ts_resource[tsid], &pm_dev_res->ts_resource[tsid]);
        if (ret != 0) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("dev-%u fid-%u ts-%u cq_int fail.\n", devid, fid, tsid);
            goto release_resouce;
#endif
        }
    }

    return 0;
#ifndef TSDRV_UT
release_resouce:
    hvtsdrv_cqsq_uninit(devid, fid);
    return -EPERM;
#endif
}

void hvtsdrv_cqsq_uninit(u32 devid, u32 fid)
{
    struct tsdrv_ts_resource *vts_res = NULL;
    struct tsdrv_vcq_info *vcq_info = NULL;
    ka_workqueue_struct_t *wq = NULL;
    unsigned long flags;
    u32 tsid, tsnum, i;

    tsdrv_res_id_uninit(devid, fid, TSDRV_SQ_ID);

    tsnum = tsdrv_get_dev_tsnum(devid);
    for (tsid = 0; tsid < tsnum; tsid++) {
        vts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
        /* include destroy callback wq */
        for (i = 0; i <= vts_res->id_res[TSDRV_CQ_ID].id_capacity; i++) {
            vcq_info = hvtsdrv_get_vcq_info(vts_res, i);
            if (vcq_info == NULL) {
                continue;
            }
            if (vcq_info->update_cq_wq != NULL) {
                wq = vcq_info->update_cq_wq;
                ka_task_spin_lock_irqsave(&vts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
                vcq_info->update_cq_wq = NULL;
                ka_task_spin_unlock_irqrestore(&vts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
                ka_task_destroy_workqueue(wq);
            }
        }
    }

    tsdrv_res_id_uninit(devid, fid, TSDRV_CQ_ID);
}

s32 hvtsdrv_sqcq_alloc(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    int ret;

    ret = devdrv_ioctl_sqcq_alloc(ctx, arg);
    TSDRV_PRINT_DEBUG("tgid(%d), vsqid(%u), vcqid(%u)\n", ctx->tgid,
        arg->sqcq_alloc_para.sqId, arg->sqcq_alloc_para.cqId);

    return ret;
}

s32 hvtsdrv_sqcq_free(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    TSDRV_PRINT_DEBUG("tgid(%d), vsqid(%u), vcqid(%u)\n", ctx->tgid,
        arg->sqcq_free_para.sqId, arg->sqcq_free_para.cqId);

    return devdrv_ioctl_sqcq_free(ctx, arg);
}

void hvtsdrv_clear_vcq_info(struct tsdrv_ts_resource *ts_res, u32 vcq_id)
{
    struct tsdrv_vcq_info *vcq_info = NULL;
    unsigned long flags;

    ka_task_spin_lock_irqsave(&ts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
    vcq_info = hvtsdrv_get_vcq_info(ts_res, vcq_id);
    if (vcq_info == NULL) {
        ka_task_spin_unlock_irqrestore(&ts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
        return;
    }
    vcq_info->head = 0;
    vcq_info->tail = 0;
    ka_task_spin_unlock_irqrestore(&ts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
}

void hvtsdrv_clear_vsq_info(struct tsdrv_ts_resource *ts_res, u32 vsq_id)
{
    struct tsdrv_vsq_info *vsq_info = NULL;
    unsigned long flags;

    ka_task_spin_lock_irqsave(&ts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
    vsq_info = hvtsdrv_get_vsq_info_inner(ts_res, vsq_id);
    if (vsq_info == NULL) {
        ka_task_spin_unlock_irqrestore(&ts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
        return;
    }

    vsq_info->head_for_tsagent = 0;
    vsq_info->head = 0;
    vsq_info->tail = 0;
    ka_task_spin_unlock_irqrestore(&ts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
}

int hvtsdrv_get_vsq_mem_phy_addr(struct tsdrv_ctx *ctx, u32 tsId, u32 sqId, struct tsdrv_phy_addr_get *info)
{
    struct tsdrv_ts_resource *ts_res = tsdrv_ctx_to_ts_res(ctx, tsId);
    struct devdrv_sq_sub_info *sq_sub = NULL;
#ifndef TSDRV_UT
    struct page *page = NULL;
#endif
    phys_addr_t vsq_vaddr;
    u32 phy_sqid;

    phy_sqid = tsdrv_vrit_to_physic_id(&ts_res->id_res[TSDRV_SQ_ID], sqId);
    sq_sub = tsdrv_get_sq_sub_info(ts_res, phy_sqid);
    if ((info->offset + info->len) != KA_MM_PAGE_ALIGN(sq_sub->size * sq_sub->depth)) {
        TSDRV_PRINT_ERR("Para is error. (offset=%u; len=%u; sq_size=%u)\n",
            info->offset, info->len, (u32)(sq_sub->size * sq_sub->depth));
        return -EINVAL;
    }

    vsq_vaddr = ts_res->mem_info[DEVDRV_SQ_MEM].virt_addr + sqId * DEVDRV_SQ_SLOT_SIZE * DEVDRV_MAX_SQ_DEPTH;
#ifndef TSDRV_UT
    page = vmalloc_to_page((void *)(uintptr_t)(vsq_vaddr + info->offset));
    if (page == NULL) {
        TSDRV_PRINT_ERR("Vmalloc to page failed. (offset=%u; len=%u; vsq_vaddr=0x%pK)\n",
            info->offset, info->len, (void *)(uintptr_t)vsq_vaddr);
        return -EFAULT;
    }
    info->paddr = (phys_addr_t)(uintptr_t)ka_mm_page_to_phys(page);
    info->paddr_len = KA_MM_PAGE_SIZE;
#else
    info->paddr = 0x1234;
    info->paddr_len = KA_MM_PAGE_ALIGN(DEVDRV_SQ_SLOT_SIZE * DEVDRV_MAX_SQ_DEPTH);
#endif

    return 0;
}

void hvtsdrv_copy_cq_to_vcq(struct devdrv_ts_cq_info *cq_info, phys_addr_t cq_vaddr,
    phys_addr_t vcq_vaddr, u32 old_tail)
{
    int ret = 0;
    u32 count, offset, size;

    if (cq_info->tail == old_tail) {
        return;
    }
#ifndef TSDRV_UT
    if (cq_info->head > cq_info->tail) {
        if (old_tail >= cq_info->head) {
            count = DEVDRV_MAX_SQ_DEPTH - old_tail;
            offset = old_tail * cq_info->slot_size;
            size = count * cq_info->slot_size;
            cq_info->receive_count += count;
            ret = memcpy_s((void *)(uintptr_t)vcq_vaddr + offset, size, (void *)(uintptr_t)cq_vaddr + offset, size);
            ka_wmb();
            TSDRV_PRINT_DEBUG("cqid(%u), head(%u), tail(%u), count(%u), offset(0x%x), size(0x%x)\n",
                cq_info->index, cq_info->head, cq_info->tail, count, offset, size);

            if (cq_info->tail > 0) {
                count = cq_info->tail - 0;
                offset = 0;
                size = count * cq_info->slot_size;
                cq_info->receive_count += count;
                ret |= memcpy_s((void *)(uintptr_t)vcq_vaddr, size, (void *)(uintptr_t)cq_vaddr, size);
                ka_wmb();
                TSDRV_PRINT_DEBUG("cqid(%u), head(%u), tail(%u), count(%u), offset(0x%x), size(0x%x)\n",
                    cq_info->index, cq_info->head, cq_info->tail, count, offset, size);
            }
        } else {
            count = cq_info->tail - old_tail;
            offset = old_tail * cq_info->slot_size;
            size = count * cq_info->slot_size;
            cq_info->receive_count += count;
            ret |= memcpy_s((void *)(uintptr_t)vcq_vaddr + offset, size, (void *)(uintptr_t)cq_vaddr + offset, size);
            ka_wmb();
            TSDRV_PRINT_DEBUG("cqid(%u), head(%u), tail(%u), count(%u), offset(0x%x), size(0x%x)\n",
                cq_info->index, cq_info->head, cq_info->tail, count, offset, size);
        }
    }
#endif
    if (cq_info->head < cq_info->tail) {
        count = cq_info->tail - old_tail;
        offset = old_tail * cq_info->slot_size;
        size = count * cq_info->slot_size;
        cq_info->receive_count += count;
        ret |= memcpy_s((void *)(uintptr_t)vcq_vaddr + offset, size, (void *)(uintptr_t)cq_vaddr + offset, size);
        ka_wmb();
        TSDRV_PRINT_DEBUG("cqid(%u), head(%u), tail(%u), count(%u), offset(0x%x), size(0x%x)\n",
            cq_info->index, cq_info->head, cq_info->tail, count, offset, size);
    }
    if (ret != 0) {
        TSDRV_PRINT_ERR("memcpy failed (ret=%d)\n", ret);
    }
}

s32 hvtsdrv_wait_cq_report(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    return devdrv_ioctl_report_wait(ctx, arg);
}

void hvtsdrv_update_vcq_head(struct tsdrv_ts_resource *ts_res, u32 vcq_id, u32 cq_head)
{
    struct tsdrv_vcq_info *vcq_info = NULL;
    unsigned long flags;

    ka_task_spin_lock_irqsave(&ts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
    vcq_info = hvtsdrv_get_vcq_info(ts_res, vcq_id);
    if (vcq_info == NULL) {
        ka_task_spin_unlock_irqrestore(&ts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
        return;
    }
    vcq_info->head = cq_head;
    ka_task_spin_unlock_irqrestore(&ts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
}

void hvtsdrv_update_vsq_head(struct tsdrv_ts_resource *ts_res, u32 vsq_id, u32 vsq_head)
{
    struct tsdrv_vsq_info *vsq_info = NULL;
    unsigned long flags;

    ka_task_spin_lock_irqsave(&ts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
    vsq_info = hvtsdrv_get_vsq_info_inner(ts_res, vsq_id);
    if (vsq_info == NULL) {
        ka_task_spin_unlock_irqrestore(&ts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
        return;
    }
    vsq_info->head = vsq_head;
    ka_task_spin_unlock_irqrestore(&ts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
}
#ifndef TSDRV_UT
STATIC void hvtsdrv_get_vsq_head(struct tsdrv_ts_resource *ts_res, u32 vsq_id, u32 *sq_head)
{
    struct tsdrv_vsq_info *vsq_info = NULL;
    unsigned long flags;

    ka_task_spin_lock_irqsave(&ts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
    vsq_info = hvtsdrv_get_vsq_info_inner(ts_res, vsq_id);
    if (vsq_info == NULL) {
        ka_task_spin_unlock_irqrestore(&ts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
        return;
    }
    *sq_head = vsq_info->head;
    ka_task_spin_unlock_irqrestore(&ts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
}
#endif
s32 hvtsdrv_cq_report_release(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
#ifndef TSDRV_UT
    struct tsdrv_ts_resource *vts_res = NULL;
    u32 cq_head = arg->sqcq_data_para.val;
    u32 vcq_id = arg->sqcq_data_para.id;
    u32 tsid = arg->tsid;
    u32 devid, fid, tgid;
    u32 cq_id;
    int ret;

    devid = tsdrv_get_devid_by_ctx(ctx);
    fid = tsdrv_get_fid_by_ctx(ctx);
    tgid = ctx->tgid;
    vts_res = tsdrv_ctx_to_ts_res(ctx, tsid);
    if (vcq_id >= vts_res->id_res[TSDRV_CQ_ID].id_capacity || cq_head >= DEVDRV_MAX_CQ_DEPTH) {
        TSDRV_PRINT_ERR("invalid para, vcq_id(%u), cq_head(%u).\n", vcq_id, cq_head);
        return -EINVAL;
    }

    ka_task_mutex_unlock(&vts_res->id_res[TSDRV_SQ_ID].id_mutex_t);
    ret = tsdrv_cq_exist_check(vts_res, ctx, vcq_id);
    if (ret != 0) {
        ka_task_mutex_unlock(&vts_res->id_res[TSDRV_SQ_ID].id_mutex_t);
        TSDRV_PRINT_ERR("cq_id(%u) is not exist.\n", vcq_id);
        return -EINVAL;
    }

    cq_id = tsdrv_vrit_to_physic_id(&vts_res->id_res[TSDRV_CQ_ID], vcq_id);
    tsdrv_set_cq_doorbell(devid, tsid, cq_id, cq_head);

    if (tsdrv_is_in_vm(devid)) {
        hvtsdrv_update_vcq_head(vts_res, vcq_id, cq_head);
        TSDRV_PRINT_DEBUG("devid(%u), fid(%u), tsid(%u), tgid(%u), cq_id(%u), head(%u).\n",
            devid, fid, tsid, tgid, vcq_id, cq_head);
    }

    tsdrv_update_cq_head(vts_res, cq_id, cq_head);
    ka_task_mutex_unlock(&vts_res->id_res[TSDRV_SQ_ID].id_mutex_t);

    TSDRV_PRINT_DEBUG("devid(%u), fid(%u), tsid(%u), tgid(%u), cq_id(%u), vcq_id(%u) head(%u).\n",
        devid, fid, tsid, tgid, cq_id, vcq_id, cq_head);
#endif
    return 0;
}

s32 hvtsdrv_get_sq_head(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
#ifndef TSDRV_UT
    struct tsdrv_ts_resource *vts_res = NULL;
    u32 devid, fid, tgid;
    u32 vsq_id, tsid;
    u32 vsq_head = 0;
    int ret;

    devid = tsdrv_get_devid_by_ctx(ctx);
    fid = tsdrv_get_fid_by_ctx(ctx);
    tgid = ctx->tgid;
    tsid = arg->tsid;
    vsq_id = arg->sqcq_data_para.id;
    vts_res = tsdrv_ctx_to_ts_res(ctx, tsid);
    if (vsq_id >= vts_res->id_res[TSDRV_SQ_ID].id_capacity) {
        TSDRV_PRINT_ERR("invalid para, vsq_id(%u).\n", vsq_id);
        return -EINVAL;
    }

    ka_task_mutex_unlock(&vts_res->id_res[TSDRV_SQ_ID].id_mutex_t);
    ret = tsdrv_sq_exist_check(vts_res, ctx, vsq_id);
    if (ret != 0) {
        ka_task_mutex_unlock(&vts_res->id_res[TSDRV_SQ_ID].id_mutex_t);
        TSDRV_PRINT_ERR("vsq_id(%u) is not exist.\n", vsq_id);
        return -EINVAL;
    }

    hvtsdrv_get_vsq_head(vts_res, vsq_id, &vsq_head);
    arg->sqcq_data_para.val = vsq_head;

    TSDRV_PRINT_DEBUG("devid(%u), fid(%u), tsid(%u), tgid(%u), vsq_id(%u), head(%u).\n",
        devid, fid, tsid, tgid, vsq_id, vsq_head);

    ka_task_mutex_unlock(&vts_res->id_res[TSDRV_SQ_ID].id_mutex_t);
#endif
    return 0;
}

s32 hvtsdrv_cq_wake_up(u32 devid, u32 fid, u32 tsid, ka_pid_t tgid, struct vtsdrv_vcq_data_para *vcq_data)
{
    struct vtsdrv_msg msg;
    int ret;

    TSDRV_PRINT_DEBUG("devid(%u), fid(%u), tsid(%u), tgid(%d), cqid(%u), tail(%u).\n",
        devid, fid, tsid, tgid, vcq_data->vcq_id, vcq_data->vcq_tail);

    msg.cmd = HVTSDRV_DEV_PROC;
    msg.hvdev_proc.tgid = tgid;
    msg.hvdev_proc.proc_msg.tsid = tsid;
    msg.hvdev_proc.proc_msg.vcq_data_para.vcq_id = vcq_data->vcq_id;
    msg.hvdev_proc.proc_msg.vcq_data_para.vcq_tail = vcq_data->vcq_tail;
    msg.hvdev_proc.proc_msg.vcq_data_para.unique_id = vcq_data->unique_id;
    msg.hvdev_proc.proc_msg.vcq_data_para.cq_type = vcq_data->cq_type;
    ret = hvtsdrv_vpc_msg_send(devid, fid, &msg);
    if (ret != 0) {
        return ret;
    }

    TSDRV_PRINT_DEBUG("devid(%u), fid(%u), tsid(%u), tgid(%d), cqid(%u), tail(%u).\n",
        devid, fid, tsid, tgid, vcq_data->vcq_id, vcq_data->vcq_tail);
    return 0;
}

#ifndef TSDRV_UT
static u32 hvtsdrv_report_get_phase(struct devdrv_ts_cq_info *cq_info)
{
    struct devdrv_cq_sub_info *cq_sub_info = (struct devdrv_cq_sub_info *)cq_info->cq_sub;
    u32 phase;

    if (cq_info->type == LOGIC_SQCQ_TYPE) {
        struct tag_ts_logic_cq_report_msg *report =
            (struct tag_ts_logic_cq_report_msg *)((uintptr_t)((unsigned long)cq_sub_info->virt_addr +
            (unsigned long)cq_info->slot_size * cq_info->tail));

        phase = report->phase;
        if (phase == cq_info->phase) {
            TSDRV_PRINT_DEBUG("Get logic type cq report. (logic_cqid=%u; SOP=%u; MOP=%u; EOP=%u; stream_id=%u; "
                "task_id=%u; cq_info_tail=%u, slot_size=%u, phase=%u)\n",
                (u32)report->logic_cq_id, (u32)report->SOP, (u32)report->MOP, (u32)report->EOP,
                (u32)report->stream_id, (u32)report->task_id, cq_info->tail, (u32)cq_info->slot_size, phase);
        }
    } else {
        struct devdrv_report *report = (struct devdrv_report *)((uintptr_t)((unsigned long)cq_sub_info->virt_addr +
            (unsigned long)cq_info->slot_size * cq_info->tail));
        phase = devdrv_report_get_phase(report);
    }
    return phase;
}

static void tsdrv_update_cq_tail(struct tsdrv_ts_resource *ts_res,
    struct devdrv_ts_cq_info *cq_info, struct devdrv_cq_sub_info *cq_sub_info)
{
    u32 head_for_check = cq_info->head;
    u32 next_tail;

    while (hvtsdrv_report_get_phase(cq_info) == cq_info->phase) {
        TSDRV_PRINT_DEBUG("(phase=%u; tail=%u)\n", cq_info->phase, cq_info->tail);
        /**
         * In the ARM environment:
         * 1.Out-of-order may cause the TS driver to read other members of the report structure (read instruction 1)
         *   to be executed before the phase comparison instruction (read instruction 2);
         * 2.The time when TS writes report is between [read instruction 1] and [read instruction 2];
         * This error timing may cause [Read instruction 1] to read the old value.
         */
        ka_rmb();
        next_tail = (cq_info->tail + 1) % cq_info->depth;
        /* cq_tail cannot exceed cq_head, if it exceeds, the report will be lost */
        if (next_tail == head_for_check) {
            TSDRV_PRINT_ERR("next_tail=%u; head_for_check=%u.\n", next_tail, head_for_check);
            break;
        }
        next_tail = cq_info->tail + 1;
        if (next_tail > (cq_info->depth - 1)) {
            cq_info->phase = ((cq_info->phase == DEVDRV_PHASE_STATE_0) ? DEVDRV_PHASE_STATE_1 : DEVDRV_PHASE_STATE_0);
            cq_info->tail = 0;
        } else {
            cq_info->tail++;
        }
    }
}
#endif

void hvtsdrv_update_cq_report(ka_work_struct_t *work)
{
#ifndef TSDRV_UT
    struct tsdrv_ts_resource *vts_res = NULL;
    struct devdrv_cq_sub_info *cq_sub_info = NULL;
    struct devdrv_ts_cq_info *cq_info = NULL;
    struct tsdrv_vcq_info *vcq_info = NULL;
    struct vtsdrv_vcq_data_para vcq_data;
    u32 tgid, cqid, old_tail, delay_time;
    phys_addr_t sqcq_info_mem_addr;
    u64 bottom_tick = (u64)jiffies;
    phys_addr_t vcq_vaddr;
    int ret;

    vcq_info = ka_container_of(work, struct tsdrv_vcq_info, update_cq_work); 
    delay_time = (unsigned int)ka_system_jiffies_to_msecs(bottom_tick - vcq_info->top_half_tick);
    if (delay_time > TSDRV_CQ_WORK_CONSUME_MAX) {
        TSDRV_PRINT_WARN("work dispatch consume vcqid(%u) (%u)ms\n", vcq_info->index, delay_time);
    }
    vts_res = tsdrv_get_ts_resoruce(vcq_info->devid, vcq_info->fid, vcq_info->tsid);
    cqid = tsdrv_vrit_to_physic_id(&vts_res->id_res[TSDRV_CQ_ID], vcq_info->index);

    sqcq_info_mem_addr = vts_res->mem_info[DEVDRV_INFO_MEM].virt_addr;
    cq_info = devdrv_calc_cq_info(sqcq_info_mem_addr, cqid);
    cq_sub_info = (struct devdrv_cq_sub_info *)cq_info->cq_sub;
    TSDRV_PRINT_DEBUG("(vcqid=%u; head=%u; tail=%u; cqid=%u; head=%u; tail=%u; slot_size=%u)\n",
        vcq_info->index, vcq_info->head, vcq_info->tail, cq_info->index, cq_info->head,
        cq_info->tail, (u32)cq_info->slot_size);

   ka_task_spin_lock_irq(&cq_sub_info->spinlock);
    if (cq_sub_info->virt_addr == 0) {
        ka_task_spin_unlock_irq(&cq_sub_info->spinlock);
        TSDRV_PRINT_ERR("invalid para, cqid %u ctx is %s, vaddr is %s.\n", cq_info->index,
            cq_sub_info->ctx == NULL ? "null" : "not null",
            cq_sub_info->virt_addr == 0 ? "null" : "not null");
        return;
    }

    old_tail = cq_info->tail;
    /* 1. update cq report, include cq tail/sq head/vsq head */
    if (cq_info->type == LOGIC_SQCQ_TYPE) {
        tsdrv_update_cq_tail(vts_res, cq_info, cq_sub_info);
        tgid = 0;
        vcq_data.unique_id = 0;
    } else {
        tsdrv_update_cq_tail_and_sq_head(vts_res, cq_info, cq_sub_info);
        tgid = cq_sub_info->ctx->tgid;
        vcq_data.unique_id = cq_sub_info->ctx->unique_id;
    }
    /* if the report was copied, it needs not to copy when wq is scheduled */
    if (cq_info->tail == old_tail) {
        ka_task_spin_unlock_irq(&cq_sub_info->spinlock);
        TSDRV_PRINT_DEBUG("no new reports: devid(%u), fid(%u), tsid(%u), tgid(%u), cqid(%u), tail(%u), old_tail(%u)\n",
            vcq_info->devid, vcq_info->fid, vcq_info->tsid, tgid, cq_info->index, cq_info->tail, old_tail);
        return;
    }

    /* 2. copy cq to vcq */
    if (cq_info->type == LOGIC_SQCQ_TYPE) {
        vcq_vaddr = vts_res->mem_info[DEVDRV_CQ_MEM].virt_addr +
            (phys_addr_t)vcq_info->index * DEVDRV_MAX_CQ_DEPTH * cq_info->slot_size;
    } else {
        struct tsdrv_dev_resource *dev_res = tsdrv_get_dev_resource(vcq_info->devid, vcq_info->fid);
        TSDRV_PRINT_DEBUG("Show vm page size. (devid=%u; fid=%u; PAGE_SIZE=%u)\n",
            vcq_info->devid, vcq_info->fid, dev_res->page_size);
        vcq_vaddr = vts_res->mem_info[DEVDRV_CQ_MEM].virt_addr +
            (phys_addr_t)vcq_info->index * ROUND_UP(DEVDRV_MAX_CQ_DEPTH * cq_info->slot_size, dev_res->page_size);
    }
    hvtsdrv_copy_cq_to_vcq(cq_info, cq_sub_info->virt_addr, vcq_vaddr, old_tail);

    /* 3. update vcq tail */
    vcq_info->tail = cq_info->tail;

    vcq_data.vcq_id = vcq_info->index;
    vcq_data.vcq_tail = vcq_info->tail;
    vcq_data.cq_type = (u32)cq_info->type;
    ka_task_spin_unlock_irq(&cq_sub_info->spinlock);

    /* 4. send vpc msg to vm */
    ret = hvtsdrv_cq_wake_up(vcq_info->devid, vcq_info->fid, vcq_info->tsid, tgid, &vcq_data);
    if (ret != 0) {
        TSDRV_PRINT_ERR("vcqid %u wake up failed\n", vcq_info->index);
        return;
    }
    TSDRV_PRINT_DEBUG("Update_cq_report success. (vcqid=%u; cqid=%u; tail=%u)\n", vcq_info->index,
        cq_info->index, vcq_info->tail);
#endif
}

#ifndef TSDRV_UT
STATIC void hvtsdrv_cq_handler_virtual(u32 devid, u32 fid, u32 tsid, struct devdrv_ts_cq_info *cq_info)
{
    struct devdrv_cq_sub_info *cq_sub_info = NULL;
    struct tsdrv_ts_resource *vts_res = NULL;
    struct tsdrv_id_info *cq_id_info = NULL;
    struct tsdrv_vcq_info *vcq_info = NULL;
    unsigned long flags;
    u32 phase;

    cq_sub_info = (struct devdrv_cq_sub_info *)cq_info->cq_sub;
    phase = hvtsdrv_report_get_phase(cq_info);
    TSDRV_PRINT_DEBUG("(virt_addr=0x%pK; slot_size=%lu; tail=%u; cq_phase=%u; report_phase=%u)\n",
        (void *)(uintptr_t)cq_sub_info->virt_addr, (unsigned long)cq_info->slot_size,
        cq_info->tail, cq_info->phase, phase);
    if (phase == cq_info->phase) {
        TSDRV_PRINT_DEBUG("(fid=%u; cqid=%u; head=%u; tail=%u; cq_phase=%u; report_phase=%u)\n",
            fid, cq_info->index, cq_info->head, cq_info->tail, cq_info->phase, phase);

        vts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
        cq_id_info = devdrv_find_one_id(&vts_res->id_res[TSDRV_CQ_ID], cq_info->index);
        if (cq_id_info == NULL) {
            TSDRV_PRINT_ERR("invalid para, devid(%u), fid(%u), tsid(%u), cqid(%u)\n",
                devid, fid, vts_res->tsid, cq_info->index);
            return;
        }

        ka_task_spin_lock_irqsave(&vts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
        vcq_info = hvtsdrv_get_vcq_info(vts_res, cq_id_info->virt_id);
        if (vcq_info == NULL) {
            ka_task_spin_unlock_irqrestore(&vts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
            TSDRV_PRINT_ERR("The hvtsdrv_get_vcq_info error. (devid=%u, fid=%u, tsid=%u, cqid=%u)\n",
                devid, fid, tsid, cq_info->index);
            return;
        }
        vcq_info->top_half_tick = (u64)jiffies;
        if (vcq_info->update_cq_wq != NULL) {
            (void)ka_task_queue_work(vcq_info->update_cq_wq, &vcq_info->update_cq_work);
        }
        ka_task_spin_unlock_irqrestore(&vts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
    }
    TSDRV_PRINT_DEBUG("Handle cq success. (devid=%u; fid=%u; tsid=%u; cqid=%u)\n",
        devid, fid, tsid, cq_info->index);
}

STATIC void hvtsdrv_cq_handler_container(u32 devid, u32 fid, u32 tsid, struct devdrv_ts_cq_info *cq_info)
{
    struct devdrv_cq_sub_info *cq_sub_info = NULL;
    struct devdrv_report *report = NULL;
    struct tsdrv_ctx *ctx = NULL;
    u32 phase;

    cq_sub_info = (struct devdrv_cq_sub_info *)cq_info->cq_sub;
    report = (struct devdrv_report *)((uintptr_t)((unsigned long)cq_sub_info->virt_addr +
        (unsigned long)cq_info->slot_size * cq_info->tail));

    phase = devdrv_report_get_phase(report);
    ctx = cq_sub_info->ctx;
    if (ctx == NULL) {
        TSDRV_PRINT_DEBUG("The cq ctx is NULL. (cqid=%u)\n", cq_sub_info->index);
        return;
    }
    /* for checking a thread is waiting for wake up */
    if (ctx->ts_ctx[tsid].cq_tail_updated != 0) {
        TSDRV_PRINT_DEBUG("receive report irq, devid(%u), fid(%u), tsid(%u), cqid: %u."
            "no runtime thread is waiting, not judge.\n", devid, fid, tsid, cq_info->index);
        return;
    }

    if (phase == cq_info->phase) {
        ctx->ts_ctx[tsid].cq_tail_updated = CQ_HEAD_UPDATE_FLAG;
        ka_mb();
        if (ka_task_waitqueue_active(&ctx->ts_ctx[tsid].report_wait) != 0) {
            ka_task_wake_up(&ctx->ts_ctx[tsid].report_wait);
        }
    }
}
#endif

void hvtsdrv_cq_handler(u32 devid, u32 fid, u32 tsid, struct devdrv_ts_cq_info *cq_info)
{
#ifndef TSDRV_UT
    if (tsdrv_is_in_vm(devid)) {
        hvtsdrv_cq_handler_virtual(devid, fid, tsid, cq_info);
    }
    if (tsdrv_is_in_container(devid)) {
        if (cq_info->type == LOGIC_SQCQ_TYPE) {
            logic_sqcq_phy_cq_handler(devid, tsid, cq_info);
        } else {
            hvtsdrv_cq_handler_container(devid, fid, tsid, cq_info);
        }
    }
#endif
}
