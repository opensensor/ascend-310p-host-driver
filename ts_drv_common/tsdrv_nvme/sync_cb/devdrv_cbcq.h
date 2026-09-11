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
#ifndef DEVDRV_CBCQ_H
#define DEVDRV_CBCQ_H

#include "devdrv_cbsqcq_common.h"
#include "devdrv_cqsq.h"
#include "ka_task_pub.h"
#include "ka_barrier_pub.h"
#include "ka_memory_pub.h"
#include "ka_list_pub.h"
#include "ka_common_pub.h"
#include "ka_system_pub.h"

#define SYNC_CB_MIN_CQE_SIZE    32U
#define SYNC_CB_MAX_CQE_SIZE    64U

#define SYNC_CB_MIN_CQE_DEPTH   1U
#define SYNC_CB_MAX_CQE_DEPTH   1U

#define SYNC_CB_MAX_CQID_NUM    1024U

#define SYNC_CB_CQ_MAX_GID      128U

#define SYNC_CB_BITMAP_SIZE     16U

struct cbcq_report {
    u16 phase : 1;       /* 用于指示翻转，对于深度为1，每次发送都反转，TS和Driver需要处理该字段 */
    u16 SOP : 1;         /* start of packet, indicates this is the first 32bit return payload */
    u16 MOP : 1;         /* middle of packet, indicates the payload is a continuation of previous task return payload */
    u16 EOP : 1;         /* *<end of packet, indicates this is the last 32bit return payload.
                             SOP & EOP can appear in the same packet, MOP & EOP can also appear on the same packet. */
    u16 reserved_1 : 12; // 预留
    u16 streamID;
    u16 taskID;
    u16 SQ_id;
    u16 SQ_head;
    u16 sequenceId; // 用于匹配
    u32 reserved_2;
    u64 HostFuncCBPtr;
    u64 fnDataPtr;
};

enum devdrv_cbcq_data_flag {
    CQREPORT_NO_DATA,
    CQREPORT_HAS_DATA
};

struct devdrv_cbcq {
    u32 devid;
    u32 tsid;
    u32 tgid;
    u32 pair_sqid;

    u32 cqid;
    u32 allocated;
    u32 phase;

    ka_list_head_t cq_list;
    phys_addr_t paddr; // cbsq slot base paddr
    void *vaddr;       // cbsq slot base vaddr
    size_t size;       // cbsq slot size
    u32 depth;         // cbsq depth
    void *ctx;         // process context
    u32 data;          // 0 : cq has no data, 1 : cq has readable data
    u32 gid;           // indicats thread group id, 0~127
    ka_task_spinlock_t lock;

    unsigned long map_va;
};

struct devdrv_cbcq_irq_data {
    u32 tsid;
    ka_tasklet_struct_t tasklet_data;
};

struct devdrv_cbcq_data {
    u32 devid;
    u32 cbcq_num[DEVDRV_MAX_TS_NUM];
    u32 cbcq_max_size[DEVDRV_MAX_TS_NUM];
    u32 cbcq_max_depth[DEVDRV_MAX_TS_NUM];

    u32 irq_num[DEVDRV_MAX_TS_NUM];
    u32 irq[DEVDRV_MAX_TS_NUM];
    u32 irq_request[DEVDRV_MAX_TS_NUM];
    struct devdrv_cbcq_irq_data irq_data[DEVDRV_MAX_TS_NUM];

    phys_addr_t paddr[DEVDRV_MAX_TS_NUM];
    size_t p_size[DEVDRV_MAX_TS_NUM];
    void *vaddr[DEVDRV_MAX_TS_NUM];

    enum devdrv_cbsqcq_mem_type mem_type[DEVDRV_MAX_TS_NUM];
    ka_mutex_t cq_mutex[DEVDRV_MAX_TS_NUM];
    ka_task_spinlock_t lock[DEVDRV_MAX_TS_NUM];

    ka_list_head_t cq_avail_head[DEVDRV_MAX_TS_NUM];
    ka_list_head_t cq_alloc_head[DEVDRV_MAX_TS_NUM];
    ka_list_head_t cq_abandon_head[DEVDRV_MAX_TS_NUM];
    struct devdrv_cbcq cbcq[DEVDRV_MAX_TS_NUM][SYNC_CB_MAX_CQID_NUM];
};

struct devdrv_report_wait_para {
    u32 gid;
    u64 cbcq_bitmap[SYNC_CB_BITMAP_SIZE];
    u32 cbcq_bitmap_size;
    int timeout;
};
int devdrv_cbcq_init(u32 devid, u32 tsid, struct devdrv_cbcq_init_para *cq_init);
void devdrv_cbcq_exit(u32 devid, u32 tsid);
int devdrv_cbcq_alloc(u32 devid, u32 tsid, struct devdrv_cbcq_alloc_para *cbcq_alloc);
int devdrv_cbcq_recycle(u32 devid, u32 tsid, struct devdrv_cbcq_alloc_para *cbcq_alloc, enum cbsqcq_alloc_flag flag);
int devdrv_cbcq_report_wait(u32 devid, u32 tsid, struct devdrv_report_wait_para *cbcq_wait, struct tsdrv_ctx *ctx);
void callback_cq_ctx_init(struct callback_ctx *cb_ctx);
void callback_cq_ctx_exit(struct callback_ctx *cb_ctx);

struct devdrv_cbcq_data *devdrv_cbcq_data_get(u32 devid);

u32 devdrv_cbcq_get_irq(u32 devid, u32 tsid);
int tsdrv_get_cb_cq_mem_phy_addr(u32 devId, u32 tsId, u32 cqId, struct tsdrv_phy_addr_get *info);

int devdrv_cbcq_setup(u32 devid);
void devdrv_cbcq_cleanup(u32 devid);
extern struct devdrv_statistic_info *devdrv_manager_get_sysfs_dfx(void);

#endif /* __DEVDRV_CBCQ_H */
