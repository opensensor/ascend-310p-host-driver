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
#include <linux/types.h>
#include "devdrv_cb.h"
#include "devdrv_cbctrl.h"
#include "devdrv_cqsq.h"
#include "devdrv_id.h"
#include "tsdrv_cb_pm.h"
#include "devdrv_devinit.h"
#include "tsdrv_vsq.h"
#include "ka_memory_pub.h"

#include "hvtsdrv_cb.h"
#include "hvtsdrv_id.h"
#include "hvtsdrv_cqsq.h"

ka_mutex_t cb_mlock[TSDRV_MAX_DAVINCI_NUM][TSDRV_MAX_FID_NUM];

s32 hvtsdrv_generate_cb_sq(u32 devid, u32 tsid, u32 fid, ka_pid_t tgid, u8 *msg)
{
    unsigned long size = (unsigned long)DEVDRV_SQ_SLOT_SIZE * DEVDRV_MAX_SQ_DEPTH;
    enum tsdrv_dev_status status = tsdrv_get_dev_status(devid, fid);
    struct cb_physic_sqcq *cb_sqcq = (struct cb_physic_sqcq *)msg;
    struct tsdrv_ts_resource *pm_ts_res = NULL;
    struct tsdrv_ts_resource *ts_res = NULL;
    struct devdrv_sq_sub_info *sq_sub = NULL;
    struct devdrv_ts_sq_info *sq_info = NULL;
    enum tsdrv_id_type type = TSDRV_SQ_ID;
    struct tsdrv_id_info *sq_id_info = NULL;
    struct devdrv_ts_cq_info *cq_info = NULL;
    struct devdrv_cq_sub_info *cq_sub = NULL;
    tsid = 0;

    if (status != TSDRV_DEV_ACTIVE) {
        TSDRV_PRINT_ERR("dev-%u fid-%u, invalid status-%d.\n", devid, fid, status);
        return -EEXIST;
    }

    cq_info = tsdrv_get_cq_info(devid, fid, tsid, GET_CALLBACK_CQ_ID(fid));
    if (cq_info == NULL || cq_info->cq_sub == NULL) {
        TSDRV_PRINT_ERR("dev-%u fid-%u tsid-%u get one id fail.\n", devid, fid, tsid);
        return -EEXIST;
    }
    cq_sub = (struct devdrv_cq_sub_info *)cq_info->cq_sub;
    ka_task_mutex_lock(&cb_mlock[devid][fid]);
    if (cq_sub->callback_sq_index != DEVDRV_INVALID_CB_SQ_ID) {
        ka_task_mutex_unlock(&cb_mlock[devid][fid]);
        TSDRV_PRINT_ERR("dev-%u fid-%u, callback sq has been initialized\n", devid, fid);
        return -EINVAL;
    }

    ts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
    ka_task_spin_lock(&ts_res->id_res[type].spinlock);
    sq_id_info = tsdrv_get_one_res_id(&ts_res->id_res[type]);
    if (sq_id_info == NULL) {
        ka_task_spin_unlock(&ts_res->id_res[type].spinlock);
        ka_task_mutex_unlock(&cb_mlock[devid][fid]);
        TSDRV_PRINT_ERR("dev-%u fid-%u tsid-%u get one id fail.\n", devid, fid, tsid);
        return -EPERM;
    }
    ka_task_spin_unlock(&ts_res->id_res[type].spinlock);

    sq_info = tsdrv_get_sq_info(devid, fid, tsid, sq_id_info->phy_id);
    if ((sq_info == NULL) || (sq_info->sq_sub == NULL)) {
        ka_task_mutex_unlock(&cb_mlock[devid][fid]);
        TSDRV_PRINT_ERR("dev-%u fid-%u tsid-%u get one id fail.\n", devid, fid, tsid);
        return -EEXIST;
    }
    sq_info->alloc_status = SQCQ_ACTIVE;
    sq_info->head = 0;
    sq_info->tail = 0;
    hvtsdrv_clear_vsq_info(ts_res, (u32)sq_id_info->id);

    sq_sub = (struct devdrv_sq_sub_info *)sq_info->sq_sub;
    pm_ts_res = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, tsid);
    sq_sub->vaddr = pm_ts_res->mem_info[DEVDRV_SQ_MEM].virt_addr + (unsigned long)sq_id_info->phy_id * size;
    sq_sub->phy_addr = pm_ts_res->mem_info[DEVDRV_SQ_MEM].phy_addr +
        (phys_addr_t)(unsigned)sq_id_info->phy_id * size;

    cq_sub->callback_sq_index = sq_id_info->phy_id;
    cq_sub->callback_sq_phy_addr = sq_sub->phy_addr;
    ka_task_mutex_unlock(&cb_mlock[devid][fid]);

    cb_sqcq->virt_id = sq_id_info->virt_id;
    TSDRV_PRINT_INFO("Callback sq alloc. (devid=%u; fid=%u; tsid=%u; vsqid=%u; phy_sqid=%u).\n",
        devid, fid, tsid, sq_id_info->virt_id, sq_id_info->phy_id);
    return 0;
}

s32 hvtsdrv_generate_cb_cq(u32 devid, u32 tsid, u32 fid, ka_pid_t tgid, u8 *msg)
{
    enum tsdrv_dev_status status = tsdrv_get_dev_status(devid, fid);
    struct cb_physic_sqcq *cb_sqcq = (struct cb_physic_sqcq *)msg;
    size_t size = DEVDRV_MAX_CBCQ_SIZE * DEVDRV_MAX_CBCQ_DEPTH;
    ka_device_t *dev = tsdrv_get_dev_p(devid);
    struct devdrv_cq_sub_info *cq_sub = NULL;
    struct devdrv_ts_cq_info *cq_info = NULL;
    struct tsdrv_ts_resource *ts_res = NULL;
    phys_addr_t cb_cq_paddr, info_mem_addr;
    void *cb_cq_vaddr = NULL;

    if (status != TSDRV_DEV_ACTIVE) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("dev-%u fid-%u, invalid status-%d.\n", devid, fid, status);
        return -EEXIST;
#endif
    }

    ts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
    info_mem_addr = ts_res->mem_info[DEVDRV_INFO_MEM].virt_addr;
    cq_info = devdrv_calc_cq_info(info_mem_addr, GET_CALLBACK_CQ_ID(fid));
    ka_task_mutex_lock(&cb_mlock[devid][fid]);
    if (cq_info->alloc_status == SQCQ_ACTIVE) {
#ifndef TSDRV_UT
        ka_task_mutex_unlock(&cb_mlock[devid][fid]);
        TSDRV_PRINT_ERR("dev-%u fid-%u, callback cq has been initialized\n", devid, fid);
        return -EINVAL;
#endif
    }

    cq_info->vfid = fid;
    cq_info->head = 0;
    cq_info->tail = 0;
    cq_info->phase = DEVDRV_PHASE_STATE_1;
    hvtsdrv_clear_vcq_info(ts_res, ts_res->id_res[TSDRV_CQ_ID].id_capacity);

    cq_sub = (struct devdrv_cq_sub_info *)cq_info->cq_sub;
    cb_cq_vaddr = tsdrv_kzalloc(size, KA_GFP_KERNEL);
    if (cb_cq_vaddr == NULL) {
#ifndef TSDRV_UT
        ka_task_mutex_unlock(&cb_mlock[devid][fid]);
        TSDRV_PRINT_ERR("dev-%u fid-%u, callback cq malloc fail.\n", devid, fid);
        return -ENOMEM;
#endif
    }

    cb_cq_paddr = ka_mm_dma_map_single(dev, cb_cq_vaddr, size, KA_DMA_FROM_DEVICE);
    if (ka_mm_dma_mapping_error(dev, cb_cq_paddr)) {
#ifndef TSDRV_UT
        ka_task_mutex_unlock(&cb_mlock[devid][fid]);
        tsdrv_kfree(cb_cq_vaddr);
        TSDRV_PRINT_ERR("dev-%u fid-%u, callback cq dma map fail\n", devid, fid);
        return -ENOMEM;
#endif
    }

    ka_task_spin_lock_irq(&cq_sub->spinlock);
    cq_sub->virt_addr = (phys_addr_t)(uintptr_t)cb_cq_vaddr;
    cq_sub->phy_addr = cb_cq_paddr;
    cq_info->alloc_status = SQCQ_ACTIVE;
    ka_task_spin_unlock_irq(&cq_sub->spinlock);
    ka_task_mutex_unlock(&cb_mlock[devid][fid]);

    cb_sqcq->virt_id = ts_res->id_res[TSDRV_CQ_ID].id_capacity;
    TSDRV_PRINT_INFO("dev-%u fid-%u tsid-%u vcqid-%u.\n", devid, fid, tsid, cb_sqcq->virt_id);
    return 0;
}

STATIC void hvtsdrv_destroy_cb_sq(u32 devid, u32 fid, u32 tsid, struct devdrv_cq_sub_info *cq_sub)
{
    u32 phy_sqid = cq_sub->callback_sq_index;
    struct devdrv_ts_sq_info *sq_info = NULL;
    struct tsdrv_ts_resource *ts_res = NULL;
    struct tsdrv_id_info *sq_id_info = NULL;
    struct devdrv_sq_sub_info *sq_sub = NULL;
    phys_addr_t info_mem_addr;

    if (phy_sqid == DEVDRV_INVALID_CB_SQ_ID) {
        TSDRV_PRINT_INFO("dev-%u fid-%u tsid-%u has uninitialized sqid, not need destroy.\n", devid, fid, tsid);
        return;
    }

    ts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
    ka_task_spin_lock(&ts_res->id_res[TSDRV_SQ_ID].spinlock);
    sq_id_info = devdrv_find_one_id(&ts_res->id_res[TSDRV_SQ_ID], phy_sqid);
    if (sq_id_info == NULL) {
#ifndef TSDRV_UT
        ka_task_spin_unlock(&ts_res->id_res[TSDRV_SQ_ID].spinlock);
        TSDRV_PRINT_ERR("dev-%u fid-%u tsid-%u sqid-%u.\n", devid, fid, tsid, phy_sqid);
        return;
#endif
    }

    info_mem_addr = ts_res->mem_info[DEVDRV_INFO_MEM].virt_addr;
    sq_info = devdrv_calc_sq_info(info_mem_addr, phy_sqid);
    sq_info->alloc_status = SQCQ_INACTIVE;
    sq_info->head = 0;
    sq_info->tail = 0;
    sq_sub = (struct devdrv_sq_sub_info *)sq_info->sq_sub;
    sq_sub->depth = 0;
    sq_sub->size = 0;
    sq_sub->bar_addr = 0;
    sq_sub->phy_addr = 0;
    sq_sub->vaddr = 0;
    sq_sub->queue_size = 0;
    sq_sub->map_va = 0;
    hvtsdrv_clear_vsq_info(ts_res, (u32)sq_id_info->id);

    cq_sub->callback_sq_index = DEVDRV_INVALID_CB_SQ_ID;
    cq_sub->callback_sq_phy_addr = 0;

    ka_list_add(&sq_id_info->list, &ts_res->id_res[TSDRV_SQ_ID].id_available_list);
    ts_res->id_res[TSDRV_SQ_ID].id_available_num++;
    ka_task_spin_unlock(&ts_res->id_res[TSDRV_SQ_ID].spinlock);

    TSDRV_PRINT_INFO("Callback sq free. (devid=%u; fid=%u; tsid=%u; phy_sqid=%u).\n", devid, fid, tsid, phy_sqid);
}

STATIC void hvtsdrv_destroy_cb_cq(u32 devid, u32 fid, u32 tsid, struct devdrv_ts_cq_info *cq_info,
    struct devdrv_cq_sub_info *cq_sub)
{
    struct tsdrv_ts_resource *ts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
    ka_device_t *dev = tsdrv_get_dev_p(devid);
    void *cb_cq_vaddr = NULL;
    phys_addr_t cb_cq_paddr;
    size_t size;

    if (cq_info->alloc_status == SQCQ_INACTIVE) {
        TSDRV_PRINT_INFO("dev-%u fid-%u has uninitialized cqid, not need destroy.\n", devid, fid);
        return;
    }

    ka_task_spin_lock_irq(&cq_sub->spinlock);
    cb_cq_vaddr = (void *)(uintptr_t)cq_sub->virt_addr;
    cb_cq_paddr = cq_sub->phy_addr;
    cq_sub->virt_addr = 0;
    cq_sub->phy_addr = 0;
    cq_sub->size = 0;
    cq_sub->slot_size = 0;
    cq_sub->depth = 0;
    cq_sub->map_va = 0;
    cq_sub->bar_addr = 0;
    cq_info->alloc_status = SQCQ_INACTIVE;
    cq_info->head = 0;
    cq_info->tail = 0;
    hvtsdrv_clear_vcq_info(ts_res, ts_res->id_res[TSDRV_CQ_ID].id_capacity);
    ka_task_spin_unlock_irq(&cq_sub->spinlock);

    size = DEVDRV_MAX_CBCQ_SIZE * DEVDRV_MAX_CBCQ_DEPTH;
    if (cb_cq_paddr != 0) {
        ka_mm_dma_unmap_single(dev, cb_cq_paddr, size, KA_DMA_FROM_DEVICE);
    }

    if (cb_cq_vaddr != NULL) {
        tsdrv_kfree(cb_cq_vaddr);
    }

    TSDRV_PRINT_INFO("dev-%u fid-%u cqid-%u\n", devid, fid, cq_sub->index);
}

void hvtsdrv_destroy_cb_sqcq(u32 devid, u32 fid)
{
    struct tsdrv_ts_resource *ts_res = NULL;
    struct devdrv_cq_sub_info *cq_sub = NULL;
    struct devdrv_ts_cq_info *cq_info = NULL;
    struct callback_phy_sqcq cb_sqcq;
    phys_addr_t info_mem_addr;
    u32 tsid = 0;

    ts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
    info_mem_addr = ts_res->mem_info[DEVDRV_INFO_MEM].virt_addr;
    cq_info = devdrv_calc_cq_info(info_mem_addr, GET_CALLBACK_CQ_ID(fid));
    cq_sub = (struct devdrv_cq_sub_info *)cq_info->cq_sub;
    ka_task_mutex_lock(&cb_mlock[devid][fid]);
    /* maybe cb_sqcq are not both initialized, just destroy one of or neither of cb_sqcq, not need to send mailbox */
    if (cq_info->alloc_status == SQCQ_INACTIVE || cq_sub->callback_sq_index == DEVDRV_INVALID_CB_SQ_ID) {
        hvtsdrv_destroy_cb_cq(devid, fid, tsid, cq_info, cq_sub);
        hvtsdrv_destroy_cb_sq(devid, fid, tsid, cq_sub);
        ka_task_mutex_unlock(&cb_mlock[devid][fid]);
        return;
    }

    cb_sqcq.devid = devid;
    cb_sqcq.tsid = tsid;
    cb_sqcq.cb_sq.index = cq_sub->callback_sq_index;
    cb_sqcq.cb_cq.index = cq_sub->index;
    cb_sqcq.plat_type = tsdrv_get_env_type();
    (void)callback_mbox_send(CALLBACK_MBOX_SQCQ_FREE, &cb_sqcq);

    hvtsdrv_destroy_cb_cq(devid, fid, tsid, cq_info, cq_sub);
    hvtsdrv_destroy_cb_sq(devid, fid, tsid, cq_sub);
    ka_task_mutex_unlock(&cb_mlock[devid][fid]);

    TSDRV_PRINT_INFO("dev-%u fid-%u tsid-%u.\n", devid, fid, tsid);
}

s32 hvtsdrv_cb_mailbox_to_ts(u32 devid, u32 tsid, u32 fid, ka_pid_t tgid, u8 *msg)
{
    struct cb_physic_sqcq *cb_sqcq = (struct cb_physic_sqcq *)msg;
    u32 type = cb_sqcq->mailbox_type;
    struct devdrv_ts_cq_info *cq_info = NULL;
    struct devdrv_cq_sub_info *cq_sub = NULL;
    struct tsdrv_cq_hwinfo *cq_hwinfo = NULL;
    struct tsdrv_ts_resource *ts_res = NULL;
    struct tsdrv_mbox_data data = {0};
    struct callback_cqsq_mbox cbcqsq;
    enum tsdrv_dev_status status;
    phys_addr_t info_mem_addr;
    u16 irq_id;
    s32 ret;
    tsid = 0;

    status = tsdrv_get_dev_status(devid, fid);
    if (status != TSDRV_DEV_ACTIVE) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("hvtsdrv dev-%u fid-%u status-%u not active.\n", devid, fid, status);
        return -EINVAL;
#endif
    }

    ts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
    info_mem_addr = ts_res->mem_info[DEVDRV_INFO_MEM].virt_addr;
    cq_info = devdrv_calc_cq_info(info_mem_addr, GET_CALLBACK_CQ_ID(fid));
    cq_sub = (struct devdrv_cq_sub_info *)cq_info->cq_sub;
    ka_task_mutex_lock(&cb_mlock[devid][fid]);
    if (cq_info->alloc_status == SQCQ_INACTIVE || cq_sub->callback_sq_index == DEVDRV_INVALID_CB_SQ_ID) {
#ifndef TSDRV_UT
        ka_task_mutex_unlock(&cb_mlock[devid][fid]);
        TSDRV_PRINT_ERR("Cbcq is not initialized. (dev=%u; fid=%u; alloc_status=%u; callback_sq_index=%u)\n",
            devid, fid, cq_info->alloc_status, cq_sub->callback_sq_index);
        return -EINVAL;
#endif
    }

    cq_hwinfo = tsdrv_get_cq_hwinfo(devid, tsid);

    /* add message header */
    cbcqsq.valid = DEVDRV_MAILBOX_MESSAGE_VALID;
    switch (type) {
        case CALLBACK_MBOX_SQCQ_ALLOC: {
            cbcqsq.cmd_type = DEVDRV_MAILBOX_CREATE_CBCQSQ;
            cbcqsq.alloc.sq_addr = cq_sub->callback_sq_phy_addr;
            cbcqsq.alloc.cq_addr = cq_sub->phy_addr;
            cbcqsq.alloc.sq_index = cq_sub->callback_sq_index;
            cbcqsq.alloc.cq_index = cq_info->index;
            cbcqsq.alloc.sqe_size = DEVDRV_SQ_SLOT_SIZE;
            cbcqsq.alloc.cqe_size = DEVDRV_MAX_CBCQ_SIZE;
            cbcqsq.alloc.sq_depth = DEVDRV_MAX_SQ_DEPTH;
            cbcqsq.alloc.cq_depth = DEVDRV_MAX_CBCQ_DEPTH;
            cbcqsq.alloc.plat_type = TSDRV_ENV_ONLINE;
            devdrv_calc_cq_irq_id(GET_CALLBACK_CQ_ID(fid), cq_hwinfo->cq_irq_num,
                DEVDRV_MAX_CQ_NUM, &irq_id);
            cbcqsq.alloc.cq_irq = cq_hwinfo->cq_irq[irq_id];
            TSDRV_PRINT_DEBUG("alloc cbsqcq mailbox: cmd_type(0x%x), sq_index(%u), sqe_size(%u), "
                "sq_depth(%u), cq_index(%u), cqe_size(%u), cq_depth(%u)\n", cbcqsq.cmd_type,
                cbcqsq.alloc.sq_index, cbcqsq.alloc.sqe_size, cbcqsq.alloc.sq_depth,
                cbcqsq.alloc.cq_index, cbcqsq.alloc.cqe_size,
                cbcqsq.alloc.cq_depth);
            break;
        }
        case CALLBACK_MBOX_SQCQ_FREE: {
            cbcqsq.cmd_type = DEVDRV_MAILBOX_RELEASE_CBCQSQ;
            cbcqsq.free.sq_index = cq_sub->callback_sq_index;
            cbcqsq.free.cq_index = cq_info->index;
            cbcqsq.free.plat_type = TSDRV_ENV_ONLINE;
            TSDRV_PRINT_DEBUG("free cbsqcq mailbox: sqid(%u) cqid(%u)\n", cbcqsq.free.sq_index, cbcqsq.free.cq_index);
            break;
        }
        default: {
            ka_task_mutex_unlock(&cb_mlock[devid][fid]);
            TSDRV_PRINT_ERR("Unknown mbox type(%d)\n", type);
            return -EINVAL;
        }
    }
    cbcqsq.result = 0;

    data.msg = &cbcqsq;
    data.msg_len = sizeof(struct callback_cqsq_mbox);
    ret = tsdrv_mailbox_send_sync(devid, tsid, &data);
    if (ret != 0) {
        TSDRV_PRINT_ERR("mailbox send fail(%d)\n", ret);
    }
    ka_task_mutex_unlock(&cb_mlock[devid][fid]);

    return ret;
}

s32 hvtsdrv_cb_mailbox_logic_to_ts(u32 devid, u32 tsid, u32 fid, ka_pid_t tgid, u8 *msg)
{
    struct cb_physic_sqcq *cb_sqcq = (struct cb_physic_sqcq *)msg;
    u32 cmd = cb_sqcq->mailbox_type;
    struct devdrv_ts_cq_info *cq_info = NULL;
    struct devdrv_cq_sub_info *cq_sub = NULL;
    struct tsdrv_ts_resource *ts_res = NULL;
    struct callback_logic_cq_mbox cq_mbox;
    struct tsdrv_mbox_data data = {0};
    struct tsdrv_cq_hwinfo *cq_hwinfo;
    enum tsdrv_dev_status status;
    phys_addr_t info_mem_addr;
    s32 ret;
    u16 irq_id;

    if ((cmd != DEVDRV_MAILBOX_CREATE_LOGIC_CBCQ) && (cmd != DEVDRV_MAILBOX_RELEASE_LOGIC_CBCQ)) {
        TSDRV_PRINT_ERR("Invalid mbox cmd(%d)\n", cmd);
        return -EINVAL;
    }

    status = tsdrv_get_dev_status(devid, fid);
    if (status != TSDRV_DEV_ACTIVE) {
        TSDRV_PRINT_ERR("hvtsdrv dev-%u fid-%u status-%u not active.\n", devid, fid, status);
        return -EINVAL;
    }

    tsid = 0;
    ts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
    info_mem_addr = ts_res->mem_info[DEVDRV_INFO_MEM].virt_addr;
    cq_info = devdrv_calc_cq_info(info_mem_addr, GET_CALLBACK_CQ_ID(fid));
    cq_sub = (struct devdrv_cq_sub_info *)cq_info->cq_sub;
    ka_task_mutex_lock(&cb_mlock[devid][fid]);
    if (cq_info->alloc_status == SQCQ_INACTIVE || cq_sub->callback_sq_index == DEVDRV_INVALID_CB_SQ_ID) {
        ka_task_mutex_unlock(&cb_mlock[devid][fid]);
        TSDRV_PRINT_ERR("Cbcq is not initialized. (dev=%u; fid=%u; alloc_status=%u; callback_sq_index=%u)\n",
            devid, fid, cq_info->alloc_status, cq_sub->callback_sq_index);
        return -EINVAL;
    }

    cq_hwinfo = tsdrv_get_cq_hwinfo(devid, tsid);
    devdrv_calc_cq_irq_id(cq_info->index, cq_hwinfo->cq_irq_num, DEVDRV_MAX_CQ_NUM, &irq_id);
    cq_mbox.cq_irq = cq_hwinfo->cq_irq[irq_id];
    cq_mbox.valid = DEVDRV_MAILBOX_MESSAGE_VALID;
    cq_mbox.cmd_type = cmd;
    cq_mbox.vpid = cb_sqcq->vpid;
    cq_mbox.grpid = cb_sqcq->grpid;
    cq_mbox.logic_cqid = cb_sqcq->logic_cqid;
    cq_mbox.phy_cqid = cq_info->index;
    cq_mbox.phy_sqid = cq_sub->callback_sq_index;
    cq_mbox.plat_type = TSDRV_ENV_ONLINE;
    cq_mbox.result = 0;

    data.msg = &cq_mbox;
    data.msg_len = sizeof(struct callback_logic_cq_mbox);
    ret = tsdrv_mailbox_send_sync(devid, tsid, &data);
    if (ret != 0) {
        TSDRV_PRINT_ERR("mailbox send fail(%d)\n", ret);
    }
    ka_task_mutex_unlock(&cb_mlock[devid][fid]);

    return ret;
}

s32 hvtsdrv_cb_inform_tsagent_vsq_tail(u32 devid, u32 fid, u32 vsqid, u32 tail)
{
#ifndef TSDRV_UT
    struct tsdrv_id_inst id_inst = {.devid = devid, .fid = fid, .tsid = 0};
    struct tsdrv_ts_resource *ts_res = NULL;
    int ret;

    ts_res = tsdrv_get_ts_resoruce(devid, fid, id_inst.tsid);
    hvtsdrv_update_vsq_tail(ts_res, vsqid, tail);

    /* Callback_sq only support send one sqe at a time */
    ret = hvtsdrv_notice_tsagent_vsq_proc(&id_inst, vsqid, CALLBACK_VSQCQ_TYPE, 1);
    if (ret != 0) {
        TSDRV_PRINT_ERR("notice tsagent faild, devid-%u, fid-%u, tsid-%u, vsqid-%u.\n",
            devid, fid, id_inst.tsid, vsqid);
        return -EINVAL;
    }

    TSDRV_PRINT_DEBUG("dev-%u fid-%u tsid-%u vsqid-%u and value-%u.\n", devid, fid, id_inst.tsid, vsqid, tail);
#endif
    return 0;
}

STATIC int hvtsdrv_cb_set_cq_head(u32 devid, u32 fid, u32 cq_head)
{
    struct callback_phy_sqcq *phy_sqcq = NULL;
    struct devdrv_ts_cq_info *cq_info = NULL;
    struct tsdrv_ts_resource *ts_res = NULL;
    phys_addr_t info_mem_addr;
    u32 tsid = 0;
    u32 vcq_id;
    u32 index;
    int ret;

    ts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
    vcq_id = ts_res->id_res[TSDRV_CQ_ID].id_capacity;
    hvtsdrv_update_vcq_head(ts_res, vcq_id, cq_head);

    info_mem_addr = ts_res->mem_info[DEVDRV_INFO_MEM].virt_addr;
    cq_info = devdrv_calc_cq_info(info_mem_addr, GET_CALLBACK_CQ_ID(fid));
    cq_info->head = cq_head;

    index = GET_CALLBACK_CQ_ID(fid) + DEVDRV_TS_DOORBELL_SQ_NUM;
    phy_sqcq = callback_physic_sqcq_get(devid, TSDRV_PM_FID, tsid);
    if (phy_sqcq == NULL) {
        TSDRV_PRINT_ERR("callback info is null.(devid:%u fid:%u tsid:%u)\n", devid, fid, tsid);
        return -EINVAL;
    }

    ret = callback_set_doorbell(phy_sqcq, index, cq_head);
    if (ret != 0) {
        TSDRV_PRINT_ERR("devid:%u, vfid:%u, set doorbell failed, cqid:%u, head:%u\n", devid, fid, index, cq_head);
    }
    return ret;
}

s32 hvtsdrv_cb_set_doorbell(u32 devid, u32 tsid, u32 fid, ka_pid_t tgid, u8 *msg)
{
    struct cb_physic_sqcq *cb_sqcq = (struct cb_physic_sqcq *)msg;
    u32 doorbell_val = cb_sqcq->doorbell_val;
    struct tsdrv_ts_resource *ts_res = NULL;
    enum tsdrv_dev_status status;
    u32 tmp_vcqid;
    tsid = 0;

    ts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
    status = tsdrv_get_dev_status(devid, fid);
    if (status != TSDRV_DEV_ACTIVE) {
        TSDRV_PRINT_ERR("dev-%u fid-%u, invalid status-%d.\n", devid, fid, status);
        return -EEXIST;
    }

    if (doorbell_val >= DEVDRV_MAX_SQ_DEPTH) {
        TSDRV_PRINT_ERR("dev-%u fid-%u, invalid val-%u.\n", devid, fid, doorbell_val);
        return -EINVAL;
    }

    tmp_vcqid = ts_res->id_res[TSDRV_CQ_ID].id_capacity + DEVDRV_TS_DOORBELL_SQ_NUM;
    TSDRV_PRINT_DEBUG("dev-%u fid-%u tsid-%u vcqid-%u and value-%u.\n", devid, fid, tsid, tmp_vcqid, doorbell_val);

    if (cb_sqcq->virt_id == tmp_vcqid) {
        return hvtsdrv_cb_set_cq_head(devid, fid, doorbell_val);
    } else if (cb_sqcq->virt_id < ts_res->id_res[TSDRV_SQ_ID].id_capacity) {
        return hvtsdrv_cb_inform_tsagent_vsq_tail(devid, fid, cb_sqcq->virt_id, doorbell_val);
    } else {
        TSDRV_PRINT_ERR("dev-%u fid-%u ioctl fail, invalid vsqid-%u.\n", devid, fid, cb_sqcq->virt_id);
        return -EINVAL;
    }
}

STATIC void hvtsdrv_cb_update_cq_tail_and_sq_head(struct tsdrv_ts_resource *ts_res,
    struct devdrv_ts_cq_info *cq_info, struct devdrv_cq_sub_info *cq_sub_info)
{
    struct devdrv_ts_sq_info *sq_info = NULL;
    tsdrv_cpu_task_t *report = NULL;
    struct tsdrv_id_info *sq_id_info = NULL;
    phys_addr_t info_mem_addr;
    u32 next_tail;
    u32 devid, fid;

    devid = tsdrv_get_devid_by_res(ts_res);
    fid = cq_info->vfid;
    info_mem_addr = ts_res->mem_info[DEVDRV_INFO_MEM].virt_addr;

    report = (tsdrv_cpu_task_t *)((uintptr_t)((unsigned long)cq_sub_info->virt_addr +
        (unsigned long)cq_info->slot_size * cq_info->tail));

    while (report->phase == cq_info->phase) {
        /**
         * In the ARM environment:
         * 1.Out-of-order may cause the TS driver to read other members of the report structure (read instruction 1)
         *   to be executed before the phase comparison instruction (read instruction 2);
         * 2.The time when TS writes report is between [read instruction 1] and [read instruction 2];
         * This error timing may cause [Read instruction 1] to read the old value.
         */
        ka_rmb();
        next_tail = (cq_info->tail + 1) % DEVDRV_MAX_CBCQ_DEPTH;
        /* use one slot to keep tail not cover the head, which may have report */
        if (next_tail == cq_info->head) {
            break;
        }

        if ((report->sqid == cq_sub_info->callback_sq_index) && (report->sq_head < DEVDRV_MAX_SQ_DEPTH)) {
            sq_info = devdrv_calc_sq_info(info_mem_addr, report->sqid);
            sq_info->head = report->sq_head;

            sq_id_info = devdrv_find_one_id(&ts_res->id_res[TSDRV_SQ_ID], sq_info->index);
            if (sq_id_info == NULL) {
                break;
            }
            report->sqid = (uint16_t)sq_id_info->virt_id;
            TSDRV_PRINT_DEBUG("update vsq head, devid(%u), fid(%u), sqid(%u), vsqid(%u)\n",
                devid, fid, sq_info->index, sq_id_info->virt_id);
            hvtsdrv_update_vsq_head(ts_res, sq_id_info->virt_id, sq_info->head);
        }

        next_tail = cq_info->tail + 1;
        if (next_tail > (DEVDRV_MAX_CBCQ_DEPTH - 1)) {
            cq_info->phase = ((cq_info->phase == DEVDRV_PHASE_STATE_0) ? DEVDRV_PHASE_STATE_1 : DEVDRV_PHASE_STATE_0);
            cq_info->tail = 0;
        } else {
            cq_info->tail++;
        }
        report = (tsdrv_cpu_task_t *)((uintptr_t)((unsigned long)cq_sub_info->virt_addr +
            (unsigned long)cq_info->slot_size * cq_info->tail));
    }
}

void hvtsdrv_cb_update_cq_report(ka_work_struct_t *work)
{
    struct tsdrv_ts_resource *vts_res = NULL;
    struct devdrv_cq_sub_info *cq_sub_info = NULL;
    struct devdrv_ts_cq_info *cq_info = NULL;
    struct tsdrv_vcq_info *vcq_info = NULL;
    struct vtsdrv_vcq_data_para vcq_data;
    struct tsdrv_dev_resource *dev_res = NULL;
    phys_addr_t sqcq_info_mem_addr;
    u64 bottom_tick = (u64)jiffies;
    phys_addr_t vcq_vaddr;
    u32 old_tail, cqid;
    u32 delay_time;
    int ret;

    vcq_info = ka_container_of(work, struct tsdrv_vcq_info, update_cq_work);

    delay_time = (unsigned int)ka_system_jiffies_to_msecs(bottom_tick - vcq_info->top_half_tick);
    if (delay_time > TSDRV_CQ_WORK_CONSUME_MAX) {
        TSDRV_PRINT_WARN("work dispatch consume (%u)ms\n", delay_time);
    }
    vts_res = tsdrv_get_ts_resoruce(vcq_info->devid, vcq_info->fid, vcq_info->tsid);
    cqid = tsdrv_vrit_to_physic_id(&vts_res->id_res[TSDRV_CQ_ID], vcq_info->index);

    sqcq_info_mem_addr = vts_res->mem_info[DEVDRV_INFO_MEM].virt_addr;
    cq_info = devdrv_calc_cq_info(sqcq_info_mem_addr, cqid);
    cq_sub_info = (struct devdrv_cq_sub_info *)cq_info->cq_sub;
    TSDRV_PRINT_DEBUG("vcqid(%u), head(%u), tail(%u), cqid(%u), head(%u), tail(%u)\n",
        vcq_info->index, vcq_info->head, vcq_info->tail, cq_info->index, cq_info->head, cq_info->tail);

    ka_task_spin_lock_irq(&cq_sub_info->spinlock);
    if (cq_sub_info->virt_addr == 0) {
        ka_task_spin_unlock_irq(&cq_sub_info->spinlock);
        TSDRV_PRINT_ERR("invalid para, vaddr is %s.\n", cq_sub_info->virt_addr == 0 ? "null" : "not null");
        return;
    }

    old_tail = cq_info->tail;
    /* 1. update cq report, include cq tail/sq head/vsq head */
    hvtsdrv_cb_update_cq_tail_and_sq_head(vts_res, cq_info, cq_sub_info);

    /* if the report was copied, it needs not to copy when wq is scheduled */
    if (cq_info->tail == old_tail) {
        ka_task_spin_unlock_irq(&cq_sub_info->spinlock);
        TSDRV_PRINT_DEBUG("no new reports: devid(%u), fid(%u), tsid(%u), cqid(%u), tail(%u), old_tail(%u)\n",
            vcq_info->devid, vcq_info->fid, vcq_info->tsid, cq_info->index, cq_info->tail, old_tail);
        return;
    }

    /* 2. copy cq to vcq */
    dev_res = tsdrv_get_dev_resource(vcq_info->devid, vcq_info->fid);
    TSDRV_PRINT_DEBUG("Show vm page size. (devid=%u; fid=%u; page_size=%u)\n",
        vcq_info->devid, vcq_info->fid, dev_res->page_size);
    vcq_vaddr = vts_res->mem_info[DEVDRV_CQ_MEM].virt_addr +
        (phys_addr_t)vcq_info->index * ROUND_UP(DEVDRV_MAX_CQ_DEPTH * DEVDRV_CQ_SLOT_SIZE, dev_res->page_size);
    hvtsdrv_copy_cq_to_vcq(cq_info, cq_sub_info->virt_addr, vcq_vaddr, old_tail);

    /* 3. update vcq tail */
    vcq_info->tail = cq_info->tail;

    vcq_data.vcq_id = vcq_info->index;
    vcq_data.vcq_tail = vcq_info->tail;
    ka_task_spin_unlock_irq(&cq_sub_info->spinlock);

    /* 4. send vpc msg to vm */
    ret = hvtsdrv_cq_wake_up(vcq_info->devid, vcq_info->fid, vcq_info->tsid, 0, &vcq_data);
    if (ret != 0) {
        TSDRV_PRINT_ERR("wake up failed\n");
        return;
    }
}

STATIC tsdrv_cpu_task_t *hvtsdrv_cb_get_report(struct devdrv_ts_cq_info *cq_info)
{
    struct devdrv_cq_sub_info *cq_sub_info = (struct devdrv_cq_sub_info *)cq_info->cq_sub;
    tsdrv_cpu_task_t *report = (tsdrv_cpu_task_t *)((uintptr_t)((unsigned long)cq_sub_info->virt_addr +
        (unsigned long)cq_info->slot_size * cq_info->tail));
    TSDRV_PRINT_DEBUG("cqid=%u; tail=%u; phase=%u; task_type=%u; sqid=%u; sq_head=%u; "
        "stream_id=%u; task_id=%u; cqid=%u\n",
        cq_info->index, cq_info->tail, report->phase, report->task_type,
        report->sqid, report->sq_head, report->stream_id, report->task_id, report->task.cb_task.cqid);
    return report;
}

void hvtsdrv_cb_cq_handler(u32 devid, u32 fid, u32 tsid, u32 cq_id)
{
    struct devdrv_cq_sub_info *cq_sub_info = NULL;
    struct tsdrv_ts_resource *vts_res = NULL;
    struct devdrv_ts_cq_info *cq_info = NULL;
    struct tsdrv_vcq_info *vcq_info = NULL;
    tsdrv_cpu_task_t *report = NULL;
    phys_addr_t info_mem_addr;
    unsigned long flags;
    u32 vcq_id, phase;

    vts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
    info_mem_addr = vts_res->mem_info[DEVDRV_INFO_MEM].virt_addr;
    cq_info = devdrv_calc_cq_info(info_mem_addr, cq_id);
    cq_sub_info = (struct devdrv_cq_sub_info *)cq_info->cq_sub;

    ka_task_spin_lock_irq(&cq_sub_info->spinlock);
    /* callback cq may not alloced */
    if (cq_info->alloc_status == SQCQ_INACTIVE) {
        ka_task_spin_unlock_irq(&cq_sub_info->spinlock);
        return;
    }
    report = hvtsdrv_cb_get_report(cq_info);
    phase = report->phase;
    ka_task_spin_unlock_irq(&cq_sub_info->spinlock);

    TSDRV_PRINT_DEBUG("fid(%u), tsid(%u), cqid(%u), head(%u), tail(%u), cq phase(%u), report phase(%u)\n",
        fid, tsid, cq_info->index, cq_info->head, cq_info->tail, cq_info->phase, phase);
    if (phase == cq_info->phase) {
        vcq_id = vts_res->id_res[TSDRV_CQ_ID].id_capacity;
        ka_task_spin_lock_irqsave(&vts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
        vcq_info = hvtsdrv_get_vcq_info(vts_res, vcq_id);
        if (vcq_info == NULL) {
            ka_task_spin_unlock_irqrestore(&vts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
            return;
        }
        vcq_info->top_half_tick = (u64)jiffies;
        if (vcq_info->update_cq_wq != NULL) {
            (void)ka_task_queue_work(vcq_info->update_cq_wq, &vcq_info->update_cq_work);
        }
        ka_task_spin_unlock_irqrestore(&vts_res->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
    }
}

void hvtsdrv_cb_mutex_init(u32 devid, u32 fid)
{
    ka_task_mutex_init(&cb_mlock[devid][fid]);
}

void hvtsdrv_cb_mutex_destroy(u32 devid, u32 fid)
{
    ka_task_mutex_destroy(&cb_mlock[devid][fid]);
}

/* used in container scenarios */
s32 hvtsdrv_ioctl_cbsqcq_alloc(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    return devdrv_ioctl_cbsqcq_alloc(ctx, arg);
}

s32 hvtsdrv_ioctl_cbsqcq_free(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    return devdrv_ioctl_cbsqcq_free(ctx, arg);
}

s32 hvtsdrv_ioctl_cbsq_send(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    return devdrv_ioctl_cbsq_send(ctx, arg);
}
