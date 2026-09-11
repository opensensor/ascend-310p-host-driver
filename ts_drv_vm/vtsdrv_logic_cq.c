/*
* Copyright (c) Huawei Technologies Co., Ltd. 2019-2022. All rights reserved.
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

#include "vtsdrv_common.h"
#include "vtsdrv_proc.h"
#include "vtsdrv_sqcq.h"
#include "tsdrv_kernel_common.h"

#include "logic_cq.h"

#ifndef VM_ST_TEST
int tsdrv_thread_bind_irq_alloc(u32 devid, u32 tsid)
{
    return -1;
}

void tsdrv_thread_bind_irq_free(u32 devid, u32 tsid, int irq)
{
}

#ifndef TSDRV_UT
void tsdrv_thread_bind_irq_set_wait_para(u32 devid, u32 tsid, int irq, wait_queue_head_t *wait_queue,
    ka_atomic_t *wait_flag)
{
}
#endif
enum devdrv_ts_status tsdrv_get_ts_status(u32 devid, u32 tsid)
{
    return TS_WORK;
}
void tsdrv_trigger_phy_cq_scan(u32 devid, u32 tsid, u32 phy_cqid)
{
}

#else
void logic_sqcq_phy_cq_handler(u32 devid, u32 tsid, struct devdrv_ts_cq_info *cq_info)
{
}
#endif

static int logic_sqcq_cmd_send(u32 devid, u32 tsid, struct logic_cqsq_mbox_msg *mbox_msg, u32 cmd, u32 sub_cmd)
{
#ifndef TSDRV_UT
    struct vtsdrv_msg msg;
    int ret;

    ret = memcpy_s((void *)msg.vdev_proc.proc_msg.msg, PROC_MSG_MAX_LEN, mbox_msg, sizeof(struct logic_cqsq_mbox_msg));
    if (ret != 0) {
        TSDRV_PRINT_ERR("Failed to memcpy. (devid=%u; tsid=%u; msg_max_len=%u; mbox_msg_len=%ld)\n", devid, tsid,
            PROC_MSG_MAX_LEN, sizeof(struct logic_cqsq_mbox_msg));
        return -ENODEV;
    }
    msg.cmd = cmd;
    msg.sub_cmd = sub_cmd;
    msg.vdev_proc.proc_msg.tsid = tsid;
    msg.vdev_proc.tgid = current->tgid;
    ret = vtsdrv_vpc_msg_send(devid, &msg);
    if (ret != 0) {
        TSDRV_PRINT_ERR("Failed to send cmd. (device=%u)\n", devid);
        return ret;
    }
#endif
    return 0;
}

int logic_sqcq_mbox_send(u32 devid, u32 tsid, u8 *mbox_msg)
{
    struct logic_cqsq_mbox_msg *m_msg = (struct logic_cqsq_mbox_msg *)mbox_msg;
    struct vtsdrv_msg msg;
    int ret;

    ret = memcpy_s((void *)msg.vdev_proc.proc_msg.msg, PROC_MSG_MAX_LEN, mbox_msg, sizeof(struct logic_cqsq_mbox_msg));
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("device-%u, cmd_type %d memcpy_s failure.\n", devid, m_msg->cmd_type);
        return ret;
#endif
    }

    TSDRV_PRINT_DEBUG("Send vpc message. (devid=%u; tsid=%u; tgid=%d)\n", devid, tsid, current->tgid);
    msg.cmd = TSDRV_DEV_PROC;
    msg.sub_cmd = (m_msg->cmd_type == TSDRV_MBOX_LOGIC_CQ_ALLOC) ? LOGIC_CQ_ALLOC : LOGIC_CQ_FREE;
    msg.vdev_proc.proc_msg.tsid = tsid;
    msg.vdev_proc.tgid = current->tgid;
    ret = vtsdrv_vpc_msg_send(devid, &msg);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Failed to send mailbox. (device=%u)\n", devid);
        return ret;
#endif
    }
    return 0;
}

/* clean phy cq in cq free msg */
int logic_cq_phy_cq_clean(struct tsdrv_id_inst *id_inst, u32 phy_cq, u32 logic_cqid, u32 type)
{
    return 0;
}

#ifndef TSDRV_UT
static phys_addr_t logic_cq_get_ts_cq_mem(u32 dev_id, u32 ts_id, u32 vcq_id)
{
    struct vtsdrv_ctrl *tsdrv_ctrl = NULL;
    phys_addr_t base_addr;
    phys_addr_t slot_addr;
    u32 sq_capacity;

    tsdrv_ctrl = vtsdrv_get_ctrl(dev_id);
    sq_capacity = tsdrv_ctrl->id_capacity[ts_id].sq_capacity;

    base_addr = tsdrv_ctrl->sqcq_hwinfo.sqcq_bar_addr;

    /* tsid is not consider */
    /* callback vcqid is max_vcqid */
    slot_addr = base_addr + sq_capacity * DEVDRV_MAX_SQ_DEPTH * DEVDRV_SQ_SLOT_SIZE +
        vcq_id * DEVDRV_MAX_CQ_DEPTH * LOGIC_CQE_SIZE;

    return slot_addr;
}
#endif

static u32 head[DEVDRV_MAX_DAVINCI_NUM][DEVDRV_MAX_TS_NUM][DEVDRV_MAX_CQ_NUM];
static u8 *vcq_vaddr[DEVDRV_MAX_DAVINCI_NUM][DEVDRV_MAX_TS_NUM][DEVDRV_MAX_CQ_NUM];

int logic_cq_vcq_handle(u32 devid, u32 tsid, u32 vcq_id, u32 vcq_tail)
{
#ifndef TSDRV_UT
    struct tag_ts_logic_cq_report_msg *report = NULL;
    struct logic_cq_info *logic_cq = NULL;
    struct logic_cqsq_mbox_msg mbox_msg;
    u32 count, i;
    int ret;

    TSDRV_PRINT_DEBUG("Handle logic vcq. (devid=%u; tsid=%u; cqid=%u; tail=%u)\n", devid, tsid, vcq_id, vcq_tail);
    if (vcq_vaddr[devid][tsid][vcq_id] == NULL) {
        phys_addr_t paddr;
        paddr = logic_cq_get_ts_cq_mem(devid, tsid, vcq_id);
        vcq_vaddr[devid][tsid][vcq_id] = ka_mm_ioremap(paddr, LOGIC_CQ_DEPTH * LOGIC_CQE_SIZE);
        if (vcq_vaddr[devid][tsid][vcq_id] == NULL) {
            TSDRV_PRINT_ERR("logic ka_mm_ioremap failed\n");
            return -ENOMEM;
        }
    }

    logic_cq = logic_cq_info_get(devid, 0, tsid);
    if (logic_cq == NULL) {
        TSDRV_PRINT_ERR("get logic_cq fail, devid=%u tsid=%u cqid=%u\n", devid, tsid, vcq_id);
        return -EFAULT;
    }

    count = head[devid][tsid][vcq_id] < vcq_tail ?
        (vcq_tail - head[devid][tsid][vcq_id]) : (LOGIC_CQ_DEPTH - (head[devid][tsid][vcq_id] - vcq_tail));
    for (i = 0; i < count; i++) {
        report = (struct tag_ts_logic_cq_report_msg *)(vcq_vaddr[devid][tsid][vcq_id] +
            LOGIC_CQE_SIZE * head[devid][tsid][vcq_id]);
        ret = logic_cq_dispatch(devid, 0, tsid, vcq_id, logic_cq, report);
        if (ret != 0) {
            TSDRV_PRINT_ERR("Dispatch logic cq fail. (devid=%u; tsid=%u; cqid=%u)\n", devid, tsid, vcq_id);
            return ret;
        }
        TSDRV_PRINT_DEBUG("Get logic type cq report. (vcq_vaddr=0x%pK; logic_cqid=%u; phase=%u; SOP=%u; MOP=%u; "
            "EOP=%u; stream_id=%u; task_id=%u; head=%u)\n", (void *)(uintptr_t)(u64)vcq_vaddr[devid][tsid][vcq_id],
            report->logic_cq_id, report->phase, report->SOP, report->MOP, report->EOP, report->stream_id,
            report->task_id, head[devid][tsid][vcq_id]);
        head[devid][tsid][vcq_id] = ((head[devid][tsid][vcq_id] + 1) % LOGIC_CQ_DEPTH);
    }
    mbox_msg.cq_head_update.logic_cq_head = head[devid][tsid][vcq_id];
    ret = logic_sqcq_cmd_send(devid, tsid, &mbox_msg, TSDRV_DEV_PROC, LOGIC_CQ_HEAD_UPDATE);
    if (ret != 0) {
        TSDRV_PRINT_ERR("Update cq_head fail. (devid=%u; tsid=%u; head=%u)\n", devid, tsid, head[devid][tsid][vcq_id]);
        return ret;
    }
    return 0;
#endif
}
