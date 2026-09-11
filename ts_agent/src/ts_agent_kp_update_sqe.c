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
 * Description: sqe update in the Kunpeng scenario
 * Author: huawei
 * Create: 2023-10-13
 */

#include "ts_agent_kp_update_sqe.h"
#include "securec.h"
#include "ts_agent_common.h"
#include "ts_agent_log.h"
#include "ts_agent_interface.h"
#ifndef TS_AGENT_UT
#include "trs_adapt.h"
#include "devdrv_interface.h"
#endif

#if defined(CFG_SOC_PLATFORM_KPSTARS)
static struct ts_agent_update_sqe_ops g_kp_update_sqe_ops = {
    .rdma_query_db_pa = NULL
};

// 多p场景下streamId(SID)的取值
static u16 g_stars_smmu_sid[STARS_DEV_NUM] = {0xff32, 0xffb2, 0xfe32, 0xfeb2, 0xfd32, 0xfdb2, 0xfc32, 0xfcb2};

int sqe_proc_rdma(u32 devid, u32 tsid, int pid, u32 sq_id, ts_stars_sqe_t *sqe)
{
    ts_stars_rdma_sqe_t *rdma_sqe = (ts_stars_rdma_sqe_t *)sqe;
    uint32_t rdma_type = rdma_sqe->rdma_qp_addr_low;
    if (rdma_type != 0U) {
        // here should use a reserved value to identify rdma task and ub sqe
        // ub sqe: rdma_qp_addr_low is not 0
        return EOK;
    }
    if (g_kp_update_sqe_ops.rdma_query_db_pa == NULL) {
        ts_agent_err("no registered function is available.");
        return -EINVAL;
    }
    uint32_t qpn = rdma_sqe->tag;
    uint32_t sqid = rdma_sqe->rdma_qp_addr_high;

    // get rdma db addr from rdma adpt, this address is a fixed
    // value for each rdma device now
    uint64_t qpn_db = g_kp_update_sqe_ops.rdma_query_db_pa(devid, sqid, qpn);
    if (qpn_db == 0ULL) {
        ts_agent_err("get rdma db addr failed, devid=%u, sqid=%u.", devid, sqid);
        return -EINVAL;
    }
    rdma_sqe->rdma_qp_addr_low = (uint32_t)(qpn_db & STARS_RDMA_QP_ADDR_LOW_MASK);
    rdma_sqe->rdma_qp_addr_high = (uint32_t)(qpn_db >> STARS_DWORD_SIZE);

    return EOK;
}

void ts_agent_update_sqe_register(struct ts_agent_update_sqe_ops *ops)
{
    if (ops != NULL) {
        g_kp_update_sqe_ops = *ops;
        ts_agent_info("Reg ops in ts_agent success.");
        return;
    }
    ts_agent_info("No ops was registed in ts_agent.");
}
EXPORT_SYMBOL_GPL(ts_agent_update_sqe_register);

void ts_agent_update_sqe_unregister(void)
{
    g_kp_update_sqe_ops.rdma_query_db_pa = NULL;
}
EXPORT_SYMBOL_GPL(ts_agent_update_sqe_unregister);

static int kp_check_and_update_sdma(ts_stars_sdma_sqe_t *sdma_sqe, int src_pid, int dst_pid, u16 sid, bool isRead)
{
    u32 own_passid;
    u32 sumitter_passid;
    if (isRead) { // 本进程读操作
        bool valid = sdma_check_auth(dst_pid, &own_passid, src_pid, &sumitter_passid);
        if (!valid) {
            ts_agent_err("sdma_check_auth failed, src_pid=%d, dst_pid=%d, sqe_id=%u",
                         src_pid, dst_pid, sdma_sqe->sqe_id);
            return -EINVAL;
        }

        sdma_sqe->dst_substreamid = own_passid;
        sdma_sqe->src_substreamid = sumitter_passid;
    } else { // 本进程写操作
        bool valid = sdma_check_auth(src_pid, &own_passid, dst_pid, &sumitter_passid);
        if (!valid) {
            ts_agent_err("sdma_check_auth failed, src_pid=%u, dst_pid=%u, sqe_id=%u",
                         src_pid, dst_pid, sdma_sqe->sqe_id);
            return -EINVAL;
        }
 
        sdma_sqe->src_substreamid = own_passid;
        sdma_sqe->dst_substreamid = sumitter_passid;
    }

    sdma_sqe->src_streamid = sid;
    sdma_sqe->dst_streamid = sid;
    return EOK;
}

int kp_sqe_proc_sdma(u32 devid, u32 tsid, int pid, u32 sqid, ts_stars_sqe_t *sqe)
{
    if (devid >= STARS_DEV_NUM) {
        ts_agent_err("devid invalid, devid=%u, pid=%d, tsid=%u", devid, pid, tsid);
        return -EINVAL;
    }
    u16 sid = g_stars_smmu_sid[devid];
    ts_stars_sdma_sqe_t *sdma_sqe = (ts_stars_sdma_sqe_t *)sqe;
    int src_pid = ((u32)sdma_sqe->src_streamid << UNSIGNED_SHORT_LEN) | (u32)sdma_sqe->src_substreamid;
    int dst_pid = ((u32)sdma_sqe->dst_streamid << UNSIGNED_SHORT_LEN) | (u32)sdma_sqe->dst_substreamid;

    if ((pid != src_pid) && (pid != dst_pid)) {
        // 既不是源进程也不是目的进程
        ts_agent_err("pid invalid, devid=%u, pid=%d, tsid=%u, src_pid=%d, dst_pid=%d",
                     devid, pid, tsid, src_pid, dst_pid);
        return -EINVAL;
    }

    if ((pid == src_pid) && (pid == dst_pid)) {
        // 本进程内拷贝
        u32 pasid;
        int ret = hal_kernel_trs_get_ssid(devid, tsid, pid, &pasid);
        if (ret != EOK) {
            ts_agent_err("get ssid failed, ret=%d, devid=%u, pid=%d, tsid=%u, sqe_id=%u",
                         ret, devid, pid, tsid, sdma_sqe->sqe_id);
            return -EINVAL;
        }

        sdma_sqe->src_substreamid = pasid;
        sdma_sqe->dst_substreamid = pasid;
        sdma_sqe->src_streamid = sid;
        sdma_sqe->dst_streamid = sid;
        return EOK;
    }

    if (pid == src_pid) {
        // 当前进程是src进程
        return kp_check_and_update_sdma(sdma_sqe, src_pid, dst_pid, sid, true);
    } else {
        // 当前进程是dst进程
        return kp_check_and_update_sdma(sdma_sqe, src_pid, dst_pid, sid, false);
    }
}

#endif  // CFG_SOC_PLATFORM_KPSTARS