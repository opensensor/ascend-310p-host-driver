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
#include <linux/vmalloc.h>
#include <linux/delay.h>

#include "tsdrv_capacity.h"
#include "tsdrv_device.h"
#include "tsdrv_dev_dfx.h"
#include "devdrv_devinit.h"
#include "tsdrv_common.h"

STATIC struct tsdrv_device *g_tsdrv_device[TSDRV_MAX_DAVINCI_NUM];

struct tsdrv_device *tsdrv_get_dev(u32 devid)
{
    return g_tsdrv_device[devid];
}

struct tsdrv_dev_resource *tsdrv_get_dev_resource(u32 devid, u32 fid)
{
    if (g_tsdrv_device[devid] == NULL) {
        TSDRV_PRINT_DEBUG("tsdrv_dev is not initiated, devid=%u\n", devid);
        return NULL;
    }
    return &g_tsdrv_device[devid]->dev_res[fid];
}

struct tsdrv_ts_resource *tsdrv_get_ts_resoruce(u32 devid, u32 fid, u32 tsid)
{
    if (g_tsdrv_device[devid] == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("The tsdrv_dev is not initiated. (devid=%u)\n", devid);
        return NULL;
#endif
    }
    return &g_tsdrv_device[devid]->dev_res[fid].ts_resource[tsid];
}
KA_EXPORT_SYMBOL_GPL(tsdrv_get_ts_resoruce);

STATIC void tsdrv_dev_resource_exit(struct tsdrv_dev_resource *dev_res, u32 fid_num)
{
    u32 fid;

    for (fid = 0; fid < fid_num; fid++) {
        ka_task_mutex_destroy(&dev_res[fid].dev_res_lock);
    }
}

STATIC int tsdrv_dev_resource_init(struct tsdrv_dev_resource *dev_res, u32 fid_num)
{
    u32 devid = tsdrv_get_devid_by_dev_res(dev_res);
    u32 fid;
    int ret;

    for (fid = 0; fid < fid_num; fid++) {
        ka_task_mutex_init(&dev_res[fid].dev_res_lock);
        dev_res[fid].fid = fid;
        dev_res[fid].capacity = INVALID_CAPACITY;
        tsdrv_set_dev_status(devid, fid, TSDRV_DEV_INACTIVE, TSDRV_PHYSICAL_TYPE);
        ret = tsdrv_set_ts_capacity(dev_res[fid].ts_resource, INVALID_CAPACITY);
        if (ret != 0) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("fid(%u) set capacity fail\n", fid);
            goto set_fail;
#endif
        }
    }
    return 0;
#ifndef TSDRV_UT
set_fail:
    tsdrv_dev_resource_exit(dev_res, fid + 1);
    return -EFAULT;
#endif
}

STATIC_INLINE struct tsdrv_dev_ctx *tsdrv_get_dev_ctx(struct tsdrv_device *tsdrv_dev, u32 fid)
{
    return &tsdrv_dev->dev_ctx[fid];
}

u32 tsdrv_dev_get_ctx_num(u32 devid, u32 fid)
{
    struct tsdrv_device *tsdrv_dev = tsdrv_get_dev(devid);
    struct tsdrv_dev_ctx *dev_ctx = NULL;
    u32 ctx_num;

    if (tsdrv_dev == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_WARN("devid=%u is not initiated\n", devid);
        return TSDRV_MAX_CTX_NUM + 1;
#endif
    }
    dev_ctx = tsdrv_get_dev_ctx(tsdrv_dev, fid);
    ka_task_spin_lock(&dev_ctx->dev_ctx_lock);
    ctx_num = dev_ctx->ctx_num;
    ka_task_spin_unlock(&dev_ctx->dev_ctx_lock);
    return ctx_num;
}

struct tsdrv_ctx *tsdrv_dev_proc_ctx_get(u32 devid, u32 fid, ka_pid_t tgid)
{
    struct tsdrv_device *tsdrv_dev = tsdrv_get_dev(devid);
    struct tsdrv_dev_ctx *dev_ctx = NULL;
    struct tsdrv_ctx *ctx = NULL;

    if (unlikely((tsdrv_dev == NULL) || (fid >= TSDRV_MAX_FID_NUM))) {
        TSDRV_PRINT_ERR("invalid params, fid(%u)\n", fid);
        return NULL;
    }
    dev_ctx = tsdrv_get_dev_ctx(tsdrv_dev, fid);

    ka_task_spin_lock(&dev_ctx->dev_ctx_lock);
    ctx = tsdrv_find_ctx(dev_ctx, tgid);
    if (ctx == NULL) {
        ka_task_spin_unlock(&dev_ctx->dev_ctx_lock);
        return NULL;
    }

    ka_task_spin_lock_bh(&ctx->ctx_lock);
    if (ctx->refflag == false) {
#ifndef TSDRV_UT
        ka_task_spin_unlock_bh(&ctx->ctx_lock);
        ka_task_spin_unlock(&dev_ctx->dev_ctx_lock);
        return NULL;
#endif
    }

    ctx->refcnt++;
    ka_task_spin_unlock_bh(&ctx->ctx_lock);
    ka_task_spin_unlock(&dev_ctx->dev_ctx_lock);

    return ctx;
}

void tsdrv_dev_proc_ctx_put(struct tsdrv_ctx *ctx)
{
    ka_task_spin_lock_bh(&ctx->ctx_lock);
    ctx->refcnt--;
    ka_task_spin_unlock_bh(&ctx->ctx_lock);
}

void tsdrv_dev_proc_ctx_ref_disable(struct tsdrv_ctx *ctx)
{
    ka_task_spin_lock_bh(&ctx->ctx_lock);
    ctx->refflag = false;
    ka_task_spin_unlock_bh(&ctx->ctx_lock);
    TSDRV_PRINT_DEBUG("ctx refflag disable tgid(%u)\n", ctx->tgid);
}

bool tsdrv_dev_proc_ctx_ref_check(struct tsdrv_ctx *ctx)
{
    ka_task_spin_lock_bh(&ctx->ctx_lock);
    if (ctx->refcnt > 0) {
#ifndef TSDRV_UT
        ka_task_spin_unlock_bh(&ctx->ctx_lock);
        return false;
#endif
    }
    ka_task_spin_unlock_bh(&ctx->ctx_lock);
    return true;
}

void tsdrv_dev_ref_check(u32 devid, u32 fid)
{
    u32 recycle_cnt = 0;

    while (tsdrv_dev_ref_cmpxchg(devid, fid, 0, TSDRRV_INVALID_DEV_REF) != 0) {
#ifndef TSDRV_UT
        if (tsdrv_get_dev_status(devid, fid) == TSDRV_DEV_REBOOTNOTICE) {
            TSDRV_PRINT_INFO("Reboot and do not wait. (devid=%u; fid=%u)\n", devid, fid);
            break;
        }
#endif
        if ((recycle_cnt % TSDRV_PRINT_FREQUENCY_CTRL) == 0) {
            TSDRV_PRINT_WARN("Device is in use and cannot be unregistered, stop process first. "
                "(devid=%u; recycle_cnt=%u)\n", devid, recycle_cnt);
        }
        ka_system_msleep(TSDRV_UNREGISTER_MSLEEP);
        recycle_cnt++;
    }
}

struct tsdrv_ctx *tsdrv_dev_ctx_get(u32 devid, u32 fid, ka_pid_t tgid)
{
    struct tsdrv_device *tsdrv_dev = tsdrv_get_dev(devid);
    struct tsdrv_dev_ctx *dev_ctx = NULL;

    if (unlikely((tsdrv_dev == NULL) || (fid >= TSDRV_MAX_FID_NUM))) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid params, devid=%u, fid=%u\n", devid, fid);
        return NULL;
#endif
    }
    dev_ctx = tsdrv_get_dev_ctx(tsdrv_dev, fid);
    return tsdrv_ctx_get(dev_ctx, tgid);
}

void tsdrv_dev_ctx_put(struct tsdrv_ctx *ctx)
{
    struct tsdrv_dev_ctx *dev_ctx = ka_container_of(ctx, struct tsdrv_dev_ctx, ctx[ctx->ctx_index]);
    tsdrv_ctx_put(dev_ctx, ctx);
}

void tsdrv_dev_set_ctx_recycle(struct tsdrv_ctx *ctx)
{
    struct tsdrv_dev_ctx *dev_ctx = NULL;
    u32 ctx_index;

    if (unlikely(ctx == NULL)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("ctx is NULL\n");
        return;
#endif
    }
    if (unlikely(ctx->ctx_index >= TSDRV_MAX_CTX_NUM)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid ctx_index(%u)\n", ctx->ctx_index);
        return;
#endif
    }
    ctx_index = ctx->ctx_index;
    dev_ctx = ka_container_of(ctx, struct tsdrv_dev_ctx, ctx[ctx_index]);
    if (unlikely(dev_ctx->fid >= TSDRV_MAX_FID_NUM)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid fid(%u)\n", dev_ctx->fid);
        return;
#endif
    }
    tsdrv_ctx_recycle(dev_ctx, ctx);
}

void tsdrv_dev_set_ctx_recycle_status(struct tsdrv_ctx *ctx, enum tsdrv_ctx_status status)
{
    struct tsdrv_dev_ctx *dev_ctx = NULL;
    u32 ctx_index;

    if (unlikely(ctx == NULL)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("ctx is NULL\n");
        return;
#endif
    }
    if (unlikely(ctx->ctx_index >= TSDRV_MAX_CTX_NUM)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid ctx_index(%u)\n", ctx->ctx_index);
        return;
#endif
    }
    ctx_index = ctx->ctx_index;
    dev_ctx = ka_container_of(ctx, struct tsdrv_dev_ctx, ctx[ctx_index]);
    if (unlikely(dev_ctx->fid >= TSDRV_MAX_FID_NUM)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid fid(%u)\n", dev_ctx->fid);
        return;
#endif
    }
    tsdrv_set_ctx_recycle_status(dev_ctx, ctx, status);
}

STATIC int tsdrv_ts_res_init(struct tsdrv_ts_resource *ts_res, u32 ts_num)
{
    u32 tsid;
    u32 i, j;

    for (tsid = 0; tsid < ts_num; tsid++) {
#if (defined(CFG_SOC_PLATFORM_MDC_V51) && !defined(AOS_LLVM_BUILD))
        ts_res[tsid].wq = alloc_workqueue("tsdrv_recycle", WQ_UNBOUND, 1);
#else
        ts_res[tsid].wq = ka_task_create_singlethread_workqueue("tsdrv_recycle");
#endif
        if (ts_res[tsid].wq == NULL) {
            TSDRV_PRINT_ERR("recycle workqueue create fail, tsid(%u)\n", tsid);
            goto err_create_workqueue;
        }

        ts_res[tsid].tsid = tsid;
        for (j = TSDRV_STREAM_ID; j < TSDRV_MAX_ID; j++) {
            KA_INIT_LIST_HEAD(&ts_res[tsid].id_res[j].id_available_list);
            ts_res[tsid].id_res[j].id_available_num = 0;
            ka_task_mutex_init(&ts_res[tsid].id_res[j].id_mutex_t);
            ka_task_spin_lock_init(&ts_res[tsid].id_res[j].spinlock);
        }
        ts_res[tsid].functional_cqsq.int_context.ts_resource = &ts_res[tsid];
    }

    return 0;
err_create_workqueue:
    for (i = 0; i < tsid; i++) {
        ka_task_destroy_workqueue(ts_res[i].wq);
        ts_res[tsid].wq = NULL;

        for (j = TSDRV_STREAM_ID; j < TSDRV_MAX_ID; j++) {
            ka_task_mutex_destroy(&ts_res[tsid].id_res[j].id_mutex_t);
        }
    }
    return -EINVAL;
}

void tsdrv_ts_res_wq_exit(struct tsdrv_ts_resource *ts_res, u32 ts_num)
{
    u32 tsid;

    for (tsid = 0; tsid < ts_num; tsid++) {
        if (ts_res[tsid].wq != NULL) {
            ka_task_destroy_workqueue(ts_res[tsid].wq);
            ts_res[tsid].wq = NULL;
        }
    }
}

STATIC void tsdrv_ts_res_mutex_exit(struct tsdrv_ts_resource *ts_res, u32 ts_num)
{
    u32 tsid, j;

    for (tsid = 0; tsid < ts_num; tsid++) {
        for (j = TSDRV_STREAM_ID; j < TSDRV_MAX_ID; j++) {
            ka_task_mutex_destroy(&ts_res[tsid].id_res[j].id_mutex_t);
        }
    }
}

/*
 * This should be called when physcial device or virtual device really detected,
 * e.g. 1. host tsdrv received msg from device side
 *      2. device tsdrv platform probed
 *      3. virtual machine detected
 * This is not for creating char dev
 */
int tsdrv_dev_register(u32 devid, u32 fid, enum TSDRV_CAPACITY capacity)
{
    struct tsdrv_dev_resource *dev_res = NULL;
    int status;
    int err;

    if (capacity > FULL_CAPACITY) {
        TSDRV_PRINT_ERR("invalid capacity, capacity(%d)\n", (int)capacity);
        return -ENOMEM;
    }
    dev_res = tsdrv_get_dev_resource(devid, fid);
    if (dev_res == NULL) {
        TSDRV_PRINT_ERR("get dev resource failed, devid(%u) fid(%u).\n", devid, fid);
        return -EINVAL;
    }

    ka_task_mutex_lock(&dev_res->dev_res_lock);
    status = ka_base_atomic_read(&dev_res->status);
    if (status != TSDRV_DEV_INACTIVE) {
        ka_task_mutex_unlock(&dev_res->dev_res_lock);
        TSDRV_PRINT_ERR("dev status isn't inactive state, devid(%u) fid(%u) status(%d)\n",
            devid, fid, (int)status);
#ifndef TSDRV_UT
        return -EBUSY;
#endif
    }
    err = tsdrv_dev_dfx_dir_create(devid, fid);
    if (err != 0) {
#ifndef TSDRV_UT
        ka_task_mutex_unlock(&dev_res->dev_res_lock);
        return -ENODEV;
#endif
    }
    err = tsdrv_ts_res_init(dev_res->ts_resource, DEVDRV_MAX_TS_NUM);
    if (err != 0) {
#ifndef TSDRV_UT
        tsdrv_dev_dfx_dir_remove(devid, fid);
        ka_task_mutex_unlock(&dev_res->dev_res_lock);
        return -ENODEV;
#endif
    }
    ka_base_atomic_set(&dev_res->status, TSDRV_DEV_BOOTING);

    dev_res->capacity = capacity;
    err = tsdrv_set_ts_capacity(dev_res->ts_resource, capacity);
    if (err != 0) {
#ifndef TSDRV_UT
        tsdrv_ts_res_mutex_exit(dev_res->ts_resource, DEVDRV_MAX_TS_NUM);
        tsdrv_ts_res_wq_exit(dev_res->ts_resource, DEVDRV_MAX_TS_NUM);
        tsdrv_dev_dfx_dir_remove(devid, fid);
        tsdrv_set_dev_status(devid, fid, TSDRV_DEV_INACTIVE, TSDRV_PHYSICAL_TYPE);
        ka_task_mutex_unlock(&dev_res->dev_res_lock);
        TSDRV_PRINT_ERR("devid(%u) fid(%u) host_flag(%d) set capacity fail\n", devid, fid, tsdrv_get_host_flag(devid));
        return -ENODEV;
#endif
    }
    ka_task_mutex_unlock(&dev_res->dev_res_lock);

    tsdrv_fid_num_inc(devid);
    return 0;
}

void tsdrv_dev_unregister(u32 devid, u32 fid)
{
    struct tsdrv_dev_resource *dev_res = NULL;
    enum tsdrv_dev_status status;

    dev_res = tsdrv_get_dev_resource(devid, fid);
    if (dev_res == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("get dev resource failed, devid(%u) fid(%u).\n", devid, fid);
        return;
#endif
    }

    ka_task_mutex_lock(&dev_res->dev_res_lock);
    status = (enum tsdrv_dev_status)ka_base_atomic_read(&dev_res->status);
    if (status == TSDRV_DEV_INACTIVE) {
#ifndef TSDRV_UT
        ka_task_mutex_unlock(&dev_res->dev_res_lock);
        TSDRV_PRINT_ERR("dev status isn't active state, devid(%u) fid(%u) status(%d)\n", devid, fid, status);
        return;
#endif
    }

    tsdrv_dev_dfx_dir_remove(devid, fid);
    tsdrv_ts_res_mutex_exit(dev_res->ts_resource, DEVDRV_MAX_TS_NUM);
    tsdrv_ts_res_wq_exit(dev_res->ts_resource, DEVDRV_MAX_TS_NUM);

    dev_res->capacity = INVALID_CAPACITY;
    (void)tsdrv_set_ts_capacity(dev_res->ts_resource, INVALID_CAPACITY);
    ka_task_mutex_unlock(&dev_res->dev_res_lock);

    tsdrv_fid_num_dec(devid);
    tsdrv_set_dev_status(devid, fid, TSDRV_DEV_INACTIVE, TSDRV_PHYSICAL_TYPE);
    TSDRV_PRINT_INFO("dev(%u), fid(%u), host_flag(%d)\n", devid, fid, tsdrv_get_host_flag(devid));
}

int tsdrv_mirror_ctx_status_set(ka_pid_t pid, u32 devid, u32 status)
{
#ifndef TSDRV_UT
    struct tsdrv_ctx *ctx = NULL;

    if (devid >= TSDRV_MAX_DAVINCI_NUM) {
        TSDRV_PRINT_ERR("invalid devid(%u)\n", devid);
        return -ENODEV;
    }
    if (pid <= 0) {
        TSDRV_PRINT_ERR("wrong pid=%d devid=%u\n", pid, devid);
        return -ESRCH;
    }
    if (tsdrv_get_env_type() == TSDRV_ENV_OFFLINE) {
        TSDRV_PRINT_ERR("offline mode, no mirror ctx exists, devid(%u) pid(%d)\n", devid, pid);
        return -ESRCH;
    }
    ctx = tsdrv_dev_proc_ctx_get(devid, TSDRV_PM_FID, pid);
    if (ctx == NULL) {
        return -ESRCH;
    }
    tsdrv_set_mirror_ctx_status(ctx, status);
    tsdrv_dev_proc_ctx_put(ctx);
    TSDRV_PRINT_INFO("set mirror ctx(pid=%d) status(%u)\n", ctx->pid, status);
#endif
    return 0;
}
KA_EXPORT_SYMBOL_GPL(tsdrv_mirror_ctx_status_set);

int tsdrv_device_setup(void)
{
    int err;

    err = tsdrv_dev_dfx_init();
    if (err != 0) {
        TSDRV_PRINT_ERR("dfx init fail, err=%d\n", err);
        return err;
    }
    return 0;
}

void tsdrv_device_cleanup(void)
{
    tsdrv_dev_dfx_exit();
}

int tsdrv_device_init(u32 devid)
{
    int err;

    g_tsdrv_device[devid] = tsdrv_vzalloc(sizeof(struct tsdrv_device));
    if (g_tsdrv_device[devid] == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("tsdrv alloc fail, devid=%u\n", devid);
        return -ENOMEM;
#endif
    }
    ka_task_mutex_init(&g_tsdrv_device[devid]->dev_lock);
    ka_task_mutex_init(&g_tsdrv_device[devid]->cqsq_mem_lock);
    g_tsdrv_device[devid]->fid_num = 0;
    g_tsdrv_device[devid]->devid = devid;

    err = tsdrv_dev_resource_init(g_tsdrv_device[devid]->dev_res, TSDRV_MAX_FID_NUM);
    if (err != 0) {
#ifndef TSDRV_UT
        goto res_init_fail;
#endif
    }

    tsdrv_dev_ctx_init(g_tsdrv_device[devid]->dev_ctx, TSDRV_MAX_FID_NUM);
    return 0;
#ifndef TSDRV_UT
res_init_fail:
    ka_task_mutex_destroy(&g_tsdrv_device[devid]->cqsq_mem_lock);
    ka_task_mutex_destroy(&g_tsdrv_device[devid]->dev_lock);
    tsdrv_vfree(g_tsdrv_device[devid]);
    g_tsdrv_device[devid] = NULL;
    return err;
#endif
}

void tsdrv_device_exit(u32 devid)
{
    if (g_tsdrv_device[devid] == NULL) {
        TSDRV_PRINT_ERR("tsdrv dev is NULL, devid=%u\n", devid);
        return;
    }
    tsdrv_dev_resource_exit(g_tsdrv_device[devid]->dev_res, TSDRV_MAX_FID_NUM);
    ka_task_mutex_destroy(&g_tsdrv_device[devid]->cqsq_mem_lock);
    ka_task_mutex_destroy(&g_tsdrv_device[devid]->dev_lock);

    tsdrv_vfree(g_tsdrv_device[devid]);
    g_tsdrv_device[devid] = NULL;
}

void tsdrv_set_dev_status(u32 devid, u32 fid, enum tsdrv_dev_status status, enum tsdrv_host_flag host_flag)
{
    struct tsdrv_dev_resource *dev_res = tsdrv_get_dev_resource(devid, fid);
    struct tsdrv_device *tsdrv_dev = tsdrv_get_dev(devid);
    enum tsdrv_dev_status tmp_status;
    u32 tmp_fid;

    if (dev_res == NULL) {
        TSDRV_PRINT_ERR("get dev resource failed, devid(%u) fid(%u).\n", devid, fid);
        return;
    }

    ka_task_mutex_lock(&tsdrv_dev->dev_lock);
    ka_base_atomic_set(&dev_res->status, status);

    if (status == TSDRV_DEV_ACTIVE) {
        ka_base_atomic_set(&tsdrv_dev->host_flag, host_flag);
    } else if (status == TSDRV_DEV_INACTIVE) {
        for (tmp_fid = 0; tmp_fid < TSDRV_MAX_FID_NUM; tmp_fid++) {
            if (tsdrv_is_pm_fid(tmp_fid)) {
                continue;
            }
            tmp_status = tsdrv_get_dev_status(devid, tmp_fid);
            if (tmp_status != TSDRV_DEV_INACTIVE) {
                ka_task_mutex_unlock(&tsdrv_dev->dev_lock);
                return;
            }
        }
        ka_base_atomic_set(&tsdrv_dev->host_flag, TSDRV_PHYSICAL_TYPE);
    }
    ka_task_mutex_unlock(&tsdrv_dev->dev_lock);
}

