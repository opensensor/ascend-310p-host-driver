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
#ifndef TSDRV_KERNEL_UT
#include "tsdrv_sync.h"
#include "hvtsdrv_id.h"
#include "tsdrv_capacity.h"
#include "hvtsdrv_tsagent.h"
#include "hvtsdrv_id_event.h"

int tsdrv_event_alloc_pm(struct tsdrv_ts_resource *res)
{
    if (res->id_res[TSDRV_EVENT_SW_ID].pm_id == NULL) {
        res->id_res[TSDRV_EVENT_SW_ID].pm_id = tsdrv_kzalloc(DEVDRV_MAX_SW_EVENT_ID * sizeof(u32), KA_GFP_KERNEL);
        if (res->id_res[TSDRV_EVENT_SW_ID].pm_id == NULL) {
            TSDRV_PRINT_ERR("kzalloc failed.");
            return -ENOMEM;
        }
    }
    return 0;
}

#ifdef TSDRV_UT
/* memory leak for id mapping !!!! */
void tsdrv_event_free_pm(u32 devid, u32 fid)
{
    struct tsdrv_ts_resource *ts_res = NULL;
    u32 tsnum = tsdrv_get_dev_tsnum(devid);
    u32 tsid;

    for (tsid = 0; tsid < tsnum; tsid++) {
        ts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
        if (ts_res->id_res[TSDRV_EVENT_SW_ID].pm_id != NULL) {
            free(ts_res->id_res[TSDRV_EVENT_SW_ID].pm_id);
            ts_res->id_res[TSDRV_EVENT_SW_ID].pm_id = NULL;
        }
    }
}
#endif

int tsdrv_event_id_maping(u32 devid, struct tsdrv_sync_event *event)
{
    struct tsdrv_dev_resource *vm_dev_res = NULL;
    struct tsdrv_ts_resource *ts_res = NULL;
    u32 tsnum;
    int ret;

    if ((event == NULL) || (event->vfid >= TSDRV_MAX_FID_NUM) ||
        (event->virt_id >= DEVDRV_MAX_SW_EVENT_ID) || (event->tsid > TSDRV_MAX_ID)) {
        return -EINVAL;
    }
    if (event->vfid == 0) {
        return 0;
    }

    tsnum = tsdrv_get_dev_tsnum(devid);
    if (event->tsid >= tsnum) {
        return -EINVAL;
    }
    vm_dev_res = tsdrv_get_dev_resource(devid, event->vfid);
    if (vm_dev_res == NULL) {
        TSDRV_PRINT_ERR("get dev resource failed. (devid=%u; vfid=%u)\n", devid, event->vfid);
        return -EINVAL;
    }
    ts_res = &vm_dev_res->ts_resource[event->tsid];
    ret = tsdrv_event_alloc_pm(ts_res);
    if (ret != 0) {
        TSDRV_PRINT_ERR("Alloc map buffer failed. (devid=%u; ts_id=%u vfid=%u)\n", devid, event->tsid, event->vfid);
        return -EINVAL;
    }

    ka_task_spin_lock(&ts_res->id_res[TSDRV_EVENT_SW_ID].spinlock);
    ts_res->id_res[TSDRV_EVENT_SW_ID].pm_id[event->virt_id] = event->phy_id;
    ka_task_spin_unlock(&ts_res->id_res[TSDRV_EVENT_SW_ID].spinlock);

    TSDRV_PRINT_DEBUG("Event id mapping succeed. (devid=%u; fid=%u; tsid=%u)\n", devid, event->vfid, event->tsid);
    return 0;
}

int tsdrv_sync_event_id_maping(u32 devid, u32 ts_id, u32 vfid, u32 phy_id, u32 virt_id)
{
#ifdef CFG_FEATURE_ID_MAPING
    struct tsdrv_id_map_sync_info id_map = {0};
    if (vfid == 0) {
        return 0;
    }
    id_map.id_type = TSDRV_EVENT_SW_ID;
    id_map.event.tsid = ts_id;
    id_map.event.vfid = vfid;
    id_map.event.phy_id = phy_id;
    id_map.event.virt_id = virt_id;
    TSDRV_PRINT_DEBUG("SW event id mapping succeed. (devid=%u; fid=%u; tsid=%u)\n", devid, vfid, ts_id);
    return tsdrv_msg_sync_id_map(devid, id_map);
#else
    return 0;
#endif
}

int tsdrv_event_id_init(u32 devid, u32 fid)
{
    struct tsdrv_dev_resource *pm_dev_res = NULL;
    struct tsdrv_dev_resource *vm_dev_res = NULL;
    struct tsdrv_ts_resource *ts_res = NULL;
    struct tsdrv_id_info *id_info = NULL;
    struct res_id_move move_info;
    u32 id_cnt, id_hw_cnt;
    u32 tsnum;
    u32 i, j;
    int ret;

    vm_dev_res = tsdrv_get_dev_resource(devid, fid);
    pm_dev_res = tsdrv_get_dev_resource(devid, TSDRV_PM_FID);
    if (vm_dev_res == NULL || pm_dev_res == NULL) {
        TSDRV_PRINT_ERR("get dev resource failed, devid(%u).\n", devid);
        return -EINVAL;
    }

    move_info.sync_flag = NEED_SYNC_FROM_DEVICE;
    tsnum = tsdrv_get_dev_tsnum(devid);
    for (i = 0; i < tsnum; i++) {
        id_cnt = vm_dev_res->ts_resource[i].id_res[TSDRV_EVENT_SW_ID].id_capacity;
        if (id_cnt == TSDRV_INVALIC_CAPACITY) {
            continue;
        }
        id_hw_cnt = vm_dev_res->ts_resource[i].id_res[TSDRV_EVENT_HW_ID].id_capacity;
        if (id_hw_cnt == TSDRV_INVALIC_CAPACITY) {
            continue;
        }
        id_cnt += id_hw_cnt;
        ts_res = &vm_dev_res->ts_resource[i];
        ts_res->id_res[TSDRV_EVENT_SW_ID].pm_id = tsdrv_kzalloc(id_cnt * sizeof(u32), KA_GFP_KERNEL);
        if (ts_res->id_res[TSDRV_EVENT_SW_ID].pm_id == NULL) {
            TSDRV_PRINT_ERR("kzalloc failed, dev-%u fid-%u ts-%u.\n", devid, fid, i);
            goto release_resouce;
        }

        for (j = 0; j < id_hw_cnt; j++) {
            move_info.id_type = TSDRV_EVENT_HW_ID;
            move_info.src_id = &pm_dev_res->ts_resource[i].id_res[TSDRV_EVENT_HW_ID];
            move_info.des_id = &ts_res->id_res[TSDRV_EVENT_HW_ID];
            id_info = tsdrv_dev_res_id_move(devid, fid, i, &move_info);
            if (id_info == NULL) {
                TSDRV_PRINT_ERR("dev-%u fid-%u ts-%u cnt-%u resouce move fail.\n", devid, fid, i, j);
                goto release_resouce;
            }
            id_info->virt_id = j;
            id_info->id = id_info->virt_id;
            ts_res->id_res[TSDRV_EVENT_SW_ID].pm_id[j] = id_info->phy_id;
            ret = tsdrv_sync_event_id_maping(devid, i, fid, id_info->phy_id, j);
            if (ret != 0) {
                TSDRV_PRINT_ERR("dev-%u fid-%u ts-%u cnt-%u sync id maping failed.\n", devid, fid, i, j);
                goto release_resouce;
            }
        }

        for (; j < id_cnt; j++) {
            move_info.id_type = TSDRV_EVENT_SW_ID;
            move_info.src_id = &pm_dev_res->ts_resource[i].id_res[TSDRV_EVENT_SW_ID];
            move_info.des_id = &ts_res->id_res[TSDRV_EVENT_SW_ID];
            id_info = tsdrv_dev_res_id_move(devid, fid, i, &move_info);
            if (id_info == NULL) {
                TSDRV_PRINT_ERR("dev-%u fid-%u ts-%u cnt-%u resouce move fail.\n", devid, fid, i, j);
                goto release_resouce;
            }
            id_info->virt_id = j;
            id_info->id = id_info->virt_id;
            ts_res->id_res[TSDRV_EVENT_SW_ID].pm_id[j] = id_info->phy_id;
            ret = tsdrv_sync_event_id_maping(devid, i, fid, id_info->phy_id, j);
            if (ret != 0) {
                TSDRV_PRINT_ERR("dev-%u fid-%u ts-%u cnt-%u sync id maping failed.\n", devid, fid, i, j);
                goto release_resouce;
            }
        }
        ts_res->id_res[TSDRV_EVENT_SW_ID].id_addr = pm_dev_res->ts_resource[i].id_res[TSDRV_EVENT_SW_ID].id_addr;
    }
    TSDRV_PRINT_DEBUG("Init event id success. (devid=%u; fid=%u; avail_num=%u)\n", devid, fid,
        move_info.des_id->id_available_num);

    return 0;
release_resouce:
    tsdrv_event_id_uninit(devid, fid);
    return -EPERM;
}

void tsdrv_event_id_uninit(u32 devid, u32 fid)
{
    struct tsdrv_dev_resource *pm_dev_res = NULL;
    struct tsdrv_dev_resource *vm_dev_res = NULL;
    struct tsdrv_id_info *id_info = NULL;
    enum tsdrv_id_type id_type;
    struct res_id_move move_info;
    u32 tsnum, num, i, j;

    vm_dev_res = tsdrv_get_dev_resource(devid, fid);
    pm_dev_res = tsdrv_get_dev_resource(devid, TSDRV_PM_FID);
    if (vm_dev_res == NULL || pm_dev_res == NULL) {
        TSDRV_PRINT_ERR("get dev resource failed, devid(%u).\n", devid);
        return;
    }

    move_info.sync_flag = DONT_SYNC_FROM_DEVICE;
    tsnum = tsdrv_get_dev_tsnum(devid);
    for (i = 0; i < tsnum; i++) {
        id_type = TSDRV_EVENT_HW_ID;
        num = vm_dev_res->ts_resource[i].id_res[id_type].id_available_num;
        for (j = 0; j < num; j++) {
            move_info.id_type = id_type;
            move_info.src_id = &vm_dev_res->ts_resource[i].id_res[id_type];
            move_info.des_id = &pm_dev_res->ts_resource[i].id_res[id_type];
            id_info = tsdrv_dev_res_id_move(devid, fid, i, &move_info);
            if (id_info == NULL) {
                TSDRV_PRINT_WARN("dev-%u ts-%u type-%d, id-%u is not recycled.\n", devid, i, (int)id_type, j);
            }
        }

        id_type = TSDRV_EVENT_SW_ID;
        num = vm_dev_res->ts_resource[i].id_res[id_type].id_available_num;
        for (j = 0; j < num; j++) {
            move_info.id_type = id_type;
            move_info.src_id = &vm_dev_res->ts_resource[i].id_res[id_type];
            move_info.des_id = &pm_dev_res->ts_resource[i].id_res[id_type];
            id_info = tsdrv_dev_res_id_move(devid, fid, i, &move_info);
            if (id_info == NULL) {
                TSDRV_PRINT_WARN("dev-%u ts-%u type-%d, id-%u is not recycled.\n", devid, i, (int)id_type, j);
            }
        }
        KA_INIT_LIST_HEAD(&vm_dev_res->ts_resource[i].id_res[TSDRV_EVENT_HW_ID].id_available_list);
        KA_INIT_LIST_HEAD(&vm_dev_res->ts_resource[i].id_res[TSDRV_EVENT_SW_ID].id_available_list);
        vm_dev_res->ts_resource[i].id_res[TSDRV_EVENT_SW_ID].id_addr = NULL;
        if (vm_dev_res->ts_resource[i].id_res[TSDRV_EVENT_SW_ID].pm_id != NULL) {
            tsdrv_kfree(vm_dev_res->ts_resource[i].id_res[TSDRV_EVENT_SW_ID].pm_id);
            vm_dev_res->ts_resource[i].id_res[TSDRV_EVENT_SW_ID].pm_id = NULL;
        }
    }
}
#else
void tsdrv_event_id_uninit(void)
{
    return;
}
#endif

