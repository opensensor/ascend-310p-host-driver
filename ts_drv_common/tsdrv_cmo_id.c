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

#ifdef CFG_FEATURE_CMO
#include <asm/page.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/gfp.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>

#include "tsdrv_device.h"
#include "tsdrv_sync.h"
#include "tsdrv_id.h"
#include "tsdrv_cmo_id.h"

STATIC u32 tsdrv_get_available_cmo_num(struct tsdrv_ts_resource *ts_resource)
{
    u32 cmo_id_num;

    ka_task_spin_lock(&ts_resource->id_res[TSDRV_CMO_ID].spinlock);
    cmo_id_num = ts_resource->id_res[TSDRV_CMO_ID].id_available_num;
    ka_task_spin_unlock(&ts_resource->id_res[TSDRV_CMO_ID].spinlock);
    return cmo_id_num;
}

STATIC struct tsdrv_id_info *tsdrv_get_one_cmo_id(struct tsdrv_ts_resource *ts_resource)
{
    struct tsdrv_id_info *id = NULL;

    if (ka_list_empty_careful(&ts_resource->id_res[TSDRV_CMO_ID].id_available_list) != 0) {
        return NULL;
    }
    id = ka_list_first_entry(&ts_resource->id_res[TSDRV_CMO_ID].id_available_list, struct tsdrv_id_info, list);
    ka_list_del(&id->list);
    ts_resource->id_res[TSDRV_CMO_ID].id_available_num--;
    return id;
}

STATIC struct tsdrv_id_info *tsdrv_find_one_cmo_id(struct tsdrv_ts_resource *ts_resource,
    struct tsdrv_ctx *ctx, int phy_id)
{
    struct tsdrv_id_info *cmo_info = NULL;
    struct tsdrv_ctx *tmp_ctx = NULL;

    cmo_info = (struct tsdrv_id_info *)(ts_resource->id_res[TSDRV_CMO_ID].id_addr +
                    ((long)sizeof(struct tsdrv_id_info) * phy_id));
    tmp_ctx = (struct tsdrv_ctx *)cmo_info->ctx;

    if ((cmo_info->phy_id != phy_id) || (tmp_ctx == NULL) || (tmp_ctx != ctx)) {
        return NULL;
    } else {
        return cmo_info;
    }
}

int tsdrv_add_cmo_msg_chan(u32 devid, u32 tsid, struct tsdrv_msg_resource_id *dev_msg_resource_id)
{
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct tsdrv_id_info *cmo_info = NULL;
    u32 ret_id_num;
    u16 cmo_id;
    u32 i;

    if ((tsid >= DEVDRV_MAX_TS_NUM) || (devid >= TSDRV_MAX_DAVINCI_NUM) || (dev_msg_resource_id == NULL)) {
        TSDRV_PRINT_ERR("invalid input parameter.\n");
        return -EINVAL;
    }
    ts_resource = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, tsid);

    ret_id_num = dev_msg_resource_id->ret_id_num;

    if (ret_id_num > DEVDRV_MANAGER_MSG_ID_NUM_MAX) {
        TSDRV_PRINT_ERR("[dev_id = %u]:ret_id_num = %u is invalid.\n", devid, ret_id_num);
        return -ENODEV;
    }

    cmo_info = (struct tsdrv_id_info *)ts_resource->id_res[TSDRV_CMO_ID].id_addr;

    ka_task_spin_lock(&ts_resource->id_res[TSDRV_CMO_ID].spinlock);
    for (i = 0; i < ret_id_num; i++) {
        cmo_id = dev_msg_resource_id->id[i];
        ka_list_add_tail(&cmo_info[cmo_id].list, &ts_resource->id_res[TSDRV_CMO_ID].id_available_list);
        ts_resource->id_res[TSDRV_CMO_ID].id_available_num++;

        TSDRV_PRINT_DEBUG("cmo_id = %u, cmo_id_num = %u\n", (u32)cmo_id,
            ts_resource->id_res[TSDRV_CMO_ID].id_available_num);
    }
    ka_task_spin_unlock(&ts_resource->id_res[TSDRV_CMO_ID].spinlock);
    return 0;
}

int tsdrv_alloc_cmo_msg_chan(u32 devid, u32 tsid, struct tsdrv_msg_resource_id *dev_msg_resource_id)
{
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct tsdrv_id_info *cmo_info = NULL;
    u32 req_id_num;
    u32 i;
    u32 j = 0;

    if ((tsid >= DEVDRV_MAX_TS_NUM) || (devid >= TSDRV_MAX_DAVINCI_NUM) || (dev_msg_resource_id == NULL)) {
        TSDRV_PRINT_ERR("invalid input parameter.\n");
        return -EINVAL;
    }
    ts_resource = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, tsid);

    req_id_num = dev_msg_resource_id->req_id_num;

    ka_task_spin_lock(&ts_resource->id_res[TSDRV_CMO_ID].spinlock);
    if (req_id_num > DEVDRV_MANAGER_MSG_ID_NUM_MAX) {
        ka_task_spin_unlock(&ts_resource->id_res[TSDRV_CMO_ID].spinlock);
        TSDRV_PRINT_ERR("[dev_id = %u]:no enough cmo id, model_id_num = %u, req_id_num = %u.\n", devid,
            ts_resource->id_res[TSDRV_CMO_ID].id_available_num, req_id_num);
        return -EINVAL;
    }

    for (i = 0; i < req_id_num; i++) {
        if (ts_resource->id_res[TSDRV_CMO_ID].id_available_num > 0) {
            cmo_info = ka_list_first_entry(&ts_resource->id_res[TSDRV_CMO_ID].id_available_list,
                struct tsdrv_id_info, list);
            ka_list_del(&cmo_info->list);
            ts_resource->id_res[TSDRV_CMO_ID].id_available_num--;
        } else {
            break;
        }
        dev_msg_resource_id->id[j++] = cmo_info->id;
        dev_msg_resource_id->ret_id_num++;
    }
    ka_task_spin_unlock(&ts_resource->id_res[TSDRV_CMO_ID].spinlock);

    return 0;
}

int tsdrv_cmo_id_init(u32 devid, u32 fid, u32 tsid, u32 cmo_id_num)
{
    struct tsdrv_id_info *cmo_info = NULL;
    struct tsdrv_ts_resource *ts_resource = NULL;
    u32 cmo_min, cmo_max;
    unsigned long size;
    int err;
    u32 i;

    ts_resource = tsdrv_get_ts_resoruce(devid, fid, tsid);
    if (ts_resource == NULL) {
        TSDRV_PRINT_ERR("param is invalid, dev_id=%u, tsid=%u\n", devid, tsid);
        return -EINVAL;
    }
    err = tsdrv_get_ts_id_range(devid, tsid, TSDRV_CMO_ID, &cmo_min, &cmo_max);
    if (err != 0) {
        return -ENODEV;
    }
    TSDRV_PRINT_DEBUG("devid(%u) fid(%u) tsid(%u) cmo_min(%u) cmo_max(%u)\n", devid, fid, tsid, cmo_min,
        cmo_max);

    if (!ka_list_empty_careful(&ts_resource->id_res[TSDRV_CMO_ID].id_available_list)) {
        TSDRV_PRINT_ERR("[dev_id = %u]:available cmo list is not empty.\n", devid);
        return -EEXIST;
    }

    size = (long)(unsigned)sizeof(struct tsdrv_id_info) * cmo_id_num;
    cmo_info = tsdrv_vmalloc(size, KA_GFP_KERNEL | __KA_GFP_ACCOUNT, KA_PAGE_KERNEL);
    if (cmo_info == NULL) {
        TSDRV_PRINT_ERR("[dev_id = %u]:cmo_info vmalloc failed\n", devid);
        return -ENOMEM;
    }

    ts_resource->id_res[TSDRV_CMO_ID].id_available_num = 0;
    for (i = 0; i < cmo_id_num; i++) {
        cmo_info[i].phy_id = i;
        cmo_info[i].id = cmo_info[i].phy_id;
        cmo_info[i].devid = devid;
        cmo_info[i].ctx = NULL;
        if ((i < cmo_max) && (i >= cmo_min)) {
            ka_list_add_tail(&cmo_info[i].list, &ts_resource->id_res[TSDRV_CMO_ID].id_available_list);
            ts_resource->id_res[TSDRV_CMO_ID].id_available_num++;
        } else {
            KA_INIT_LIST_HEAD(&cmo_info[i].list);
        }
    }
    ts_resource->id_res[TSDRV_CMO_ID].id_addr = cmo_info;

    return 0;
}

void tsdrv_cmo_id_destroy(u32 devid, u32 fid, u32 tsid)
{
    struct tsdrv_ts_resource *ts_resource = NULL;

    ts_resource = tsdrv_get_ts_resoruce(devid, fid, tsid);
    if (ts_resource == NULL) {
        TSDRV_PRINT_ERR("param is invalid, dev_id=%u, fid=%u, tsid=%u\n", devid, fid, tsid);
        return;
    }

    KA_INIT_LIST_HEAD(&ts_resource->id_res[TSDRV_CMO_ID].id_available_list);
    tsdrv_vfree(ts_resource->id_res[TSDRV_CMO_ID].id_addr);
    ts_resource->id_res[TSDRV_CMO_ID].id_addr = NULL;
    return;
}

static int tsdrv_alloc_cmo_id(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct tsdrv_id_info *cmo_info = NULL;
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    u32 tsid = arg->tsid;
    u32 cmo_num;
    int ret = 0;

    ts_resource = tsdrv_ctx_to_ts_res(ctx, tsid);

    ka_task_mutex_lock(&ts_resource->id_res[TSDRV_CMO_ID].id_mutex_t);
    cmo_num = tsdrv_get_available_cmo_num(ts_resource);
    if (cmo_num == 0) {
        TSDRV_PRINT_DEBUG("no cmo, try to get cmo from opposite side, cce_ctrl->cmo_id_num = %u\n",
            ts_resource->id_res[TSDRV_CMO_ID].id_available_num);
        ret = tsdrv_msg_alloc_sync_cmo(devid, tsid);
        if (ret != 0) {
            ka_task_mutex_unlock(&ts_resource->id_res[TSDRV_CMO_ID].id_mutex_t);
            if (ret == -ENODEV) {
                arg->id_para.res_id = DEVDRV_MAX_CMO_ID;
                arg->result = ID_IS_EXHAUSTED;
            }
            TSDRV_PRINT_ERR("try to get cmo id from opposite side failed\n");
            return DEVDRV_MAX_CMO_ID;
        }

        TSDRV_PRINT_DEBUG("got cmo id, cce_ctrl->cmo_id_num = %u\n",
            ts_resource->id_res[TSDRV_CMO_ID].id_available_num);
    }

    ka_task_spin_lock(&ts_resource->id_res[TSDRV_CMO_ID].spinlock);
    cmo_info = tsdrv_get_one_cmo_id(ts_resource);
    if (cmo_info == NULL) {
        ka_task_spin_unlock(&ts_resource->id_res[TSDRV_CMO_ID].spinlock);
        ka_task_mutex_unlock(&ts_resource->id_res[TSDRV_CMO_ID].id_mutex_t);
        TSDRV_PRINT_ERR("devdrv_get_one_cmo_id return NULL.\n");
        return DEVDRV_MAX_CMO_ID;
    }
    cmo_info->ctx = ctx;
    ka_list_add(&cmo_info->list, &ctx->ts_ctx[tsid].id_ctx[TSDRV_CMO_ID].id_list);
    ctx->ts_ctx[tsid].id_ctx[TSDRV_CMO_ID].id_num++;
    ka_task_spin_unlock(&ts_resource->id_res[TSDRV_CMO_ID].spinlock);
    ka_task_mutex_unlock(&ts_resource->id_res[TSDRV_CMO_ID].id_mutex_t);

    return cmo_info->id;
}

static int tsdrv_free_cmo_id(struct tsdrv_ctx *ctx, u32 tsid, int id)
{
    struct tsdrv_id_info *cmo_info = NULL;
    struct tsdrv_ts_resource *ts_resource = NULL;

    if ((id < 0) || (id >= DEVDRV_MAX_CMO_ID)) {
        TSDRV_PRINT_ERR("invalid input argument.\n");
        return -EINVAL;
    }
    ts_resource = tsdrv_ctx_to_ts_res(ctx, tsid);
    ka_task_spin_lock(&ts_resource->id_res[TSDRV_CMO_ID].spinlock);
    cmo_info = tsdrv_find_one_cmo_id(ts_resource, ctx, id);
    if ((cmo_info == NULL) || (cmo_info->ctx == NULL)) {
        ka_task_spin_unlock(&ts_resource->id_res[TSDRV_CMO_ID].spinlock);
        TSDRV_PRINT_ERR("find cmo id failed\n");
        return -ENODATA;
    }
    cmo_info->ctx = NULL;

    ka_list_del(&cmo_info->list);
    ka_list_add(&cmo_info->list, &ts_resource->id_res[TSDRV_CMO_ID].id_available_list);
    ts_resource->id_res[TSDRV_CMO_ID].id_available_num++;
    ctx->ts_ctx[ts_resource->tsid].id_ctx[TSDRV_CMO_ID].id_num--;
    ka_task_spin_unlock(&ts_resource->id_res[TSDRV_CMO_ID].spinlock);
    return 0;
}

int tsdrv_ioctl_alloc_cmo(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    int cmo_id = tsdrv_alloc_cmo_id(ctx, arg);
    if (cmo_id >= DEVDRV_MAX_CMO_ID) {
        TSDRV_PRINT_ERR("alloc cmo id fail cmo_id(%d)\n", cmo_id);
        return -EINVAL;
    }
    arg->id_para.res_id = (u32)cmo_id;

    return 0;
}

int tsdrv_ioctl_free_cmo(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    int ret = tsdrv_free_cmo_id(ctx, arg->tsid, arg->id_para.res_id);
    if (ret != 0) {
        TSDRV_PRINT_ERR("free cmo id fail ret(%d)\n", ret);
        return ret;
    }

    return 0;
}
#endif /* CFG_FEATURE_CMO */
