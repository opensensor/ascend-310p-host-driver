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
#ifndef TSDRV_SHM_SQCQ_H
#define TSDRV_SHM_SQCQ_H

#include <linux/types.h>

#include "securec.h"
#include "devdrv_common.h"
#include "devdrv_cqsq.h"
#include "tsdrv_device.h"
#include "tsdrv_ioctl.h"
#include "ka_task_pub.h"
#include "ka_base_pub.h"

struct shm_sqcq_ts_ctx {
    ka_pid_t pid;
    ka_pid_t tgid;
    u32 sqid;
    u32 cqid;
    u32 shm_sqcq_num;
    ka_mutex_t lock;
    ka_atomic_t rec_ready_flag;
    ka_wait_queue_head_t rec_ready;

    struct vm_area_struct *vma;

    unsigned long sq_map_va;
    unsigned long sq_map_len;
};

struct shm_sqcq_mbox_msg {
    u16 valid;    /* validity judgement, 0x5a5a is valid */
    u16 cmd;      /* command type */
    u32 result;   /* TS's process result succ or fail: no error: 0, error: not 0 */

    u64 sq_addr;    /* invalid addr: 0x0 */
    u64 cq_addr;

    u16 sq_id;  /* invalid index: 0xFFFF */
    u16 cq_id; /* sq's return */

    u8 app_type : 2;  /* inform TS, msg is sent from host or device, device: 0 host: 1 */
    u8 fid : 6;       /* 0:hsot, 1~16:virt machine */
    u8 sq_cq_side;    /* bit 0 sq side, bit 1 cq side. device: 0 host: 1  */

    u8 sqesize;
    u8 cqesize;  /* calculation cq's slot size, default: 12 bytes */
    u16 cqdepth;
    u16 sqdepth;
    ka_pid_t pid;
    u32 cq_irq;
    uint32_t info[SQCQ_RTS_INFO_LENGTH];
};

int shm_sqcq_ctx_init(struct tsdrv_ctx *ctx, u32 tsnum);
void shm_sqcq_ctx_exit(struct tsdrv_ctx *ctx, u32 tsnum);

int shm_sqcq_dev_init(u32 devid, u32 fid, u32 tsnum);
void shm_sqcq_dev_exit(u32 devid, u32 fid, u32 tsnum);
int shm_ioctl_cqsq_alloc(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
int shm_ioctl_cqsq_free(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);

void shm_sqcq_phy_cq_handler(u32 devid, u32 tsid, struct devdrv_ts_cq_info *cq_info);

void shm_sqcq_rec_work(ka_work_struct_t *work);
int shm_sqcq_dev_res_recycle(u32 devid, struct tsdrv_dev_resource *dev_res, struct tsdrv_ctx *ctx);

#endif /* __TSDRV_SHM_SQCQ_H */

