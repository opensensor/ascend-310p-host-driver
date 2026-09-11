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

#ifndef TSDRV_CTX_H
#define TSDRV_CTX_H

#include <linux/list.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <linux/atomic.h>

#include "tsdrv_kernel_common.h"
#include "devdrv_common.h"
#include "tsdrv_ioctl.h"
#include "ka_memory_pub.h"
#include "ka_base_pub.h"
#include "ka_list_pub.h"
#include "ka_task_pub.h"
#include "ka_system_pub.h"
#define TSDRV_MAX_CTX_NUM       128U

#define TSDRV_PROC_MAX_THREAD_BIND_IRQ_NUM 2

#define LEAK_CHK_PRINT    0
#define LEAK_CHK_NO_PRINT 1

#define TSDRV_INVALID_PID   (-1)
#define TSDRV_INVALID_TGID  (-1)

enum tsdrv_ts_ctx_status {
    TSDRV_TS_CTX_IDLE,
    TSDRV_TS_CTX_START_RECYCLE,
    TSDRV_TS_CTX_RECYCLE_ERR
};

struct tsdrv_ts_id_ctx {
    u32 id_num;
    ka_list_head_t id_list;
    ka_task_spinlock_t id_lock;
};

struct tsdrv_ts_ctx {
    u32 tsid;
    ka_atomic_t status;
    u32 send_count;
    u64 receive_count;

    int32_t cq_tail_updated;
    ka_wait_queue_head_t report_wait;

    ka_work_struct_t recycle_work;

    volatile int32_t cbcq_wait_flag[DEVDRV_CBCQ_MAX_GID];
    ka_wait_queue_head_t cbcq_wait[DEVDRV_CBCQ_MAX_GID];

    void *shm_sqcq_ctx;
    void *logic_sqcq_ctx;
    void *sync_cb_sqcq_ctx; // mini callback

    struct tsdrv_ts_id_ctx id_ctx[TSDRV_MAX_ID];

    /* only used by vm */
    u32 vcqid;
    u32 wait_queue_inited;
    u32 cq_report_status;
    int32_t cq_tail;
    ka_task_spinlock_t ctx_lock;

    u32 recycle_cbsqcq_num; // min callback
};

#define TSDRV_CTX_RECYCLE_TIMEOUT 30000 /* wait timeout 300s. */
/* ctx status */
enum tsdrv_ctx_status {
    TSDRV_CTX_INVALID = 0,       /* ctx is not allocated by any process */
    TSDRV_CTX_VALID = 1,         /* ctx is allocated by process */
    TSDRV_CTX_RUN = 2,           /* ctx is in running state */
    TSDRV_CTX_WAIT = 3,          /* ctx is in waiting state */
    TSDRV_CTX_RELEASE = 4,       /* ctx is start to release */
    TSDRV_CTX_RECYCLE = 5,       /* ctx is start to recycle resources */
    TSDRV_CTX_RECYCLE_ERR = 6,   /* recycle resouce fail */
    TSDRV_CTX_STOP_RECYCLE = 7,  /* stop recycle resouce */
    TSDRV_CTX_STATUS_MAX = 8
};

/* ctx type */
enum tsdrv_ctx_type {
    TSDRV_CTX_NORMAL_TYPE = 0,       /* normal sqcq flow */
    TSDRV_CTX_NOTHREAD_TYPE = 1     /* no thread sqcq flow */
};

/**
 * struct tsdrv_ctx -
 * @ctx_index:
 * @status:
 * @tgid:
 * @pid:
 * @mirror_ctx_status:
 * @node:
 * @list:
 * @stream_mutex:
 * @vma:
 * @ctx_lock:
 * @ts_ctx:
 */
struct tsdrv_ctx {
    u32 ctx_index;
    ka_atomic_t status;
    ka_atomic_t type;
    ka_pid_t tgid;
    ka_pid_t pid;
    int32_t ssid;
    u32 thread_bind_irq_num;
    u32 mirror_ctx_status;
    struct rb_node node;
    ka_list_head_t list;
    ka_task_spinlock_t ctx_lock;
    ka_mutex_t mutex_lock;
    struct tsdrv_ts_ctx ts_ctx[DEVDRV_MAX_TS_NUM];
    s64 unique_id; /* mark unique processes for vm */
    ka_wait_queue_head_t vm_recycle_work; /* for graceful vm release */
    volatile int32_t vm_recycle_flag;

    long refcnt;
    bool refflag;   /* it can continue to add when true, else can not */
    bool is_remote_open;
};

struct tsdrv_dev_ctx {
    u32 fid;
    u32 ctx_num;
    ka_task_spinlock_t dev_ctx_lock;
    ka_list_head_t head;
    struct rb_root root;
    struct tsdrv_ctx ctx[TSDRV_MAX_CTX_NUM];
};

static inline void tsdrv_set_ctx_run(struct tsdrv_ctx *ctx)
{
    ka_base_atomic_set(&ctx->status, TSDRV_CTX_RUN);
}

static inline bool tsdrv_ctx_is_run(struct tsdrv_ctx *ctx)
{
    enum tsdrv_ctx_status status = (enum tsdrv_ctx_status)ka_base_atomic_read(&ctx->status);

    return status == TSDRV_CTX_RUN;
}

static inline int32_t tsdrv_set_ctx_releasing(struct tsdrv_ctx *ctx)
{
    enum tsdrv_ctx_status status;
    status = (enum tsdrv_ctx_status)ka_base_atomic_cmpxchg(&ctx->status, TSDRV_CTX_RUN, TSDRV_CTX_RELEASE);
    if (status != TSDRV_CTX_RUN) {
        return -ENODEV;
    }
    return 0;
}

void tsdrv_set_ctx_release(struct tsdrv_ctx *ctx);

static inline struct tsdrv_ctx *tsdrv_tsctx_to_ctx(struct tsdrv_ts_ctx *ts_ctx)
{
    struct tsdrv_ctx * ctx = ka_container_of(ts_ctx, struct tsdrv_ctx, ts_ctx[ts_ctx->tsid]);

    return ctx;
}

static inline void tsdrv_set_ctx_normal_type(struct tsdrv_ctx *ctx)
{
    ka_base_atomic_set(&ctx->type, TSDRV_CTX_NORMAL_TYPE);
}

static inline void tsdrv_set_ctx_nothread_type(struct tsdrv_ctx *ctx)
{
    ka_base_atomic_set(&ctx->type, TSDRV_CTX_NOTHREAD_TYPE);
}

static inline bool tsdrv_ctx_is_nothread_type(struct tsdrv_ctx *ctx)
{
    enum tsdrv_ctx_type type = (enum tsdrv_ctx_type)ka_base_atomic_read(&ctx->type);

    return type == TSDRV_CTX_NOTHREAD_TYPE;
}

static inline void tsdrv_set_ctx_remote_open(struct tsdrv_ctx *ctx, bool type)
{
    ctx->is_remote_open = type;
}

static inline bool tsdrv_get_ctx_remote_open(struct tsdrv_ctx *ctx)
{
    return ctx->is_remote_open;
}

void tsdrv_ctx_recycle(struct tsdrv_dev_ctx *dev_ctx, struct tsdrv_ctx *ctx);
void tsdrv_set_mirror_ctx_status(struct tsdrv_ctx *ctx, u32 status);
bool tsdrv_mirror_ctx_abnormal(struct tsdrv_ctx *ctx);
u32 tsdrv_get_mirror_ctx_status(struct tsdrv_ctx *ctx);

struct tsdrv_ctx *tsdrv_ctx_get(struct tsdrv_dev_ctx *dev_ctx, ka_pid_t tgid);
void tsdrv_ctx_put(struct tsdrv_dev_ctx *dev_ctx, struct tsdrv_ctx *ctx);

struct tsdrv_ctx *tsdrv_find_one_ctx(struct tsdrv_dev_ctx *dev_ctx);

void tsdrv_dev_ctx_init_per_fid(struct tsdrv_dev_ctx *dev_ctx, u32 fid);
void tsdrv_dev_ctx_init(struct tsdrv_dev_ctx *dev_ctx, u32 fid_num);
struct tsdrv_ctx *tsdrv_find_ctx(struct tsdrv_dev_ctx *dev_ctx, ka_pid_t tgid);
bool tsdrv_ctx_id_res_chk(struct tsdrv_ctx *ctx, int32_t print_flag);
bool tsdrv_ts_ctx_id_res_chk(struct tsdrv_ts_ctx *ts_ctx, ka_pid_t tgid, ka_pid_t pid, int32_t print_flag);
void tsdrv_set_ctx_recycle_status(struct tsdrv_dev_ctx *dev_ctx, struct tsdrv_ctx *ctx,
    enum tsdrv_ctx_status status);
void tsdrv_set_ctxs_stop_recycle(u32 devid, u32 fid);
void tsdrv_ctx_print_alloc_id(u32 devid, u32 fid, u32 tsid, enum tsdrv_id_type type);

#ifndef AOS_LLVM_BUILD
int tsdrv_remap_va(struct tsdrv_ctx *ctx, u32 tsId, struct tsdrv_mem_map_para *map_para);
int tsdrv_unmap_va(struct tsdrv_ctx *ctx, u32 tsId, struct tsdrv_mem_unmap_para *unmap_para);
int tsdrv_check_va_map(struct vm_area_struct *vma, unsigned long addr, unsigned long size);
int tsdrv_check_va_unmap(struct vm_area_struct *vma, unsigned long addr, unsigned long size);
int tsdrv_check_va_range(struct tsdrv_ctx *ctx, struct vm_area_struct *vma,
    unsigned long addr, unsigned long size);
#else
int tsdrv_get_user_va(phys_addr_t vaddr, unsigned long size, unsigned long *va);
#endif

void tsdrv_zap_vma_ptes(struct vm_area_struct *vma, unsigned long vaddr, size_t size);

#endif /* TSDRV_CTX_H */