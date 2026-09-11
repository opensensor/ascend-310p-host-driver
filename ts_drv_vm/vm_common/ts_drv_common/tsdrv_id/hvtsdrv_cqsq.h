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
#ifndef HVTSDRV_CQSQ_H
#define HVTSDRV_CQSQ_H

#include "ka_task_pub.h"
#include "devdrv_common.h"
#include "tsdrv_ioctl.h"
#include "tsdrv_device.h"
#include "vtsdrv_common.h"
#include "hvtsdrv_tsagent.h"
#include "devdrv_cqsq.h"

#define ROUND_UP(x, align) (((int) (x) + ((align) - 1)) & ~((align) - 1))

struct tsdrv_vsq_info {
    u32 index;
    u32 head;
    u32 tail;

    u32 head_for_tsagent; /* head for ts_agent, ts_agent use to judge if vsq have tasks */

    u64 vsq_vaddr;
    u64 vsq_size;
    u32 depth;
};

struct tsdrv_vcq_info {
    u32 devid;
    u32 fid;
    u32 tsid;

    u32 index;
    u32 head;
    u32 tail;
    u32 slot_size;
    drvSqCqAllocType_t alloc_status;
    ka_task_spinlock_t spinlock;

    phys_addr_t phy_addr;  /* slot addr */
    phys_addr_t vir_addr;

    u64 top_half_tick; /* dfx for check the time of wake up wq */
    ka_workqueue_struct_t *update_cq_wq; /* update cq tail for vm */
    ka_work_struct_t  update_cq_work; /* update cq tail for vm */
};

#define TSDRV_CQ_WORK_CONSUME_MAX 10

#define tsdrv_calc_vsq_info(addr, index)                                  \
    ({                                                                    \
        struct tsdrv_vsq_info *vsq;                                       \
        vsq = (struct tsdrv_vsq_info *)((uintptr_t)((addr) +              \
            (long)(unsigned)sizeof(struct tsdrv_vsq_info) * (index)));    \
        vsq;                                                              \
    })

#define tsdrv_calc_vcq_info(addr, index)                                  \
    ({                                                                    \
        struct tsdrv_vcq_info *vcq;                                       \
        vcq = (struct tsdrv_vcq_info *)((uintptr_t)((addr) +              \
            (long)(unsigned)sizeof(struct tsdrv_vcq_info) * (index)));    \
        vcq;                                                              \
    })

int hvtsdrv_cqsq_init(u32 devid, u32 fid);
void hvtsdrv_cqsq_uninit(u32 devid, u32 fid);
s32 hvtsdrv_sqcq_alloc(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
s32 hvtsdrv_sqcq_free(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
s32 hvtsdrv_notice_tsagent_vsq_proc(struct tsdrv_id_inst *id_inst, u32 vsqid, enum vsqcq_type type, u32 sqe_num);
s32 hvtsdrv_cq_report_release(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
s32 hvtsdrv_get_sq_head(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
void hvtsdrv_update_vsq_head(struct tsdrv_ts_resource *ts_res, u32 vsq_id, u32 vsq_head);
s32 hvtsdrv_cq_wake_up(u32 devid, u32 fid, u32 tsid, ka_pid_t tgid, struct vtsdrv_vcq_data_para *vcq_data);
void hvtsdrv_update_cq_report(ka_work_struct_t *work);
void hvtsdrv_cq_handler(u32 devid, u32 fid, u32 tsid, struct devdrv_ts_cq_info *cq_info);
void hvtsdrv_copy_cq_to_vcq(struct devdrv_ts_cq_info *cq_info, phys_addr_t cq_vaddr,
    phys_addr_t vcq_vaddr, u32 old_tail);
s32 hvtsdrv_wait_cq_report(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
void hvtsdrv_update_vcq_head(struct tsdrv_ts_resource *ts_res, u32 vcq_id, u32 cq_head);
int hvtsdrv_get_vsq_mem_phy_addr(struct tsdrv_ctx *ctx, u32 tsId, u32 sqId, struct tsdrv_phy_addr_get *info);
int tsdrv_notice_tsagent_dev_create(u32 devid, u32 fid);
void tsdrv_notice_tsagent_dev_destroy(u32 devid, u32 fid);
void hvtsdrv_clear_vcq_info(struct tsdrv_ts_resource *ts_res, u32 vcq_id);
void hvtsdrv_clear_vsq_info(struct tsdrv_ts_resource *ts_res, u32 vsq_id);

static inline struct tsdrv_vcq_info *hvtsdrv_get_vcq_info(struct tsdrv_ts_resource *vts_res, u32 vcqid)
{
    struct tsdrv_vcq_info *vcq_info = NULL;
    phys_addr_t info_mem;

    if (vts_res->mem_info[DEVDRV_VCQ_INFO_MEM].virt_addr != 0) {
        info_mem = vts_res->mem_info[DEVDRV_VCQ_INFO_MEM].virt_addr;
        vcq_info = tsdrv_calc_vcq_info(info_mem, vcqid);
    }

    return vcq_info;
}

static inline struct tsdrv_vsq_info *hvtsdrv_get_vsq_info_inner(struct tsdrv_ts_resource *vts_res,
    u32 vsqid)
{
    struct tsdrv_vsq_info *vsq_info = NULL;
    phys_addr_t info_mem;

    if (vts_res->mem_info[DEVDRV_VSQ_INFO_MEM].virt_addr != 0) {
        info_mem = vts_res->mem_info[DEVDRV_VSQ_INFO_MEM].virt_addr;
        vsq_info = tsdrv_calc_vsq_info(info_mem, vsqid);
    }

    return vsq_info;
}

#endif
