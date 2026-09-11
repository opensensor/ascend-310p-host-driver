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

#include <linux/errno.h>
#include <linux/slab.h>
#include "dms_define.h"
#include "dms_event.h"
#include "dms_event_distribute.h"
#include "dms_event_converge.h"
#include "dms_event_distribute_proc.h"
#include "dms_event_bbox_map.h"
#include "dms_kernel_version_adapt.h"
#include "kernel_version_adapt.h"
#include "ka_kernel_def_pub.h"
static add_exception_handle g_add_exception_func = NULL;

#if (!defined AOS_LLVM_BUILD) && (!defined CFG_FEATURE_NEW_EVENT_CODE)
static struct dms_event_bbox_map g_event_bbox_map[] = {
    {
        .event_id = 0x80E01801U,
        .assertion = DMS_EVENT_TYPE_OCCUR,
        .bbox_code = 0xA8028802U
    },
    {
        .event_id = 0x80E01801U,
        .assertion = DMS_EVENT_TYPE_RESUME,
        .bbox_code = 0x98028802U
    },
    {
        .event_id = 0x80E18402U,
        .assertion = DMS_EVENT_TYPE_OCCUR,
        .bbox_code = 0xA8028801U
    },
    {
        .event_id = 0x80E18401U,
        .assertion = DMS_EVENT_TYPE_OCCUR,
        .bbox_code = 0xA4028801U
    },
#ifndef CFG_SOC_PLATFORM_MINIV2
    {
        .event_id = 0x80fa4e00U,
        .assertion = DMS_EVENT_TYPE_OCCUR,
        .bbox_code = 0xA607FFFFU
    },
#endif
    {
        .event_id = 0x40F84E00U,
        .assertion = DMS_EVENT_TYPE_OCCUR,
        .bbox_code = DMS_DEVICE_HEAT_BEAT_LOST
    },
    {
        .event_id = 0x8C0A4E00U,
        .assertion = DMS_EVENT_TYPE_OCCUR,
        .bbox_code = 0xA4025081U /* dmp_daemon heartbeat lost */
    },
    {
        .event_id = 0x8C0A4E00U,
        .assertion = DMS_EVENT_TYPE_RESUME,
        .bbox_code = 0x94025081U /* dmp_daemon heartbeat recover */
    },
    {
        .event_id = 0x8C204E00U,
        .assertion = DMS_EVENT_TYPE_OCCUR,
        .bbox_code = 0xA60250A1U /* tsd_daemon heartbeat lost */
    },
    {
        .event_id = 0x8C204E00U,
        .assertion = DMS_EVENT_TYPE_RESUME,
        .bbox_code = 0x960250A1U /* tsd_daemon heartbeat recover */
    },
    {
        .event_id = 0x8C084E00U,
        .assertion = DMS_EVENT_TYPE_OCCUR,
        .bbox_code = 0xA4025021U /* adda heartbeat lost */
    },
    {
        .event_id = 0x8C084E00U,
        .assertion = DMS_EVENT_TYPE_RESUME,
        .bbox_code = 0x94025021U /* adda heartbeat recover */
    },
    {
        .event_id = 0x8C0E4E00U,
        .assertion = DMS_EVENT_TYPE_OCCUR,
        .bbox_code = 0xA4025041U /* slogd heartbeat lost */
    },
    {
        .event_id = 0x8C0E4E00U,
        .assertion = DMS_EVENT_TYPE_RESUME,
        .bbox_code = 0x94025041U /* slogd heartbeat recover */
    },
    {
        .event_id = 0x8C104E00U,
        .assertion = DMS_EVENT_TYPE_OCCUR,
        .bbox_code = 0xA40250E1U /* log-daemon heartbeat lost */
    },
    {
        .event_id = 0x8C104E00U,
        .assertion = DMS_EVENT_TYPE_RESUME,
        .bbox_code = 0x940250E1U /* log-daemon heartbeat recover */
    },
    {
        .event_id = 0x8C0C4E00U,
        .assertion = DMS_EVENT_TYPE_OCCUR,
        .bbox_code = 0xA4025061U /* sklogd heartbeat lost */
    },
    {
        .event_id = 0x8C0C4E00U,
        .assertion = DMS_EVENT_TYPE_RESUME,
        .bbox_code = 0x94025061U /* sklogd heartbeat recover */
    },
    {
        .event_id = 0x8C124E00U,
        .assertion = DMS_EVENT_TYPE_OCCUR,
        .bbox_code = 0xA4025101U /* hdcd heartbeat lost */
    },
    {
        .event_id = 0x8C124E00U,
        .assertion = DMS_EVENT_TYPE_RESUME,
        .bbox_code = 0x94025101U /* hdcd heartbeat recover */
    }
};

static struct dms_event_bbox_map *dms_event_bbox_map(DMS_EVENT_NODE_STRU *exception_node)
{
    unsigned int map_size, i;

    map_size = sizeof(g_event_bbox_map) / sizeof(struct dms_event_bbox_map);

    for (i = 0; i < map_size; i++) {
        if ((exception_node->event.event_id == g_event_bbox_map[i].event_id) &&
            (exception_node->event.assertion == g_event_bbox_map[i].assertion)) {
            return &g_event_bbox_map[i];
        }
    }

    return NULL;
}

static int dms_event_call_add_exception_handle(u32 devid, u32 code)
{
    struct timespec stamp = current_kernel_time();
    if (g_add_exception_func != NULL) {
        return g_add_exception_func(devid, code, stamp);
    }
    return 0;
}
#endif

int dms_event_set_add_exception_handle(add_exception_handle func)
{
    g_add_exception_func = func;
    return 0;
}
KA_EXPORT_SYMBOL(dms_event_set_add_exception_handle);

static int dms_event_distribute_to_bbox(DMS_EVENT_NODE_STRU *exception_node)
{
#if (!defined AOS_LLVM_BUILD) && (!defined CFG_FEATURE_NEW_EVENT_CODE)
    struct dms_event_bbox_map *bbox_map = NULL;
    int ret = DRV_ERROR_NONE;

#ifdef CFG_HOST_ENV
    if (!DMS_EVENT_CODE_IS_HOST(exception_node->event.event_id)) {
        return ret;
    }
#else
    if (!DMS_EVENT_CODE_IS_DEVICE(exception_node->event.event_id)) {
        return ret;
    }
#endif
    bbox_map = dms_event_bbox_map(exception_node);
    if (bbox_map == NULL) {
#ifdef CFG_HOST_ENV
        dms_warn("Unsupport bbox event_code. (dev_id=%u; event_code=0x%x)\n",
            exception_node->event.deviceid, exception_node->event.event_code);
#endif
        return ret;
    }
    ret = dms_event_call_add_exception_handle(exception_node->event.deviceid, bbox_map->bbox_code);
    if (ret) {
        dms_err("Add event_id to bbox failed. (dev_id=%u; event_code=0x%x; ret=%d)\n",
            exception_node->event.deviceid, exception_node->event.event_code, ret);
        return ret;
    }
    dms_event("Add event_id to bbox. (dev_id=%u; event_id=0x%x;bbox_code=0x%x)\n",
        exception_node->event.deviceid, exception_node->event.event_id, bbox_map->bbox_code);
#endif
    return DRV_ERROR_NONE;
}

STATIC int dms_event_add_report_to_dfx(DMS_EVENT_NODE_STRU *exception_node)
{
    struct dms_event_dfx_table *dfx_table = NULL;
    struct dms_dev_ctrl_block *dev_cb = NULL;

    dev_cb = dms_get_dev_cb(exception_node->event.deviceid);
    if (dev_cb == NULL) {
        dms_err("Get dev_ctrl block failed. (dev_id=%u)\n", exception_node->event.deviceid);
        return DRV_ERROR_NO_DEVICE;
    }

    dfx_table = &dev_cb->dev_event_cb.dfx_table;
    if (exception_node->event.assertion < DMS_EVENT_TYPE_MAX) {
        atomic_inc(&dfx_table->report_to_consumer[exception_node->event.assertion]);
        return DRV_ERROR_NONE;
    }

    dms_err("Invalid parameter. (event_assertion=%u)\n", exception_node->event.assertion);
    return DRV_ERROR_PARA_ERROR;
}

int dms_event_distribute_handle_init(void)
{
    int ret_bbox, ret_process, ret_dfx;

    ret_bbox = dms_event_subscribe_register(dms_event_distribute_to_bbox, DMS_DISTRIBUTE_PRIORITY0);
    ret_process = dms_event_subscribe_register(dms_event_distribute_to_process, DMS_DISTRIBUTE_PRIORITY2);
    ret_dfx = dms_event_subscribe_register(dms_event_add_report_to_dfx, DMS_DISTRIBUTE_PRIORITY3);
    if (ret_bbox || ret_process || ret_dfx) {
        dms_err("Distribute handle register failed. "
                "(bbox=%d; process=%d; ret_dfx=%d)\n",
                ret_bbox, ret_process, ret_dfx);
        dms_event_subscribe_unregister_all();
        return DRV_ERROR_INNER_ERR;
    }

    return DRV_ERROR_NONE;
}

