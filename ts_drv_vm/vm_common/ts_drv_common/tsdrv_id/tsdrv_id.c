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
#include "tsdrv_id.h"
#include "tsdrv_log.h"
#include "tsdrv_device.h"
#include "devdrv_cqsq.h"
#define TS_BASE_SPEC_MODULE_ID_NUM          (64)
#define TS_PREMIUM_SPEC_MODULE_ID_NUM       (128)
#define TS_ULTIMATE_SPEC_MODULE_ID_NUM      (256)

struct tsdrv_ts_id_range {
    enum tsdrv_id_type id_type;
    u32 min; /* just used in boot stage */
    u32 max; /* just used in boot stage */
    u32 cur_num; /* maximum id number currently assigned in this environment */
    ka_task_spinlock_t lock;
};

struct tsdrv_ts_id_info {
    u32 tsid;
    struct tsdrv_ts_id_range ts_id_range[TSDRV_MAX_ID];
};

struct tsdrv_dev_id_info {
    u32 devid;
    struct tsdrv_ts_id_info ts_id_info[DEVDRV_MAX_TS_NUM];
} g_dev_id_info[TSDRV_MAX_DAVINCI_NUM];

STATIC struct tsdrv_ts_id_range *tsdrv_get_ts_id_range_l(u32 devid, u32 tsid, enum tsdrv_id_type id_type)
{
    return &g_dev_id_info[devid].ts_id_info[tsid].ts_id_range[id_type];
}

STATIC int tsdrv_id_range_param_check(u32 devid, u32 tsid, enum tsdrv_id_type id_type)
{
    if ((devid >= TSDRV_MAX_DAVINCI_NUM) || (tsid >= DEVDRV_MAX_TS_NUM)) {
        TSDRV_PRINT_ERR("invalid param, devid(%u) tsid(%u)\n", devid, tsid);
        return -ENODEV;
    }
    if (id_type >= TSDRV_MAX_ID) {
        TSDRV_PRINT_ERR("invalid id_type(%d) devid(%u) tsid(%u)\n", (int)id_type, devid, tsid);
        return -EFAULT;
    }
    return 0;
}

static void tsdrv_set_id_cur_num(u32 devid, u32 tsid, enum tsdrv_id_type id_type, u32 num)
{
#ifndef TSDRV_UT
    struct tsdrv_ts_id_range *ts_id_range = NULL;

    ts_id_range = tsdrv_get_ts_id_range_l(devid, tsid, id_type);
    ka_task_spin_lock(&ts_id_range->lock);
    ts_id_range->cur_num = num;
    ka_task_spin_unlock(&ts_id_range->lock);
    TSDRV_PRINT_DEBUG("(devid=%u; tsid=%u; id_type=%d; cur_num=%u)\n", devid, tsid, (unsigned int)id_type, num);
#endif
}

void tsdrv_set_ids_cur_num(u32 devid, u32 tsid)
{
    struct tsdrv_ts_resource *ts_res = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, tsid);
    enum tsdrv_id_type id_type;

    for (id_type = TSDRV_STREAM_ID; id_type < TSDRV_MAX_ID; id_type++) {
        tsdrv_set_id_cur_num(devid, tsid, id_type, ts_res->id_res[id_type].id_capacity);
    }
}

#ifndef CFG_MANAGER_HOST_ENV
STATIC void tsdrv_set_ts_id_range(u32 devid, u32 tsid, enum tsdrv_id_type id_type, u32 min, u32 max)
{
    struct tsdrv_ts_id_range *ts_id_range = NULL;

    ts_id_range = tsdrv_get_ts_id_range_l(devid, tsid, id_type);
    ts_id_range->max = max;
    ts_id_range->min = min;
    ka_task_spin_lock(&ts_id_range->lock);
    ts_id_range->cur_num = max - min;
    ka_task_spin_unlock(&ts_id_range->lock);
}
#endif

int tsdrv_get_ts_id_range(u32 devid, u32 tsid, enum tsdrv_id_type id_type, u32 *min, u32 *max)
{
    struct tsdrv_ts_id_range *ts_id_range = NULL;
    int err;

    err = tsdrv_id_range_param_check(devid, tsid, id_type);
    if (err != 0) {
        return -ENODEV;
    }
    ts_id_range = tsdrv_get_ts_id_range_l(devid, tsid, id_type);
    *max = ts_id_range->max;
    *min = ts_id_range->min;
    return 0;
}

static u32 g_max_id_num[TSDRV_MAX_ID] = {
    DEVDRV_MAX_STREAM_ID,
    DEVDRV_MAX_NOTIFY_ID,
    DEVDRV_MAX_MODEL_ID,
    DEVDRV_MAX_SW_EVENT_ID,
    0,  /* TSDRV_EVENT_HW_ID */
    DEVDRV_MAX_SQ_NUM,
    DEVDRV_MAX_CQ_NUM,
    DEVDRV_MAX_CMO_ID,
};

u32 tsdrv_get_max_id_num(enum tsdrv_id_type id_type)
{
#ifndef TSDRV_UT
    return g_max_id_num[id_type];
#endif
}

u32 tsdrv_get_model_id_max_num(u32 devid, u32 tsid)
{
#if defined(CFG_MANAGER_HOST_ENV) || defined(CFG_SOC_PLATFORM_CLOUD_V2)
    return DEVDRV_MAX_MODEL_ID;
#else
    if (tsdrv_id_range_param_check(devid, tsid, TSDRV_MODEL_ID) != 0) {
        return 0;
    }
    return tsdrv_get_ts_id_range_l(devid, tsid, TSDRV_MODEL_ID)->max;
#endif
}

u32 tsdrv_get_stream_id_max_num(u32 devid, u32 tsid)
{
#if defined(CFG_MANAGER_HOST_ENV) || defined(CFG_SOC_PLATFORM_CLOUD_V2)
    return DEVDRV_MAX_STREAM_ID;
#else
    if (tsdrv_id_range_param_check(devid, tsid, TSDRV_STREAM_ID) != 0) {
        return 0;
    }
    return tsdrv_get_ts_id_range_l(devid, tsid, TSDRV_STREAM_ID)->max;
#endif
}

int tsdrv_inc_ts_id_cur_num(u32 devid, u32 tsid, enum tsdrv_id_type id_type, u32 cur_num)
{
    struct tsdrv_ts_id_range *ts_id_range = NULL;
    int err;

    err = tsdrv_id_range_param_check(devid, tsid, id_type);
    if (err != 0) {
        return -ENODEV;
    }
    ts_id_range = tsdrv_get_ts_id_range_l(devid, tsid, id_type);
    ka_task_spin_lock(&ts_id_range->lock);
    ts_id_range->cur_num += cur_num;
    ka_task_spin_unlock(&ts_id_range->lock);
    return 0;
}

int tsdrv_dec_ts_id_cur_num(u32 devid, u32 tsid, enum tsdrv_id_type id_type, u32 cur_num)
{
    struct tsdrv_ts_id_range *ts_id_range = NULL;
    int err;

    err = tsdrv_id_range_param_check(devid, tsid, id_type);
    if (err != 0) {
        return -ENODEV;
    }
    ts_id_range = tsdrv_get_ts_id_range_l(devid, tsid, id_type);
    ka_task_spin_lock(&ts_id_range->lock);
    ts_id_range->cur_num -= cur_num;
    ka_task_spin_unlock(&ts_id_range->lock);
    return 0;
}

int tsdrv_get_ts_id_cur_num(u32 devid, u32 tsid, enum tsdrv_id_type id_type, u32 *cur_num)
{
    struct tsdrv_ts_id_range *ts_id_range = NULL;
    int err;

    err = tsdrv_id_range_param_check(devid, tsid, id_type);
    if (err != 0) {
        return -ENODEV;
    }
    if (cur_num == NULL) {
        TSDRV_PRINT_ERR("num is NULL, devid(%u) tsid(%u) id_type(%d)\n", devid, tsid, (int)id_type);
        return -ENOMEM;
    }
    ts_id_range = tsdrv_get_ts_id_range_l(devid, tsid, id_type);
    ka_task_spin_lock(&ts_id_range->lock);
    *cur_num = ts_id_range->cur_num;
    ka_task_spin_unlock(&ts_id_range->lock);
    return 0;
}

STATIC void tsdrv_ts_id_range_init(struct tsdrv_ts_id_info *ts_id_info)
{
    int id_type;

    for (id_type = 0; id_type < (int)TSDRV_MAX_ID; id_type++) {
        ts_id_info->ts_id_range[id_type].id_type = (enum tsdrv_id_type)id_type;
        ts_id_info->ts_id_range[id_type].max = 0;
        ts_id_info->ts_id_range[id_type].min = 0;
        ts_id_info->ts_id_range[id_type].cur_num = 0;
        ka_task_spin_lock_init(&ts_id_info->ts_id_range[id_type].lock);
    }
}

STATIC void tsdrv_ts_id_info_init(struct tsdrv_ts_id_info *ts_id_info, u32 ts_num)
{
    u32 tsid;

    for (tsid = 0; tsid < ts_num; tsid++) {
        ts_id_info[tsid].tsid = tsid;
        tsdrv_ts_id_range_init(&ts_id_info[tsid]);
    }
}

static void tsdrv_dev_show_id_num(u32 devid, u32 tsid)
{
    struct tsdrv_ts_id_range *id_range = NULL;
    u32 i;

    id_range = g_dev_id_info[devid].ts_id_info[tsid].ts_id_range;

    TSDRV_PRINT_DEBUG("init resource: devid[%u] tsid[%u].\n", devid, tsid);
    for (i = 0; i < TSDRV_MAX_ID; ++i) {
        TSDRV_PRINT_DEBUG("id: %u, min[%u] max[%u] cur_num[%u]\n",
            i, id_range[i].min, id_range[i].max, id_range[i].cur_num);
    }
}

void tsdrv_dev_id_info_init(void)
{
    u32 devid;

    for (devid = 0; devid < TSDRV_MAX_DAVINCI_NUM; devid++) {
        g_dev_id_info[devid].devid = devid;
        tsdrv_ts_id_info_init(g_dev_id_info[devid].ts_id_info, DEVDRV_MAX_TS_NUM);
    }
}

#ifndef CFG_MANAGER_HOST_ENV
#if defined(CFG_SOC_PLATFORM_MINI) && !defined(CFG_SOC_PLATFORM_MINIV2) && !defined(CFG_SOC_PLATFORM_MINIV3)
#else
STATIC void tsdrv_set_id_by_ts_spec(struct devdrv_info *dev_info, u32 tsid, enum tsdrv_id_type id_type, u32 default_max)
{
    u32 max_id_num = 0;

#ifdef CFG_SOC_PLATFORM_MDC_V51
    switch (dev_info->pdata->ts_spec) {
        case HWTS_SPEC_BASE:
            max_id_num = TS_BASE_SPEC_MODULE_ID_NUM;
            break;
        case HWTS_SPEC_PREMIUM:
            max_id_num = TS_PREMIUM_SPEC_MODULE_ID_NUM;
            break;
        case HWTS_SPEC_ULTIMATE:
            max_id_num = TS_ULTIMATE_SPEC_MODULE_ID_NUM;
            break;
        default:
            break;
    }
#else
    max_id_num = default_max;
#endif
    tsdrv_set_ts_id_range(dev_info->dev_id, tsid, id_type, 0, max_id_num);
}
#endif
#endif

STATIC void tsdrv_set_ts_ids_range(struct devdrv_info *dev_info, u32 tsid)
{
    u32 devid = dev_info->dev_id;

#ifndef CFG_MANAGER_HOST_ENV
#if defined(CFG_SOC_PLATFORM_MINI) && !defined(CFG_SOC_PLATFORM_MINIV2) && !defined(CFG_SOC_PLATFORM_MINIV3)
    tsdrv_set_ts_id_range(devid, tsid, TSDRV_STREAM_ID, 0, DEVDRV_MAX_STREAM_ID);
    tsdrv_set_ts_id_range(devid, tsid, TSDRV_MODEL_ID, 0, DEVDRV_MAX_MODEL_ID);
    tsdrv_set_ts_id_range(devid, tsid, TSDRV_EVENT_HW_ID, 0, DEVDRV_MAX_HW_EVENT_ID);
    tsdrv_set_ts_id_range(devid, tsid, TSDRV_SQ_ID, 0, DEVDRV_MAX_SQ_NUM);
    tsdrv_set_ts_id_range(devid, tsid, TSDRV_CQ_ID, 0, DEVDRV_MAX_CQ_NUM);
#else
#if defined(CFG_SOC_PLATFORM_MINIV3) || defined(CFG_SOC_PLATFORM_CLOUD_V2)
    tsdrv_set_ts_id_range(devid, tsid, TSDRV_STREAM_ID, 0, DEVDRV_MAX_STREAM_ID);
#else
    tsdrv_set_id_by_ts_spec(dev_info, tsid, TSDRV_STREAM_ID, DEVDRV_MAX_STREAM_ID);
#endif
    tsdrv_set_ts_id_range(devid, tsid, TSDRV_EVENT_HW_ID, DEVDRV_MIN_HW_EVENT_ID, DEVDRV_MAX_HW_EVENT_ID);
    tsdrv_set_ts_id_range(devid, tsid, TSDRV_EVENT_SW_ID, DEVDRV_MAX_HW_EVENT_ID + 1, DEVDRV_MAX_SW_EVENT_ID);
    tsdrv_set_ts_id_range(devid, tsid, TSDRV_NOTIFY_ID, 0, DEVDRV_MAX_NOTIFY_ID);
    tsdrv_set_id_by_ts_spec(dev_info, tsid, TSDRV_MODEL_ID, DEVDRV_MAX_MODEL_ID);
#ifdef CFG_FEATURE_CMO
    tsdrv_set_ts_id_range(devid, tsid, TSDRV_CMO_ID, 1, DEVDRV_MAX_CMO_ID);
#else
    tsdrv_set_ts_id_range(devid, tsid, TSDRV_CMO_ID, 0, 0);
#endif
    tsdrv_set_ts_id_range(devid, tsid, TSDRV_SQ_ID, 0, DEVDRV_MAX_SQ_NUM);
    tsdrv_set_ts_id_range(devid, tsid, TSDRV_CQ_ID, 0, DEVDRV_MAX_CQ_NUM);
#endif
#else
    tsdrv_ts_id_range_init(&g_dev_id_info[devid].ts_id_info[tsid]);
#endif
}

/* temporary */
void tsdrv_dev_set_id_range(struct devdrv_info *dev_info, u32 tsnum)
{
    u32 tsid;

    for (tsid = 0; tsid < tsnum; tsid++) {
        tsdrv_set_ts_ids_range(dev_info, tsid);
        tsdrv_dev_show_id_num(dev_info->dev_id, tsid);
    }
}

