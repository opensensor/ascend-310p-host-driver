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

#ifndef TSDRV_DEVICE_H
#define TSDRV_DEVICE_H

#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#ifndef AOS_LLVM_BUILD
#include <linux/numa.h>
#endif

#include "devdrv_user_common.h"
#include "devdrv_common.h"
#include "tsdrv_ctx.h"
#include "tsdrv_hwinfo.h"
#include "tsdrv_status.h"
#include "tsdrv_kernel_common.h"
#include "devdrv_interface.h"
#include "devdrv_functional_cqsq.h"
#include "devdrv_mailbox.h"
#include "tsdrv_log.h"
#include "ka_base_pub.h"
#include "ka_fs_pub.h"
#include "ka_list_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_task_pub.h"
#ifdef __cplusplus
extern "C" {
#endif

#define TSDRV_PM_FID            0
#define TSDRV_MAX_PM_FID_NUM    1U
#define TSDRV_MAX_VM_FID_NUM    16U

#if defined(CFG_SOC_PLATFORM_MINI) && !defined(CFG_SOC_PLATFORM_MINIV2)
#define TSDRV_MAX_FID_NUM       1U /* support only one fid for mini */
#else
#define TSDRV_MAX_FID_NUM       (TSDRV_MAX_PM_FID_NUM  + TSDRV_MAX_VM_FID_NUM)
#endif
#define TSDRV_MAX_DEV_REF       0xffffffff
#define TSDRRV_INVALID_DEV_REF  0xfffffffff

struct tsdrv_wait_tid_info {
    ka_list_head_t node;
    ka_pid_t pid;
    ka_pid_t os_tid;
};

struct tsdrv_stream_info {
    u32 valid;
    u32 sq;
    u32 cq;
    int remote_tgid;
    u32 event_enable;
    u32 event_id;
    u32 grp_id;
    u32 list_num;
    ka_list_head_t head;
    ka_task_spinlock_t lock;
};

struct tsdrv_thread_bind_intr_ctx {
    int valid;
    ka_atomic_t *wait_flag; /* 1 valid, 0 invalid */
    ka_wait_queue_head_t *wait_queue;
};

struct tsdrv_thread_bind_intr_mng {
    ka_mutex_t mutex;
    struct tsdrv_thread_bind_intr_ctx intr_ctx[DEVDRV_CQ_IRQ_NUM];
};
#define ID_SYNC_MSG_IDLE 0
#define ID_SYNC_MSG_LOCAL_TO_REMOTE 1
#define ID_SYNC_MSG_REMOTE_TO_LOCAL 2

struct tsdrv_id_resource {
    u32 id_available_num;
    u32 id_capacity;    // Device specifications, which remain unchanged after initialization
    void *id_addr;
    u32 *pm_id;
    ka_list_head_t id_available_list;
    ka_mutex_t  id_mutex_t;
    ka_task_spinlock_t spinlock;
    ka_atomic_t id_sync_msg_status;
};

#define TSRDV_SHM_SQ_NUM (DEVDRV_MAX_STREAM_ID / DEVDRV_MAX_SQ_DEPTH)

struct tsdrv_reserved_sqcq_info {
    u32 sqid;
    u32 cqid;
    u32 pool_id;
    u32 pri;
};

#define TSDRV_SQCQ_RESEARVED_MAX_NUM 10
struct tsdrv_reserved_sqcq {
    struct devdrv_ts_sq_info sq_info;
    struct devdrv_sq_sub_info sq_sub;
    struct devdrv_ts_cq_info cq_info;
    struct devdrv_cq_sub_info cq_sub;
    u32 pool_id;
    u32 pri;
};

struct tsdrv_ts_resource {
    u32 tsid;

    struct devdrv_mailbox mailbox;
    struct devdrv_int_context int_context[DEVDRV_CQ_IRQ_NUM];
    struct devdrv_functional_cqsq functional_cqsq;

    struct tsdrv_thread_bind_intr_mng thread_bind_intr_mng;

    void *sq_sub_addr;
    void *cq_sub_addr;

    ka_workqueue_struct_t *wq;  /* for recycle work */
    u32 wakelock_num; /* delete it */

    u32 shm_phy_sq[TSRDV_SHM_SQ_NUM];
    size_t shm_offset;
    void *shm_map_addr;

    u32 logic_phy_cq;
    int logic_phy_cq_flag; /* -1 not alloc, 0 alloc, 1 send to ts */

    /* sq cq reserved for ts and esched */
    struct tsdrv_reserved_sqcq reserved_sqcq[TSDRV_SQCQ_RESEARVED_MAX_NUM];
    u32 sqcq_reserved_num;

    /* manage memory & registers shared with user space */
    phys_addr_t unalign_addr;
    struct devdrv_mem_info mem_info[DEVDRV_MAX_MEM];

    ka_atomic_t power_mode; /* for power manager: mode flag */

    ka_atomic_t recycle_work_num;
    struct devdrv_contiguous_mem cm;

    struct tsdrv_id_resource id_res[TSDRV_MAX_ID];

    struct tsdrv_stream_info stream_info[DEVDRV_MAX_STREAM_ID];
};

/* inactive->booting->active->rebooting->inactive  */
enum tsdrv_dev_status {
    TSDRV_DEV_INACTIVE,
    TSDRV_DEV_BOOTING,
    TSDRV_DEV_ACTIVE,
    TSDRV_DEV_REBOOTNOTICE, /* os notice tsdrv reboot */
    TSDRV_DEV_REBOOTING,
    TSDRV_DEV_SUSPEND,
    TSDRV_DEV_DOWN,     /* device heart beat lost */
    TSDRV_DEV_HOTRESET,  /* set for check hotreset */
    TSDRV_DEV_INVALID   /* invalid device status */
};

enum TSDRV_CAPACITY {
    CAPACITY_TYPE_C1,     /* 1 core */
    CAPACITY_TYPE_C2,     /* 2 cores */
    CAPACITY_TYPE_C4,     /* 4 cores */
    CAPACITY_TYPE_C8,     /* 8 cores */
    CAPACITY_TYPE_C16,    /* 16 cores */
    INVALID_CAPACITY,     /* invalid */
    FULL_CAPACITY
};

enum TSDRV_CORE_NUM {
    CAPACITY_CORE1  = 1,
    CAPACITY_CORE2  = 2,
    CAPACITY_CORE4  = 4,
    CAPACITY_CORE6  = 6,
    CAPACITY_CORE8  = 8,
    CAPACITY_CORE12 = 12,
    CAPACITY_CORE16 = 16,
    CAPACITY_CORE24 = 24,
    CAPACITY_CORE32 = 32
};

enum TSDRV_CAPACITY_RATIO {
    CAPACITY_RATIO_1  = 1,   /* 1/1 */
    CAPACITY_RATIO_2  = 2,   /* 1/2 */
    CAPACITY_RATIO_4  = 4,   /* 1/4 */
    CAPACITY_RATIO_8  = 8,   /* 1/8 */
    CAPACITY_RATIO_16 = 16,   /* 1/16 */
};

struct tsdrv_dev_resource {
    u32 fid;

    ka_work_struct_t vm_res_recycle_work;
    ka_workqueue_struct_t *vm_res_wq;  /* for recycle vm_id_res to pm_id_res */

    ka_mutex_t dev_res_lock;
    ka_atomic_t status;
    ka_atomic64_t dev_ref;
    u32 aicore_num;
    u32 cap_num;
    u32 page_size; /* vm page_size, for copy cq to vcq */
    enum TSDRV_CAPACITY capacity;
    unsigned long aic_bitmap;
    struct tsdrv_ts_resource ts_resource[DEVDRV_MAX_TS_NUM];
};

enum tsdrv_env_type {
    TSDRV_ENV_OFFLINE = 0,  /* do not change the value, the value is used for mbox communication */
    TSDRV_ENV_ONLINE = 1
};

struct tsdrv_device {
    u32 devid;
    ka_device_t *dev;
    u32 fid_num;
    u32 tsnum;
    ka_atomic_t host_flag;
    ka_mutex_t dev_lock;
    ka_mutex_t cqsq_mem_lock;
    struct tsdrv_dev_resource dev_res[TSDRV_MAX_FID_NUM];
    struct tsdrv_dev_ctx dev_ctx[TSDRV_MAX_FID_NUM];
    u32 stl_enable_flag[DEVDRV_MAX_TS_NUM];
};

int32_t tsdrv_device_setup(void);
void tsdrv_device_cleanup(void);

int32_t tsdrv_device_init(u32 devid);
void tsdrv_device_exit(u32 devid);

struct tsdrv_device *tsdrv_get_dev(u32 devid);
struct tsdrv_dev_resource *tsdrv_get_dev_resource(u32 devid, u32 fid);
struct tsdrv_ts_resource *tsdrv_get_ts_resoruce(u32 devid, u32 fid, u32 tsid);
int32_t tsdrv_set_dev_capacity(u32 devid, u32 fid, enum TSDRV_CAPACITY capacity);

struct tsdrv_ctx *tsdrv_dev_ctx_get(u32 devid, u32 fid, ka_pid_t tgid);
void tsdrv_dev_ctx_put(struct tsdrv_ctx *ctx);

void tsdrv_ts_res_wq_exit(struct tsdrv_ts_resource *ts_res, u32 ts_num);
int32_t tsdrv_dev_register(u32 devid, u32 fid, enum TSDRV_CAPACITY capacity);
void tsdrv_dev_unregister(u32 devid, u32 fid);
struct tsdrv_ctx *tsdrv_dev_proc_ctx_get(u32 devid, u32 fid, ka_pid_t tgid);
void tsdrv_dev_proc_ctx_put(struct tsdrv_ctx *ctx);
void tsdrv_dev_proc_ctx_ref_disable(struct tsdrv_ctx *ctx);
bool tsdrv_dev_proc_ctx_ref_check(struct tsdrv_ctx *ctx);
void tsdrv_dev_ctx_wakeup_all(u32 devid, u32 tsid);
u32 tsdrv_dev_get_ctx_num(u32 devid, u32 fid);
void tsdrv_dev_set_ctx_recycle(struct tsdrv_ctx *ctx);
void tsdrv_dev_set_ctx_recycle_status(struct tsdrv_ctx *ctx, enum tsdrv_ctx_status status);
void tsdrv_set_dev_status(u32 devid, u32 fid, enum tsdrv_dev_status status, enum tsdrv_host_flag host_flag);

static inline enum tsdrv_env_type tsdrv_get_env_type(void)
{
#if defined(CFG_MANAGER_HOST_ENV)
    return TSDRV_ENV_ONLINE;
#else
    return TSDRV_ENV_OFFLINE;
#endif
}

static inline u32 tsdrv_get_devid_by_ctx(struct tsdrv_ctx *ctx)
{
    struct tsdrv_dev_ctx *dev_ctx = ka_container_of(ctx, struct tsdrv_dev_ctx, ctx[ctx->ctx_index]);
    struct tsdrv_device *tsdrv_dev = ka_container_of(dev_ctx, struct tsdrv_device, dev_ctx[dev_ctx->fid]);

    return tsdrv_dev->devid;
}

static inline u32 tsdrv_get_devid_by_ts_ctx(struct tsdrv_ts_ctx *ts_ctx)
{
    struct tsdrv_ctx *ctx = ka_container_of(ts_ctx, struct tsdrv_ctx, ts_ctx[ts_ctx->tsid]);
    struct tsdrv_dev_ctx *dev_ctx = ka_container_of(ctx, struct tsdrv_dev_ctx, ctx[ctx->ctx_index]);
    struct tsdrv_device *tsdrv_dev = ka_container_of(dev_ctx, struct tsdrv_device, dev_ctx[dev_ctx->fid]);

    return tsdrv_dev->devid;
}

static inline u32 tsdrv_get_fid_by_ctx(struct tsdrv_ctx *ctx)
{
    struct tsdrv_dev_ctx *dev_ctx = ka_container_of(ctx, struct tsdrv_dev_ctx, ctx[ctx->ctx_index]);
    return dev_ctx->fid;
}

static inline bool tsdrv_is_pm_ctx(struct tsdrv_ctx *ctx)
{
    u32 fid = tsdrv_get_fid_by_ctx(ctx);
    return fid == TSDRV_PM_FID;
}

static inline struct tsdrv_device *tsdrv_ctx_to_ts_dev(struct tsdrv_ctx *ctx)
{
    struct tsdrv_dev_ctx *dev_ctx = ka_container_of(ctx, struct tsdrv_dev_ctx, ctx[ctx->ctx_index]);
    struct tsdrv_device *tsdrv_dev = ka_container_of(dev_ctx, struct tsdrv_device, dev_ctx[dev_ctx->fid]);

    return tsdrv_dev;
}

static inline struct tsdrv_ts_resource *tsdrv_ctx_to_ts_res(struct tsdrv_ctx *ctx, u32 tsid)
{
    struct tsdrv_dev_ctx *dev_ctx = ka_container_of(ctx, struct tsdrv_dev_ctx, ctx[ctx->ctx_index]);
    struct tsdrv_device *tsdrv_dev = ka_container_of(dev_ctx, struct tsdrv_device, dev_ctx[dev_ctx->fid]);

    return &tsdrv_dev->dev_res[dev_ctx->fid].ts_resource[tsid];
}

static inline struct tsdrv_device *tsdrv_res_to_dev(struct tsdrv_ts_resource *ts_res)
{
    struct tsdrv_dev_resource *dev_resource = ka_container_of(ts_res, struct tsdrv_dev_resource,
        ts_resource[ts_res->tsid]);
    struct tsdrv_device *ts_dev = ka_container_of(dev_resource, struct tsdrv_device, dev_res[dev_resource->fid]);

    return ts_dev;
}

static inline u32 tsdrv_get_fid_by_res(struct tsdrv_ts_resource *ts_res)
{
    struct tsdrv_dev_resource *dev_resource = ka_container_of(ts_res, struct tsdrv_dev_resource,
        ts_resource[ts_res->tsid]);

    return dev_resource->fid;
}

static inline u32 tsdrv_get_devid_by_res(struct tsdrv_ts_resource *ts_res)
{
    struct tsdrv_dev_resource *dev_resource = ka_container_of(ts_res, struct tsdrv_dev_resource,
        ts_resource[ts_res->tsid]);
    struct tsdrv_device *ts_dev = ka_container_of(dev_resource, struct tsdrv_device, dev_res[dev_resource->fid]);

    return ts_dev->devid;
}

static inline u32 tsdrv_get_devid_by_dev_res(struct tsdrv_dev_resource *dev_res)
{
    struct tsdrv_device *ts_dev = ka_container_of(dev_res, struct tsdrv_device, dev_res[dev_res->fid]);

    return ts_dev->devid;
}

static inline void tsdrv_set_dev_p(u32 devid, struct device *dev)
{
    struct tsdrv_device *tsdrv_dev = tsdrv_get_dev(devid);

    if (tsdrv_dev == NULL) {
        TSDRV_PRINT_WARN("tsdrv_dev is not initiated, devid=%u\n", devid);
        return;
    }
    tsdrv_dev->dev = dev;
}

static inline ka_device_t *tsdrv_get_dev_p(u32 devid)
{
    struct tsdrv_device *tsdrv_dev = tsdrv_get_dev(devid);

    if (tsdrv_dev == NULL) {
        TSDRV_PRINT_WARN("tsdrv_dev is not initiated, devid=%u\n", devid);
        return NULL;
    }
    return tsdrv_dev->dev;
}

static inline int32_t tsdrv_set_dev_tsnum(u32 devid, u32 tsnum)
{
    struct tsdrv_device *tsdrv_dev = tsdrv_get_dev(devid);
    if ((tsdrv_dev == NULL) || (tsnum > DEVDRV_MAX_TS_NUM)) {
        TSDRV_PRINT_ERR("invalid param, devid(%u) tsnum(%u)\n", devid, tsnum);
        return -ENODEV;
    }
    tsdrv_dev->tsnum = tsnum;
    TSDRV_PRINT_DEBUG("set tsnum, devid(%u) tsnum(%u)\n", devid, tsnum);
    return 0;
}

static inline u32 tsdrv_get_dev_tsnum(u32 devid)
{
    struct tsdrv_device *tsdrv_dev = tsdrv_get_dev(devid);

    if (tsdrv_dev == NULL) {
        TSDRV_PRINT_WARN("tsdrv_dev is not initiated, devid=%u\n", devid);
        return 0;
    }
    return tsdrv_dev->tsnum;
}

/* This function called by online to get device index with host phy devid
 * (e.g. , an smp system has 4 chips,host phy devid 4 ~7 get device index 0 ~ 3),
 * or called by offline to get the same value with device phy devid
 */
static inline int tsdrv_get_device_index(u32 devid)
{
#if defined(CFG_MANAGER_HOST_ENV)
    return devdrv_get_device_index(devid);
#else
    return (int)devid;
#endif
}

static inline enum tsdrv_dev_status tsdrv_get_dev_status(u32 devid, u32 fid)
{
    struct tsdrv_device *tsdrv_dev = tsdrv_get_dev(devid);
    struct tsdrv_dev_resource *dev_res = NULL;
    enum tsdrv_dev_status status;

    if (tsdrv_dev == NULL) {
        TSDRV_PRINT_WARN("tsdrv_dev is not initiated, devid=%u\n", devid);
        return TSDRV_DEV_INVALID;
    }
    dev_res = &tsdrv_dev->dev_res[fid];
    status = (enum tsdrv_dev_status)ka_base_atomic_read(&dev_res->status);
    return status;
}

static inline bool tsdrv_is_pm_fid(u32 fid)
{
    if (fid == TSDRV_PM_FID) {
        return true;
    }
    return false;
}

static inline void tsdrv_fid_num_inc(u32 devid)
{
    struct tsdrv_device *tsdrv_dev = tsdrv_get_dev(devid);

    if (tsdrv_dev == NULL) {
        TSDRV_PRINT_WARN("tsdrv_dev is not initiated, devid=%u\n", devid);
        return;
    }

    ka_task_mutex_lock(&tsdrv_dev->dev_lock);
    tsdrv_dev->fid_num++;
    ka_task_mutex_unlock(&tsdrv_dev->dev_lock);
}

static inline void tsdrv_fid_num_dec(u32 devid)
{
    struct tsdrv_device *tsdrv_dev = tsdrv_get_dev(devid);

    if (tsdrv_dev == NULL) {
        TSDRV_PRINT_WARN("tsdrv_dev is not initiated, devid=%u\n", devid);
        return;
    }

    ka_task_mutex_lock(&tsdrv_dev->dev_lock);
    tsdrv_dev->fid_num--;
    ka_task_mutex_unlock(&tsdrv_dev->dev_lock);
}

static inline u32 tsdrv_get_fid_num(u32 devid)
{
    struct tsdrv_device *tsdrv_dev = tsdrv_get_dev(devid);
    u32 fid_num;

    if (tsdrv_dev == NULL) {
        TSDRV_PRINT_WARN("tsdrv_dev is not initiated, devid=%u\n", devid);
        return 0;
    }
    ka_task_mutex_lock(&tsdrv_dev->dev_lock);
    fid_num = tsdrv_dev->fid_num;
    ka_task_mutex_unlock(&tsdrv_dev->dev_lock);
    return fid_num;
}

static inline enum tsdrv_host_flag tsdrv_get_host_flag(u32 devid)
{
    struct tsdrv_device *tsdrv_dev = tsdrv_get_dev(devid);
    enum tsdrv_host_flag host_flag;

    if (tsdrv_dev == NULL) {
        TSDRV_PRINT_WARN("tsdrv_dev is not initiated, devid=%u\n", devid);
        return TSDRV_MAX_VM_TYPE;
    }
    host_flag = (enum tsdrv_host_flag)ka_base_atomic_read(&tsdrv_dev->host_flag);
    return host_flag;
}

static inline bool tsdrv_is_in_pm(u32 devid)
{
    enum tsdrv_host_flag host_flag = tsdrv_get_host_flag(devid);
    if (host_flag == TSDRV_PHYSICAL_TYPE) {
        return true;
    }
    return false;
}

static inline bool tsdrv_is_in_vm(u32 devid)
{
    enum tsdrv_host_flag host_flag = tsdrv_get_host_flag(devid);
    if (host_flag == TSDRV_VIRTUAL_TYPE) {
        return true;
    }
    return false;
}

static inline bool tsdrv_is_in_container(u32 devid)
{
    enum tsdrv_host_flag host_flag = tsdrv_get_host_flag(devid);
    if (host_flag == TSDRV_CONTAINER_TYPE) {
        return true;
    }
    return false;
}

static inline void tsdrv_dev_ref_set(u32 devid, u32 fid, long value)
{
    struct tsdrv_dev_resource *dev_res = tsdrv_get_dev_resource(devid, fid);

    if (dev_res == NULL) {
        return;
    }
    ka_base_atomic64_set(&dev_res->dev_ref, value);
}

static inline long tsdrv_dev_ref_inc_return(u32 devid, u32 fid)
{
    struct tsdrv_dev_resource *dev_res = tsdrv_get_dev_resource(devid, fid);

    if (dev_res == NULL) {
        return (long)TSDRV_MAX_DEV_REF + 1;
    }
    return ka_base_atomic64_inc_return(&dev_res->dev_ref);
}

static inline void tsdrv_dev_ref_dec(u32 devid, u32 fid)
{
    struct tsdrv_dev_resource *dev_res = tsdrv_get_dev_resource(devid, fid);

    if (dev_res == NULL) {
        return;
    }
    ka_base_atomic64_dec(&dev_res->dev_ref);
}


static inline long tsdrv_dev_ref_cmpxchg(u32 devid, u32 fid, long old_val, long new_val)
{
    struct tsdrv_dev_resource *dev_res = tsdrv_get_dev_resource(devid, fid);

    if (dev_res == NULL) {
        return TSDRRV_INVALID_DEV_REF;
    }
    return (long)ka_base_atomic64_cmpxchg(&dev_res->dev_ref, old_val, new_val);
}

void tsdrv_dev_ref_check(u32 devid, u32 fid);

#ifdef __cplusplus
}
#endif

#endif /* TSDRV_DEVICE_H  */
