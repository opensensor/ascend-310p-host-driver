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
#include <linux/delay.h>

#include "vtsdrv_common.h"
#include "hvtsdrv_id.h"
#include "hvtsdrv_id_event.h"
#include "hvtsdrv_tsagent.h"
#include "shm_sqcq.h"
#include "tsdrv_capacity.h"
#include "tsdrv_sync.h"
#include "devdrv_recycle.h"
#include "devdrv_id.h"
#include "hvtsdrv_proc.h"
#include "tsdrv_common.h"

struct tsdrv_id_info *tsdrv_get_one_res_id(struct tsdrv_id_resource *id_res)
{
    struct tsdrv_id_info *id = NULL;

    if (ka_list_empty_careful(&id_res->id_available_list)) {
        return NULL;
    }
    id = ka_list_first_entry(&id_res->id_available_list, struct tsdrv_id_info, list);
    ka_list_del(&id->list);
    id_res->id_available_num--;
    return id;
}

struct tsdrv_id_info *tsdrv_find_one_id(struct tsdrv_id_resource *id_res, struct tsdrv_ctx *ctx, s32 phy_id)
{
    struct tsdrv_id_info *tmp = NULL;
    struct tsdrv_ctx *tmp_ctx = NULL;

    tmp = (struct tsdrv_id_info *)(id_res->id_addr + (long)sizeof(struct tsdrv_id_info) * phy_id);
    tmp_ctx = (struct tsdrv_ctx *)tmp->ctx;

    if ((tmp->phy_id != phy_id) || (tmp_ctx == NULL) || (tmp_ctx != ctx)) {
        return NULL;
    } else {
        return tmp;
    }
}

struct tsdrv_id_info *tsdrv_dev_res_id_move(u32 devid, u32 fid, u32 tsid, struct res_id_move *move_info)
{
    struct tsdrv_ts_resource *pm_ts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
    struct tsdrv_id_resource *src_id = move_info->src_id;
    struct tsdrv_id_resource *des_id = move_info->des_id;
    enum tsdrv_id_type id_type = move_info->id_type;
    enum tsdrv_id_type id_type_for_mux = devdrv_manager_id_type_replace(move_info->id_type);
    struct tsdrv_id_info *id_info = NULL;
    s32 ret;

    ka_task_mutex_lock(&pm_ts_res->id_res[id_type_for_mux].id_mutex_t);
    ka_task_spin_lock(&src_id->spinlock);
    if (src_id->id_available_num == 0) {
        ka_task_spin_unlock(&src_id->spinlock);
        if (move_info->sync_flag == NEED_SYNC_FROM_DEVICE) {
            ret = tsdrv_msg_alloc_sync_id(devid, tsid, id_type);
            if (ret != 0) {
#ifndef TSDRV_UT
                ka_task_mutex_unlock(&pm_ts_res->id_res[id_type_for_mux].id_mutex_t);
                TSDRV_PRINT_ERR("dev-%u tsid-%u id_type-%d sync fail.\n", devid, tsid, (int)id_type);
                return NULL;
#endif
            }
            TSDRV_PRINT_DEBUG("got id_num=%u id_type=%d.\n", src_id->id_available_num, (int)id_type);
            ka_task_spin_lock(&src_id->spinlock);
        } else {
            ka_task_mutex_unlock(&pm_ts_res->id_res[id_type_for_mux].id_mutex_t);
            TSDRV_PRINT_INFO("no id left,id_type=%d.\n", (int)id_type);
            return NULL;
        }
    }

    id_info = tsdrv_get_one_res_id(src_id);
    if (id_info == NULL) {
        ka_task_spin_unlock(&src_id->spinlock);
        ka_task_mutex_unlock(&pm_ts_res->id_res[id_type_for_mux].id_mutex_t);
        TSDRV_PRINT_ERR("get one id fail, id_type=%d.\n", (int)id_type);
        return NULL;
    }
    ka_task_spin_unlock(&src_id->spinlock);

    ka_task_spin_lock(&des_id->spinlock);
    /* when back to the host ,the id must be set back to phy_id */
    if (move_info->sync_flag == DONT_SYNC_FROM_DEVICE) {
        id_info->id = id_info->phy_id;
    }
    ka_list_add(&id_info->list, &des_id->id_available_list);
    des_id->id_available_num++;
    ka_task_spin_unlock(&des_id->spinlock);
    ka_task_mutex_unlock(&pm_ts_res->id_res[id_type_for_mux].id_mutex_t);

    return id_info;
}

/* resource_id convert: virt_id to phy_id */
s32 tsdrv_get_pmid(u32 devid, u32 fid, u32 tsid, s32 virtid, enum tsdrv_id_type id_type)
{
    struct tsdrv_dev_resource *dev_res = NULL;
    struct tsdrv_ts_resource *ts_res = NULL;
    u32 id_capacity;
    s32 pm_id;

    if (virtid < 0) {
        TSDRV_PRINT_ERR("dev-%u fid-%u tsid-%u virtid-%d id_type-%d invalid.\n", devid, fid, tsid,
            virtid, (int)id_type);
        return -EINVAL;
    }

    dev_res = tsdrv_get_dev_resource(devid, fid);
    if (dev_res == NULL) {
        TSDRV_PRINT_ERR("get dev resource failed, devid(%u) fid(%u).\n", devid, fid);
        return -EINVAL;
    }

    if (id_type == TSDRV_EVENT_HW_ID || id_type == TSDRV_EVENT_SW_ID) {
        id_type = TSDRV_EVENT_SW_ID;
        id_capacity = dev_res->ts_resource[tsid].id_res[TSDRV_EVENT_HW_ID].id_capacity +
            dev_res->ts_resource[tsid].id_res[TSDRV_EVENT_SW_ID].id_capacity;
            TSDRV_PRINT_DEBUG("hw(%u), sw(%u).\n", dev_res->ts_resource[tsid].id_res[TSDRV_EVENT_HW_ID].id_capacity,
                dev_res->ts_resource[tsid].id_res[TSDRV_EVENT_SW_ID].id_capacity);
    } else {
        id_capacity = dev_res->ts_resource[tsid].id_res[id_type].id_capacity;
    }

    if ((u32)virtid >= id_capacity) {
        TSDRV_PRINT_ERR("dev(%u) fid(%u) tsid(%u) virtid(%d) id_type(%d) capacity(%u) invalid.\n",
            devid, fid, tsid, virtid, (int)id_type, id_capacity);
        return -EINVAL;
    }

    ts_res = &dev_res->ts_resource[tsid];
    ka_task_spin_lock(&ts_res->id_res[id_type].spinlock);
    if (ts_res->id_res[id_type].pm_id == NULL) {
        ka_task_spin_unlock(&ts_res->id_res[id_type].spinlock);
        TSDRV_PRINT_ERR("dev-%u fid-%u tsid-%u id_type-%d pm_id is null.\n", devid, fid, tsid, (int)id_type);
        return -EINVAL;
    }

    pm_id = ts_res->id_res[id_type].pm_id[virtid];
    ka_task_spin_unlock(&ts_res->id_res[id_type].spinlock);

    return pm_id;
}

STATIC s32 tsdrv_res_id_init(u32 devid, u32 fid, enum tsdrv_id_type id_type)
{
    struct tsdrv_dev_resource *pm_dev_res = NULL;
    struct tsdrv_dev_resource *vm_dev_res = NULL;
    struct tsdrv_ts_resource *ts_res = NULL;
    struct tsdrv_id_info *id_info = NULL;
    struct res_id_move move_info;
    u32 id_cnt, tsnum, i, j;

    if ((id_type == TSDRV_EVENT_SW_ID) || (id_type == TSDRV_EVENT_HW_ID)) {
        return 0;
    }

    vm_dev_res = tsdrv_get_dev_resource(devid, fid);
    pm_dev_res = tsdrv_get_dev_resource(devid, TSDRV_PM_FID);
    if (vm_dev_res == NULL || pm_dev_res == NULL) {
        TSDRV_PRINT_ERR("get dev resource failed, devid(%u).\n", devid);
        return -EINVAL;
    }

    move_info.sync_flag = NEED_SYNC_FROM_DEVICE;
    tsnum = tsdrv_get_dev_tsnum(devid);
    for (i = 0; i < tsnum; i++) {
        id_cnt = vm_dev_res->ts_resource[i].id_res[id_type].id_capacity;
        if (id_cnt == TSDRV_INVALIC_CAPACITY) {
            continue;
        }

        ts_res = &vm_dev_res->ts_resource[i];
        ts_res->id_res[id_type].pm_id = tsdrv_kzalloc(id_cnt * sizeof(u32), KA_GFP_KERNEL);
        if (ts_res->id_res[id_type].pm_id == NULL) {
            TSDRV_PRINT_ERR("kzalloc failed, dev-%u fid-%u ts-%u id_type-%d.\n", devid, fid, i, (int)id_type);
            goto release_resouce;
        }

        for (j = 0; j < id_cnt; j++) {
            move_info.id_type = id_type;
            move_info.src_id = &pm_dev_res->ts_resource[i].id_res[id_type];
            move_info.des_id = &ts_res->id_res[id_type];
            id_info = tsdrv_dev_res_id_move(devid, fid, i, &move_info);
            if (id_info == NULL) {
                TSDRV_PRINT_ERR("dev-%u fid-%u ts-%u type-%d cnt-%u resouce move fail.\n", devid, fid,
                    i, (int)id_type, j);
                goto release_resouce;
            }
            id_info->virt_id = j;
            id_info->id = id_info->virt_id;
            ts_res->id_res[id_type].pm_id[j] = id_info->phy_id;
        }
        ts_res->id_res[id_type].id_addr = pm_dev_res->ts_resource[i].id_res[id_type].id_addr;
    }

    return 0;
release_resouce:
    tsdrv_res_id_uninit(devid, fid, id_type);
    return -EPERM;
}

void tsdrv_res_id_uninit(u32 devid, u32 fid, enum tsdrv_id_type id_type)
{
    struct tsdrv_dev_resource *pm_dev_res = NULL;
    struct tsdrv_dev_resource *vm_dev_res = NULL;
    struct tsdrv_id_info *id_info = NULL;
    struct res_id_move move_info;
    u32 tsnum, num, i, j;

    vm_dev_res = tsdrv_get_dev_resource(devid, fid);
    pm_dev_res = tsdrv_get_dev_resource(devid, TSDRV_PM_FID);
    if (vm_dev_res == NULL || pm_dev_res == NULL) {
        TSDRV_PRINT_ERR("get dev resource failed, devid(%u).\n", devid);
        return;
    }

    move_info.id_type = id_type;
    move_info.sync_flag = DONT_SYNC_FROM_DEVICE;
    tsnum = tsdrv_get_dev_tsnum(devid);
    for (i = 0; i < tsnum; i++) {
        num = vm_dev_res->ts_resource[i].id_res[id_type].id_available_num;
        for (j = 0; j < num; j++) {
            move_info.src_id = &vm_dev_res->ts_resource[i].id_res[id_type];
            move_info.des_id = &pm_dev_res->ts_resource[i].id_res[id_type];
            id_info = tsdrv_dev_res_id_move(devid, fid, i, &move_info);
            if (id_info == NULL) {
                TSDRV_PRINT_WARN("dev-%u ts-%u id_type-%d, id-%u is not recycled.\n", devid, i, (int)id_type, j);
            }
        }
        KA_INIT_LIST_HEAD(&vm_dev_res->ts_resource[i].id_res[id_type].id_available_list);
        vm_dev_res->ts_resource[i].id_res[id_type].id_addr = NULL;
        if (vm_dev_res->ts_resource[i].id_res[id_type].pm_id != NULL) {
            tsdrv_kfree(vm_dev_res->ts_resource[i].id_res[id_type].pm_id);
            vm_dev_res->ts_resource[i].id_res[id_type].pm_id = NULL;
        }
    }
}

s32 tsdrv_res_id_alloc(struct tsdrv_ctx *ctx, enum tsdrv_id_type type, struct devdrv_ioctl_arg *arg)
{
    struct tsdrv_ts_resource *ts_res = NULL;
    struct tsdrv_id_info *id = NULL;
    u32 devid, fid, tsid;

    devid = tsdrv_get_devid_by_ctx(ctx);
    fid = tsdrv_get_fid_by_ctx(ctx);
    tsid = arg->tsid;
    if (type >= TSDRV_EVENT_HW_ID) {
        TSDRV_PRINT_ERR("dev-%u fid-%u tsid-%u id_type-%d tsid invalid.\n", devid, fid, tsid, (int)type);
        return -EINVAL;
    }

    ts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
    ka_task_spin_lock(&ts_res->id_res[type].spinlock);
    if (type == TSDRV_EVENT_SW_ID) {
        id = tsdrv_get_one_res_id(&ts_res->id_res[TSDRV_EVENT_HW_ID]);
        if (id == NULL) {
            id = tsdrv_get_one_res_id(&ts_res->id_res[TSDRV_EVENT_SW_ID]);
            if (id == NULL) {
                ka_task_spin_unlock(&ts_res->id_res[type].spinlock);
                arg->id_para.res_id = ts_res->id_res[TSDRV_EVENT_HW_ID].id_capacity +
                    ts_res->id_res[TSDRV_EVENT_SW_ID].id_capacity;
                arg->result = ID_IS_EXHAUSTED;
                TSDRV_PRINT_ERR("dev-%u fid-%u tsid-%u id_type-%d get one event fail.\n", devid, fid, tsid, (int)type);
                return -EPERM;
            }
        }
        ka_base_atomic_inc(&id->ref);
        id->valid = 1;
    } else {
        /* model_id, stream_id */
        id = tsdrv_get_one_res_id(&ts_res->id_res[type]);
        if (id == NULL) {
            ka_task_spin_unlock(&ts_res->id_res[type].spinlock);
            arg->id_para.res_id = ts_res->id_res[type].id_capacity;
            arg->result = ID_IS_EXHAUSTED;
            TSDRV_PRINT_ERR("dev-%u fid-%u tsid-%u id_type-%d get one id fail.\n", devid, fid, tsid, (int)type);
            return -EPERM;
        }
    }

    id->tgid = ctx->tgid;
    id->ctx = ctx;
    ka_list_add(&id->list, &ctx->ts_ctx[tsid].id_ctx[type].id_list);
    ctx->ts_ctx[tsid].id_ctx[type].id_num++;
    ka_task_spin_unlock(&ts_res->id_res[type].spinlock);

    arg->id_para.res_id = id->virt_id;
    TSDRV_PRINT_DEBUG("Alloc resource id success. (id_type=%d; virt_id=%d)\n", (int)type, id->virt_id);

    return 0;
}

s32 tsdrv_res_id_free(struct tsdrv_ctx *ctx, enum tsdrv_id_type type, struct devdrv_ioctl_arg *arg)
{
    struct tsdrv_ts_resource *ts_res = NULL;
    struct tsdrv_id_info *tmp = NULL;
    u32 devid, fid, tsid;
    s32 pm_id, id;

    devid = tsdrv_get_devid_by_ctx(ctx);
    fid = tsdrv_get_fid_by_ctx(ctx);
    tsid = arg->tsid;
    if (type >= TSDRV_EVENT_HW_ID) {
        TSDRV_PRINT_ERR("dev-%u fid-%u tsid-%u id_type-%d tsid invalid.\n", devid, fid, tsid, (int)type);
        return -EINVAL;
    }

    id = arg->id_para.res_id;
    TSDRV_PRINT_DEBUG("Free resource id. (id_type=%d; virt_id=%u)\n", (int)type, arg->id_para.res_id);
    pm_id = tsdrv_get_pmid(devid, fid, tsid, id, type);
    if (pm_id < 0) {
        return -EINVAL;
    }

    ts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);

    ka_task_spin_lock(&ts_res->id_res[type].spinlock);
    if (type == TSDRV_EVENT_SW_ID) {
        tmp = tsdrv_find_one_id(&ts_res->id_res[TSDRV_EVENT_SW_ID], ctx, pm_id);
    } else {
        tmp = tsdrv_find_one_id(&ts_res->id_res[type], ctx, pm_id);
    }
    if (tmp == NULL) {
        ka_task_spin_unlock(&ts_res->id_res[type].spinlock);
        TSDRV_PRINT_ERR("dev-%u fid-%u tsid-%u id_type-%d find id fail.\n", devid, fid, tsid, (int)type);
        return -ENODATA;
    }

    tmp->ctx = NULL;
    ka_list_del(&tmp->list);
    ctx->ts_ctx[tsid].id_ctx[type].id_num--;
    if (type == TSDRV_EVENT_SW_ID) {
        if (ka_base_atomic_dec_and_test(&tmp->ref) && (tmp->valid == 1)) {
            if (tmp->phy_id >= DEVDRV_MAX_HW_EVENT_ID) {
                ka_list_add(&tmp->list, &ts_res->id_res[TSDRV_EVENT_SW_ID].id_available_list);
                ts_res->id_res[TSDRV_EVENT_SW_ID].id_available_num++;
            } else {
                ka_list_add(&tmp->list, &ts_res->id_res[TSDRV_EVENT_HW_ID].id_available_list);
                ts_res->id_res[TSDRV_EVENT_HW_ID].id_available_num++;
            }
            tmp->valid = 0;
        }
    } else {
        ka_list_add(&tmp->list, &ts_res->id_res[type].id_available_list);
        ts_res->id_res[type].id_available_num++;
    }
    ka_task_spin_unlock(&ts_res->id_res[type].spinlock);

    return 0;
}

s32 tsdrv_notify_id_alloc(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    return devdrv_alloc_notify_id(ctx, arg);
}

s32 tsdrv_notify_id_free(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    return devdrv_free_notify_id(ctx, arg);
}

void hvtsdrv_recycle(u32 devid, u32 fid, ka_pid_t tgid, u64 unique_id, int recycle_status)
{
    struct vtsdrv_msg msg;
    int ret;

    msg.cmd = HVTSDRV_DEV_RECYCLE;
    msg.hvdev_proc.tgid = tgid;
    msg.hvdev_proc.proc_msg.tsid = 0;
    msg.hvdev_proc.proc_msg.rec_data_para.unique_id = unique_id;
    msg.hvdev_proc.proc_msg.rec_data_para.recycle_status = recycle_status;
    ret = hvtsdrv_vpc_msg_send(devid, fid, &msg);
    if (ret != 0) {
        TSDRV_PRINT_ERR("Failed to send recycle status."
            "devid[%u], fid[%u], tgid[%d], unique_id[%llu], rec_status[%d].\n",
            devid, fid, tgid, unique_id, recycle_status);
        return;
    }

    TSDRV_PRINT_INFO("send recycle finish vpc msg."
        "devid[%u], fid[%u], tgid[%d], unique_id[%llu], rec_status[%d].\n",
        devid, fid, tgid, unique_id, recycle_status);
}

s32 tsdrv_ctx_ids_recycle(u32 devid, u32 fid, struct tsdrv_ctx *ctx)
{
    struct tsdrv_dev_resource *dev_res = NULL;
    int ret;

    dev_res = tsdrv_get_dev_resource(devid, fid);
    if (dev_res == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("get dev resource failed, devid(%u) fid(%u).\n", devid, fid);
        return -EINVAL;
#endif
    }

    ret = hvtsdrv_proc_recycle(devid, ctx, dev_res, fid);
    if (ret == TSDRV_IDS_RECYCLE_STOP) {
        return 0;
    } else if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Proc recycle failed. (devid=%u; fid=%u; ret=%d)\n", devid, fid, ret);
        return ret;
#endif
    }

    if (tsdrv_is_in_vm(devid) == true) {
        // normal release process, only vm can send recycle vpc msg.
        hvtsdrv_recycle(devid, fid, ctx->tgid, ctx->unique_id, 0);
    }
    shm_sqcq_ctx_exit(ctx, tsdrv_get_dev_tsnum(devid));
    tsdrv_dev_set_ctx_recycle_status(ctx, TSDRV_CTX_INVALID);

    ret = tsdrv_set_runtime_available_conflict_check(devid);
    if (ret != 0) {
        TSDRV_PRINT_ERR("set runtime status fail, devid=%u fid=%u ret=%d.\n", devid, fid, ret);
        return -EINVAL;
    }

    return 0;
}

STATIC void tsdrv_dev_ids_async_recycle(u32 devid, u32 fid)
{
#ifndef TSDRV_UT
    struct tsdrv_device *tsdrv_dev = NULL;
    struct tsdrv_dev_ctx *dev_ctx = NULL;
    struct tsdrv_ctx *ctx = NULL;
    u32 ctx_num, i;
    int ret;

    tsdrv_dev = tsdrv_get_dev(devid);
    dev_ctx = &tsdrv_dev->dev_ctx[fid];
    ctx_num = dev_ctx->ctx_num;
    for (i = 0; i < ctx_num; i++) {
        ctx = tsdrv_find_one_ctx(dev_ctx);
        if (ctx != NULL) {
            tsdrv_dev_proc_ctx_ref_disable(ctx);
            if (tsdrv_dev_proc_ctx_ref_check(ctx) == false) {
                continue;
            }

            ret = tsdrv_set_ctx_releasing(ctx);
            if (ret != 0) {
                continue;
            }
            tsdrv_dev_set_ctx_recycle(ctx);
            (void)tsdrv_ctx_ids_recycle(devid, fid, ctx);
        } else {
            TSDRV_PRINT_INFO("dev-%u fid-%u left ctx number %u.\n", devid, fid, dev_ctx->ctx_num);
        }
    }
#endif
}

s32 tsdrv_ids_init(u32 devid, u32 fid)
{
    int id_type, i;
    s32 ret;

    ret = tsdrv_event_id_init(devid, fid);
    if (ret != 0) {
        TSDRV_PRINT_ERR("dev-%u fid-%u event id init fail.\n", devid, fid);
        return ret;
    }

    for (id_type = (int)TSDRV_STREAM_ID; id_type < (int)TSDRV_EVENT_SW_ID; id_type++) {
        ret = tsdrv_res_id_init(devid, fid, id_type);
        if (ret != 0) {
            TSDRV_PRINT_ERR("dev-%u fid-%u id_type-%d id init fail.\n", devid, fid, id_type);
            goto id_init_failed;
        }
    }
    TSDRV_PRINT_INFO("dev-%u fid-%u id init success.\n", devid, fid);

    return ret;

id_init_failed:
    for (i = id_type; i >= (int)TSDRV_STREAM_ID; i--) {
        tsdrv_res_id_uninit(devid, fid, i);
    }
    tsdrv_event_id_uninit(devid, fid);

    return ret;
}

void tsdrv_wait_dev_ids_recycle_complete(u32 devid, u32 fid)
{
#define CUR_TIMEOUT_FREQ 300

    unsigned long timeout;
    u32 ctx_num;

    timeout = jiffies + CUR_TIMEOUT_FREQ * KA_HZ;
    while (1) {
        ctx_num = tsdrv_dev_get_ctx_num(devid, fid);
        if (ctx_num == 0) {
            break;
        }
        tsdrv_dev_ids_async_recycle(devid, fid);
        ka_system_msleep(1);
        if (ka_system_time_after(jiffies, timeout)) {
            TSDRV_PRINT_ERR("dev-%u fid-%u ctx_num-%u waited timeout, ts failed to notify drv to recycle all ids.\n",
                devid, fid, ctx_num);
            tsdrv_set_ctxs_stop_recycle(devid, fid);
            break;
        }
    }
}

void tsdrv_ids_uninit(u32 devid, u32 fid)
{
    enum tsdrv_id_type id_type;

    /* Recycle resources allocated by virtual machines to host */
    tsdrv_event_id_uninit(devid, fid);
    for (id_type = TSDRV_STREAM_ID; id_type < TSDRV_EVENT_SW_ID; id_type++) {
        tsdrv_res_id_uninit(devid, fid, id_type);
    }

    TSDRV_PRINT_INFO("dev-%u fid-%u id uninit success.\n", devid, fid);
}

s32 hvtsdrv_id_info_query(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    return tsdrv_ioctl_id_info_query(ctx, arg);
}

/* tsdrv event_id convert interface: virt to physical */
int hvtsdrv_event_id_v2p(u32 devid, u32 tsid, u32 fid, u32 virt, u32 *phy)
{
#ifndef TSDRV_UT
    struct tsdrv_dev_resource *dev_res = NULL;
    struct tsdrv_ts_resource *ts_res = NULL;

    if ((devid >= TSDRV_MAX_DAVINCI_NUM) || (fid >= TSDRV_MAX_FID_NUM) ||
        (fid == TSDRV_PM_FID) || (tsid >= DEVDRV_MAX_TS_NUM) || (phy == NULL)) {
        TSDRV_PRINT_ERR("Para error. (dev_id=%u; tsid=%u; fid=%u).\n", devid, tsid, fid);
        return -EINVAL;
    }

    dev_res = tsdrv_get_dev_resource(devid, fid);
    if (dev_res == NULL) {
        TSDRV_PRINT_ERR("get dev resource failed, devid(%u) fid(%u).\n", devid, fid);
        return -EINVAL;
    }

    ts_res = &dev_res->ts_resource[tsid];
    ka_task_spin_lock(&ts_res->id_res[TSDRV_EVENT_SW_ID].spinlock);
    if (ts_res->id_res[TSDRV_EVENT_SW_ID].pm_id == NULL) {
        ka_task_spin_unlock(&ts_res->id_res[TSDRV_EVENT_SW_ID].spinlock);
        TSDRV_PRINT_ERR("dev-%u fid-%u tsid-%u id_type-%d pm_id is null.\n",
            devid, fid, tsid, (int)TSDRV_EVENT_SW_ID);
        return -EINVAL;
    }

    *phy = ts_res->id_res[TSDRV_EVENT_SW_ID].pm_id[virt];
    ka_task_spin_unlock(&ts_res->id_res[TSDRV_EVENT_SW_ID].spinlock);
#endif
    return 0;
}
KA_EXPORT_SYMBOL_GPL(hvtsdrv_event_id_v2p);

