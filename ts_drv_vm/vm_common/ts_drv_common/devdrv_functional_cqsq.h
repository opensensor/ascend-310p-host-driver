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
#ifndef DEVDRV_FUNCTIONAL_CQSQ_H
#define DEVDRV_FUNCTIONAL_CQSQ_H
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#ifdef AOS_LLVM_BUILD
#include <linux/interrupt.h>
#endif

#include "ka_memory_pub.h"
#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_barrier_pub.h"
#include "devdrv_functional_cqsq_api.h"
#include "devdrv_mailbox.h"
#include "ka_common_pub.h"

#define DEVDRV_MAX_FUNCTIONAL_SQ_DEPTH 1024
#define DEVDRV_MAX_FUNCTIONAL_CQ_DEPTH 1024
#define DEVDRV_MAX_FUNCTIONAL_SQE_SIZE 128
#define DEVDRV_MAX_FUNCTIONAL_CQE_SIZE 128

#define DEVDRV_FUNCTIONAL_PHASE_ZERO 0
#define DEVDRV_FUNCTIONAL_PHASE_ONE 1

#define DEVDRV_FUNCTIONAL_BRIEF_CQ_OFFSET 4
#define DEVDRV_FUNCTIONAL_DETAILED_CQ_OFFSET 8

#define DEVDRV_FUNCTIONAL_DETAILED_CQ_LENGTH 128

#define devdrv_functional_sq_doorbell_index(index) ((index) + DEVDRV_FUNCTIONAL_SQ_FIRST_INDEX)
#define devdrv_functional_sq_array_index(index) ((index)-DEVDRV_FUNCTIONAL_SQ_FIRST_INDEX)

#define devdrv_functional_cq_doorbell_index(index) ((index) + DEVDRV_FUNCTIONAL_CQ_FIRST_INDEX)
#define devdrv_functional_cq_array_index(index) ((index)-DEVDRV_FUNCTIONAL_CQ_FIRST_INDEX)

#define FUNCTIONAL_SQ_DFX_SUPPORT 1
#define SQ_DFX_RECORD_COUNT 10

struct devdrv_functional_sq_dfx_info {
    u32 head_count;
    u32 tail_count;
    u32 head[SQ_DFX_RECORD_COUNT];
    u32 tail[SQ_DFX_RECORD_COUNT];
    ka_timespec64_t head_timestamp[SQ_DFX_RECORD_COUNT];
    ka_timespec64_t tail_timestamp[SQ_DFX_RECORD_COUNT];
};

struct devdrv_functional_sq_info {
    u32 devid;
    u32 index;
    u32 depth;
    u32 slot_len;

    u8 *addr;   /* va: device map device pa; host map bar pa */
    phys_addr_t bar_addr;
    phys_addr_t phy_addr;    /* device pa, ts view */
    dma_addr_t host_dma_addr;
    void *host_dma_buffer; /* online use, alloc 128 bytes as a buffer for dma */
    u32 head;
    u32 tail;
    u32 credit;
    u32 *doorbell;

    enum devdrv_cqsq_func function;
    struct devdrv_functional_sq_dfx_info sq_dfx_info;
};

struct devdrv_functional_cq_info {
    u32 devid;
    u32 index;
    u32 depth;
    u32 slot_len;
    u32 type;
    ka_workqueue_struct_t *wq;
    ka_work_struct_t work;
    struct tsdrv_ts_resource *ts_resource;

    u8 *addr;
    phys_addr_t phy_addr;

    dma_addr_t host_dma_addr;
    void *host_dma_buffer; /* online use, alloc 128 bytes as a buffer for dma */
    ka_device_t *dev;

    u32 head;
    u32 tail;
    u32 phase;
    u32 *doorbell;
    ka_list_head_t int_list_node;
    void (*callback)(u32 device_id, u32 tsid, const u8 *cq_slot, u8 *sq_slot);

    enum devdrv_cqsq_func function;
    ka_mutex_t lock;
    u32 tsid;
    u64 start_timestamp;
};

struct devdrv_functional_int_context {
    int irq_num;
    ka_task_spinlock_t spinlock;
    struct tsdrv_ts_resource *ts_resource;
    ka_list_head_t int_list_header;
    struct tasklet_struct cqsq_tasklet;
};

struct devdrv_functional_cqsq {
    struct devdrv_functional_sq_info *sq_info;
    struct devdrv_functional_cq_info *cq_info;
    int sq_num;
    int cq_num;
    ka_task_spinlock_t spinlock;
    struct devdrv_functional_int_context int_context;
};

struct devdrv_functional_cq_report {
    u8 phase;
    u8 reserved[3];
    u16 sq_index;
    u16 sq_head;
};

#endif
