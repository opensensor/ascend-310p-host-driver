/*
* Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
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
* Create: 2022-11-14
*/

#include <linux/kthread.h>
#include <linux/delay.h>

#include "ka_system_pub.h"

#include "dms_ts_info.h"
#include "dms_ts_scan.h"
#include "dms_timer.h"
#include "devdrv_user_common.h"
#include "dms_cmd_def.h"
#include "dms_ipc_interface.h"
#include "devdrv_manager_common.h"

#define TS_TIMER_INVALID 0xffff

#define SCAN_SWITCH 1

STATIC unsigned int g_timer_task_id = TS_TIMER_INVALID;

STATIC int ts_scan_period_check(struct ts_retry_cycle *ctrl)
{
    if (ctrl->remain_cnt != 0) {
        ctrl->remain_cnt--;
        return -EINVAL;
    }

    return 0;
}

STATIC void ts_scan_period_update(struct ts_retry_cycle *ctrl, int result, u32 *value)
{
    ka_ktime_t time_current = 0;
    long times_gap = 0;

    if (result == 0) {
        ctrl->remain_cnt = 0;
        ctrl->interval_cnt = 0;
        ctrl->prev_fail_time = 0;
        return;
    }

    *value = SCAN_VALUE_INVALID;

    time_current = ka_system_ktime_get();

    if (ka_system_ktime_to_ns(ctrl->prev_fail_time) != 0) {
        times_gap = (long)(ka_system_ktime_to_ns(time_current) - ka_system_ktime_to_ns(ctrl->prev_fail_time)) / KA_NSEC_PER_MSEC;
    }

    /* Stop when the interval exceeds 1 minute. The maximum interval is within 1 to 2 minutes. */
    if (times_gap < TS_MAX_INTERNEL_TIME) {
        ctrl->interval_cnt = ctrl->interval_cnt == 0 ? 1 : ctrl->interval_cnt * 2; /* 2: x=x*2 */
    }
    ctrl->prev_fail_time = time_current;
    ctrl->remain_cnt = ctrl->interval_cnt;
}

STATIC struct ts_scan_list g_ts_scan_list[] = {
    {ts_get_ai_utilization, DMS_SUBCMD_GET_AI_INFO_FROM_TS, LPM_AICORE0_ID, {0}, {{0}}, SCAN_SWITCH},
};

struct ts_scan_list *find_operate_ts_info_str(u32 type_id, u32 core_id)
{
    unsigned int i;
    unsigned int table_size = sizeof(g_ts_scan_list) / sizeof(struct ts_scan_list);

    for (i = 0; i < table_size; i++) {
        if ((type_id == g_ts_scan_list[i].type_id) && (core_id == g_ts_scan_list[i].core_id)) {
            return &g_ts_scan_list[i];
        }
    }

    return NULL;
}

STATIC void ts_get_aicore_utilization(unsigned int index, unsigned int devid)
{
    int ret;
    unsigned int i = index;
    unsigned int dev_id = devid;
    struct ts_retry_cycle *ctrl = NULL;

    ctrl = &g_ts_scan_list[i].period_ctrl[dev_id];
    if ((g_ts_scan_list[i].scan_flag != 0) && (ts_scan_period_check(ctrl) == 0)) {
        ret = g_ts_scan_list[i].get_value_handle(dev_id, 0, g_ts_scan_list[i].core_id,
            &g_ts_scan_list[i].pf_value[dev_id]);
        ts_scan_period_update(ctrl, ret, &g_ts_scan_list[i].pf_value[dev_id]);
    }

    return;
}

STATIC int ts_scan_info(u64 arg)
{
    int ret;
    unsigned int i, dev_id;
    unsigned int num_dev = 0;
    unsigned int table_size = sizeof(g_ts_scan_list) / sizeof(struct ts_scan_list);
    struct devdrv_info *dev_info = NULL;

    ret = devdrv_get_devnum(&num_dev);
    if (ret != 0) {
        dms_err("Get device num failed. (ret=%d)\n", ret);
        return -1;
    }

    for (i = 0; i < table_size; i++) {
        for (dev_id = 0; (dev_id < num_dev) && (dev_id < TS_DEVICE_NUM_MAX); dev_id++) {
            dev_info = devdrv_manager_get_devdrv_info(dev_id);
            if ((dev_info != NULL) && (dev_info->dmp_started)) {
                ts_get_aicore_utilization(i, dev_id);
            }
            msleep(TS_SCAN_DELAY);
        }
    }

    return 0;
}

int ts_scan_init(void)
{
    int ret;
    struct dms_timer_task ts_scan_task = {0};

    ts_scan_task.expire_ms = TS_SCAN_INFO_CYCLE;
    ts_scan_task.handler_mode = COMMON_WORK;
    ts_scan_task.exec_task = ts_scan_info;

    ret = dms_timer_task_register(&ts_scan_task, &g_timer_task_id);
    if (ret) {
        dms_err("Dms timer task register failed. (ret=%d) \n", ret);
        return ret;
    }

    return 0;
}

int ts_scan_uninit(void)
{
    int ret;

    ret = dms_timer_task_unregister(g_timer_task_id);
    if (ret) {
        dms_err("Dms timer task unregister failed. (ret=%d) \n", ret);
        return ret;
    }

    return 0;
}