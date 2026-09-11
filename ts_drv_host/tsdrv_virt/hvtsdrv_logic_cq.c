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

#include "tsdrv_device.h"
#include "logic_cq.h"
#include "devdrv_id.h"
#include "tsdrv_logic_cq.h"
#include "hvtsdrv_tsagent.h"
#include "hvtsdrv_cqsq.h"
#include "hvtsdrv_logic_cq.h"

int hvtsdrv_logic_refresh_alloc_msg(u32 devid, u32 tsid, u32 fid, struct logic_cqsq_mbox_msg *mbox_msg)
{
    struct tsdrv_dev_resource *dev_res = tsdrv_get_dev_resource(devid, fid);
    struct tsdrv_ts_resource *ts_res = &dev_res->ts_resource[tsid];
    struct devdrv_ts_cq_info *cq_info = NULL;
    struct devdrv_cq_sub_info *cq_sub = NULL;
    struct tsdrv_cq_hwinfo *cq_hwinfo = NULL;
    u16 irq_id;

    ka_task_mutex_lock(&dev_res->dev_res_lock);
    if (ts_res->logic_phy_cq_flag < 0) {
        int cqid;
        cqid = tsdrv_kernel_alloc_cq(devid, fid, tsid, LOGIC_CQE_SIZE, LOGIC_CQ_DEPTH, LOGIC_SQCQ_TYPE);
        if (cqid < 0) {
#ifndef TSDRV_UT
            ka_task_mutex_unlock(&dev_res->dev_res_lock);
            TSDRV_PRINT_ERR("dev %u fid %u tsid %d alloc cq failed.\n", devid, fid, tsid);
            return -EFAULT;
#endif
        }
        ts_res->logic_phy_cq_flag = 0;
        ts_res->logic_phy_cq = cqid;
        TSDRV_PRINT_DEBUG("Alloc phy_cq success. (cqid=%u)\n", ts_res->logic_phy_cq);
    }

    cq_info = tsdrv_get_cq_info(devid, fid, tsid, ts_res->logic_phy_cq);
    if (cq_info == NULL) {
#ifndef TSDRV_UT
        ka_task_mutex_unlock(&dev_res->dev_res_lock);
        TSDRV_PRINT_ERR("dev %u fid %u tsid %d cq_id %d get cq info failed.\n",
            devid, fid, tsid, ts_res->logic_phy_cq);
        return -EINVAL;
#endif
    }
    cq_info->vfid = fid;
    cq_sub = cq_info->cq_sub;
    mbox_msg->alloc.thread_bind_irq_flag = 0;
    cq_hwinfo = tsdrv_get_cq_hwinfo(devid, tsid);
    devdrv_calc_cq_irq_id(ts_res->logic_phy_cq, cq_hwinfo->cq_irq_num, DEVDRV_MAX_CQ_NUM, &irq_id);
    mbox_msg->alloc.cq_irq = cq_hwinfo->cq_irq[irq_id];

    if (ts_res->logic_phy_cq_flag == 0) {
        ts_res->logic_phy_cq_flag = 1;
        mbox_msg->alloc.phy_cq_addr = cq_sub->phy_addr;
        cq_sub->ctx = NULL;
    } else {
        mbox_msg->alloc.phy_cq_addr = 0;
    }

    mbox_msg->alloc.vfid = fid;
    mbox_msg->alloc.phy_cqid = ts_res->logic_phy_cq;
    ka_task_mutex_unlock(&dev_res->dev_res_lock);

    TSDRV_PRINT_DEBUG("(cqid=%u; devid=%u; fid=%u; tsid=%u)\n", ts_res->logic_phy_cq, devid, fid, tsid);
    return 0;
}

int hvtsdrv_logic_cq_head_update(u32 devid, u32 tsid, u32 fid, ka_pid_t tgid, u8 *msg)
{
#ifndef TSDRV_UT
    struct tsdrv_dev_resource *dev_res = tsdrv_get_dev_resource(devid, fid);
    struct tsdrv_ts_resource *vts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
    struct logic_cqsq_mbox_msg *mbox_msg = (struct logic_cqsq_mbox_msg *)msg;
    enum tsdrv_dev_status status = tsdrv_get_dev_status(devid, fid);
    u32 cq_head = mbox_msg->cq_head_update.logic_cq_head;
    struct tsdrv_id_info *cq_id_info = NULL;
    u32 phy_cqid;

    if (status != TSDRV_DEV_ACTIVE) {
        TSDRV_PRINT_ERR("Invalid dev status. (devid=%u; fid=%u; tsid=%u; tgid=%d; status=%d)\n",
            devid, fid, tsid, tgid, status);
        return -EEXIST;
    }
    if (cq_head >= LOGIC_CQ_DEPTH) {
        TSDRV_PRINT_ERR("Invliad cq head. (devid=%u; fid=%u; tsid=%u; tgid=%d; cq_head=%u)\n",
            devid, fid, tsid, tgid, cq_head);
        return -EFAULT;
    }

    ka_task_mutex_lock(&dev_res->dev_res_lock);
    phy_cqid = vts_res->logic_phy_cq;
    if (vts_res->logic_phy_cq_flag < 0) {
        ka_task_mutex_unlock(&dev_res->dev_res_lock);
        TSDRV_PRINT_ERR("Invalid phy_cqid, must alloc before use. (devid=%u; fid=%u; tsid=%u; tgid=%d; phy_cqid=%u)\n",
            devid, fid, tsid, tgid, phy_cqid);
        return -EEXIST;
    }

    cq_id_info = devdrv_find_one_id(&vts_res->id_res[TSDRV_CQ_ID], phy_cqid);
    if (cq_id_info == NULL) {
        ka_task_mutex_unlock(&dev_res->dev_res_lock);
        TSDRV_PRINT_ERR("Get cq_id_info fail. (devid=%u; fid=%u; tsid=%u; tgid=%d;  phy_cqid=%u)\n",
            devid, fid, tsid, tgid, phy_cqid);
        return -ENODEV;
    }
    tsdrv_set_cq_doorbell(devid, tsid, phy_cqid, cq_head);
    hvtsdrv_update_vcq_head(vts_res, cq_id_info->virt_id, cq_head);
    tsdrv_update_cq_head(vts_res, phy_cqid, cq_head);

    ka_task_mutex_unlock(&dev_res->dev_res_lock);
    TSDRV_PRINT_DEBUG("(devid=%u; fid=%u; phy_cqid=%u; virt_cqid=%u; head=%u)\n", devid, fid, phy_cqid,
        cq_id_info->virt_id, cq_head);
#endif
    return 0;
}

s32 hvtsdrv_logic_cq_alloc(u32 devid, u32 tsid, u32 fid, ka_pid_t tgid, u8 *msg)
{
    enum tsdrv_dev_status status = tsdrv_get_dev_status(devid, fid);
    struct logic_cqsq_mbox_msg *mbox_msg = (struct logic_cqsq_mbox_msg *)msg;
    struct hvtsdrv_trans_mailbox_ctx trans_mbox;
    struct tsdrv_ctx *ctx = NULL;
    u32 disable_thread;
    int ret;

    if (status != TSDRV_DEV_ACTIVE) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("dev %u fid %u tsid %d tgid %d invalid status %d.\n", devid, fid, tsid, tgid, status);
        return -EEXIST;
#endif
    }

    if ((mbox_msg->alloc.cqe_size != LOGIC_CQE_SIZE) || (mbox_msg->alloc.cq_depth != LOGIC_CQ_DEPTH)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("dev %u fid %u tsid %d tgid %d invalid cqe_size %d or cq_depth %d.\n",
            devid, fid, tsid, tgid, mbox_msg->alloc.cqe_size, mbox_msg->alloc.cq_depth);
        return -EINVAL;
#endif
    }

    ret = hvtsdrv_logic_refresh_alloc_msg(devid, tsid, fid, mbox_msg);
    if (ret != 0) {
        return ret;
    }

#ifndef CFG_SOC_PLATFORM_MDC_V51
    TSDRV_PRINT_DEBUG("Transform logic cq. (valid=%u; cmd=%u)\n", mbox_msg->valid, mbox_msg->cmd_type);
    ctx = tsdrv_dev_proc_ctx_get(devid, fid, tgid);
    if (ctx == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Failed to find ctx. (dev=%u; fid=%u; tgid=%d)\n", devid, fid, tgid);
        return -ESRCH;
#endif
    }

    disable_thread = (tsdrv_ctx_is_nothread_type(ctx) == true) ? 1 : 0;
    tsdrv_dev_proc_ctx_put(ctx);
    hvtsdrv_fill_trans_info_mbox(devid, fid, tsid, disable_thread, &trans_mbox);
    ret = hvtsdrv_trans_info_msg(&trans_mbox, (void *)mbox_msg, sizeof(struct logic_cqsq_mbox_msg));
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Failed to translate logic cq info. (ret=%u; devid=%u; fid=%u; tsid=%u)\n",
            ret, devid, fid, tsid);
        return ret;
#endif
    }

#endif

#ifndef VM_ST_TEST
    return logic_sqcq_mbox_send(devid, tsid, msg);
#else
    return 0;
#endif
}

void hvtsdrv_logic_refresh_free_msg(u32 devid, u32 tsid, u32 fid, struct logic_cqsq_mbox_msg *mbox_msg)
{
    mbox_msg->free.vfid = fid;
    mbox_msg->free.phy_cqid = TSDRV_INVALID_PHY_CQID;
}

s32 hvtsdrv_logic_cq_free(u32 devid, u32 tsid, u32 fid, ka_pid_t tgid, u8 *msg)
{
    struct logic_cqsq_mbox_msg *mbox_msg = (struct logic_cqsq_mbox_msg *)msg;
    struct tsdrv_ts_resource *ts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
    struct tsdrv_id_inst id_inst;
    enum tsdrv_dev_status status = tsdrv_get_dev_status(devid, fid);
    int ret;

    if (status != TSDRV_DEV_ACTIVE) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("dev %u fid %u tsid %d tgid %d invalid status %d.\n", devid, fid, tsid, tgid, status);
        return -EEXIST;
#endif
    }

    hvtsdrv_logic_refresh_free_msg(devid, tsid, fid, mbox_msg);

#ifndef VM_ST_TEST
    ret = logic_sqcq_mbox_send(devid, tsid, msg);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("dev %u fid %u tsid %d tgid %d msg send failed.\n", devid, fid, tsid, tgid);
        return ret;
#endif
    }
#endif

    tsdrv_pack_id_inst(devid, fid, tsid, &id_inst);
    ret = logic_cq_phy_cq_clean(&id_inst, ts_res->logic_phy_cq, mbox_msg->free.logic_cqid, NORMAL_SQCQ_TYPE);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("dev %u fid %u tsid %d tgid %d msg send failed.\n", devid, fid, tsid, tgid);
#endif
    }

    return 0;
}

