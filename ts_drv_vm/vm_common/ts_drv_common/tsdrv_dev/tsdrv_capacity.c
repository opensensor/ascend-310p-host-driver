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
#include <linux/bitmap.h>
#include "devdrv_interface.h"
#include "devdrv_devinit.h"
#include "tsdrv_capacity.h"
#include "tsdrv_device.h"
#include "devdrv_cb.h"
#include "tsdrv_common.h"

#include "hvtsdrv_tsagent.h"
#include "tsdrv_id.h"

int tsdrv_set_ts_capacity(struct tsdrv_ts_resource *ts_res, enum TSDRV_CAPACITY capacity)
{
    struct tsdrv_device *tsdrv_dev = tsdrv_res_to_dev(ts_res);
    struct tsdrv_dev_resource *dev_res = ka_container_of(ts_res, struct tsdrv_dev_resource,
        ts_resource[ts_res->tsid]);
    struct tsdrv_id_capacity id_cap = {0};
    u32 aicore_num = 32; /* inited */
    u32 tsid, id_type, cap_num;
    int ret;

    /* default value */
    dev_res->aicore_num = aicore_num;
    dev_res->cap_num = aicore_num;

#ifndef AOS_LLVM_BUILD
    if ((capacity != FULL_CAPACITY) && (capacity != INVALID_CAPACITY)) {
        ret = hvdevmng_get_aicore_num(tsdrv_dev->devid, TSDRV_PM_FID, &aicore_num);
        if ((ret != 0) || (aicore_num == 0)) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("(devid=%u; fid=%u; ret=%d; aicore_num=%u)\n",
                tsdrv_dev->devid, dev_res->fid, ret, aicore_num);
            return -EINVAL;
#endif
        }
    }
#endif
    for (tsid = 0; tsid < tsdrv_get_dev_tsnum(tsdrv_dev->devid); tsid++) {
        if (capacity == FULL_CAPACITY) {
            cap_num = aicore_num;
        } else {
            cap_num = 1U << (u32)capacity;
        }
        if (capacity == INVALID_CAPACITY) {
            for (id_type = TSDRV_STREAM_ID; id_type < TSDRV_MAX_ID; id_type++) {
                ts_res[tsid].id_res[id_type].id_capacity = TSDRV_INVALIC_CAPACITY;
            }
            continue;
        }
        TSDRV_PRINT_INFO("(devid=%u; fid=%u; cap_num=%u; aicore_num=%u)\n", tsdrv_dev->devid, dev_res->fid,
            cap_num, aicore_num);
        dev_res->aicore_num = aicore_num;
        dev_res->cap_num = cap_num;

        ts_res[tsid].id_res[TSDRV_SQ_ID].id_capacity = DEVDRV_MAX_SQ_NUM * cap_num / aicore_num;
        ts_res[tsid].id_res[TSDRV_CQ_ID].id_capacity =
            (DEVDRV_MAX_CQ_NUM - CALLBACK_MAX_CQ_NUM) * cap_num / aicore_num;
        ts_res[tsid].id_res[TSDRV_STREAM_ID].id_capacity =
            tsdrv_get_stream_id_max_num(tsdrv_dev->devid, tsid) * cap_num / aicore_num;
        ts_res[tsid].id_res[TSDRV_EVENT_HW_ID].id_capacity =
            (DEVDRV_MAX_HW_EVENT_ID - DEVDRV_MIN_HW_EVENT_ID) * cap_num / aicore_num;
        ts_res[tsid].id_res[TSDRV_EVENT_SW_ID].id_capacity =
            (DEVDRV_MAX_SW_EVENT_ID - DEVDRV_MAX_HW_EVENT_ID - 1) * cap_num / aicore_num;
        ts_res[tsid].id_res[TSDRV_MODEL_ID].id_capacity =
            tsdrv_get_model_id_max_num(tsdrv_dev->devid, tsid) * cap_num / aicore_num;
#if defined(CFG_SOC_PLATFORM_MINI) && !defined(CFG_SOC_PLATFORM_MINIV2) && !defined(CFG_SOC_PLATFORM_MINIV3)
        ts_res[tsid].id_res[TSDRV_NOTIFY_ID].id_capacity = 0;
#else
        ts_res[tsid].id_res[TSDRV_NOTIFY_ID].id_capacity = DEVDRV_MAX_NOTIFY_ID * cap_num / aicore_num;
#endif
#ifdef CFG_FEATURE_CMO
        ts_res[tsid].id_res[TSDRV_CMO_ID].id_capacity = (DEVDRV_MAX_CMO_ID - 1) * cap_num / aicore_num;
#else
        ts_res[tsid].id_res[TSDRV_CMO_ID].id_capacity = 0;
#endif

        TSDRV_PRINT_INFO("sq(%u), cq(%u), stream(%u), event hw(%u), event sw(%u), model(%u), notify(%u),"
            "cmo(%u).\n",
            ts_res[tsid].id_res[TSDRV_SQ_ID].id_capacity,
            ts_res[tsid].id_res[TSDRV_CQ_ID].id_capacity,
            ts_res[tsid].id_res[TSDRV_STREAM_ID].id_capacity,
            ts_res[tsid].id_res[TSDRV_EVENT_HW_ID].id_capacity,
            ts_res[tsid].id_res[TSDRV_EVENT_SW_ID].id_capacity,
            ts_res[tsid].id_res[TSDRV_MODEL_ID].id_capacity,
            ts_res[tsid].id_res[TSDRV_NOTIFY_ID].id_capacity,
            ts_res[tsid].id_res[TSDRV_CMO_ID].id_capacity);

        id_cap.stream_capacity = ts_res[tsid].id_res[TSDRV_STREAM_ID].id_capacity;
        id_cap.event_capacity = ts_res[tsid].id_res[TSDRV_EVENT_SW_ID].id_capacity +
            ts_res[tsid].id_res[TSDRV_EVENT_HW_ID].id_capacity;
        id_cap.notify_capacity = ts_res[tsid].id_res[TSDRV_NOTIFY_ID].id_capacity;
        id_cap.model_capacity = ts_res[tsid].id_res[TSDRV_MODEL_ID].id_capacity;
#ifndef AOS_LLVM_BUILD
        hvdevmng_set_dev_ts_resource(tsdrv_dev->devid, dev_res->fid, tsid, (void *)&id_cap);
#endif
    }
    return 0;
}

int tsdrv_get_ts_capacity(struct tsdrv_ts_resource *ts_res, struct tsdrv_id_capacity *capacity)
{
    capacity->stream_capacity = ts_res->id_res[TSDRV_STREAM_ID].id_capacity;
    capacity->event_capacity  = ts_res->id_res[TSDRV_EVENT_SW_ID].id_capacity +
        ts_res->id_res[TSDRV_EVENT_HW_ID].id_capacity;
    capacity->model_capacity  = ts_res->id_res[TSDRV_MODEL_ID].id_capacity;
    capacity->notify_capacity = ts_res->id_res[TSDRV_NOTIFY_ID].id_capacity;
    capacity->sq_capacity     = ts_res->id_res[TSDRV_SQ_ID].id_capacity;
    capacity->cq_capacity     = ts_res->id_res[TSDRV_CQ_ID].id_capacity;
    capacity->cmo_capacity    = ts_res->id_res[TSDRV_CMO_ID].id_capacity;

    TSDRV_PRINT_DEBUG("[devid-%u tsid-%u fid-%u] capacity:stream(%u),event(%u),model(%u),notify(%u),sq(%u),cq(%u),"
        "cmo(%u)\n",
        tsdrv_get_devid_by_res(ts_res), ts_res->tsid, tsdrv_get_fid_by_res(ts_res),
        capacity->stream_capacity,
        capacity->event_capacity,
        capacity->model_capacity,
        capacity->notify_capacity,
        capacity->sq_capacity,
        capacity->cq_capacity,
        capacity->cmo_capacity);
    return 0;
}

