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

#include "tsdrv_cb_pm.h"
#include "vtsdrv_init.h"
#include "vtsdrv_proc.h"
#include "vtsdrv_sqcq.h"
#include "ka_memory_pub.h"

int callback_get_physic_sq(struct callback_phy_sqcq *cb_sqcq)
{
    struct vtsdrv_ctrl *tsdrv_ctrl = vtsdrv_get_ctrl(cb_sqcq->devid);
    struct vtsdrv_msg msg;
    unsigned long size;
    phys_addr_t paddr;
    s32 ret;

    size = (unsigned long)DEVDRV_SQ_SLOT_SIZE * DEVDRV_MAX_SQ_DEPTH;
    msg.cmd = TSDRV_DEV_PROC;
    msg.sub_cmd = CALLBACK_DEV_ALLOC_SQ;
    msg.vdev_proc.proc_msg.tsid = cb_sqcq->tsid;
    ret = vtsdrv_vpc_msg_send(cb_sqcq->devid, &msg);
    if (ret != 0) {
        TSDRV_PRINT_ERR("device-%u, get phy sq failure.\n", cb_sqcq->devid);
        return ret;
    }

    if (msg.vdev_proc.proc_msg.cb_sqcq.virt_id >= tsdrv_ctrl->id_capacity[0].sq_capacity) {
        TSDRV_PRINT_ERR("device-%u, vsqid-%u invalid.\n", cb_sqcq->devid, msg.vdev_proc.proc_msg.cb_sqcq.virt_id);
        return -EINVAL;
    }
    cb_sqcq->cb_sq.index = msg.vdev_proc.proc_msg.cb_sqcq.virt_id;
    paddr = vtsdrv_get_ts_sq_mem(cb_sqcq->devid, 0, cb_sqcq->cb_sq.index);
    cb_sqcq->cb_sq.vaddr = (void *)ka_mm_ioremap(paddr, size);
    if (cb_sqcq->cb_sq.vaddr == NULL) {
        TSDRV_PRINT_ERR("cbsq ka_mm_ioremap failed\n");
        return -ENOMEM;
    }

    TSDRV_PRINT_DEBUG("device-%u, vsqid-%u vaddr-%pK.\n", cb_sqcq->devid, cb_sqcq->cb_sq.index,
        (void *)(uintptr_t)cb_sqcq->cb_sq.vaddr);
    cb_sqcq->cb_sq.paddr = paddr;
    return 0;
}

void callback_free_physic_sq(struct callback_phy_sqcq *cb_sqcq)
{
    void *cb_sq_vaddr = NULL;

    ka_task_spin_lock(&cb_sqcq->cb_sq.lock);
    cb_sq_vaddr = cb_sqcq->cb_sq.vaddr;
    cb_sqcq->cb_sq.vaddr = NULL;
    cb_sqcq->cb_sq.paddr = 0;
    ka_task_spin_unlock(&cb_sqcq->cb_sq.lock);

    if (cb_sq_vaddr != NULL) {
        ka_mm_iounmap(cb_sq_vaddr);
    }
}

int callback_get_physic_cq(struct callback_phy_sqcq *cb_sqcq)
{
    struct vtsdrv_ctrl *tsdrv_ctrl = vtsdrv_get_ctrl(cb_sqcq->devid);
    struct vtsdrv_msg msg;
    unsigned long size;
    phys_addr_t paddr;
    s32 ret;

    size = (unsigned long)DEVDRV_MAX_CBCQ_SIZE * DEVDRV_MAX_CBCQ_DEPTH;
    msg.cmd = TSDRV_DEV_PROC;
    msg.sub_cmd = CALLBACK_DEV_ALLOC_CQ;
    msg.vdev_proc.proc_msg.tsid = cb_sqcq->tsid;
    ret = vtsdrv_vpc_msg_send(cb_sqcq->devid, &msg);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("device-%u, get phy cq failure.\n", cb_sqcq->devid);
        return ret;
#endif
    }

    cb_sqcq->cb_cq.index = tsdrv_ctrl->id_capacity[0].cq_capacity;
    paddr = vtsdrv_get_ts_cq_mem(cb_sqcq->devid, 0, cb_sqcq->cb_cq.index);
    cb_sqcq->cb_cq.vaddr = (void *)ka_mm_ioremap_wc(paddr, size);
    if (cb_sqcq->cb_cq.vaddr == NULL) {
        TSDRV_PRINT_ERR("cbcq ka_mm_ioremap failed\n");
        return -ENOMEM;
    }
    cb_sqcq->cb_cq.paddr = paddr;
    return 0;
}

void callback_destory_cq(struct callback_phy_sqcq *cb_sqcq)
{
    void *cb_cq_vaddr = NULL;

    ka_task_spin_lock(&cb_sqcq->cb_cq.lock);
    cb_cq_vaddr = cb_sqcq->cb_cq.vaddr;
    cb_sqcq->cb_cq.vaddr = NULL;
    cb_sqcq->cb_cq.paddr = 0;
    ka_task_spin_unlock(&cb_sqcq->cb_cq.lock);

    if (cb_cq_vaddr != NULL) {
        ka_mm_iounmap(cb_cq_vaddr);
    }
}

s32 callback_mbox_send(u32 type, struct callback_phy_sqcq *cb_sqcq)
{
    struct vtsdrv_msg msg;
    s32 ret;

    msg.cmd = TSDRV_DEV_PROC;
    msg.sub_cmd = CALLBACK_DEV_MAILBOX_SEND;
    msg.vdev_proc.proc_msg.cb_sqcq.mailbox_type = type;
    msg.vdev_proc.proc_msg.tsid = cb_sqcq->tsid;
    ret = vtsdrv_vpc_msg_send(cb_sqcq->devid, &msg);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_DEBUG("device-%u, send failed.\n", cb_sqcq->devid);
        return -EINVAL;
#endif
    }
    return 0;
}

s32 callback_mbox_logic_cq(u32 cmd, struct callback_phy_sqcq *phy_sqcq,
    struct callback_logic_cq_sub *sub_cq)
{
    struct vtsdrv_msg msg;
    s32 ret;

    if ((cmd != DEVDRV_MAILBOX_CREATE_LOGIC_CBCQ) && (cmd != DEVDRV_MAILBOX_RELEASE_LOGIC_CBCQ)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Invalid mbox cmd(%d)\n", cmd);
        return -EINVAL;
#endif
    }

    msg.cmd = TSDRV_DEV_PROC;
    msg.sub_cmd = CALLBACK_DEV_MAILBOX_LOGIC_SEND;
    msg.vdev_proc.proc_msg.cb_sqcq.mailbox_type = cmd;
    msg.vdev_proc.proc_msg.cb_sqcq.vpid = sub_cq->vpid;
    msg.vdev_proc.proc_msg.cb_sqcq.grpid = sub_cq->grpid;
    msg.vdev_proc.proc_msg.cb_sqcq.logic_cqid = sub_cq->cqid;
    msg.vdev_proc.proc_msg.tsid = phy_sqcq->tsid;
    ret = vtsdrv_vpc_msg_send(phy_sqcq->devid, &msg);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("device-%u, send failed.\n", phy_sqcq->devid);
        return -EINVAL;
#endif
    }
    return 0;
}

int callback_set_doorbell(struct callback_phy_sqcq *cb_sqcq, u32 index, u32 val)
{
    struct vtsdrv_msg msg;
    s32 ret;

    msg.cmd = TSDRV_DEV_PROC;
    msg.sub_cmd = CALLBACK_DEV_SET_DOORBELL;
    msg.vdev_proc.proc_msg.cb_sqcq.virt_id = index;
    msg.vdev_proc.proc_msg.cb_sqcq.doorbell_val = val;
    msg.vdev_proc.proc_msg.tsid = cb_sqcq->tsid;
    ret = vtsdrv_vpc_msg_send(cb_sqcq->devid, &msg);

    TSDRV_PRINT_DEBUG("device-%u, set doorbell(%u %u).\n", cb_sqcq->devid, index, val);
    return ret;
}

int callback_vcq_handle(u32 devid, u32 vcq_id, u32 vcq_tail)
{
    struct callback_phy_sqcq *phy_sqcq = NULL;
    u32 tsid = 0;

    phy_sqcq = callback_physic_sqcq_get(devid, TSDRV_PM_FID, tsid);
    if (phy_sqcq == NULL) {
        TSDRV_PRINT_ERR("device-%u, vcqid-%u, callback info is null.\n", devid, vcq_id);
        return -EINVAL;
    }
    if (vcq_id == phy_sqcq->cb_cq.index) {
        task_dispatch_handler_ex(devid, TSDRV_PM_FID, tsid, 1, vcq_tail);
    }

    TSDRV_PRINT_DEBUG("device-%u, vcqid-%u.\n", devid, vcq_id);
    return 0;
}

void callback_func_lock(struct callback_info *cb_info)
{
    ka_task_mutex_lock(&cb_info->mutex_t);
}

void callback_func_unlock(struct callback_info *cb_info)
{
    ka_task_mutex_unlock(&cb_info->mutex_t);
}

void callback_irq_lock(struct callback_phy_sqcq_info *phy_sq)
{
    ka_task_spin_lock_bh(&phy_sq->lock);
}

void callback_irq_unlock(struct callback_phy_sqcq_info *phy_sq)
{
    ka_task_spin_unlock_bh(&phy_sq->lock);
}

