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
#ifndef DEVDRV_CBSQCQ_COMMON_H
#define DEVDRV_CBSQCQ_COMMON_H

#include "tsdrv_ioctl.h"
#include "tsdrv_ctx.h"
#include "tsdrv_kernel_common.h"

#define DEVDRV_MAX_CBCQ_IRQ_NUM 1U

#define SYNC_CB_MIN_SQE_SIZE    32U
#define SYNC_CB_MAX_SQE_SIZE    64U

#define SYNC_CB_MIN_SQE_DEPTH   1U
#define SYNC_CB_MAX_SQE_DEPTH   1U

#define SYNC_CB_MIN_CQE_SIZE    32U
#define SYNC_CB_MAX_CQE_SIZE    64U

#define SYNC_CB_MIN_CQE_DEPTH   1U
#define SYNC_CB_MAX_CQE_DEPTH   1U

#define SYNC_CB_MAX_SQID_NUM    1024U
#define SYNC_CB_MAX_CQID_NUM    1024U

#define SYNC_CB_CQ_MAX_GID      128U

#define SYNC_CB_BITMAP_SIZE     16U

enum cbsqcq_alloc_flag {
    CBSQCQ_UNALLOC = 0,
    CBSQCQ_ALLOC,
    CBSQCQ_ABANDON,
    CBSQCQ_CMDMAX
};

enum devdrv_cbsqcq_mem_type {
    ALLOCATE_MEM = 0,
    RESERVED_MEM,
    MEMTYPE_MAX
};

struct devdrv_cbsq_init_para {
    u32 cbsq_num[DEVDRV_MAX_TS_NUM];

    enum devdrv_cbsqcq_mem_type mem_type[DEVDRV_MAX_TS_NUM];
    phys_addr_t paddr[DEVDRV_MAX_TS_NUM];
    size_t p_size[DEVDRV_MAX_TS_NUM];

    u32 depth[DEVDRV_MAX_TS_NUM];
    u32 slot_size[DEVDRV_MAX_TS_NUM];
};

struct devdrv_cbcq_init_para {
    u32 cbcq_num[DEVDRV_MAX_TS_NUM];

    enum devdrv_cbsqcq_mem_type mem_type[DEVDRV_MAX_TS_NUM];
    phys_addr_t paddr[DEVDRV_MAX_TS_NUM];
    size_t p_size[DEVDRV_MAX_TS_NUM];

    u32 slot_size[DEVDRV_MAX_TS_NUM];
    u32 depth[DEVDRV_MAX_TS_NUM];
};

struct callback_ctx {
    volatile int cbcq_wait_flag[DEVDRV_CBCQ_MAX_GID];
    wait_queue_head_t cbcq_wait[DEVDRV_CBCQ_MAX_GID];
    ka_list_head_t cbsq_recycle;
    ka_list_head_t cbcq_recycle;
    ka_mutex_t lock;
};

struct devdrv_cbsq_alloc_para {
    // input
    u32 size;
    u32 depth;

    // output
    phys_addr_t paddr;
    void *vaddr;

    u32 sqid; // input, output
    void *ctx; // process context
};

struct devdrv_cbcq_alloc_para {
    // input
    u32 gid; // indicates thread in the process, 0~127
    u32 size;
    u32 depth;
    void *ctx; // process context

    // output
    phys_addr_t paddr;
    void *vaddr;

    u32 cqid; // input, output
    unsigned long cqMapVaddr;
};

struct devdrv_cbsqcq_init_para {
    struct devdrv_cbsq_init_para sq_init_para;
    struct devdrv_cbcq_init_para cq_init_para;
};

struct devdrv_cbsqcq_alloc_para {
    struct devdrv_cbsq_alloc_para sq_alloc_para;
    struct devdrv_cbcq_alloc_para cq_alloc_para;
};

struct devdrv_cbsqcq_para {
    union {
        struct devdrv_cbsqcq_init_para init_para;
        struct devdrv_cbsqcq_alloc_para alloc_para;
    };
};

void devdrv_cbsqcq_ctx_init(struct tsdrv_ctx *ctx, u32 tsnum);
int devdrv_ioctl_cbsqcq_alloc(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);

int devdrv_ioctl_cbcq_wait(struct tsdrv_ctx *ctx, void __user * arg);

int devdrv_ioctl_cbsqcq_free(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);

int callback_sync_dev_init(u32 devid, u32 fid, u32 tsnum);
void callback_sync_dev_exit(u32 devid, u32 fid, u32 tsnum);

int devdrv_recycle_cbcqsq_id(struct tsdrv_ctx *ctx);

#endif /* __DEVDRV_CBSQCQ_COMMON_H */
