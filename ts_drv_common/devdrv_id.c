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
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/gfp.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>

#include <asm/page.h>
#include <securec.h>

#include "devdrv_manager_comm.h"
#include "devdrv_interface.h"
#include "devdrv_id.h"
#include "devdrv_cqsq.h"
#include "devdrv_devinit.h"
#include "tsdrv_osal_mm.h"
#include "tsdrv_drvops.h"
#include "tsdrv_device.h"
#include "tsdrv_sync.h"
#include "tsdrv_id.h"
#include "tsdrv_capacity.h"
#include "tsdrv_logic_cq.h"
#include "ka_kernel_def_pub.h"
#include "ka_task_pub.h"

#include "hvtsdrv_id.h"
#ifndef CFG_SOC_PLATFORM_MDC_V51
#include "hvtsdrv_cqsq.h"
#endif

#include "logic_cq.h"

#define EVENT_HW_TYPE  0
#define ENENT_SW_TYPE  1
#define GET_REAL_STREAM_ID(x) ((x) & 0x7fff)

STATIC struct tsdrv_id_info *devdrv_get_one_model_id(struct tsdrv_ts_resource *ts_resource);

static void tsdrv_id_release(u32 devid, u32 fid, u32 tsnum, enum tsdrv_id_type id_type)
{
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct tsdrv_id_info *id_info = NULL;
    u32 tsid, num, i;

    for (tsid = 0; tsid < tsnum; tsid++) {
        ts_resource = tsdrv_get_ts_resoruce(devid, fid, tsid);
        TSDRV_PRINT_DEBUG("Uninit res_id. (devid=%u; fid=%u; tsid=%u; id_type=%d; id_available_num=%u)\n",
            devid, fid, tsid, (unsigned int)id_type, ts_resource->id_res[id_type].id_available_num);
        ka_task_spin_lock(&ts_resource->id_res[id_type].spinlock);
        num = ts_resource->id_res[id_type].id_available_num;
        for (i = 0; i < num; i++) {
            id_info = ka_list_first_entry(&ts_resource->id_res[id_type].id_available_list,
                struct tsdrv_id_info, list);
            ka_list_del(&id_info->list);
            ts_resource->id_res[id_type].id_available_num--;
        }
        ka_task_spin_unlock(&ts_resource->id_res[id_type].spinlock);
    }
}

void tsdrv_ids_release(u32 devid, u32 fid, u32 tsnum)
{
    enum tsdrv_id_type id_type;

    for (id_type = TSDRV_STREAM_ID; id_type < TSDRV_MAX_ID; id_type++) {
        tsdrv_id_release(devid, fid, tsnum, id_type);
    }
}

struct tsdrv_id_info *devdrv_find_one_id(struct tsdrv_id_resource *id_res, int phy_id)
{
    struct tsdrv_id_info *id_info = NULL;

    id_info = (struct tsdrv_id_info *)(id_res->id_addr + (long)sizeof(struct tsdrv_id_info) * phy_id);
    if (id_info->phy_id != phy_id) {
        return NULL;
    } else {
        return id_info;
    }
}

int devdrv_model_id_init(u32 devid, u32 fid, u32 tsid, u32 model_id_num)
{
    struct tsdrv_id_info *model_info = NULL;
    struct tsdrv_ts_resource *ts_resource = NULL;
    u32 model_min, model_max;
    unsigned long size;
    int err;
    u32 i;

    ts_resource = tsdrv_get_ts_resoruce(devid, fid, tsid);
    if (ts_resource == NULL) {
        TSDRV_PRINT_ERR("param is invalid, dev_id=%u, tsid=%u\n", devid, tsid);
        return -EINVAL;
    }
    err = tsdrv_get_ts_id_range(devid, tsid, TSDRV_MODEL_ID, &model_min, &model_max);
    if (err != 0) {
        return -ENODEV;
    }
    TSDRV_PRINT_DEBUG("devid(%u) fid(%u) tsid(%u) model_min(%u) model_max(%u)\n", devid, fid, tsid, model_min,
        model_max);

    if (!ka_list_empty_careful(&ts_resource->id_res[TSDRV_MODEL_ID].id_available_list)) {
        TSDRV_PRINT_ERR("[dev_id = %u]:available model list is not empty.\n", devid);
        return -EEXIST;
    }

    size = (long)(unsigned)sizeof(struct tsdrv_id_info) * model_id_num;
    model_info = tsdrv_vmalloc(size, KA_GFP_KERNEL | __KA_GFP_ACCOUNT, KA_PAGE_KERNEL);
    if (model_info == NULL) {
        TSDRV_PRINT_ERR("[dev_id = %u]:model_info vmalloc failed\n", devid);
        return -ENOMEM;
    }

    ts_resource->id_res[TSDRV_MODEL_ID].pm_id = tsdrv_kzalloc(model_id_num * sizeof(u32), KA_GFP_KERNEL);
    if (ts_resource->id_res[TSDRV_MODEL_ID].pm_id == NULL) {
        TSDRV_PRINT_ERR("Failed to kzalloc model id map table. (devid=%u)\n", devid);
        tsdrv_vfree(model_info);
        return -ENOMEM;
    }

    ts_resource->id_res[TSDRV_MODEL_ID].id_available_num = 0;
    for (i = 0; i < model_id_num; i++) {
        model_info[i].phy_id = i;
        model_info[i].id = model_info[i].phy_id;
        model_info[i].devid = devid;
        model_info[i].ctx = NULL;
        ka_base_atomic_set(&model_info[i].ref, 0);
        if ((i < model_max) && (i >= model_min)) {
            ka_list_add_tail(&model_info[i].list, &ts_resource->id_res[TSDRV_MODEL_ID].id_available_list);
            ts_resource->id_res[TSDRV_MODEL_ID].id_available_num++;
        } else {
            KA_INIT_LIST_HEAD(&model_info[i].list);
        }
    }
    ts_resource->id_res[TSDRV_MODEL_ID].id_addr = model_info;

    return 0;
}

/* must call after all id are released into available list */
/* release all id in devdrv_cce_ctrl available list */
void devdrv_model_id_destroy(u32 devid, u32 fid, u32 tsid)
{
    struct tsdrv_ts_resource *ts_resource = NULL;

    ts_resource = tsdrv_get_ts_resoruce(devid, fid, tsid);
    if (ts_resource == NULL) {
        TSDRV_PRINT_ERR("param is invalid, dev_id=%u, fid=%u, tsid=%u\n", devid, fid, tsid);
        return;
    }

    KA_INIT_LIST_HEAD(&ts_resource->id_res[TSDRV_MODEL_ID].id_available_list);
    tsdrv_vfree(ts_resource->id_res[TSDRV_MODEL_ID].id_addr);
    ts_resource->id_res[TSDRV_MODEL_ID].id_addr = NULL;
    if (ts_resource->id_res[TSDRV_MODEL_ID].pm_id != NULL) {
        tsdrv_kfree(ts_resource->id_res[TSDRV_MODEL_ID].pm_id);
    }
    return;
}

int devdrv_alloc_model_id(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct tsdrv_id_info *model_info = NULL;
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    u32 tsid = arg->tsid;
    u32 model_num;
    int ret = 0;

    ts_resource = tsdrv_ctx_to_ts_res(ctx, tsid);

    ka_task_mutex_lock(&ts_resource->id_res[TSDRV_MODEL_ID].id_mutex_t);
    model_num = devdrv_get_available_id_num(ts_resource, TSDRV_MODEL_ID);
    if (model_num == 0) {
        TSDRV_PRINT_DEBUG("no model, try to get model from opposite side, cce_ctrl->model_id_num = %d\n",
            ts_resource->id_res[TSDRV_MODEL_ID].id_available_num);
        ret = tsdrv_msg_alloc_sync_model(devid, tsid);
        if (ret != 0) {
            ka_task_mutex_unlock(&ts_resource->id_res[TSDRV_MODEL_ID].id_mutex_t);
            if (ret == -EBUSY) {
                arg->id_para.res_id = tsdrv_get_model_id_max_num(devid, tsid);
                arg->result = ID_IS_EXHAUSTED;
            }
            TSDRV_PRINT_ERR("try to get model id from opposite side failed\n");
            return tsdrv_get_model_id_max_num(devid, tsid);
        }

        TSDRV_PRINT_DEBUG("got model id, cce_ctrl->model_id_num = %d\n",
            ts_resource->id_res[TSDRV_MODEL_ID].id_available_num);
    }

    ka_task_spin_lock(&ts_resource->id_res[TSDRV_MODEL_ID].spinlock);
    model_info = devdrv_get_one_model_id(ts_resource);
    if (model_info == NULL) {
        ka_task_spin_unlock(&ts_resource->id_res[TSDRV_MODEL_ID].spinlock);
        ka_task_mutex_unlock(&ts_resource->id_res[TSDRV_MODEL_ID].id_mutex_t);
        TSDRV_PRINT_ERR("devdrv_get_one_model_id return NULL.\n");
        return tsdrv_get_model_id_max_num(devid, tsid);
    }
    model_info->ctx = ctx;
    model_info->tgid = ctx->tgid;
    ka_list_add(&model_info->list, &ctx->ts_ctx[tsid].id_ctx[TSDRV_MODEL_ID].id_list);
    ctx->ts_ctx[tsid].id_ctx[TSDRV_MODEL_ID].id_num++;
    ka_task_spin_unlock(&ts_resource->id_res[TSDRV_MODEL_ID].spinlock);
    ka_task_mutex_unlock(&ts_resource->id_res[TSDRV_MODEL_ID].id_mutex_t);
    TSDRV_PRINT_DEBUG("Alloc model id. (devid=%u; phy_id=%u; id=%u)\n", devid, model_info->phy_id, model_info->id);

    return model_info->id;
}

STATIC struct tsdrv_id_info *devdrv_find_one_model_id(struct tsdrv_ts_resource *ts_resource,
    struct tsdrv_ctx *ctx, int phy_id)
{
    struct tsdrv_id_info *model_info = NULL;
    struct tsdrv_ctx *tmp_ctx = NULL;

    model_info = (struct tsdrv_id_info *)(ts_resource->id_res[TSDRV_MODEL_ID].id_addr +
                    ((long)sizeof(struct tsdrv_id_info) * phy_id));
    tmp_ctx = (struct tsdrv_ctx *)model_info->ctx;

    if ((model_info->phy_id != phy_id) || (tmp_ctx == NULL) || (tmp_ctx != ctx)) {
        return NULL;
    } else {
        return model_info;
    }
}

int devdrv_free_model_id(struct tsdrv_ctx *ctx, u32 tsid, int id)
{
    struct tsdrv_id_info *model_info = NULL;
    struct tsdrv_ts_resource *ts_resource = NULL;
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    if ((id < 0) || ((u32)id >= tsdrv_get_model_id_max_num(devid, tsid))) {
        TSDRV_PRINT_ERR("invalid input argument.\n");
        return -EINVAL;
    }

    ts_resource = tsdrv_ctx_to_ts_res(ctx, tsid);
    ka_task_spin_lock(&ts_resource->id_res[TSDRV_MODEL_ID].spinlock);
    model_info = devdrv_find_one_model_id(ts_resource, ctx, id);
    if ((model_info == NULL) || (model_info->ctx == NULL)) {
        ka_task_spin_unlock(&ts_resource->id_res[TSDRV_MODEL_ID].spinlock);
        TSDRV_PRINT_ERR("find model id failed\n");
        return -ENODATA;
    }
    model_info->ctx = NULL;

    ka_list_del(&model_info->list);
    ka_list_add(&model_info->list, &ts_resource->id_res[TSDRV_MODEL_ID].id_available_list);
    ts_resource->id_res[TSDRV_MODEL_ID].id_available_num++;
    ctx->ts_ctx[ts_resource->tsid].id_ctx[TSDRV_MODEL_ID].id_num--;
    ka_task_spin_unlock(&ts_resource->id_res[TSDRV_MODEL_ID].spinlock);
    TSDRV_PRINT_DEBUG("Free model id. (devid=%u; phy_id=%u; id=%u)\n", tsdrv_get_devid_by_ctx(ctx),
        model_info->phy_id, model_info->id);

    return 0;
}

STATIC struct tsdrv_id_info *devdrv_find_one_notify_id(struct tsdrv_ts_resource *ts_resource,
    struct tsdrv_ctx *ctx, int phy_id)
{
    struct tsdrv_id_info *notify_info = NULL;
    struct tsdrv_ctx *tmp_ctx = NULL;

    notify_info = (struct tsdrv_id_info *)(ts_resource->id_res[TSDRV_NOTIFY_ID].id_addr +
        ((long)sizeof(struct tsdrv_id_info) * phy_id));
    tmp_ctx = (struct tsdrv_ctx *)notify_info->ctx;
    if ((notify_info->phy_id != phy_id) || (tmp_ctx == NULL) || (tmp_ctx != ctx)) {
        return NULL;
    } else {
        return notify_info;
    }
}

int devdrv_free_one_notify_id(struct tsdrv_ts_resource *ts_resource, struct tsdrv_ctx *ctx, int phy_id,
    int inform_type)
{
    struct tsdrv_id_info *notify_info = NULL;
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    struct tsdrv_ctx *tmp_ctx = NULL;
    int ret;

    if ((phy_id < 0) || (phy_id >= DEVDRV_MAX_NOTIFY_ID)) {
        TSDRV_PRINT_ERR("invalid input argument, phy_id(%d), devid(%u).\n", phy_id, devid);
        return -EINVAL;
    }
    ka_task_mutex_lock(&ts_resource->id_res[TSDRV_NOTIFY_ID].id_mutex_t);
    notify_info = devdrv_find_one_notify_id(ts_resource, ctx, phy_id);
    if (notify_info == NULL) {
        ka_task_mutex_unlock(&ts_resource->id_res[TSDRV_NOTIFY_ID].id_mutex_t);
        TSDRV_PRINT_ERR("find notify id failed, phy_id(%d), devid(%u).\n", phy_id, devid);
        return -EINVAL;
    }
    /*
     *  if notify ref is bigger than 1 means current notify is still in use
     *  don't need  to send message to ts to reset current notify
     */
    if ((ka_base_atomic_read(&notify_info->ref) == 1) && (inform_type == DEVDRV_NOTIFY_INFORM_TS)) {
        ret = devdrv_notify_ts_msg(devid, ts_resource->tsid, NOTIFY_ID_TYPE, notify_info->phy_id, notify_info);
        if (ret != 0) {
            ka_task_mutex_unlock(&ts_resource->id_res[TSDRV_NOTIFY_ID].id_mutex_t);
            TSDRV_PRINT_ERR("send ts notify alloc msg failed, ret(%d), phy_id(%d), devid(%u)\n",
                ret, notify_info->phy_id, devid);
            return ret;
        }
    }
    ka_task_spin_lock(&notify_info->spinlock);
    tmp_ctx = (struct tsdrv_ctx *)notify_info->ctx;
     /* send message to ts will unlock, we should check again  */
    if ((notify_info->phy_id != phy_id) || (tmp_ctx == NULL) || (tmp_ctx->pid != ctx->pid)) {
#ifndef TSDRV_UT
        ka_task_spin_unlock(&notify_info->spinlock);
        ka_task_mutex_unlock(&ts_resource->id_res[TSDRV_NOTIFY_ID].id_mutex_t);
        TSDRV_PRINT_ERR("check notify info failed, phy_id(%d), notify_info.phy_id(%d), cce_context->pid(%d),"
            "devid(%u)\n", phy_id, notify_info->phy_id, ctx->pid, devid);
        return -EINVAL;
#endif
    }
    notify_info->ctx = NULL;
    ka_task_spin_unlock(&notify_info->spinlock);

    ka_list_del(&notify_info->list);
    ctx->ts_ctx[ts_resource->tsid].id_ctx[TSDRV_NOTIFY_ID].id_num--;
    if (ka_base_atomic_dec_and_test(&notify_info->ref) && (notify_info->valid == 1)) {
        ka_list_add(&notify_info->list, &ts_resource->id_res[TSDRV_NOTIFY_ID].id_available_list);
        notify_info->valid = 0;
        ts_resource->id_res[TSDRV_NOTIFY_ID].id_available_num++;
    }

    ka_task_mutex_unlock(&ts_resource->id_res[TSDRV_NOTIFY_ID].id_mutex_t);
    TSDRV_PRINT_DEBUG("Free notify id. (devid=%u; phy_id=%u; id=%u)\n", devid, notify_info->phy_id, notify_info->id);
    return 0;
}

int devdrv_add_model_msg_chan(u32 devid, u32 tsid, struct tsdrv_msg_resource_id *dev_msg_resource_id)
{
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct tsdrv_id_info *model_info = NULL;
    u32 ret_id_num;
    u16 model_id;
    u32 i;

    if ((tsid >= DEVDRV_MAX_TS_NUM) || (devid >= TSDRV_MAX_DAVINCI_NUM) || (dev_msg_resource_id == NULL)) {
        TSDRV_PRINT_ERR("Invalid input parameter.\n");
        return -EINVAL;
    }
    ts_resource = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, tsid);

    ret_id_num = dev_msg_resource_id->ret_id_num;

    if (ret_id_num > DEVDRV_MANAGER_MSG_ID_NUM_MAX) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("[dev_id = %u]:ret_id_num = %u is invalid.\n", devid, ret_id_num);
        return -ENODEV;
#endif
    }

    model_info = (struct tsdrv_id_info *)ts_resource->id_res[TSDRV_MODEL_ID].id_addr;

    ka_task_spin_lock(&ts_resource->id_res[TSDRV_MODEL_ID].spinlock);
    for (i = 0; i < ret_id_num; i++) {
        model_id = dev_msg_resource_id->id[i];
        if (model_id >= tsdrv_get_model_id_max_num(devid, tsid)) {
#ifndef TSDRV_UT
            ka_task_spin_unlock(&ts_resource->id_res[TSDRV_MODEL_ID].spinlock);
            TSDRV_PRINT_ERR("[dev_id = %u]:model_id = %u is invalid.\n", devid, model_id);
            return -ENODEV;
#endif
        }
        ts_resource->id_res[TSDRV_MODEL_ID].pm_id[model_id] = model_id;
        ka_list_add_tail(&model_info[model_id].list, &ts_resource->id_res[TSDRV_MODEL_ID].id_available_list);
        ts_resource->id_res[TSDRV_MODEL_ID].id_available_num++;

        TSDRV_PRINT_DEBUG("model_id = %u, model_id_num = %u\n", (u32)model_id,
            ts_resource->id_res[TSDRV_MODEL_ID].id_available_num);
    }
    ka_task_spin_unlock(&ts_resource->id_res[TSDRV_MODEL_ID].spinlock);

    return 0;
}

int devdrv_add_notify_msg_chan(u32 devid, u32 tsid,
                               struct tsdrv_msg_resource_id *dev_msg_resource_id)
{
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct tsdrv_id_info *notify_info = NULL;
    u32 ret_id_num;
    u16 notify_id;
    u32 i;

    if ((tsid >= DEVDRV_MAX_TS_NUM) || (devid >= TSDRV_MAX_DAVINCI_NUM) || (dev_msg_resource_id == NULL)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid input parameter.\n");
        return -EINVAL;
#endif
    }

    ts_resource = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, tsid);

    ret_id_num = dev_msg_resource_id->ret_id_num;

    if (ret_id_num > DEVDRV_MANAGER_MSG_ID_NUM_MAX) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("[dev_id = %u]:ret_id_num = %u is invalid.\n", devid, ret_id_num);
        return -ENODEV;
#endif
    }

    notify_info = (struct tsdrv_id_info *)ts_resource->id_res[TSDRV_NOTIFY_ID].id_addr;
    ka_task_spin_lock(&ts_resource->id_res[TSDRV_NOTIFY_ID].spinlock);
    for (i = 0; i < ret_id_num; i++) {
        notify_id = dev_msg_resource_id->id[i];
        if (notify_id >= DEVDRV_MAX_NOTIFY_ID) {
#ifndef TSDRV_UT
            ka_task_spin_unlock(&ts_resource->id_res[TSDRV_NOTIFY_ID].spinlock);
            TSDRV_PRINT_ERR("[dev_id = %u]:notify_id = %u is invalid.\n", devid, notify_id);
            return -ENODEV;
#endif
        }
        ts_resource->id_res[TSDRV_NOTIFY_ID].pm_id[notify_id] = notify_id;
        ka_list_add_tail(&notify_info[notify_id].list, &ts_resource->id_res[TSDRV_NOTIFY_ID].id_available_list);
        ts_resource->id_res[TSDRV_NOTIFY_ID].id_available_num++;

        TSDRV_PRINT_DEBUG("notify_id = %u, notify_id_num = %u.\n", notify_id,
            ts_resource->id_res[TSDRV_NOTIFY_ID].id_available_num);
    }
    ka_task_spin_unlock(&ts_resource->id_res[TSDRV_NOTIFY_ID].spinlock);

    return 0;
}

int devdrv_alloc_model_msg_chan(u32 devid, u32 tsid, struct tsdrv_msg_resource_id *dev_msg_resource_id)
{
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct tsdrv_id_info *model_info = NULL;
    u32 req_id_num, i, j = 0;

    dev_msg_resource_id->ret_id_num = 0;

    ts_resource = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, tsid);
    if (!tsdrv_set_id_sync_non_idle(ts_resource, TSDRV_MODEL_ID, ID_SYNC_MSG_REMOTE_TO_LOCAL)) {
#ifndef TSDRV_UT
        return -EPERM;
#endif
    }

    ka_task_spin_lock(&ts_resource->id_res[TSDRV_MODEL_ID].spinlock);
    if (ts_resource->id_res[TSDRV_MODEL_ID].id_available_num == 0) {
        ka_task_spin_unlock(&ts_resource->id_res[TSDRV_MODEL_ID].spinlock);
        tsdrv_set_id_sync_idle(ts_resource, TSDRV_MODEL_ID);
        TSDRV_PRINT_WARN("No available model id.\n");
        return 0;
    }

    req_id_num = ts_resource->id_res[TSDRV_MODEL_ID].id_available_num < dev_msg_resource_id->req_id_num ?
        ts_resource->id_res[TSDRV_MODEL_ID].id_available_num : dev_msg_resource_id->req_id_num;

    for (i = 0; i < req_id_num; i++) {
        model_info = ka_list_first_entry(&ts_resource->id_res[TSDRV_MODEL_ID].id_available_list,
            struct tsdrv_id_info, list);
        ka_list_del(&model_info->list);
        ts_resource->id_res[TSDRV_MODEL_ID].id_available_num--;
        dev_msg_resource_id->id[j++] = model_info->id;
        dev_msg_resource_id->ret_id_num++;
    }
    ka_task_spin_unlock(&ts_resource->id_res[TSDRV_MODEL_ID].spinlock);
    tsdrv_set_id_sync_idle(ts_resource, TSDRV_MODEL_ID);

    TSDRV_PRINT_DEBUG("Sync model id success. (req_id_num=%u; ret_id_num=%u)\n",
        dev_msg_resource_id->req_id_num, dev_msg_resource_id->ret_id_num);

    return 0;
}

int devdrv_alloc_notify_msg_chan(u32 devid, u32 tsid, struct tsdrv_msg_resource_id *dev_msg_resource_id)
{
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct tsdrv_id_info *notify_info = NULL;
    u32 req_id_num, i, j = 0;

    dev_msg_resource_id->ret_id_num = 0;

    ts_resource = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, tsid);
    if (!tsdrv_set_id_sync_non_idle(ts_resource, TSDRV_NOTIFY_ID, ID_SYNC_MSG_REMOTE_TO_LOCAL)) {
#ifndef TSDRV_UT
        return -EPERM;
#endif
    }

    ka_task_mutex_lock(&ts_resource->id_res[TSDRV_NOTIFY_ID].id_mutex_t);
    if (ts_resource->id_res[TSDRV_NOTIFY_ID].id_available_num == 0) {
        ka_task_mutex_unlock(&ts_resource->id_res[TSDRV_NOTIFY_ID].id_mutex_t);
        tsdrv_set_id_sync_idle(ts_resource, TSDRV_NOTIFY_ID);
        TSDRV_PRINT_DEBUG("No available notify id.\n");
        return 0;
    }

    req_id_num = ts_resource->id_res[TSDRV_NOTIFY_ID].id_available_num < dev_msg_resource_id->req_id_num ?
        ts_resource->id_res[TSDRV_NOTIFY_ID].id_available_num : dev_msg_resource_id->req_id_num;

    for (i = 0; i < req_id_num; i++) {
        notify_info = ka_list_first_entry(&ts_resource->id_res[TSDRV_NOTIFY_ID].id_available_list,
            struct tsdrv_id_info, list);
        ka_list_del(&notify_info->list);
        ts_resource->id_res[TSDRV_NOTIFY_ID].id_available_num--;
        dev_msg_resource_id->id[j++] = notify_info->id;
        dev_msg_resource_id->ret_id_num++;
    }
    ka_task_mutex_unlock(&ts_resource->id_res[TSDRV_NOTIFY_ID].id_mutex_t);
    tsdrv_set_id_sync_idle(ts_resource, TSDRV_NOTIFY_ID);

    TSDRV_PRINT_DEBUG("Sync notify id success. (req_id_num=%u; ret_id_num=%u)\n",
        dev_msg_resource_id->req_id_num, dev_msg_resource_id->ret_id_num);

    return 0;
}

int devdrv_notify_id_init(u32 devid, u32 fid, u32 tsid, u32 notify_id_num)
{
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct tsdrv_id_info *notify_info = NULL;
    u32 notify_min, notify_max;
    unsigned long size;
    int err;
    u32 i;

    ts_resource = tsdrv_get_ts_resoruce(devid, fid, tsid);
    if (ts_resource == NULL) {
        TSDRV_PRINT_ERR("param is invalid, dev_id=%u, fid=%u, tsid=%u, notify_id_num=%u\n",
            devid, fid, tsid, notify_id_num);
        return -EINVAL;
    }
    err = tsdrv_get_ts_id_range(devid, tsid, TSDRV_NOTIFY_ID, &notify_min, &notify_max);
    if (err != 0) {
        return -ENODEV;
    }
    TSDRV_PRINT_DEBUG("devid(%u) fid(%u) tsid(%u) notify_min(%u) notify_max(%u) \n",
        devid, fid, tsid, notify_min, notify_max);

    if (!ka_list_empty_careful(&ts_resource->id_res[TSDRV_NOTIFY_ID].id_available_list)) {
        TSDRV_PRINT_ERR("dev_id=%u, fid=%u, tsid=%u, notify_id_num=%u: available notify list is not empty.\n",
            devid, fid, tsid, notify_id_num);
        return -EEXIST;
    }

    size = (long)(unsigned)sizeof(struct tsdrv_id_info) * notify_id_num;
    notify_info = tsdrv_vzalloc(size);
    if (notify_info == NULL) {
        TSDRV_PRINT_ERR("[dev_id = %u]:model_info vzalloc failed\n", devid);
        return -ENOMEM;
    }
    ts_resource->id_res[TSDRV_NOTIFY_ID].pm_id = tsdrv_kzalloc(notify_id_num * sizeof(u32), KA_GFP_KERNEL);
    if (ts_resource->id_res[TSDRV_NOTIFY_ID].pm_id == NULL) {
        TSDRV_PRINT_ERR("[dev_id = %u]:pm_id kzalloc failed\n", devid);
        tsdrv_vfree(notify_info);
        return -ENOMEM;
    }

    ts_resource->id_res[TSDRV_NOTIFY_ID].id_available_num = 0;
    for (i = 0; i < notify_id_num; i++) {
        notify_info[i].phy_id = i;
        notify_info[i].id = notify_info[i].phy_id;
        notify_info[i].devid = devid;
        notify_info[i].ctx = NULL;
        notify_info[i].valid = 0;
        ka_base_atomic_set(&notify_info[i].ref, 0);
        ka_task_spin_lock_init(&notify_info[i].spinlock);
        if ((i < notify_max) && (i >= notify_min)) {
            ka_list_add_tail(&notify_info[i].list, &ts_resource->id_res[TSDRV_NOTIFY_ID].id_available_list);
            ts_resource->id_res[TSDRV_NOTIFY_ID].id_available_num++;
        } else {
            KA_INIT_LIST_HEAD(&notify_info[i].list);
        }
        ts_resource->id_res[TSDRV_NOTIFY_ID].pm_id[i] = notify_info[i].phy_id;
    }
    ts_resource->id_res[TSDRV_NOTIFY_ID].id_addr = notify_info;

    return 0;
}

void devdrv_notify_id_destroy(u32 devid, u32 fid, u32 tsid)
{
    struct tsdrv_ts_resource *ts_resource = NULL;

    ts_resource = tsdrv_get_ts_resoruce(devid, fid, tsid);
    if (ts_resource == NULL) {
        TSDRV_PRINT_ERR("param is invalid, dev_id=%u, fid=%u, tsid=%u.\n", devid, fid, tsid);
        return;
    }

    KA_INIT_LIST_HEAD(&ts_resource->id_res[TSDRV_NOTIFY_ID].id_available_list);
    if (ts_resource->id_res[TSDRV_NOTIFY_ID].id_addr != NULL) {
        tsdrv_vfree(ts_resource->id_res[TSDRV_NOTIFY_ID].id_addr);
        ts_resource->id_res[TSDRV_NOTIFY_ID].id_addr = NULL;
    }

    if (ts_resource->id_res[TSDRV_NOTIFY_ID].pm_id != NULL) {
        tsdrv_kfree(ts_resource->id_res[TSDRV_NOTIFY_ID].pm_id);
        ts_resource->id_res[TSDRV_NOTIFY_ID].pm_id = NULL;
    }

    return;
}

int devdrv_stream_id_init(u32 devid, u32 fid, u32 tsid, u32 streams)
{
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct tsdrv_id_info *stream_tmp = NULL;
    u32 stream_min, stream_max;
    unsigned long size;
    int err;
    u32 i;

    ts_resource = tsdrv_get_ts_resoruce(devid, fid, tsid);
    err = tsdrv_get_ts_id_range(devid, tsid, TSDRV_STREAM_ID, &stream_min, &stream_max);
    if (err != 0) {
        return -ENODEV;
    }
    ts_resource->id_res[TSDRV_STREAM_ID].id_available_num = 0;

    if (!ka_list_empty_careful(&ts_resource->id_res[TSDRV_STREAM_ID].id_available_list)) {
        TSDRV_PRINT_ERR("available list not empty dev_id=%u, fid=%u, tsid=%u.\n", devid, fid, tsid);
        return -EEXIST;
    }

    size = (long)(unsigned)sizeof(*stream_tmp) * streams;
    stream_tmp = tsdrv_vmalloc(size, KA_GFP_KERNEL | __KA_GFP_ACCOUNT, KA_PAGE_KERNEL);
    if (stream_tmp == NULL) {
        TSDRV_PRINT_ERR("dev_id=%u, fid=%u, tsid=%u, streams=%u.\n", devid, fid, tsid, streams);
        return -ENOMEM;
    }

    ts_resource->id_res[TSDRV_STREAM_ID].pm_id = tsdrv_kzalloc(streams * sizeof(u32), KA_GFP_KERNEL);
    if (ts_resource->id_res[TSDRV_STREAM_ID].pm_id == NULL) {
        TSDRV_PRINT_ERR("Failed to kzalloc stream id map table. (devid=%u)\n", devid);
        tsdrv_vfree(stream_tmp);
        return -ENOMEM;
    }

    for (i = 0; i < streams; i++) {
        ts_resource->stream_info[i].event_enable = 0;
        KA_INIT_LIST_HEAD(&ts_resource->stream_info[i].head);
        ka_task_spin_lock_init(&ts_resource->stream_info[i].lock);

        stream_tmp[i].ctx = NULL;
        stream_tmp[i].phy_id = i;
        stream_tmp[i].id = stream_tmp[i].phy_id;
        stream_tmp[i].devid = devid;
        ka_base_atomic_set(&stream_tmp[i].ref, 0);
        if ((i < stream_max) && (i >= stream_min)) {
            ka_list_add_tail(&stream_tmp[i].list, &ts_resource->id_res[TSDRV_STREAM_ID].id_available_list);
            ts_resource->id_res[TSDRV_STREAM_ID].id_available_num++;
        } else {
            KA_INIT_LIST_HEAD(&stream_tmp[i].list);
        }
    }
    ts_resource->id_res[TSDRV_STREAM_ID].id_addr = stream_tmp;

    return 0;
}

int devdrv_event_id_init(u32 devid, u32 fid, u32 tsid, u32 events)
{
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct tsdrv_id_info *event_tmp = NULL;
    u32 event_hw_min, event_hw_max;
    u32 event_sw_min, event_sw_max;
    unsigned long size;
    int err;
    u32 i;

    ts_resource = tsdrv_get_ts_resoruce(devid, fid, tsid);
    err = tsdrv_get_ts_id_range(devid, tsid, TSDRV_EVENT_HW_ID, &event_hw_min, &event_hw_max);
    if (err != 0) {
        return -ENODEV;
    }
    err = tsdrv_get_ts_id_range(devid, tsid, TSDRV_EVENT_SW_ID, &event_sw_min, &event_sw_max);
    if (err != 0) {
        return -ENODEV;
    }
    TSDRV_PRINT_DEBUG("event_min_hw(%u) event_max_hw(%u) event_min_sw(%u) event_max_sw(%u) \n",
        event_hw_min, event_hw_max, event_sw_min, event_sw_max);

    ts_resource->id_res[TSDRV_EVENT_HW_ID].id_available_num = 0;
    ts_resource->id_res[TSDRV_EVENT_SW_ID].id_available_num = 0;

    if (!ka_list_empty_careful(&ts_resource->id_res[TSDRV_EVENT_HW_ID].id_available_list) ||
        !ka_list_empty_careful(&ts_resource->id_res[TSDRV_EVENT_SW_ID].id_available_list)) {
        TSDRV_PRINT_ERR("available list not empty. dev_id=%u, fid=%u, tsid=%u.\n", devid, fid, tsid);
        return -EEXIST;
    }

    size = (long)(unsigned)sizeof(*event_tmp) * events;
    event_tmp = tsdrv_vzalloc(size);
    if (event_tmp == NULL) {
        TSDRV_PRINT_ERR("dev_id=%u, fid=%u, tsid=%u, events=%u.\n", devid, fid, tsid, events);
        return -ENOMEM;
    }
    ts_resource->id_res[TSDRV_EVENT_SW_ID].pm_id = tsdrv_kzalloc(events * sizeof(u32), KA_GFP_KERNEL);
    if (ts_resource->id_res[TSDRV_EVENT_SW_ID].pm_id == NULL) {
        TSDRV_PRINT_ERR("Failed to kzalloc event id map table. (devid=%u)\n", devid);
        tsdrv_vfree(event_tmp);
        return -ENOMEM;
    }
    for (i = 0; i < events; i++) {
        event_tmp[i].phy_id = i;
        event_tmp[i].id = event_tmp[i].phy_id;
        event_tmp[i].devid = devid;
        event_tmp[i].ctx = NULL;
        event_tmp[i].valid = 0;
        ka_task_spin_lock_init(&event_tmp[i].spinlock);
        ka_base_atomic_set(&event_tmp[i].ref, 0);
        if ((i < event_hw_max) && (i >= event_hw_min)) {
            ka_list_add_tail(&event_tmp[i].list, &ts_resource->id_res[TSDRV_EVENT_HW_ID].id_available_list);
            ts_resource->id_res[TSDRV_EVENT_HW_ID].id_available_num++;
        } else if ((i < event_sw_max) && (i >= event_sw_min)) {
            ka_list_add_tail(&event_tmp[i].list, &ts_resource->id_res[TSDRV_EVENT_SW_ID].id_available_list);
            ts_resource->id_res[TSDRV_EVENT_SW_ID].id_available_num++;
        } else {
            KA_INIT_LIST_HEAD(&event_tmp[i].list);
        }
    }
    TSDRV_PRINT_DEBUG("id init, local event: hw num(%u) sw num(%u) \n",
        ts_resource->id_res[TSDRV_EVENT_HW_ID].id_available_num,
        ts_resource->id_res[TSDRV_EVENT_SW_ID].id_available_num);

    ts_resource->id_res[TSDRV_EVENT_SW_ID].id_addr = (void *)event_tmp;

    return 0;
}

/* must call after all id are released into available list */
/* release all id in devdrv_cce_ctrl available list */
void devdrv_event_id_destroy(u32 devid, u32 fid, u32 tsid)
{
    struct tsdrv_ts_resource *ts_resource = NULL;

    ts_resource = tsdrv_get_ts_resoruce(devid, fid, tsid);
    if (ts_resource == NULL) {
        TSDRV_PRINT_ERR("dev_id=%u, fid=%u, tsid=%u.\n", devid, fid, tsid);
        return;
    }

    KA_INIT_LIST_HEAD(&ts_resource->id_res[TSDRV_EVENT_HW_ID].id_available_list);
    KA_INIT_LIST_HEAD(&ts_resource->id_res[TSDRV_EVENT_SW_ID].id_available_list);
    tsdrv_vfree(ts_resource->id_res[TSDRV_EVENT_SW_ID].id_addr);
    ts_resource->id_res[TSDRV_EVENT_SW_ID].id_addr = NULL;

    if (ts_resource->id_res[TSDRV_EVENT_SW_ID].pm_id != NULL) {
        tsdrv_kfree(ts_resource->id_res[TSDRV_EVENT_SW_ID].pm_id);
        ts_resource->id_res[TSDRV_EVENT_SW_ID].pm_id = NULL;
    }

    return;
}

/* must call after all id are released into available list */
/* release all id in devdrv_cce_ctrl available list */
void devdrv_stream_id_destroy(u32 devid, u32 fid, u32 tsid)
{
    struct tsdrv_ts_resource *ts_resource = NULL;

    ts_resource = tsdrv_get_ts_resoruce(devid, fid, tsid);
    if (ts_resource == NULL) {
        TSDRV_PRINT_ERR("dev_id=%u, fid=%u, tsid=%u.\n", devid, fid, tsid);
        return;
    }

    KA_INIT_LIST_HEAD(&ts_resource->id_res[TSDRV_STREAM_ID].id_available_list);
    tsdrv_vfree(ts_resource->id_res[TSDRV_STREAM_ID].id_addr);
    ts_resource->id_res[TSDRV_STREAM_ID].id_addr = NULL;
    if (ts_resource->id_res[TSDRV_STREAM_ID].pm_id != NULL) {
        tsdrv_kfree(ts_resource->id_res[TSDRV_STREAM_ID].pm_id);
        ts_resource->id_res[TSDRV_STREAM_ID].pm_id = NULL;
    }
    return;
}

struct tsdrv_id_info *devdrv_get_one_stream_id(struct tsdrv_ts_resource *ts_resource)
{
    struct tsdrv_id_info *sub_stream = NULL;

    if (ka_list_empty_careful(&ts_resource->id_res[TSDRV_STREAM_ID].id_available_list) != 0) {
        return NULL;
    }

    sub_stream = ka_list_first_entry(&ts_resource->id_res[TSDRV_STREAM_ID].id_available_list, struct tsdrv_id_info, list);
    ka_list_del(&sub_stream->list);
    ts_resource->id_res[TSDRV_STREAM_ID].id_available_num--;
    return sub_stream;
}

STATIC struct tsdrv_id_info *devdrv_get_one_notify_id(struct tsdrv_ts_resource *ts_resource)
{
    struct tsdrv_id_info *notify_info = NULL;
    ka_list_head_t *pos = NULL, *n = NULL;

    if (!ka_list_empty_careful(&ts_resource->id_res[TSDRV_NOTIFY_ID].id_available_list)) {
        ka_list_for_each_safe(pos, n, &ts_resource->id_res[TSDRV_NOTIFY_ID].id_available_list) {
            notify_info = ka_list_entry(pos, struct tsdrv_id_info, list);
            if (ka_base_atomic_read(&notify_info->ref) == 0) {
                ka_list_del(&notify_info->list);
                notify_info->valid = 1;
                ts_resource->id_res[TSDRV_NOTIFY_ID].id_available_num--;
                return notify_info;
            }
        }
    }
    return NULL;
}

STATIC struct tsdrv_id_info *devdrv_get_one_event_id(struct tsdrv_ts_resource *ts_resource)
{
    struct tsdrv_id_info *id = NULL;

    if (!ka_list_empty_careful(&ts_resource->id_res[TSDRV_EVENT_HW_ID].id_available_list)) {
        id = ka_list_first_entry(&ts_resource->id_res[TSDRV_EVENT_HW_ID].id_available_list,
            struct tsdrv_id_info, list);
        if (ka_base_atomic_read(&id->ref) == 0) {
            ka_list_del(&id->list);
            id->valid = 1;  /* this is for ipc notify. */
            ts_resource->id_res[TSDRV_EVENT_HW_ID].id_available_num--;
            return id;
        }
    }

    if (!ka_list_empty_careful(&ts_resource->id_res[TSDRV_EVENT_SW_ID].id_available_list)) {
        id = ka_list_first_entry(&ts_resource->id_res[TSDRV_EVENT_SW_ID].id_available_list,
            struct tsdrv_id_info, list);
        if (ka_base_atomic_read(&id->ref) == 0) {
            ka_list_del(&id->list);
            id->valid = 1;  /* this is for ipc notify. but willn't into here. */
            ts_resource->id_res[TSDRV_EVENT_SW_ID].id_available_num--;
            return id;
        }
    }

    return NULL;
}

STATIC struct tsdrv_id_info *devdrv_get_one_model_id(struct tsdrv_ts_resource *ts_resource)
{
    struct tsdrv_id_info *id = NULL;

    if (ka_list_empty_careful(&ts_resource->id_res[TSDRV_MODEL_ID].id_available_list) != 0) {
        return NULL;
    }
    id = ka_list_first_entry(&ts_resource->id_res[TSDRV_MODEL_ID].id_available_list, struct tsdrv_id_info, list);
    ka_list_del(&id->list);
    ts_resource->id_res[TSDRV_MODEL_ID].id_available_num--;
    return id;
}

STATIC struct tsdrv_id_info *devdrv_find_one_stream_id(struct tsdrv_ts_resource *ts_resource,
    struct tsdrv_ctx *ctx, int phy_id)
{
    struct tsdrv_id_info *tmp = NULL;

    tmp = (struct tsdrv_id_info *)(ts_resource->id_res[TSDRV_STREAM_ID].id_addr +
        ((long)sizeof(struct tsdrv_id_info) * phy_id));
    if ((tmp->phy_id != phy_id) || (tmp->ctx == NULL) || (tmp->ctx != ctx)) {
        return NULL;
    } else {
        return tmp;
    }
}

STATIC struct tsdrv_id_info *devdrv_find_one_event_id(struct tsdrv_ts_resource *ts_resource,
    struct tsdrv_ctx *ctx, int phy_id)
{
    struct tsdrv_id_info *tmp = NULL;
    struct tsdrv_ctx *tmp_ctx = NULL;

    tmp = (struct tsdrv_id_info *)(ts_resource->id_res[TSDRV_EVENT_SW_ID].id_addr +
        ((long)sizeof(struct tsdrv_id_info) * phy_id));
    tmp_ctx = (struct tsdrv_ctx *)tmp->ctx;

    if ((tmp->phy_id != phy_id) || (tmp_ctx == NULL) || (tmp_ctx != ctx)) {
        TSDRV_PRINT_ERR("return NULL tmp(%d=%d %pK %pK).\n", tmp->phy_id, phy_id, (void *)(uintptr_t)tmp_ctx,
            (void *)(uintptr_t)ctx);
        return NULL;
    } else {
        return tmp;
    }
}

int tsdrv_id_is_belong_to_proc(struct tsdrv_id_inst *id_inst, ka_pid_t tgid,
    u32 id, enum tsdrv_id_type id_type)
{
    struct tsdrv_ts_resource *ts_res = NULL;
    struct tsdrv_id_info *id_info = NULL;
    struct tsdrv_id_info *n = NULL;
    struct tsdrv_ctx *ctx = NULL;

    if ((id_inst == NULL) || (id_inst->devid >= TSDRV_MAX_DAVINCI_NUM)) {
        return -EINVAL;
    }

    if ((id_inst->fid >= TSDRV_MAX_FID_NUM) || (id_inst->tsid >= tsdrv_get_dev_tsnum(id_inst->devid))) {
        return -EINVAL;
    }

    ctx = tsdrv_dev_proc_ctx_get(id_inst->devid, id_inst->fid, tgid);
    if (ctx == NULL) {
        return -ENODEV;
    }

    ts_res = tsdrv_get_ts_resoruce(id_inst->devid, id_inst->fid, id_inst->tsid);
    ka_task_spin_lock(&ts_res->id_res[id_type].spinlock);
    if (ka_list_empty_careful(&ctx->ts_ctx[id_inst->tsid].id_ctx[id_type].id_list) == 0) {
        ka_list_for_each_entry_safe(id_info, n, &ctx->ts_ctx[id_inst->tsid].id_ctx[id_type].id_list, list) {
            if (((u32)id_info->id == id) && (id_info->tgid == tgid)) {
                ka_task_spin_unlock(&ts_res->id_res[id_type].spinlock);
                tsdrv_dev_proc_ctx_put(ctx);
                return 0;
            }
        }
    }

    ka_task_spin_unlock(&ts_res->id_res[id_type].spinlock);
    tsdrv_dev_proc_ctx_put(ctx);

    return -EFAULT;
}
KA_EXPORT_SYMBOL_GPL(tsdrv_id_is_belong_to_proc);

STATIC struct tsdrv_id_info *devdrv_alloc_stream_id(
    struct tsdrv_ctx *ctx, struct tsdrv_ts_resource *ts_resource, u32 tsid)
{
    struct tsdrv_id_info *stream_info = NULL;
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    u32 fid = tsdrv_get_fid_by_ctx(ctx);

    ka_task_spin_lock(&ts_resource->id_res[TSDRV_STREAM_ID].spinlock);
    stream_info = devdrv_get_one_stream_id(ts_resource);
    if (stream_info == NULL) {
        ka_task_spin_unlock(&ts_resource->id_res[TSDRV_STREAM_ID].spinlock);
        goto err;
    }
    stream_info->tgid = ctx->tgid;
    stream_info->ctx = ctx;
    ka_list_add(&stream_info->list, &ctx->ts_ctx[tsid].id_ctx[TSDRV_STREAM_ID].id_list);
    ctx->ts_ctx[tsid].id_ctx[TSDRV_STREAM_ID].id_num++;
    ka_task_spin_unlock(&ts_resource->id_res[TSDRV_STREAM_ID].spinlock);
    TSDRV_PRINT_DEBUG("Alloc stream id success. (devid=%u; fid=%u; phy_id=%u; id=%u)\n",
        devid, fid, stream_info->phy_id, stream_info->id);

    return stream_info;
err:
    tsdrv_ctx_print_alloc_id(devid, fid, tsid, TSDRV_STREAM_ID);
    return NULL;
}

int devdrv_init_sq(struct tsdrv_ts_resource *ts_resource, struct tsdrv_ctx *ctx,
    struct devdrv_sq_sub_info *sq_sub_info, u32 size)
{
    struct tsdrv_device *tsdrv_dev = tsdrv_res_to_dev(ts_resource);
    struct tsdrv_ts_resource *pm_ts_res = NULL;
    struct tsdrv_sq_hwinfo *sq_hwinfo = NULL;

    if (sq_sub_info->phy_addr != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_DEBUG("Check. (devid=%u; tsid=%u; sqid=%u).\n", tsdrv_dev->devid, ts_resource->tsid,
            sq_sub_info->index);
#endif
        return 0;
    }

    pm_ts_res = tsdrv_get_ts_resoruce(tsdrv_dev->devid, TSDRV_PM_FID, ts_resource->tsid);
    sq_hwinfo = tsdrv_get_ts_sq_hwinfo(tsdrv_dev->devid, ts_resource->tsid);
    if (tsdrv_get_env_type() == TSDRV_ENV_ONLINE) {
        /* pa is device memory */
        sq_sub_info->phy_addr = sq_hwinfo->paddr +
            ((unsigned long)sq_sub_info->index * DEVDRV_SQ_SLOT_SIZE * DEVDRV_MAX_SQ_DEPTH);
        sq_sub_info->bar_addr = sq_hwinfo->bar_addr +
            ((unsigned long)sq_sub_info->index * DEVDRV_SQ_SLOT_SIZE * DEVDRV_MAX_SQ_DEPTH);
        sq_sub_info->vaddr = pm_ts_res->mem_info[DEVDRV_SQ_MEM].virt_addr +
            ((unsigned long)sq_sub_info->index * DEVDRV_SQ_SLOT_SIZE * DEVDRV_MAX_SQ_DEPTH);
        sq_sub_info->addr_side = TSDRV_MEM_ON_DEVICE_SIDE;
    } else {
        sq_sub_info->phy_addr = sq_hwinfo->paddr +
            ((unsigned long)sq_sub_info->index * DEVDRV_SQ_SLOT_SIZE * DEVDRV_MAX_SQ_DEPTH);
        sq_sub_info->vaddr = pm_ts_res->mem_info[DEVDRV_SQ_MEM].virt_addr +
            ((unsigned long)sq_sub_info->index * DEVDRV_SQ_SLOT_SIZE * DEVDRV_MAX_SQ_DEPTH);
        sq_sub_info->addr_side = TSDRV_MEM_ON_DEVICE_SIDE;
    }

    TSDRV_PRINT_DEBUG("[devid=%u tsid=%u] init sq id: %d.\n", tsdrv_dev->devid, ts_resource->tsid, sq_sub_info->index);

    return 0;
}

int devdrv_init_cq(struct tsdrv_ctx *ctx, u32 tsid, struct devdrv_cq_sub_info *cq_sub_info, u32 size)
{
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    enum tsdrv_env_type env_type;
    phys_addr_t bar_addr = 0;
    struct device *dev = NULL;
    phys_addr_t virt_addr;
    phys_addr_t phy_addr;
    u32 addr_side = 0;

    if (cq_sub_info->virt_addr != 0) {
        return 0;
    }
    if (size == 0) {
        TSDRV_PRINT_ERR("invalid size(%u)\n", size);
        return -EINVAL;
    }
    dev = tsdrv_get_dev_p(devid);

    env_type = tsdrv_get_env_type();
    if (env_type == TSDRV_ENV_OFFLINE) {
#ifdef CFG_SOC_PLATFORM_MINIV2
        int32_t ret;

        if (size > CQ_RESERVE_MEM_CQ_OFFSET) {
            TSDRV_PRINT_ERR("cq que_size=0x%x larger than cq_offset=0x%lx, devid=%u tsid=%u.\n", size,
                CQ_RESERVE_MEM_CQ_OFFSET, devid, tsid);
            return -EFAULT;
        }
        phy_addr = (phys_addr_t)CQ_RESERVE_MEM_BASE + ((phys_addr_t)devid * CHIP_BASEADDR_PA_OFFSET) +
                   ((phys_addr_t)tsid * (phys_addr_t)CQ_RESERVE_MEM_SIZE) +
                   ((phys_addr_t)cq_sub_info->index * (phys_addr_t)CQ_RESERVE_MEM_CQ_OFFSET);
        virt_addr = (phys_addr_t)(uintptr_t)ka_mm_ioremap_wc(phy_addr, size);
        if (!virt_addr) {
            TSDRV_PRINT_ERR("ioremap fail, no mem for cq\n");
            return -ENOMEM;
        }
        ret = memset_s((void *)(uintptr_t)virt_addr, size, 0, size);
        if (ret != 0) {
            TSDRV_PRINT_ERR("cq uio memset failed.\n");
            ka_mm_iounmap((void *)(uintptr_t)virt_addr);
            return -EINVAL;
        }
#else
        size = KA_MM_PAGE_ALIGN(size);
        virt_addr = (phys_addr_t)(uintptr_t)tsdrv_alloc_pages_exact_nid(
            devdrv_manager_devid_to_nid(devid, (u32)DEVDRV_TS_NODE_DDR_MEM), size,
            __KA_GFP_THISNODE | KA_GFP_HIGHUSER_MOVABLE);
        if (virt_addr == 0) {
            TSDRV_PRINT_ERR("[dev_id=%u,tsid=%u,cqid=%u]:kzalloc fail, no mem for cq\n",
                devid, tsid, cq_sub_info->index);
            return -ENOMEM;
        }
        tsdrv_get_drv_ops()->flush_cache((u64)virt_addr, size);

        phy_addr = ka_mm_virt_to_phys((void *)(uintptr_t)virt_addr);
#endif /* CFG_SOC_PLATFORM_MINIV2 */
        ka_task_spin_lock_irq(&cq_sub_info->spinlock);
        cq_sub_info->virt_addr = virt_addr;
        cq_sub_info->phy_addr = phy_addr;
        cq_sub_info->size = size;
        cq_sub_info->addr_side = TSDRV_MEM_ON_DEVICE_SIDE;
        ka_task_spin_unlock_irq(&cq_sub_info->spinlock);
    } else {
        addr_side = TSDRV_MEM_ON_HOST_SIDE;
        /* kzalloc self-adaption */
        size = KA_MM_PAGE_ALIGN(size);
        virt_addr = (phys_addr_t)(uintptr_t)tsdrv_alloc_pages_exact(size, KA_GFP_KERNEL);
        if (!virt_addr) {
            TSDRV_PRINT_ERR("[dev_id=%u]:kzalloc fail, no mem for cq\n", devid);
            return -ENOMEM;
        }

        phy_addr = ka_mm_dma_map_single(dev, (void *)(uintptr_t)virt_addr, size, KA_DMA_BIDIRECTIONAL);
        if (ka_mm_dma_mapping_error(dev, phy_addr)) {
            TSDRV_PRINT_ERR("[dev_id=%u]:dma map single error !\n", devid);
            tsdrv_free_pages_exact((void *)(uintptr_t)virt_addr, cq_sub_info->size);
            return -ENOMEM;
        }

        ka_task_spin_lock_irq(&cq_sub_info->spinlock);
        cq_sub_info->addr_side = addr_side;
        cq_sub_info->bar_addr = bar_addr;      /* only for MINI V3 */
        cq_sub_info->virt_addr = virt_addr;
        cq_sub_info->phy_addr = phy_addr;
        cq_sub_info->size = size;
        cq_sub_info->dev = dev;
        ka_task_spin_unlock_irq(&cq_sub_info->spinlock);
        TSDRV_PRINT_DEBUG("(devid=%u; cqid=%u; va=%pK; side=%u)\n", devid,
            cq_sub_info->index, (void *)(uintptr_t)cq_sub_info->virt_addr, cq_sub_info->addr_side);
    }

    TSDRV_PRINT_DEBUG("devid %u cq_index %u \n", devid, cq_sub_info->index);

    return 0;
}

int devdrv_exit_cq(struct tsdrv_ctx *ctx, u32 tsid, struct devdrv_cq_sub_info *cq_sub_info)
{
#ifndef AOS_LLVM_BUILD
    struct tsdrv_mem_unmap_para unmap_para;
#endif
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    enum tsdrv_env_type env_type;
    phys_addr_t  sub_cq_virt_addr;
    phys_addr_t  sub_cq_phy_addr;
    struct device *dev = NULL;
    int err;
    if ((tsdrv_is_in_pm(devid) || tsdrv_is_in_container(devid)) && (tsdrv_ctx_is_run(ctx) == true)) {
#ifndef AOS_LLVM_BUILD
        unmap_para.va = cq_sub_info->map_va;
        unmap_para.len = cq_sub_info->size;
        err = tsdrv_unmap_va(ctx, tsid, &unmap_para);
        if (err != 0) {
            TSDRV_PRINT_ERR("Unmap cq fail. (va=0x%pK; len=0x%lx)\n", (void *)(uintptr_t)unmap_para.va, unmap_para.len);
            return -ENODEV;
        }
#else
        err = mg_sp_unshare(cq_sub_info->map_va, cq_sub_info->size, 0);
        if (err != 0) {
            TSDRV_PRINT_ERR("Unshare cq fail. (va=0x%pK; len=0x%lx)\n",
                (void *)(uintptr_t)cq_sub_info->map_va, cq_sub_info->size);
            return -ENODEV;
        }
#endif
    }

    TSDRV_PRINT_DEBUG("devdrv_exit_cq. (cq_id=%u; bar_addr=%pK; phy_addr=%pK;"
        "vaddr=%pK; addr_side=%u; queue_size=%u; slot_size=%lu; depth=%u)\n",
        cq_sub_info->index, (void *)(uintptr_t)cq_sub_info->bar_addr,
        (void *)(uintptr_t)cq_sub_info->phy_addr, (void *)(uintptr_t)cq_sub_info->virt_addr,
        cq_sub_info->addr_side, cq_sub_info->size, cq_sub_info->slot_size, cq_sub_info->depth);

    ka_task_spin_lock_irq(&cq_sub_info->spinlock);
    if (cq_sub_info->virt_addr == 0) {
        ka_task_spin_unlock_irq(&cq_sub_info->spinlock);
        return 0;
    }
    sub_cq_virt_addr = cq_sub_info->virt_addr;
    sub_cq_phy_addr = cq_sub_info->phy_addr;
    cq_sub_info->virt_addr = 0;
    cq_sub_info->phy_addr = 0;
    cq_sub_info->bar_addr = 0;
    ka_task_spin_unlock_irq(&cq_sub_info->spinlock);

    env_type = tsdrv_get_env_type();
    dev = tsdrv_get_dev_p(devid);
    if (env_type == TSDRV_ENV_ONLINE) {
#ifndef CFG_SOC_PLATFORM_MINIV3
        ka_mm_dma_unmap_single(dev, sub_cq_phy_addr, cq_sub_info->size, KA_DMA_BIDIRECTIONAL);
        tsdrv_free_pages_exact((void *)(uintptr_t)sub_cq_virt_addr, cq_sub_info->size);
#endif
    } else {
#ifdef CFG_SOC_PLATFORM_MINIV2
        ka_mm_iounmap((void *)(uintptr_t)sub_cq_virt_addr);
#else
        tsdrv_free_pages_exact((void *)(uintptr_t)sub_cq_virt_addr, cq_sub_info->size);
#endif /* CFG_SOC_PLATFORM_MINIV2 */
    }
    return 0;
}

int devdrv_exit_sq(struct tsdrv_ts_resource *ts_resource, struct tsdrv_ctx *ctx,
    struct devdrv_ts_sq_info *sq_info)
{
    struct tsdrv_device *tsdrv_dev = tsdrv_res_to_dev(ts_resource);
    struct devdrv_sq_sub_info *sq_sub_info = NULL;
    struct tsdrv_mem_unmap_para unmap_para;
    int err;

    TSDRV_PRINT_DEBUG("sqId(%u) \n", sq_info->index);
    sq_sub_info = (struct devdrv_sq_sub_info *)sq_info->sq_sub;
    if (sq_sub_info->phy_addr == 0) {
        return 0;
    }

    if ((tsdrv_is_in_pm(tsdrv_dev->devid) || tsdrv_is_in_container(tsdrv_dev->devid)) &&
        (tsdrv_ctx_is_run(ctx) == true)) {
        tsdrv_sq_head_tail_unmap(ctx, sq_info);
        tsdrv_sq_db_unmap(ctx, sq_info);

        unmap_para.va = sq_sub_info->map_va;
        unmap_para.len = sq_sub_info->size * sq_sub_info->depth;
        err = tsdrv_unmap_va(ctx, ts_resource->tsid, &unmap_para);
        if (err != 0) {
            TSDRV_PRINT_ERR("Unmap sq fail. (va=0x%pK; len=0x%lx)\n",
                (void *)(uintptr_t)unmap_para.va, unmap_para.len);
            return -ENOMEM;
        }
    }

    TSDRV_PRINT_DEBUG("devdrv_exit_sq. (sq_id=%u; bar_addr=%pK; phy_addr=%pK; vaddr=%pK; "
        "addr_side=%u; queue_size=%u; slot_size=%lu; depth=%u)\n",
        sq_sub_info->index, (void *)(uintptr_t)sq_sub_info->bar_addr,
        (void *)(uintptr_t)sq_sub_info->phy_addr, (void *)(uintptr_t)sq_sub_info->vaddr,
        sq_sub_info->addr_side, sq_sub_info->queue_size, sq_sub_info->size, sq_sub_info->depth);

    sq_sub_info->phy_addr = 0;
    sq_sub_info->map_va = 0;
    return 0;
}

STATIC u32 devdrv_get_available_event_num(struct tsdrv_ts_resource *ts_resource, int type)
{
    u32 event_num;

    ka_task_spin_lock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].spinlock);
    if (type == EVENT_HW_TYPE) {
        event_num = ts_resource->id_res[TSDRV_EVENT_HW_ID].id_available_num;
    } else {
        event_num = ts_resource->id_res[TSDRV_EVENT_SW_ID].id_available_num;
    }
    ka_task_spin_unlock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].spinlock);
    return event_num;
}

u32 devdrv_get_available_id_num(struct tsdrv_ts_resource *ts_resource, enum tsdrv_id_type type)
{
    u32 id_num;

    ka_task_spin_lock(&ts_resource->id_res[type].spinlock);
    id_num = ts_resource->id_res[type].id_available_num;
    ka_task_spin_unlock(&ts_resource->id_res[type].spinlock);
    return id_num;
}

int devdrv_alloc_stream_check(struct tsdrv_ts_resource *ts_resource, int *no_stream)
{
    struct tsdrv_device *tsdrv_dev = NULL;
    u32 cur_stream_num;
    u32 stream_num;
    int ret;

    tsdrv_dev = tsdrv_res_to_dev(ts_resource);
    ret = tsdrv_get_ts_id_cur_num(tsdrv_dev->devid, ts_resource->tsid, TSDRV_STREAM_ID, &cur_stream_num);
    if (ret != 0) {
        TSDRV_PRINT_ERR("get stream cur num fail, ret(%d)\n", ret);
        return -ENODEV;
    }

    stream_num = devdrv_get_available_id_num(ts_resource, TSDRV_STREAM_ID);
    if (stream_num == 0) {
        TSDRV_PRINT_DEBUG("No stream, try to get stream from opposite side\n");
        ret = tsdrv_msg_alloc_sync_stream(tsdrv_dev->devid, ts_resource->tsid);
        if (ret != 0) {
            if (cur_stream_num < tsdrv_get_stream_id_max_num(tsdrv_dev->devid, ts_resource->tsid)) {
                TSDRV_PRINT_ERR("try to get stream from opposite side failed\n");
            }
            *no_stream = 1;
            return -EINVAL;
        }
        TSDRV_PRINT_DEBUG("got stream, cce_ctrl->stream_num = %d \n",
            ts_resource->id_res[TSDRV_STREAM_ID].id_available_num);
    }
    return 0;
}

struct tsdrv_id_info *devdrv_alloc_stream(struct tsdrv_ctx *ctx, u32 tsid, int *no_stream)
{
    struct tsdrv_id_info *stream_info = NULL;
    struct tsdrv_ts_resource *ts_resource = NULL;
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    u32 fid = tsdrv_get_fid_by_ctx(ctx);
    int ret;

    DRV_PRINT_START();
    if (no_stream == NULL) {
        TSDRV_PRINT_ERR("tsid:%u:invalid input handler.\n", tsid);
        return NULL;
    }
    *no_stream = 0;
    ts_resource = tsdrv_ctx_to_ts_res(ctx, tsid);
    ret = devdrv_alloc_stream_check(ts_resource, no_stream);
    if (ret != 0) {
        TSDRV_PRINT_ERR("alloc stream check failed, ret(%d).\n", ret);
        tsdrv_ctx_print_alloc_id(devid, fid, tsid, TSDRV_STREAM_ID);
        return NULL;
    }

    stream_info = devdrv_alloc_stream_id(ctx, ts_resource, tsid);
    if ((stream_info == NULL) || (stream_info->id < 0) ||
        ((u32)stream_info->id >= tsdrv_get_stream_id_max_num(devid, tsid))) {
        TSDRV_PRINT_ERR("got wrong stream id!\n");
        *no_stream = 1;
        return NULL;
    }

    DRV_PRINT_END();
    return stream_info;
}

int devdrv_free_stream(struct tsdrv_ctx *ctx, u32 tsid, int id, int flag)
{
    struct tsdrv_id_info *stream_info = NULL;
    struct tsdrv_ts_resource *ts_resource = NULL;
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
#ifdef CFG_FEATURE_STARS
    u32 fid = tsdrv_get_fid_by_ctx(ctx);
#endif

    DRV_PRINT_START();
    if ((id < 0) || ((u32)id >= tsdrv_get_stream_id_max_num(devid, tsid))) {
        TSDRV_PRINT_ERR("invalid free stream id %d\n", id);
        return -EINVAL;
    }
    ts_resource = tsdrv_ctx_to_ts_res(ctx, tsid);

    ka_task_spin_lock(&ts_resource->id_res[TSDRV_STREAM_ID].spinlock);
    stream_info = devdrv_find_one_stream_id(ts_resource, ctx, id);
    if (stream_info == NULL) {
        ka_task_spin_unlock(&ts_resource->id_res[TSDRV_STREAM_ID].spinlock);
        TSDRV_PRINT_ERR("devdrv_hash_find_one_stream_id return NULL.\n");
        return -ENODATA;
    }
#if defined(CFG_SOC_PLATFORM_CLOUD_V2) || defined(CFG_SOC_PLATFORM_MINIV3)
    if (ts_resource->stream_info[id].valid == 1) {
        if (flag == DEVDRV_FREE_BY_USER) {
            ka_task_spin_unlock(&ts_resource->id_res[TSDRV_STREAM_ID].spinlock);
            TSDRV_PRINT_ERR("Stream_id is binding sq, assure first free normal cqsq.\n");
            return -EPERM;
        } else {
            ts_resource->stream_info[id].valid = 0;
        }
    }
#endif

    logic_cq_send_event(ts_resource, (u32)id);
    ka_task_spin_lock_bh(&ts_resource->stream_info[id].lock);
    ts_resource->stream_info[id].event_enable = 0;
    ka_task_spin_unlock_bh(&ts_resource->stream_info[id].lock);

    ka_list_del(&stream_info->list);
    ka_list_add(&stream_info->list, &ts_resource->id_res[TSDRV_STREAM_ID].id_available_list);
    ts_resource->id_res[TSDRV_STREAM_ID].id_available_num++;
    ctx->ts_ctx[tsid].id_ctx[TSDRV_STREAM_ID].id_num--;
    stream_info->ctx = NULL;

    ka_task_spin_unlock(&ts_resource->id_res[TSDRV_STREAM_ID].spinlock);
    TSDRV_PRINT_DEBUG("Free stream id success. (devid=%u; fid=%u; phy_id=%d; id=%d)\n",
        tsdrv_get_devid_by_ctx(ctx), tsdrv_get_fid_by_ctx(ctx), stream_info->phy_id, stream_info->id);

    DRV_PRINT_END();
    return 0;
}

/**
 * para notifyid is for ipc_event scene
 **/
int devdrv_notify_ts_msg(u32 devid, u32 tsid, u32 notify_type, int notifyId, struct tsdrv_id_info *notify_info)
{
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct devdrv_ts_notify_msg msg;
    int result = -1;
    int ret;
    u32 chip_type;

    if ((devid >= TSDRV_MAX_DAVINCI_NUM) || (tsid >= DEVDRV_MAX_TS_NUM)) {
        TSDRV_PRINT_ERR("param is invalid\n");
        return -ENOMEM;
    }

#ifndef AOS_LLVM_BUILD
    chip_type = devdrv_get_dev_chip_type(devid);
#else
    chip_type = HISI_MINI_V2;
#endif
    if (chip_type == HISI_CHIP_UNKNOWN) {
        TSDRV_PRINT_ERR("devid %u devdrv_get_dev_chip_type failed, unknown \n", devid);
        return HISI_CHIP_UNKNOWN;
    }

    ts_resource = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, tsid);

    msg.header.valid = DEVDRV_MAILBOX_MESSAGE_VALID;
    msg.header.cmd_type = DEVDRV_MAILBOX_RESET_NOTIFY_ID;
    msg.header.result = 0;
    msg.notifyId = (u16)notifyId;
    msg.virt_notifyId = notify_info->id;
    msg.plat_type = (u16)tsdrv_get_env_type();
    msg.tgid = notify_info->tgid;
    msg.fid = notify_info->fid;
    msg.notify_type = notify_type;

    TSDRV_PRINT_DEBUG("[dev_id=%u tsid=%u fid=%u]devdrv_notify_ts_msg:notifyId(%u) virtId(%u) notify_type(%u) \n",
        devid, tsid, (u32)msg.fid, (u32)msg.notifyId, (u32)msg.virt_notifyId, notify_type);
    ret = devdrv_mailbox_kernel_sync_no_feedback(&ts_resource->mailbox, (u8 *)&msg, sizeof(msg), &result);
    if ((ret != 0) || (result != 0)) {
        TSDRV_PRINT_ERR("[dev_id=%u tsid=%u fid=%u]:inform ts reset notify failed, ret = %d result = %d\n",
            devid, tsid, (u32)msg.fid, ret, result);
        return -EFAULT;
    };
    return 0;
}

int devdrv_alloc_notify_id(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    struct tsdrv_id_info *notify_info = NULL;
    struct tsdrv_ts_resource *ts_resource = NULL;
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    struct tsdrv_res_query info;
    u32 notify_num;
    int ret = 0;

    ts_resource = tsdrv_ctx_to_ts_res(ctx, arg->tsid);

    ka_task_mutex_lock(&ts_resource->id_res[TSDRV_NOTIFY_ID].id_mutex_t);
    notify_num = devdrv_get_available_id_num(ts_resource, TSDRV_NOTIFY_ID);
    if (notify_num == 0) {
        if (!tsdrv_is_in_pm(devid)) {
            ka_task_mutex_unlock(&ts_resource->id_res[TSDRV_NOTIFY_ID].id_mutex_t);
            arg->id_para.res_id = ts_resource->id_res[TSDRV_NOTIFY_ID].id_capacity;
            arg->result = ID_IS_EXHAUSTED;
            TSDRV_PRINT_WARN("no notify_id available.\n");
            return -EINVAL;
        }
        ret = tsdrv_msg_alloc_sync_notify(devid, arg->tsid);
        if (ret != 0) {
            ka_task_mutex_unlock(&ts_resource->id_res[TSDRV_NOTIFY_ID].id_mutex_t);
            if (ret == -EBUSY) {
                arg->id_para.res_id = DEVDRV_MAX_NOTIFY_ID;
                arg->result = ID_IS_EXHAUSTED;
            }
            TSDRV_PRINT_ERR("try to get notify from opposite side failed\n");
            return -EINVAL;
        }
    }

    ka_task_spin_lock(&ts_resource->id_res[TSDRV_NOTIFY_ID].spinlock);
    notify_info = devdrv_get_one_notify_id(ts_resource);
    if (notify_info == NULL) {
        ka_task_spin_unlock(&ts_resource->id_res[TSDRV_NOTIFY_ID].spinlock);
        ka_task_mutex_unlock(&ts_resource->id_res[TSDRV_NOTIFY_ID].id_mutex_t);
        TSDRV_PRINT_ERR("devdrv_get_one_notify_id return NULL.\n");
        return -EINVAL;
    }
    ka_task_spin_lock(&notify_info->spinlock);
    notify_info->ctx = ctx;
    notify_info->tgid = ctx->tgid;
    notify_info->fid = tsdrv_get_fid_by_ctx(ctx);
    ka_task_spin_unlock(&notify_info->spinlock);

    ka_list_add(&notify_info->list, &ctx->ts_ctx[arg->tsid].id_ctx[TSDRV_NOTIFY_ID].id_list);
    ctx->ts_ctx[arg->tsid].id_ctx[TSDRV_NOTIFY_ID].id_num++;
    ka_base_atomic_inc(&notify_info->ref);
    ka_task_spin_unlock(&ts_resource->id_res[TSDRV_NOTIFY_ID].spinlock);
    ka_task_mutex_unlock(&ts_resource->id_res[TSDRV_NOTIFY_ID].id_mutex_t);

    if (tsdrv_get_drv_ops()->resid_query != NULL) {
#ifndef TSDRV_UT
        info.cmd = TSDRV_RES_QUERY_ADDR;
        info.id_type = TSDRV_NOTIFY_ID;
        info.id = (u32)notify_info->id;

        ret = tsdrv_get_drv_ops()->resid_query(devid, arg->tsid, &info);
        if (ret != 0) {
            TSDRV_PRINT_ERR("get table addr failed, (notify_id=%d)\n", notify_info->phy_id);
            goto err;
        }

        arg->id_para.value = info.value;
        TSDRV_PRINT_DEBUG("get table value ok, (id=%d)\n", notify_info->phy_id);
#endif
    }

    ret = devdrv_notify_ts_msg(devid, arg->tsid, NOTIFY_ID_TYPE, notify_info->phy_id, notify_info);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("send ts notify alloc msg failed, notify_id = %d\n", notify_info->phy_id);
        goto err;
#endif
    }
    arg->id_para.res_id = notify_info->id;
    TSDRV_PRINT_DEBUG("Alloc notify id. (devid=%u; phy_id=%u; id=%u)\n", devid, notify_info->phy_id, notify_info->id);

    return 0;
err:
    if (devdrv_free_one_notify_id(ts_resource, ctx, notify_info->phy_id, DEVDRV_NOTIFY_NOT_INFORM_TS) != 0) {
        TSDRV_PRINT_ERR("free one notify id failed, id = %d\n", notify_info->phy_id);
        return -EINVAL;
    }
    return -EFAULT;
}

int devdrv_free_notify_id(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    struct tsdrv_ts_resource *ts_resource = NULL;
    u32 phy_id, chip_type, id_capacity;
    int ret;

    ts_resource = tsdrv_ctx_to_ts_res(ctx, arg->tsid);
#ifndef AOS_LLVM_BUILD
    chip_type = devdrv_get_dev_chip_type(tsdrv_get_devid_by_ctx(ctx));
#else
    chip_type = HISI_MINI_V2;
#endif
    if (chip_type == HISI_CLOUD_V2) {
        id_capacity = DEVDRV_MAX_NOTIFY_ID;
    } else {
        id_capacity = ts_resource->id_res[TSDRV_NOTIFY_ID].id_capacity;
    }
    if (arg->id_para.res_id >= id_capacity) {
        TSDRV_PRINT_ERR("para err, notify_id(%u)\n", arg->id_para.res_id);
        return -EINVAL;
    }

    phy_id = tsdrv_vrit_to_physic_id(&ts_resource->id_res[TSDRV_NOTIFY_ID], arg->id_para.res_id);
    ret = devdrv_free_one_notify_id(ts_resource, ctx, phy_id, DEVDRV_NOTIFY_INFORM_TS);
    if (ret != 0) {
        TSDRV_PRINT_ERR("free one notify id fail reg(%d)\n", ret);
        return ret;
    }

    return 0;
}

struct tsdrv_id_info *devdrv_alloc_event_id(struct tsdrv_ctx *ctx, u32 tsid)
{
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct tsdrv_ts_resource *pm_ts_res = NULL;
    struct tsdrv_id_info *event_info = NULL;
    struct resource_mapping_notice_mailbox_t mailbox_msg = {{0}};
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    u32 available_event_num;
    u32 cur_event_hw_num;
    u32 cur_event_sw_num;
    int ret;

    pm_ts_res = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, tsid);
    ts_resource = tsdrv_ctx_to_ts_res(ctx, tsid);

    if (tsdrv_get_pfvf_type_by_devid(devid) == DEVDRV_SRIOV_TYPE_PF) {
        available_event_num = devdrv_get_available_event_num(ts_resource, EVENT_HW_TYPE);
        if (available_event_num != 0) {
            TSDRV_PRINT_DEBUG("have hwevent, get hwevent from local side, local hwevent(%u)\n", available_event_num);
            goto get_event_id_from_local;
        }
        ret = tsdrv_get_ts_id_cur_num(devid, tsid, TSDRV_EVENT_HW_ID, &cur_event_hw_num);
        if (ret != 0) {
            TSDRV_PRINT_ERR("get cur hw event failed, ret(%d).\n", ret);
            return ERR_PTR(-ENODEV);
        }
        if (cur_event_hw_num != (DEVDRV_MAX_HW_EVENT_ID - DEVDRV_MIN_HW_EVENT_ID)) {
            TSDRV_PRINT_DEBUG("get hwevent from other side, ts_res hwevent(%u)\n", cur_event_hw_num);
            goto get_event_id_from_peer;
        }
    }

    available_event_num = devdrv_get_available_event_num(ts_resource, ENENT_SW_TYPE);
    if (available_event_num != 0) {
        TSDRV_PRINT_DEBUG("have swevent, get swevent from local side, local swevent(%u)\n", available_event_num);
        goto get_event_id_from_local;
    }
    ret = tsdrv_get_ts_id_cur_num(devid, tsid, TSDRV_EVENT_SW_ID, &cur_event_sw_num);
    if (ret != 0) {
        TSDRV_PRINT_ERR("get cur sw event failed, ret(%d).\n", ret);
        return ERR_PTR(-ENODEV);
    }
    if (cur_event_sw_num != (DEVDRV_MAX_SW_EVENT_ID - DEVDRV_MAX_HW_EVENT_ID - 1)) {
        TSDRV_PRINT_DEBUG("get swevent from other side, ts_res swevent(%u)\n", cur_event_sw_num);
        goto get_event_id_from_peer;
    }
    TSDRV_PRINT_WARN("event resources are used up. hw_local(%u) sw_local(%u).\n",
        cur_event_hw_num, cur_event_sw_num);
    /* return -EBUSY when resource is exhausted */
    return ERR_PTR(-EBUSY);

get_event_id_from_peer:
    ret = tsdrv_msg_alloc_sync_event(devid, tsid);
    if (ret != 0) {
        TSDRV_PRINT_WARN("devdrv get event sync fail (ret:%d).\n", ret);
        if (ret == -EBUSY) {
            return ERR_PTR(-EBUSY);
        }
        return ERR_PTR(-EFAULT);
    }

get_event_id_from_local:
    ka_task_spin_lock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].spinlock);
    event_info = devdrv_get_one_event_id(ts_resource);
    if (event_info == NULL) {
        ka_task_spin_unlock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].spinlock);
        TSDRV_PRINT_ERR("devdrv_get_one_event_id return NULL.\n");
        return ERR_PTR(-ENODEV);
    }
    ka_task_spin_lock(&event_info->spinlock);
    event_info->ctx = ctx;
    event_info->tgid = ctx->tgid;
    ka_task_spin_unlock(&event_info->spinlock);

    ka_list_add(&event_info->list, &ctx->ts_ctx[tsid].id_ctx[TSDRV_EVENT_SW_ID].id_list);
    ctx->ts_ctx[tsid].id_ctx[TSDRV_EVENT_SW_ID].id_num++;
    ka_base_atomic_inc(&event_info->ref);
    ka_task_spin_unlock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].spinlock);
    mailbox_msg.operation_type = 0;
    mailbox_msg.resource_type = 1; /* event_id */
    mailbox_msg.phy_id = event_info->phy_id;
    mailbox_msg.vir_id = event_info->virt_id;
    (void)tsdrv_sync_id_maping_mailbox(&pm_ts_res->mailbox, ctx, &mailbox_msg);

    TSDRV_PRINT_DEBUG("Get a event id. (devid=%u; phy_id=%d; id=%d).\n", devid, event_info->phy_id, event_info->id);
    return event_info;
}

STATIC int tsdrv_inform_ts_free_event_id(u32 devid, u32 tsid, u32 event_id)
{
#if !defined(CFG_SOC_PLATFORM_CLOUD_V2) && !defined(CFG_SOC_PLATFORM_MINIV3)
    return 0;
#else
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct tsdrv_free_event_id_msg msg;
    int result = 0;
    int ret;

    ts_resource = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, tsid);

    msg.header.valid = DEVDRV_MAILBOX_MESSAGE_VALID;
    msg.header.cmd_type = DEVDRV_MAILBOX_RESET_EVENT_ID;
    msg.header.result = 0;
    msg.event_id = event_id;

    ret = devdrv_mailbox_kernel_sync_no_feedback(&ts_resource->mailbox, (u8 *)&msg, sizeof(msg), &result);
    if ((ret != 0) || (result != 0)) {
        TSDRV_PRINT_ERR("devid %u event_id %u inform ts failed, ret %d result %d\n", devid, event_id, ret, result);
        return -EFAULT;
    }

    return 0;
#endif
}

int devdrv_free_event_id(struct tsdrv_ctx *ctx, u32 tsid, int id)
{
    struct tsdrv_ts_resource *pm_ts_res = NULL, *ts_resource = NULL;
    struct resource_mapping_notice_mailbox_t mailbox_msg = {{0}};
    struct tsdrv_id_info *tmp = NULL;
    u32 devid = tsdrv_get_devid_by_ctx(ctx);

    if ((id < 0) || (id >= DEVDRV_MAX_SW_EVENT_ID)) {
        TSDRV_PRINT_ERR("invalid input argument.\n");
        return -EINVAL;
    }

    ts_resource = tsdrv_ctx_to_ts_res(ctx, tsid);
    pm_ts_res = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, tsid);
    ka_task_mutex_lock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].id_mutex_t);

    tmp = devdrv_find_one_event_id(ts_resource, ctx, id);
    if (tmp == NULL) {
        ka_task_mutex_unlock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].id_mutex_t);
        return -ENODATA;
    }
    mailbox_msg.operation_type = 1;
    mailbox_msg.resource_type = 1; /* event_id */
    mailbox_msg.phy_id = tmp->phy_id;
    mailbox_msg.vir_id = tmp->virt_id;
    (void)tsdrv_inform_ts_free_event_id(tsdrv_get_devid_by_ctx(ctx), tsid, (u32)id);

    ka_task_spin_lock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].spinlock);
    tmp = devdrv_find_one_event_id(ts_resource, ctx, id);
    if (tmp == NULL) {
#ifndef TSDRV_UT
        ka_task_spin_unlock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].spinlock);
        ka_task_mutex_unlock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].id_mutex_t);
        return -ENODATA;
#endif
    }
    ka_task_spin_lock(&tmp->spinlock);
    tmp->ctx = NULL;
    ka_task_spin_unlock(&tmp->spinlock);

    ka_list_del(&tmp->list);
    ctx->ts_ctx[tsid].id_ctx[TSDRV_EVENT_SW_ID].id_num--;

    if (ka_base_atomic_dec_and_test(&tmp->ref) && (tmp->valid == 1)) {
        if (id >= DEVDRV_MAX_HW_EVENT_ID) {
            ka_list_add(&tmp->list, &ts_resource->id_res[TSDRV_EVENT_SW_ID].id_available_list);
            ts_resource->id_res[TSDRV_EVENT_SW_ID].id_available_num++;
        } else {
            ka_list_add(&tmp->list, &ts_resource->id_res[TSDRV_EVENT_HW_ID].id_available_list);
            ts_resource->id_res[TSDRV_EVENT_HW_ID].id_available_num++;
        }
        tmp->valid = 0;
    }

    ka_task_spin_unlock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].spinlock);
    ka_task_mutex_unlock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].id_mutex_t);
    (void)tsdrv_sync_id_maping_mailbox(&pm_ts_res->mailbox, ctx, &mailbox_msg);
    TSDRV_PRINT_DEBUG("Free event id success. (devid=%u; phy_id=%u; id=%u)\n", devid, tmp->phy_id, tmp->id);
    return 0;
}

int devdrv_alloc_stream_msg_chan(u32 devid, u32 tsid, struct tsdrv_msg_resource_id *dev_msg_resource_id)
{
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct tsdrv_id_info *stream_info = NULL;
    u32 req_id_num, i, j = 0;

    dev_msg_resource_id->ret_id_num = 0;

    ts_resource = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, tsid);
    if (!tsdrv_set_id_sync_non_idle(ts_resource, TSDRV_STREAM_ID, ID_SYNC_MSG_REMOTE_TO_LOCAL)) {
#ifndef TSDRV_UT
        return 0;
#endif
    }

    ka_task_spin_lock(&ts_resource->id_res[TSDRV_STREAM_ID].spinlock);
    if (ts_resource->id_res[TSDRV_STREAM_ID].id_available_num == 0) {
        ka_task_spin_unlock(&ts_resource->id_res[TSDRV_STREAM_ID].spinlock);
        tsdrv_set_id_sync_idle(ts_resource, TSDRV_STREAM_ID);
        TSDRV_PRINT_WARN("No available stream id.\n");
        return 0;
    }

    req_id_num = ts_resource->id_res[TSDRV_STREAM_ID].id_available_num < dev_msg_resource_id->req_id_num ?
        ts_resource->id_res[TSDRV_STREAM_ID].id_available_num : dev_msg_resource_id->req_id_num;

    for (i = 0; i < req_id_num; i++) {
        stream_info = ka_list_first_entry(&ts_resource->id_res[TSDRV_STREAM_ID].id_available_list,
            struct tsdrv_id_info, list);
        ka_list_del(&stream_info->list);
        ts_resource->id_res[TSDRV_STREAM_ID].id_available_num--;
        dev_msg_resource_id->id[j++] = (u16)stream_info->id;
        dev_msg_resource_id->ret_id_num++;
    }
    ka_task_spin_unlock(&ts_resource->id_res[TSDRV_STREAM_ID].spinlock);
    tsdrv_set_id_sync_idle(ts_resource, TSDRV_STREAM_ID);

    TSDRV_PRINT_DEBUG("Sync id success. (req_id_num=%u; ret_id_num=%u)\n",
        dev_msg_resource_id->req_id_num, dev_msg_resource_id->ret_id_num);

    return 0;
}

int devdrv_add_stream_msg_chan(u32 devid, u32 tsid, struct tsdrv_msg_resource_id *dev_msg_resource_id)
{
    u32 ret_id_num = dev_msg_resource_id->ret_id_num;
    struct tsdrv_id_info *stream_info = NULL;
    struct tsdrv_ts_resource *ts_resource = NULL;
    u16 stream_id;
    u32 i;

    if ((tsid >= DEVDRV_MAX_TS_NUM) || (devid >= TSDRV_MAX_DAVINCI_NUM)) {
        TSDRV_PRINT_ERR("invalid input parameter.\n");
        return -EINVAL;
    }

    if (ret_id_num > DEVDRV_MANAGER_MSG_ID_NUM_MAX) {
        TSDRV_PRINT_ERR("dev_id(%u): invalid input parameter, ret_id_num = %u\n", devid, ret_id_num);
        return -EINVAL;
    }

    ts_resource = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, tsid);

    stream_info = (struct tsdrv_id_info *)ts_resource->id_res[TSDRV_STREAM_ID].id_addr;
    TSDRV_PRINT_DEBUG("1. stream num = %d\n", ts_resource->id_res[TSDRV_STREAM_ID].id_available_num);

    ka_task_spin_lock(&ts_resource->id_res[TSDRV_STREAM_ID].spinlock);
    for (i = 0; i < ret_id_num; i++) {
        stream_id = dev_msg_resource_id->id[i];
        if (stream_id >= tsdrv_get_stream_id_max_num(devid, tsid)) {
            ka_task_spin_unlock(&ts_resource->id_res[TSDRV_STREAM_ID].spinlock);
            TSDRV_PRINT_ERR("[dev_id=%u]:invliad stream id, stream_id = %u\n", devid, stream_id);
            return -EFAULT;
        }
        ts_resource->id_res[TSDRV_STREAM_ID].pm_id[stream_id] = stream_id;
        ka_list_add_tail(&stream_info[stream_id].list, &ts_resource->id_res[TSDRV_STREAM_ID].id_available_list);

        TSDRV_PRINT_DEBUG("stream_id = %d, stream_num = %d\n", stream_id,
            ts_resource->id_res[TSDRV_STREAM_ID].id_available_num);

        ts_resource->id_res[TSDRV_STREAM_ID].id_available_num++;
    }
    ka_task_spin_unlock(&ts_resource->id_res[TSDRV_STREAM_ID].spinlock);

    TSDRV_PRINT_DEBUG("2. stream num = %d\n", ts_resource->id_res[TSDRV_STREAM_ID].id_available_num);

    return 0;
}

int devdrv_add_event_msg_chan(u32 devid, u32 tsid, struct tsdrv_msg_resource_id *dev_msg_resource_id)
{
    u32 ret_id_num = dev_msg_resource_id->ret_id_num;
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct tsdrv_id_info *event_info = NULL;
    u32 event_id;
    u32 i;

    if ((tsid >= DEVDRV_MAX_TS_NUM) || (devid >= TSDRV_MAX_DAVINCI_NUM)) {
        TSDRV_PRINT_ERR("invalid input parameter.\n");
        return -EINVAL;
    }

    ts_resource = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, tsid);

    if (ret_id_num > DEVDRV_MANAGER_MSG_ID_NUM_MAX) {
        TSDRV_PRINT_ERR("dev_id(%u): invalid input parameter, ret_id_num = %u\n", devid, ret_id_num);
        return -EINVAL;
    }

    event_info = (struct tsdrv_id_info *)ts_resource->id_res[TSDRV_EVENT_SW_ID].id_addr;
    TSDRV_PRINT_DEBUG("Available event num. (hw=%u; sw=%u)\n",
        ts_resource->id_res[TSDRV_EVENT_HW_ID].id_available_num,
        ts_resource->id_res[TSDRV_EVENT_SW_ID].id_available_num);

    ka_task_spin_lock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].spinlock);
    for (i = 0; i < ret_id_num; i++) {
        event_id = dev_msg_resource_id->id[i];
        if (event_id >= DEVDRV_MAX_SW_EVENT_ID) {
            ka_task_spin_unlock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].spinlock);
            TSDRV_PRINT_ERR("dev_id(%u): invalid event id, event_id = %u\n", devid, event_id);
            return -EFAULT;
        } else if (event_id >= DEVDRV_MAX_HW_EVENT_ID) {
            ka_list_add_tail(&event_info[event_id].list, &ts_resource->id_res[TSDRV_EVENT_SW_ID].id_available_list);
            ts_resource->id_res[TSDRV_EVENT_SW_ID].id_available_num++;
        } else {
            ka_list_add_tail(&event_info[event_id].list, &ts_resource->id_res[TSDRV_EVENT_HW_ID].id_available_list);
            ts_resource->id_res[TSDRV_EVENT_HW_ID].id_available_num++;
        }
        ts_resource->id_res[TSDRV_EVENT_SW_ID].pm_id[event_id] = event_id;
    }
    ka_task_spin_unlock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].spinlock);

    TSDRV_PRINT_DEBUG("Available event num. (hw=%u; sw=%u; ret_id_num=%u)\n",
        ts_resource->id_res[TSDRV_EVENT_HW_ID].id_available_num,
        ts_resource->id_res[TSDRV_EVENT_SW_ID].id_available_num, ret_id_num);

    return 0;
}

STATIC int devdrv_add_id_msg_chan_l(u32 devid, struct tsdrv_msg_resource_id *dev_msg_resource_id,
    struct tsdrv_ts_resource *ts_resource)
{
    u32 ret_id_num = dev_msg_resource_id->ret_id_num;
    struct tsdrv_id_info *id_info = NULL;
    u32 id;
    u32 i;

    id_info = (struct tsdrv_id_info *)ts_resource->id_res[TSDRV_EVENT_SW_ID].id_addr;
    TSDRV_PRINT_DEBUG("1. event_num: hw(%u) sw(%u)\n", ts_resource->id_res[TSDRV_EVENT_HW_ID].id_available_num,
        ts_resource->id_res[TSDRV_EVENT_SW_ID].id_available_num);

    ka_task_spin_lock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].spinlock);
    for (i = 0; i < ret_id_num; i++) {
        id = dev_msg_resource_id->id[i];
        if (id >= DEVDRV_MAX_SW_EVENT_ID) {
            ka_task_spin_unlock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].spinlock);
            TSDRV_PRINT_ERR("dev_id(%u): invalid event id, event_id = %u\n", devid, id);
            return -EFAULT;
        } else if (id >= DEVDRV_MAX_HW_EVENT_ID) {
            ka_list_add_tail(&id_info[id].list, &ts_resource->id_res[TSDRV_EVENT_SW_ID].id_available_list);
            ts_resource->id_res[TSDRV_EVENT_SW_ID].id_available_num++;
        } else {
            ka_list_add_tail(&id_info[id].list, &ts_resource->id_res[TSDRV_EVENT_HW_ID].id_available_list);
            ts_resource->id_res[TSDRV_EVENT_HW_ID].id_available_num++;
        }
        ts_resource->id_res[TSDRV_EVENT_SW_ID].pm_id[id] = id;
    }

    ka_task_spin_unlock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].spinlock);
    TSDRV_PRINT_DEBUG("2. event_num hw(%u) sw(%u).\n", ts_resource->id_res[TSDRV_EVENT_HW_ID].id_available_num,
        ts_resource->id_res[TSDRV_EVENT_SW_ID].id_available_num);

    return 0;
}

int devdrv_add_id_msg_chan(u32 devid, u32 tsid, struct tsdrv_msg_resource_id *dev_msg_resource_id)
{
    u32 ret_id_num = dev_msg_resource_id->ret_id_num;
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct tsdrv_id_info *id_info = NULL;
    enum tsdrv_id_type id_type = dev_msg_resource_id->id_type;
    u32 id, i;
    int ret;

    if ((devid >= TSDRV_MAX_DAVINCI_NUM) || (tsid >= tsdrv_get_dev_tsnum(devid)) || (id_type >= TSDRV_MAX_ID)) {
        TSDRV_PRINT_ERR("invalid input tsid(%u) devid(%u) id_type(%d).\n", tsid, devid, (int)id_type);
        return -EINVAL;
    }

    ts_resource = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, tsid);

    if (ret_id_num > DEVDRV_MANAGER_MSG_ID_NUM_MAX) {
        TSDRV_PRINT_ERR("dev_id(%u): invalid input parameter, ret_id_num = %u\n", devid, ret_id_num);
        return -EINVAL;
    }

    if ((id_type == TSDRV_EVENT_HW_ID) || (id_type == TSDRV_EVENT_SW_ID)) {
        ret = devdrv_add_id_msg_chan_l(devid, dev_msg_resource_id, ts_resource);
        if (ret != 0) {
            TSDRV_PRINT_ERR("id add failed, ret(%d),id_type(%d).\n", ret, (int)id_type);
            return -EINVAL;
        }
    } else {
        id_info = (struct tsdrv_id_info *)ts_resource->id_res[id_type].id_addr;
        TSDRV_PRINT_DEBUG("1. id_num(%u)\n", ts_resource->id_res[id_type].id_available_num);

        ka_task_spin_lock(&ts_resource->id_res[id_type].spinlock);
        for (i = 0; i < ret_id_num; i++) {
            id = dev_msg_resource_id->id[i];
            if (id >= ts_resource->id_res[id_type].id_capacity) {
                ka_task_spin_unlock(&ts_resource->id_res[id_type].spinlock);
                TSDRV_PRINT_ERR("dev_id(%u): invalid id(%u) id_type(%d)\n", devid, id, (int)id_type);
                return -EFAULT;
            } else {
                ka_list_add_tail(&id_info[id].list, &ts_resource->id_res[id_type].id_available_list);
                ts_resource->id_res[id_type].id_available_num++;
                ts_resource->id_res[id_type].pm_id[id] = id;
            }
        }

        ka_task_spin_unlock(&ts_resource->id_res[id_type].spinlock);
        TSDRV_PRINT_DEBUG("2. id_num(%u) id_type(%d).\n", ts_resource->id_res[id_type].id_available_num,
            (int)id_type);
    }

    return 0;
}

/* Notes:Physical machines apply for hardware event IDs first, and VMs apply for event IDs based on id_type. */
STATIC int devdrv_alloc_id_msg_chan_l(u32 devid, struct tsdrv_msg_resource_id *dev_msg_resource_id,
    struct tsdrv_ts_resource *ts_resource)
{
    enum tsdrv_id_type id_type = dev_msg_resource_id->id_type;
    u32 req_id_num = dev_msg_resource_id->req_id_num;
    struct tsdrv_id_info *id_info = NULL;
    u32 i;
    u32 j = 0;

    ka_task_spin_lock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].spinlock);
    if (req_id_num > DEVDRV_MANAGER_MSG_ID_NUM_MAX) {
#ifndef TSDRV_UT
        ka_task_spin_unlock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].spinlock);
        TSDRV_PRINT_ERR("dev_id(%u):event_num hw(%u) sw(%u), req_id_num(%u), no enough event id\n",
            devid,
            ts_resource->id_res[TSDRV_EVENT_HW_ID].id_available_num,
            ts_resource->id_res[TSDRV_EVENT_SW_ID].id_available_num,
            req_id_num);
        return -EINVAL;
#endif
    }
    if (ts_resource->id_res[id_type].id_available_num < req_id_num) {
        req_id_num = ts_resource->id_res[id_type].id_available_num;
    }
    for (i = 0; i < req_id_num; i++) {
        if (id_type == TSDRV_EVENT_HW_ID) {
            id_info = ka_list_first_entry(&ts_resource->id_res[TSDRV_EVENT_HW_ID].id_available_list,
                struct tsdrv_id_info, list);
            ka_list_del(&id_info->list);
            ts_resource->id_res[TSDRV_EVENT_HW_ID].id_available_num--;
        } else {
            id_info = ka_list_first_entry(&ts_resource->id_res[TSDRV_EVENT_SW_ID].id_available_list,
                struct tsdrv_id_info, list);
            ka_list_del(&id_info->list);
            ts_resource->id_res[TSDRV_EVENT_SW_ID].id_available_num--;
        }
        TSDRV_PRINT_DEBUG("event_id[%u]:%d, ret_id_num(%u), event_num hw(%u) sw(%u).\n",
            j, id_info->id, dev_msg_resource_id->ret_id_num,
            ts_resource->id_res[TSDRV_EVENT_HW_ID].id_available_num,
            ts_resource->id_res[TSDRV_EVENT_SW_ID].id_available_num);
        dev_msg_resource_id->id[j++] = id_info->id;
        dev_msg_resource_id->ret_id_num++;
    }
    ka_task_spin_unlock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].spinlock);
    return 0;
}
int devdrv_alloc_id_msg_chan(u32 devid, u32 tsid, struct tsdrv_msg_resource_id *dev_msg_resource_id)
{
    u32 req_id_num = dev_msg_resource_id->req_id_num;
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct tsdrv_id_info *id_info = NULL;
    enum tsdrv_id_type id_type = dev_msg_resource_id->id_type;
    u32 i;
    u32 j = 0;
    int ret;

    if ((devid >= TSDRV_MAX_DAVINCI_NUM) || (tsid >= tsdrv_get_dev_tsnum(devid)) || (id_type >= TSDRV_MAX_ID)) {
        TSDRV_PRINT_ERR("invalid input tsid(%u) devid(%u) id_type(%d).\n", tsid, devid, (int)id_type);
        return -EINVAL;
    }

    ts_resource = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, tsid);

    if ((id_type == TSDRV_EVENT_HW_ID) || (id_type == TSDRV_EVENT_SW_ID)) {
        ret = devdrv_alloc_id_msg_chan_l(devid, dev_msg_resource_id, ts_resource);
        if (ret != 0) {
            TSDRV_PRINT_ERR("id alloc failed, ret(%d),id_type(%d).\n", ret, (int)id_type);
            return -EINVAL;
        }
    } else {
        ka_task_spin_lock(&ts_resource->id_res[id_type].spinlock);
        if (req_id_num > DEVDRV_MANAGER_MSG_ID_NUM_MAX) {
#ifndef TSDRV_UT
            ka_task_spin_unlock(&ts_resource->id_res[id_type].spinlock);
            TSDRV_PRINT_ERR("dev_id(%u):id_num(%u) id_type(%d), req_id_num(%u), no enough event id\n",
                devid,
                ts_resource->id_res[id_type].id_available_num,
                (int)id_type,
                req_id_num);
            return -EINVAL;
#endif
        }

        if (ts_resource->id_res[id_type].id_available_num < req_id_num) {
            req_id_num = ts_resource->id_res[id_type].id_available_num;
        }
        for (i = 0; i < req_id_num; i++) {
            id_info = ka_list_first_entry(&ts_resource->id_res[id_type].id_available_list,
                struct tsdrv_id_info, list);
            ka_list_del(&id_info->list);
            ts_resource->id_res[id_type].id_available_num--;
            TSDRV_PRINT_DEBUG("(devid=%u; j=%u; id=%d; ret_id_num=%u; id_num=%u; id_type=%d)\n", devid, j, id_info->id,
                dev_msg_resource_id->ret_id_num, ts_resource->id_res[id_type].id_available_num, (int)id_type);
            dev_msg_resource_id->id[j++] = id_info->id;
            dev_msg_resource_id->ret_id_num++;
        }
        ka_task_spin_unlock(&ts_resource->id_res[id_type].spinlock);
    }
    return 0;
}

int devdrv_alloc_event_msg_chan(u32 devid, u32 tsid, struct tsdrv_msg_resource_id *dev_msg_resource_id,
    u32 *sync_hw_event_num, u32 *sync_sw_event_num)
{
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct tsdrv_id_info *event_info = NULL;
    u32 req_id_num, i, j = 0;
    u32 hw_event_avail_num, sw_event_avail_num;

    *sync_hw_event_num = 0;
    *sync_sw_event_num = 0;
    dev_msg_resource_id->ret_id_num = 0;

    ts_resource = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, tsid);
	if (!tsdrv_set_id_sync_non_idle(ts_resource, TSDRV_EVENT_SW_ID, ID_SYNC_MSG_REMOTE_TO_LOCAL)) {
#ifndef TSDRV_UT
        return -EPERM;
#endif
    }

    ka_task_spin_lock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].spinlock);

    hw_event_avail_num = ts_resource->id_res[TSDRV_EVENT_HW_ID].id_available_num;
    sw_event_avail_num = ts_resource->id_res[TSDRV_EVENT_SW_ID].id_available_num;

    if ((hw_event_avail_num + sw_event_avail_num) == 0) {
        ka_task_spin_unlock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].spinlock);
        tsdrv_set_id_sync_idle(ts_resource, TSDRV_EVENT_SW_ID);
        TSDRV_PRINT_WARN("No available event id.\n");
        return 0;
    }

    req_id_num = (hw_event_avail_num + sw_event_avail_num) < dev_msg_resource_id->req_id_num ?
        (hw_event_avail_num + sw_event_avail_num) : dev_msg_resource_id->req_id_num;

    for (i = 0; i < req_id_num; i++) {
        if (ts_resource->id_res[TSDRV_EVENT_HW_ID].id_available_num > 0) {
            event_info = ka_list_first_entry(&ts_resource->id_res[TSDRV_EVENT_HW_ID].id_available_list,
                struct tsdrv_id_info, list);
            ka_list_del(&event_info->list);
            ts_resource->id_res[TSDRV_EVENT_HW_ID].id_available_num--;
            (*sync_hw_event_num)++;
        } else if (ts_resource->id_res[TSDRV_EVENT_SW_ID].id_available_num > 0) {
            event_info = ka_list_first_entry(&ts_resource->id_res[TSDRV_EVENT_SW_ID].id_available_list,
                struct tsdrv_id_info, list);
            ka_list_del(&event_info->list);
            ts_resource->id_res[TSDRV_EVENT_SW_ID].id_available_num--;
            (*sync_sw_event_num)++;
        }
        dev_msg_resource_id->id[j++] = event_info->id;
        dev_msg_resource_id->ret_id_num++;
    }
    ka_task_spin_unlock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].spinlock);
    tsdrv_set_id_sync_idle(ts_resource, TSDRV_EVENT_SW_ID);

    TSDRV_PRINT_DEBUG("Sync event id success. (req_id_num=%u; ret_id_num=%u; "
        "sync_hw_event_num=%u; sync_sw_event_num=%u)\n",
        dev_msg_resource_id->req_id_num, dev_msg_resource_id->ret_id_num, *sync_hw_event_num, *sync_sw_event_num);

    return 0;
}

int devdrv_alloc_ipc_event_id(struct tsdrv_ctx *ctx, u32 tsid, u32 *id)
{
    struct tsdrv_id_info *id_info = NULL;
    struct tsdrv_ts_resource *ts_resource = NULL;
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    u32 id_num;
    int ret = 0;

    ts_resource = tsdrv_ctx_to_ts_res(ctx, tsid);
    ka_task_mutex_lock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].id_mutex_t);
    id_num = devdrv_get_available_event_num(ts_resource, EVENT_HW_TYPE);
    if (id_num == 0) {
        ret = tsdrv_msg_alloc_sync_event(devid, tsid);
        if (ret != 0) {
            ka_task_mutex_unlock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].id_mutex_t);
            if (ret == -EBUSY) {
                return ret;
            }
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("try to get ipc event id from opposite side failed\n");
            return -EINVAL;
#endif
        }
    }

    ka_task_spin_lock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].spinlock);
    id_info = devdrv_get_one_event_id(ts_resource);
    if (id_info == NULL) {
#ifndef TSDRV_UT
        ka_task_spin_unlock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].spinlock);
        ka_task_mutex_unlock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].id_mutex_t);
        TSDRV_PRINT_ERR("get ipc event id return NULL.\n");
        return -EINVAL;
#endif
    }

    ka_task_spin_lock(&id_info->spinlock);
    id_info->ctx = ctx;
    id_info->tgid = ctx->tgid;
    id_info->fid = tsdrv_get_fid_by_ctx(ctx);
    ka_task_spin_unlock(&id_info->spinlock);

    ka_list_add(&id_info->list, &ctx->ts_ctx[tsid].id_ctx[TSDRV_EVENT_HW_ID].id_list);
    ctx->ts_ctx[tsid].id_ctx[TSDRV_EVENT_HW_ID].id_num++;
    ka_base_atomic_inc(&id_info->ref);
    ka_task_spin_unlock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].spinlock);
    ka_task_mutex_unlock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].id_mutex_t);

    ret = devdrv_notify_ts_msg(devid, tsid, IPC_EVENT_TYPE, (u32)id_info->phy_id | IPC_EVENT_MASK, id_info);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("send ts msg failed, id_info id = %d\n", id_info->id);
        goto notify_ts_msg_failed;
#endif
    }
    TSDRV_PRINT_DEBUG("Alloc ok. (devid=%u; tsid=%u; id=%d).\n", devid, tsid, id_info->id);
    *id = (u32)(id_info->id);
    return 0;
#ifndef TSDRV_UT
notify_ts_msg_failed:
    if (devdrv_free_ipc_event_id(ctx, tsid, id_info->id, DEVDRV_NOTIFY_NOT_INFORM_TS) != 0) {
        TSDRV_PRINT_ERR("free one ipc event id failed, id = %d\n", id_info->id);
    }
    return ret;
#endif
}

int devdrv_free_ipc_event_id(struct tsdrv_ctx *ctx, u32 tsid, int id, int inform_type)
{
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct tsdrv_id_info *id_info = NULL;
    struct tsdrv_ctx *tmp_ctx = NULL;
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    int ret;

    if ((id < 0) || (id >= DEVDRV_MAX_HW_EVENT_ID)) {
        TSDRV_PRINT_ERR("invalid input argument.\n");
        return -EINVAL;
    }

    ts_resource = tsdrv_ctx_to_ts_res(ctx, tsid);
    ka_task_mutex_lock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].id_mutex_t);
    id_info = devdrv_find_one_event_id(ts_resource, ctx, id);
    if (id_info == NULL) {
#ifndef TSDRV_UT
        ka_task_mutex_unlock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].id_mutex_t);
        TSDRV_PRINT_ERR("devdrv_find_one_ipc_event_id return NULL.\n");
        return -ENODATA;
#endif
    }

    if ((ka_base_atomic_read(&id_info->ref) == 1) && (inform_type == DEVDRV_NOTIFY_INFORM_TS)) {
#ifndef TSDRV_UT
        ret = devdrv_notify_ts_msg(devid, ts_resource->tsid, IPC_EVENT_TYPE,
            (u32)id_info->phy_id | IPC_EVENT_MASK, id_info);
        if (ret != 0) {
            ka_task_mutex_unlock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].id_mutex_t);
            TSDRV_PRINT_ERR("send ts ipc event alloc msg failed, ipc event id = %d\n", id_info->id);
            return ret;
        }
#endif
    }

    /* device sync eventid from host in devdrv_alloc_id_msg_chan_l() func in bottom half,
    so needing ka_task_spin_lock protect list here */
    ka_task_spin_lock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].spinlock);
    ka_task_spin_lock(&id_info->spinlock);
    tmp_ctx = (struct tsdrv_ctx *)id_info->ctx;
    if ((id_info->id != id) || (tmp_ctx == NULL) || (tmp_ctx->pid != ctx->pid)) {
#ifndef TSDRV_UT
        ka_task_spin_unlock(&id_info->spinlock);
        ka_task_spin_unlock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].spinlock);
        ka_task_mutex_unlock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].id_mutex_t);
        TSDRV_PRINT_ERR("check ipc event info failed, id(%d), ipc event->id(%d), ctx->pid(%d), devid(%u)\n",
            id, id_info->id, ctx->pid, devid);
        return -EINVAL;
#endif
    }

    id_info->ctx = NULL;
    ka_list_del(&id_info->list);
    ctx->ts_ctx[tsid].id_ctx[TSDRV_EVENT_HW_ID].id_num--;
    if (ka_base_atomic_dec_and_test(&id_info->ref) && (id_info->valid == 1)) {
        ka_list_add(&id_info->list, &ts_resource->id_res[TSDRV_EVENT_HW_ID].id_available_list);
        id_info->valid = 0;
        ts_resource->id_res[TSDRV_EVENT_HW_ID].id_available_num++;
    }
    ka_task_spin_unlock(&id_info->spinlock);
    ka_task_spin_unlock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].spinlock);
    ka_task_mutex_unlock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].id_mutex_t);
    TSDRV_PRINT_DEBUG("free ok. (devid=%u; tsid=%u; id=%u).\n", devid, tsid, id);
    return 0;
}

STATIC int tsdrv_id_config_dfx_query(struct tsdrv_ts_resource *ts_res, struct tsdrv_id_config_info *id_config)
{
    enum tsdrv_id_type id_type = id_config->id_type;
    u32 devid = tsdrv_get_devid_by_res(ts_res);
    u32 fid = tsdrv_get_fid_by_res(ts_res);
    int err = 0;

    if (id_type < TSDRV_STREAM_ID || id_type >= TSDRV_MAX_ID) {
        TSDRV_PRINT_ERR("[devid-%u tsid-%u fid-%u]invalid id_type(%d).\n",
            devid, ts_res->tsid, fid, (int)id_type);
        return -EINVAL;
    }

    id_config->avail_num = ts_res->id_res[id_type].id_available_num;
    id_config->capacity = ts_res->id_res[id_type].id_capacity;
    if (fid == TSDRV_PM_FID) {
        err = tsdrv_get_ts_id_cur_num(devid, ts_res->tsid, id_type, &id_config->cur_num);
    } else {
        id_config->cur_num = ts_res->id_res[id_type].id_capacity;
    }
    TSDRV_PRINT_DEBUG("[devid-%u tsid-%u fid-%u]id config:id_type(%d), capacity(%u), cur_num(%u), avail_num(%u).\n",
        devid, ts_res->tsid, fid, (int)id_type, id_config->capacity, id_config->cur_num, id_config->avail_num);
    return err;
}


int tsdrv_ioctl_id_info_query(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    struct tsdrv_ts_resource *ts_res = tsdrv_ctx_to_ts_res(ctx, arg->tsid);

    switch (arg->id_query_para.opt) {
        case QUERY_IDS_CAPACITY:
            return tsdrv_get_ts_capacity(ts_res, &arg->id_query_para.id_capacity);
        case QUERY_ID_CONFIG_INFO:
            return tsdrv_id_config_dfx_query(ts_res, &arg->id_query_para.id_config_info);
        default:
            TSDRV_PRINT_ERR("invalid opt(%d).\n", (int)arg->id_query_para.opt);
            return -EINVAL;
    }
}

#ifdef CFG_SOC_PLATFORM_HELPER
static void tsdrv_stream_enable_event(u32 devid, u32 fid, u32 tsid, u32 stream_id, struct tsdrv_res_config_para *para)
{
    struct tsdrv_ts_resource *res_info = NULL;
    struct tsdrv_stream_info *stream_ctx = NULL;

    res_info = tsdrv_get_ts_resoruce(devid, fid, tsid);
    stream_ctx = &res_info->stream_info[stream_id];

    ka_task_spin_lock_bh(&stream_ctx->lock);
    stream_ctx->event_enable = 1;
    stream_ctx->grp_id = para->grp_id;
    stream_ctx->event_id = para->event_id;
    ka_task_spin_unlock_bh(&stream_ctx->lock);
}
#endif

int tsdrv_ioctl_res_config(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    struct tsdrv_res_config_para *para = (struct tsdrv_res_config_para*)&arg->res_config_para;
    struct tsdrv_id_inst id_inst;
    enum tsdrv_logic_cq_set_prop prop;
    u32 devid, fid, tsid, stream_id;
    int ret = 0;

    devid = tsdrv_get_devid_by_ctx(ctx);
    fid = tsdrv_get_fid_by_ctx(ctx);
    tsid = arg->tsid;
    stream_id = para->stream_id;
    prop = (enum tsdrv_logic_cq_set_prop)para->prop;

    id_inst.devid = devid;
    id_inst.fid = fid;
    id_inst.tsid = tsid;

    if (tsdrv_id_is_belong_to_proc(&id_inst, ctx->tgid, stream_id, TSDRV_STREAM_ID) != 0) {
        TSDRV_PRINT_ERR("StreamId is not belong to this proc. (streamId=%u; ret=%d)\n",
            stream_id, ret);
        return -EINVAL;
    }

    switch (prop) {
        case TSDRV_STREAM_ENABLE_EVENT:
#ifdef CFG_SOC_PLATFORM_HELPER
            tsdrv_stream_enable_event(devid, fid, tsid, stream_id, para);
#else
            ret = -EPERM;
#endif
            break;
        default:
            TSDRV_PRINT_WARN("config res cmd not support. (prop=%d)\n", para->prop);
            ret = -ENOTSUPP;
            break;
    }

    return ret;
}
