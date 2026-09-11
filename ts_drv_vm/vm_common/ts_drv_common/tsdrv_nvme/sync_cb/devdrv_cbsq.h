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
#ifndef DEVDRV_CBSQ_H
#define DEVDRV_CBSQ_H

#include "devdrv_cbsqcq_common.h"
#include "ka_memory_pub.h"
#define SYNC_CB_MIN_SQE_SIZE    32U
#define SYNC_CB_MAX_SQE_SIZE    64U

#define SYNC_CB_MIN_SQE_DEPTH   1U
#define SYNC_CB_MAX_SQE_DEPTH   1U

#define SYNC_CB_DB_INDEX        (512 + 352 - 1) /* last cq doorbell */

#define SYNC_CB_MAX_SQID_NUM    1024U

struct cbsq_sendmsg {
    u16 phase : 1;       /* 用于指示翻转，对于深度为1，每次发送都反转，TS和Driver需要处理该字段 */
    u16 SOP : 1;         /* start of packet, indicates this is the first 32bit return payload */
    u16 MOP : 1;         /* middle of packet, indicates the payload is a continuation of previous task return payload */
    u16 EOP : 1;         /* *<end of packet, indicates this is the last 32bit return payload.
                            SOP & EOP can appear in the same packet, MOP & EOP can also appear on the same packet. */
    u16 reserved_1 : 12; // 预留
    u16 streamID;
    u16 taskID;
    u16 CQ_id;
    u16 CQ_tail;
    u16 sequenceId; // 用于匹配
    u32 reserved_2;
};

struct devdrv_cbsq {
    u32 devid;
    u32 tsid;
    u32 tgid;
    u32 pair_cqid;

    u32 phase;
    u32 sqid;
    ka_atomic_t allocated;

    struct list_head sq_list;
    phys_addr_t paddr; // cbsq slot base paddr
    void *vaddr;       // cbsq slot base vaddr
    size_t size;       // cbsq slot size
    u32 depth;         // cbsq depth
    void *ctx;         // process context
};

struct devdrv_cbsq_data {
    u32 cbsq_num[DEVDRV_MAX_TS_NUM];
    u32 cbsq_max_size[DEVDRV_MAX_TS_NUM];
    u32 cbsq_max_depth[DEVDRV_MAX_TS_NUM];

    enum devdrv_cbsqcq_mem_type mem_type[DEVDRV_MAX_TS_NUM];

    phys_addr_t paddr[DEVDRV_MAX_TS_NUM];
    size_t p_size[DEVDRV_MAX_TS_NUM];
    void *vaddr[DEVDRV_MAX_TS_NUM];

    ka_mutex_t sq_mutex[DEVDRV_MAX_TS_NUM];
    struct list_head sq_avail_head[DEVDRV_MAX_TS_NUM];

    struct list_head sq_alloc_head[DEVDRV_MAX_TS_NUM];
    struct list_head sq_abandon_head[DEVDRV_MAX_TS_NUM];
    struct devdrv_cbsq cbsq[DEVDRV_MAX_TS_NUM][SYNC_CB_MAX_SQID_NUM];
};

int devdrv_cbsq_init(u32 devid, u32 tsid, struct devdrv_cbsq_init_para *sq_init);
void devdrv_cbsq_exit(u32 devid, u32 tsid);

int devdrv_cbsq_msg_send(u32 devid, u32 tsid, struct callback_send_sq_para *sq_para);

int devdrv_cbsq_alloc(u32 devid, u32 tsid, struct devdrv_cbsq_alloc_para *cbsq_alloc);
int devdrv_cbsq_recycle(u32 devid, u32 tsid, struct devdrv_cbsq_alloc_para *cbsq_alloc, enum cbsqcq_alloc_flag flag);
struct devdrv_cbsq_data *devdrv_cbsq_data_get(u32 devid);

int devdrv_cbsq_setup(u32 devid);
void devdrv_cbsq_cleanup(u32 devid);

#endif /* __DEVDRV_CBSQ_H */
