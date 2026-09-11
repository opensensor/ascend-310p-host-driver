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
#ifndef CFG_MANAGER_HOST_ENV
#include <linux/securec.h>
#else
#include "securec.h"
#endif

#include "devdrv_common.h"
#include "hvtsdrv_id.h"
#include "hvtsdrv_cqsq.h"
#include "tsdrv_kernel_common.h"
#include "tsdrv_ctx.h"
#include "tsdrv_id.h"
#include "devdrv_functional_cqsq.h"
#include "hvtsdrv_tsagent.h"
#include "ka_kernel_def_pub.h"

static struct hvtsdrv_tsagent_ops tsagent_ops;

void hvtsdrv_tsagent_register(struct hvtsdrv_tsagent_ops *ops)
{
#ifndef TSDRV_UT
    if (ops != NULL) {
        tsagent_ops.tsagent_vf_create = ops->tsagent_vf_create;
        tsagent_ops.tsagent_vf_destroy = ops->tsagent_vf_destroy;
        tsagent_ops.tsagent_vsq_proc = ops->tsagent_vsq_proc;
        tsagent_ops.tsagent_trans_mailbox_msg = ops->tsagent_trans_mailbox_msg;
        TSDRV_PRINT_INFO("tsagent reg successful.\n");
    }
#endif
}
KA_EXPORT_SYMBOL_GPL(hvtsdrv_tsagent_register);

struct hvtsdrv_tsagent_ops *hvtsdrv_get_tsagent_ops(void)
{
    return &tsagent_ops;
}

void hvtsdrv_tsagent_unregister(void)
{
#ifndef TSDRV_UT
    tsagent_ops.tsagent_vf_create = NULL;
    tsagent_ops.tsagent_vf_destroy = NULL;
    tsagent_ops.tsagent_vsq_proc = NULL;
    tsagent_ops.tsagent_trans_mailbox_msg = NULL;
    TSDRV_PRINT_INFO("tsagent unreg successful.\n");
#endif
}
KA_EXPORT_SYMBOL_GPL(hvtsdrv_tsagent_unregister);

s32 hvtsdrv_notice_tsagent_vsq_proc(struct tsdrv_id_inst *id_inst, u32 vsqid, enum vsqcq_type type, u32 sqe_num)
{
#ifndef TSDRV_UT
    if (tsagent_ops.tsagent_vsq_proc == NULL) {
        TSDRV_PRINT_ERR("tsagent vsq proc func is null\n");
        return -EINVAL;
    }
#endif
    return tsagent_ops.tsagent_vsq_proc(id_inst, vsqid, type, sqe_num);
}

STATIC s32 hvtsdrv_tsagent_para_check(u32 dev_id, u32 fid, u32 tsid)
{
#ifndef TSDRV_UT
    enum tsdrv_dev_status status;
    u32 tsnum;

    if ((dev_id >= TSDRV_MAX_DAVINCI_NUM) || (fid >= TSDRV_MAX_FID_NUM)) {
        TSDRV_PRINT_ERR("para error. dev_id %u, fid %u.\n", dev_id, fid);
        return -EINVAL;
    }

    tsnum = tsdrv_get_dev_tsnum(dev_id);
    if (tsid >= tsnum) {
        TSDRV_PRINT_ERR("para error. dev_id %u, fid %u tsid %u.\n", dev_id, fid, tsid);
        return -EINVAL;
    }

    status = tsdrv_get_dev_status(dev_id, fid);
    if (status != TSDRV_DEV_ACTIVE) {
        TSDRV_PRINT_ERR("devid(%u), fid(%u), tsid(%u), status(%d) is invalid.\n", dev_id, fid, tsid, (int)status);
        return -EINVAL;
    }
#endif
    return 0;
}

STATIC int hvtsdrv_sq_write_para_check(u32 devid, u32 fid, u32 tsid, struct hvtsdrv_vsq_slot_info *data)
{
#ifndef TSDRV_UT
    int ret;

    ret = hvtsdrv_tsagent_para_check(devid, fid, tsid);
    if (ret != 0) {
        return -EINVAL;
    }

    if (data == NULL) {
        TSDRV_PRINT_ERR("data is null.\n");
        return -EINVAL;
    }

    if (data->vsq_slot_addr == NULL || data->vsq_slot_size == 0 || data->vsq_slot_size > DEVDRV_SQ_SLOT_SIZE) {
        TSDRV_PRINT_ERR("vsq slot addr is null or invalid size(%u).\n", data->vsq_slot_size);
        return -EINVAL;
    }
#endif
    return 0;
}

STATIC void hvtsdrv_update_vsq_head_for_tsagent(struct tsdrv_ts_resource *ts_res, u32 vsq_id)
{
#ifndef TSDRV_UT
    struct tsdrv_vsq_info *vsq_info = NULL;
    unsigned long flags;

    ka_task_spin_lock_irqsave(&ts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
    vsq_info = hvtsdrv_get_vsq_info_inner(ts_res, vsq_id);
    if (vsq_info == NULL) {
        ka_task_spin_unlock_irqrestore(&ts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
        return;
    }
#ifndef CFG_FEATURE_SUPPORT_VSQ_MNG
    vsq_info->head_for_tsagent = (vsq_info->head_for_tsagent + 1) % DEVDRV_MAX_SQ_DEPTH;
#else
    vsq_info->head_for_tsagent = (vsq_info->head_for_tsagent + 1) % vsq_info->depth;
#endif
    ka_task_spin_unlock_irqrestore(&ts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);

    TSDRV_PRINT_DEBUG("vsqid(%u), head_for_tsagent(%u), head(%u), tail(%u).\n",
        vsq_id, vsq_info->head_for_tsagent, vsq_info->head, vsq_info->tail);
#endif
}

int hvtsdrv_sq_write(u32 devid, u32 fid, u32 tsid, struct hvtsdrv_vsq_slot_info *data)
{
#ifndef TSDRV_UT
    struct tsdrv_ts_resource *vts_res = NULL;
    struct devdrv_ts_sq_info *sq_info = NULL;
    struct devdrv_sq_sub_info *sq_sub_info = NULL;
    u32 sq_depth = DEVDRV_MAX_SQ_DEPTH;
    phys_addr_t info_mem_addr;
    u32 vsq_id, sq_id, chip_type, id_max;
    int ret;

    ret = hvtsdrv_sq_write_para_check(devid, fid, tsid, data);
    if (ret != 0) {
        return -EINVAL;
    }

    vsq_id = data->vsq_id;
    vts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
    chip_type = devdrv_get_dev_chip_type(devid);
    if (chip_type == HISI_CLOUD_V2) {
        id_max = tsdrv_get_max_id_num(TSDRV_SQ_ID);
    } else {
        id_max = vts_res->id_res[TSDRV_SQ_ID].id_capacity;
    }
    if (vsq_id >= id_max) {
        TSDRV_PRINT_ERR("devid(%u), fid(%u), tsid(%u), vsq(%u) is invalid.\n", devid, fid, tsid, vsq_id);
        return -EINVAL;
    }

    ka_task_mutex_lock(&vts_res->id_res[TSDRV_SQ_ID].id_mutex_t);

    sq_id = tsdrv_vrit_to_physic_id(&vts_res->id_res[TSDRV_SQ_ID], vsq_id);
    info_mem_addr = vts_res->mem_info[DEVDRV_INFO_MEM].virt_addr;
    sq_info = devdrv_calc_sq_info(info_mem_addr, sq_id);
    if (sq_info->alloc_status == SQCQ_INACTIVE) {
        ka_task_mutex_unlock(&vts_res->id_res[TSDRV_SQ_ID].id_mutex_t);
        TSDRV_PRINT_ERR("devid(%u), fid(%u), tsid(%u), sq(%u), status(%d) is invalid.\n", devid, fid, tsid, sq_id,
            (int)sq_info->alloc_status);
        return -EINVAL;
    }

    TSDRV_PRINT_DEBUG("Check. (sqid=%u; vsqid=%u; head=%u; tail=%u).\n", sq_id, vsq_id, sq_info->head, sq_info->tail);
#ifdef CFG_FEATURE_SUPPORT_VSQ_MNG
    sq_depth = sq_info->depth;
#endif
    if ((sq_info->tail + 1) % sq_depth == sq_info->head) {
        ka_task_mutex_unlock(&vts_res->id_res[TSDRV_SQ_ID].id_mutex_t);
        TSDRV_PRINT_DEBUG("sq have no slot, sqid(%u), head(%u), tail(%u)\n", sq_info->index, sq_info->head,
            sq_info->tail);
        return -EAGAIN;
    }

    sq_sub_info = sq_info->sq_sub;
    ret = memcpy_s((void *)(uintptr_t)sq_sub_info->vaddr + sq_info->tail * DEVDRV_SQ_SLOT_SIZE, DEVDRV_SQ_SLOT_SIZE,
        data->vsq_slot_addr, data->vsq_slot_size);
    if (ret != 0) {
        ka_task_mutex_unlock(&vts_res->id_res[TSDRV_SQ_ID].id_mutex_t);
        TSDRV_PRINT_ERR("Memcpy. (dest=%pK, src=%pK, size=%u; ret=%d; sqid=%u, vsqid=%u, fid=%u).\n",
            (void *)sq_sub_info->vaddr + sq_info->tail * DEVDRV_SQ_SLOT_SIZE, data->vsq_slot_addr, data->vsq_slot_size,
            ret, sq_id, vsq_id, fid);
        return ret;
    }

    ka_wmb();

    sq_info->tail = (sq_info->tail + 1) % sq_depth;
    TSDRV_PRINT_DEBUG("sqid(%u), head(%u), tail(%u).\n", sq_id, sq_info->head, sq_info->tail);

    hvtsdrv_update_vsq_head_for_tsagent(vts_res, vsq_id);
    ka_task_mutex_unlock(&vts_res->id_res[TSDRV_SQ_ID].id_mutex_t);
#endif
    return 0;
}
KA_EXPORT_SYMBOL_GPL(hvtsdrv_sq_write);

void hvtsdrv_sq_irq_trigger(u32 devid, u32 fid, u32 tsid, u32 vsq_id)
{
#ifndef TSDRV_UT
    struct tsdrv_ts_resource *vts_res = NULL;
    struct devdrv_ts_sq_info *sq_info = NULL;
    struct tsdrv_db_hwinfo_t *db_hwinfo = NULL;
    phys_addr_t info_mem_addr;
    u32 *doorbell = NULL;
    u32 sq_id, chip_type, id_max;
    int ret;

    ret = hvtsdrv_tsagent_para_check(devid, fid, tsid);
    if (ret != 0) {
        return;
    }

    vts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
    chip_type = devdrv_get_dev_chip_type(devid);
    if (chip_type == HISI_CLOUD_V2) {
        id_max = tsdrv_get_max_id_num(TSDRV_SQ_ID);
    } else {
        id_max = vts_res->id_res[TSDRV_SQ_ID].id_capacity;
    }
    if (vsq_id >= id_max) {
        TSDRV_PRINT_ERR("devid(%u), fid(%u), tsid(%u), vsq(%u) is invalid.\n", devid, fid, tsid, vsq_id);
        return;
    }

    ka_task_mutex_lock(&vts_res->id_res[TSDRV_SQ_ID].id_mutex_t);
    sq_id = tsdrv_vrit_to_physic_id(&vts_res->id_res[TSDRV_SQ_ID], vsq_id);
    info_mem_addr = vts_res->mem_info[DEVDRV_INFO_MEM].virt_addr;
    sq_info = devdrv_calc_sq_info(info_mem_addr, sq_id);
    if (sq_info->alloc_status == SQCQ_INACTIVE) {
        ka_task_mutex_unlock(&vts_res->id_res[TSDRV_SQ_ID].id_mutex_t);
        TSDRV_PRINT_ERR("devid(%u), fid(%u), tsid(%u), sq(%u), status(%u) is invalid.\n", devid, fid, tsid, sq_id,
            sq_info->alloc_status);
        return;
    }
    TSDRV_PRINT_DEBUG("devid(%u), fid(%u), tsid(%u), vsq(%u), sqid(%u), tail(%u).\n",
        devid, fid, tsid, vsq_id, sq_id, sq_info->tail);
    db_hwinfo = tsdrv_get_db_hwinfo_t(devid, tsid, TSDRV_SQ_DB);
    doorbell = db_hwinfo->dbs + (long)(unsigned)sq_id * db_hwinfo->db_stride + DEVDRV_SQ_TAIL_OFFSET;
    ka_mm_writel(sq_info->tail, (void __iomem *)doorbell);
    ka_task_mutex_unlock(&vts_res->id_res[TSDRV_SQ_ID].id_mutex_t);
#endif
    return;
}
KA_EXPORT_SYMBOL_GPL(hvtsdrv_sq_irq_trigger);

/* tsdrv resource_id convert interface: virt to physical */
int hvtsdrv_resid_v2p(u32 devid, u32 fid, u32 tsid, struct hvtsdrv_id_v2p *data)
{
#ifndef TSDRV_UT
    s32 ret, phy_id;

    ret = hvtsdrv_tsagent_para_check(devid, fid, tsid);
    if (ret != 0) {
        return -EINVAL;
    }

    if ((data == NULL) || (data->id_type >= TSDRV_MAX_ID) || (data->id_type < 0)) {
        TSDRV_PRINT_ERR("dev-%u fid-%u tsid-%u type-%d.\n", devid, fid, tsid,
            (data == NULL) ? 0 : (int)data->id_type);
        return -EINVAL;
    }

#ifndef CFG_SOC_PLATFORM_CLOUD
    if ((data->id_type == TSDRV_EVENT_SW_ID) && (data->virt_id >= DEVDRV_MAX_HW_EVENT_ID)) {
        data->phy_id = data->virt_id;
        return 0;
    }
#endif
    phy_id = tsdrv_get_pmid(devid, fid, tsid, data->virt_id, data->id_type);
    if (phy_id < 0) {
        TSDRV_PRINT_ERR("dev-%u fid-%u tsid-%u id_type-%u vid-%u invalid.\n", devid, fid, tsid, (int)data->id_type,
            data->virt_id);
        return -EINVAL;
    }

    data->phy_id = (u32)phy_id;
#endif
    return 0;
}
KA_EXPORT_SYMBOL_GPL(hvtsdrv_resid_v2p);

int hvtsdrv_get_res_num(u32 devid, u32 fid, u32 tsid, enum tsdrv_id_type id_type, u32 *num)
{
#ifndef TSDRV_UT
    struct tsdrv_ts_resource *ts_resource = NULL;
    enum tsdrv_id_type id_type_tmp;
    u32 chip_type;
    s32 ret;

    ret = hvtsdrv_tsagent_para_check(devid, fid, tsid);
    if (ret != 0) {
        return -EINVAL;
    }

    if ((num == NULL) || (id_type >= TSDRV_MAX_ID)) {
        TSDRV_PRINT_ERR("dev-%u fid-%u tsid-%u type-%d invalid.\n", devid, fid, tsid, (int)id_type);
        return -EINVAL;
    }

    ts_resource = tsdrv_get_ts_resoruce(devid, fid, tsid);
    chip_type = devdrv_get_dev_chip_type(devid);
    if (chip_type == HISI_CLOUD_V2) {
        id_type_tmp = id_type;
        if (id_type_tmp == TSDRV_EVENT_HW_ID) {
            id_type_tmp = TSDRV_EVENT_SW_ID;
        }
        *num = tsdrv_get_max_id_num(id_type_tmp);
    } else {
        if (id_type == TSDRV_EVENT_HW_ID || id_type == TSDRV_EVENT_SW_ID) {
            *num = ts_resource->id_res[TSDRV_EVENT_HW_ID].id_capacity +
                ts_resource->id_res[TSDRV_EVENT_SW_ID].id_capacity;
        } else {
            *num = ts_resource->id_res[id_type].id_capacity;
        }
    }

    TSDRV_PRINT_DEBUG("(devid=%u; fid=%u; tsid=%u; id_type=%u; num=%u)\n", devid, fid, tsid, id_type, *num);
#endif
    return 0;
}
KA_EXPORT_SYMBOL_GPL(hvtsdrv_get_res_num);

int hvtsdrv_get_vsq_head_and_tail(u32 devid, u32 fid, u32 tsid, u32 vsq_id, struct hvtsdrv_vsq_head_tail *out)
{
#ifndef TSDRV_UT
    struct tsdrv_ts_resource *vts_res = NULL;
    struct tsdrv_vsq_info *vsq_info = NULL;
    u32 chip_type, id_max;
    unsigned long flags;
    int ret;

    ret = hvtsdrv_tsagent_para_check(devid, fid, tsid);
    if ((ret != 0) || (out == NULL)) {
        return -EINVAL;
    }

    vts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
    chip_type = devdrv_get_dev_chip_type(devid);
    if (chip_type == HISI_CLOUD_V2) {
        id_max = tsdrv_get_max_id_num(TSDRV_SQ_ID);
    } else {
        id_max = vts_res->id_res[TSDRV_SQ_ID].id_capacity;
    }
    if (vsq_id >= id_max) {
        TSDRV_PRINT_ERR("invalid para, vsq_id(%u).\n", vsq_id);
        return -EINVAL;
    }

    ka_task_spin_lock_irqsave(&vts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
    vsq_info = hvtsdrv_get_vsq_info_inner(vts_res, vsq_id);
    if (vsq_info == NULL) {
        ka_task_spin_unlock_irqrestore(&vts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
        return -EINVAL;
    }
    out->head = vsq_info->head_for_tsagent;
    out->tail = vsq_info->tail;
    ka_task_spin_unlock_irqrestore(&vts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
    TSDRV_PRINT_DEBUG("(devid=%u; fid=%u; vsq=%u; head_for_tsagent=%u; head=%u; vsq_tail=%u)\n",
        devid, fid, vsq_id, vsq_info->head_for_tsagent, vsq_info->head, vsq_info->tail);
#endif
    return 0;
}
KA_EXPORT_SYMBOL_GPL(hvtsdrv_get_vsq_head_and_tail);

int hvtsdrv_get_vsq_info(u32 devid, u32 fid, u32 tsid, u32 vsq_id, struct hvtsdrv_vsq_info *vsq_info)
{
#ifndef TSDRV_UT
    struct tsdrv_ts_resource *vts_res = NULL;
#ifdef CFG_FEATURE_SUPPORT_VSQ_MNG
    struct tsdrv_vsq_info *local_vsq_info = NULL;
    unsigned long flags;
#else
    phys_addr_t vsq_slot_mem_addr;
#endif
    u32 chip_type, id_max;
    int ret;

    ret = hvtsdrv_tsagent_para_check(devid, fid, tsid);
    if ((ret != 0) || (vsq_info == NULL)) {
        TSDRV_PRINT_ERR("invalid para, ret(%d), vsq_info is %s.\n", ret, (vsq_info == NULL) ? "NULL" : "not NULL");
        return -EINVAL;
    }

    vts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
    chip_type = devdrv_get_dev_chip_type(devid);
    if (chip_type == HISI_CLOUD_V2) {
        id_max = tsdrv_get_max_id_num(TSDRV_SQ_ID);
    } else {
        id_max = vts_res->id_res[TSDRV_SQ_ID].id_capacity;
    }
    if (vsq_id >= id_max) {
        TSDRV_PRINT_ERR("invalid para, vsq_id(%u).\n", vsq_id);
        return -EINVAL;
    }

#ifdef CFG_FEATURE_SUPPORT_VSQ_MNG
    ka_task_spin_lock_irqsave(&vts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
    local_vsq_info = hvtsdrv_get_vsq_info_inner(vts_res, vsq_id);
    if (vsq_info == NULL) {
        ka_task_spin_unlock_irqrestore(&vts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
        TSDRV_PRINT_ERR("vsq info is not inited or has been destroy. (vsq_id=%u)\n", vsq_id);
        return -EINVAL;
    }
    vsq_info->vsq_base_addr = (void *)(uintptr_t)local_vsq_info->vsq_vaddr;
    vsq_info->vsq_dep = local_vsq_info->depth;
    vsq_info->vsq_slot_size = DEVDRV_SQ_SLOT_SIZE;
    ka_task_spin_unlock_irqrestore(&vts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
#else
    vsq_slot_mem_addr = vts_res->mem_info[DEVDRV_SQ_MEM].virt_addr +
        vsq_id * DEVDRV_MAX_SQ_DEPTH * DEVDRV_SQ_SLOT_SIZE;
    vsq_info->vsq_base_addr = (void *)(uintptr_t)vsq_slot_mem_addr;
    vsq_info->vsq_dep = DEVDRV_MAX_SQ_DEPTH;
    vsq_info->vsq_slot_size = DEVDRV_SQ_SLOT_SIZE;
#endif

    TSDRV_PRINT_DEBUG("devid(%u), fid(%u), tsid(%u), vsq_id(%u), dep(%u), slot(%u)\n",
        devid, fid, tsid, vsq_id, vsq_info->vsq_dep, vsq_info->vsq_slot_size);
#endif
    return 0;
}
KA_EXPORT_SYMBOL_GPL(hvtsdrv_get_vsq_info);

int tsdrv_notice_tsagent_dev_create(u32 devid, u32 fid)
{
#ifndef TSDRV_UT
    if (hvtsdrv_get_tsagent_ops()->tsagent_vf_create != NULL) {
        int ret = hvtsdrv_get_tsagent_ops()->tsagent_vf_create(devid, fid);
        if (ret != 0) {
            TSDRV_PRINT_ERR("dev-%u fid-%u dev_create failed, ret-%d.\n", devid, fid, ret);
            return ret;
        }
        TSDRV_PRINT_INFO("inform tasgent dev-%u fid-%u create success.\n", devid, fid);
    }
#endif
    return 0;
}
KA_EXPORT_SYMBOL_GPL(tsdrv_notice_tsagent_dev_create);

void tsdrv_notice_tsagent_dev_destroy(u32 devid, u32 fid)
{
#ifndef TSDRV_UT
    if (hvtsdrv_get_tsagent_ops()->tsagent_vf_destroy != NULL) {
        hvtsdrv_get_tsagent_ops()->tsagent_vf_destroy(devid, fid);
        TSDRV_PRINT_INFO("inform tasgent dev-%u fid-%u destroy success.\n", devid, fid);
    }
#endif
}
KA_EXPORT_SYMBOL_GPL(tsdrv_notice_tsagent_dev_destroy);

void hvtsdrv_fill_trans_info_mbox(u32 devid, u32 fid, u32 tsid, u32 disable_thread,
    struct hvtsdrv_trans_mailbox_ctx *trans_mbox)
{
    trans_mbox->dev_id = devid;
    trans_mbox->fid = fid;
    trans_mbox->ts_id = tsid;
    trans_mbox->disable_thread = disable_thread;
}

int hvtsdrv_trans_info_msg(struct hvtsdrv_trans_mailbox_ctx *trans_mbox, void *info, size_t msg_len)
{
    if (trans_mbox->fid != 0 && hvtsdrv_get_tsagent_ops()->tsagent_trans_mailbox_msg != NULL) {
#ifndef TSDRV_UT
        int ret;
        ret = hvtsdrv_get_tsagent_ops()->tsagent_trans_mailbox_msg(info, msg_len, trans_mbox);
        if (ret != 0) {
            TSDRV_PRINT_ERR("Failed to send translate info message. (ret=%d; devid=%u; fid=%u; tsid=%u; "
                "disable_thread=%u)\n", ret, trans_mbox->dev_id, trans_mbox->fid, trans_mbox->ts_id,
                trans_mbox->disable_thread);
            return ret;
        }
#endif
    }
    return 0;
}
