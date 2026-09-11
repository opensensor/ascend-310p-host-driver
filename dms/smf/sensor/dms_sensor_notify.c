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

#include "ka_task_pub.h"
#include "ka_kernel_def_pub.h"

#include "dms_sensor.h"
#include "dms_define.h"
#include "dms_sensor_notify.h"

struct dms_sensor_notify_queue g_sensor_notify_queue; /* device0  hbm ddr soc */

void dms_sensor_notify_init(void)
{
    g_sensor_notify_queue.head = 0;
    g_sensor_notify_queue.tail = 0;
    g_sensor_notify_queue.size = DMS_MAX_NOTIFY_SENSOR_OBJ_NUM;
    g_sensor_notify_queue.count = 0;
    ka_task_spin_lock_init(&g_sensor_notify_queue.notify_lock);
    return;
}

void dms_sensor_notify_exit(void)
{
    return;
}

void dms_sensor_notify_count_clear(void)
{
    g_sensor_notify_queue.count = 0;
}

static int dms_sensor_add_notify_event(unsigned int dev_id, struct dms_sensor_object_cfg *psensor_obj_cfg)
{
    unsigned long flags;
    struct dms_sensor_notify_queue *notify_queue = &g_sensor_notify_queue;

    /* multiple producer add notify event at tail, need notify_lock for notify_queue->tail */
    spin_lock_irqsave(&notify_queue->notify_lock, flags);
    if ((notify_queue->tail + 1) % notify_queue->size == notify_queue->head) {
        spin_unlock_irqrestore(&notify_queue->notify_lock, flags);
        dms_err("notify queue is full, (dev_id=%u; sensor_type=%u; sensor_name=%.*s; size=%u; head=%u; tail=%u).\n",
            dev_id, psensor_obj_cfg->sensor_type, DMS_SENSOR_DESCRIPT_LENGTH, psensor_obj_cfg->sensor_name,
            notify_queue->size, notify_queue->head, notify_queue->tail);
        return -ENODATA;
    }

    notify_queue->sensor_obj_queue[notify_queue->tail].notify_sensor_obj = *psensor_obj_cfg;
    notify_queue->sensor_obj_queue[notify_queue->tail].dev_id = dev_id;
    notify_queue->tail = (notify_queue->tail + 1) % notify_queue->size;
    spin_unlock_irqrestore(&notify_queue->notify_lock, flags);

    return 0;
}

static int dms_sensor_scan_notify_sensor_obj(struct dms_dev_sensor_cb *dev_sensor_cb,
    struct dms_node_sensor_cb *node_sensor_cb, struct dms_sensor_object_cfg *psensor_obj_cfg)
{
    struct dms_sensor_object_cb *psensor_obj_cb = NULL;
    struct dms_sensor_object_cb *tmp_sensor_ctl = NULL;
    unsigned int result;
    struct dms_sensor_scan_time_recorder ptime_recorder = {0};

    ptime_recorder.record_scan_time_flag = DMS_SENSOR_CHECK_NOT_RECORD;
    list_for_each_entry_safe(psensor_obj_cb, tmp_sensor_ctl, &(node_sensor_cb->sensor_object_table), list) {
        if ((psensor_obj_cb->sensor_object_cfg.sensor_type != psensor_obj_cfg->sensor_type) ||
            (strcmp(psensor_obj_cb->sensor_object_cfg.sensor_name, psensor_obj_cfg->sensor_name) != 0) ||
            (psensor_obj_cb->sensor_object_cfg.private_data != psensor_obj_cfg->private_data)) {
            continue;
        }
        result = dms_sensor_scan_one_node_object(dev_sensor_cb, node_sensor_cb, psensor_obj_cb, &ptime_recorder);
        if (result != 0) {
            dms_err("Scan one node object failed, (result=%u)\n", result);
        }
        /* update node health */
        node_sensor_cb->health = (psensor_obj_cb->fault_status > 0) ? (psensor_obj_cb->fault_status) : 0;
#if (defined(CFG_SOC_PLATFORM_MDC_V51) || defined(CFG_SOC_PLATFORM_MDC_V11)) && (!defined(DRV_SOC_MISC_UT))
        node_sensor_cb->owner_node->state = node_sensor_cb->health;
#endif
        return 0;
    }

    return -1;
}

static void dms_sensor_scan_notify_sensor(struct dms_dev_sensor_cb *dev_sensor_cb,
    struct dms_sensor_object_cfg *psensor_obj_cfg, unsigned int scan_mode)
{
    struct dms_node_sensor_cb *node_sensor_cb = NULL;
    struct dms_node_sensor_cb *tmp_ctl = NULL;
    unsigned short dev_health = dev_sensor_cb->health;

    if (scan_mode == DMS_SERSOR_SCAN_NOTIFY) {
        mutex_lock(&dev_sensor_cb->dms_sensor_mutex);
    }
    list_for_each_entry_safe(node_sensor_cb, tmp_ctl, &(dev_sensor_cb->dms_node_sensor_cb_list), list)
    {
        if (node_sensor_cb->sensor_object_num == 0) {
            dms_warn("not sensor type and obj, (nodeid = 0x%x)\n", node_sensor_cb->node_id);
            continue;
        }
        /* From user mode, but the process PID is invalid */
        if ((node_sensor_cb->env_type == DMS_SENSOR_ENV_USER_SPACE) && (node_sensor_cb->pid <= 0)) {
            dms_err("env error, (nodeid=0x%x; pid=%d)\n", node_sensor_cb->env_type, node_sensor_cb->pid);
            continue;
        }
        if (dms_sensor_scan_notify_sensor_obj(dev_sensor_cb, node_sensor_cb, psensor_obj_cfg) == 0) {
            dev_health = dev_health > node_sensor_cb->health ? dev_health : node_sensor_cb->health;
            break;
        }
    }
    dev_sensor_cb->health = dev_health;
    if (scan_mode == DMS_SERSOR_SCAN_NOTIFY) {
        mutex_unlock(&dev_sensor_cb->dms_sensor_mutex);
    }
}

/* note: there is no lock design for g_sensor_notify_queue.
 * only one consumer get notify event at head, don't need notify_lock;
 * sometime need support multiple consumer or threads, you must redesign here.
*/
static void dms_sensor_report_notify_event(unsigned int scan_mode)
{
    struct dms_dev_ctrl_block *dev_ctrl = NULL;
    struct dms_sensor_object_cfg *psensor_obj_cfg = NULL;
    struct dms_sensor_notify_queue *notify_queue = &g_sensor_notify_queue;

    while (notify_queue->head != notify_queue->tail) {
        dev_ctrl = dms_get_dev_cb(notify_queue->sensor_obj_queue[notify_queue->head].dev_id);
        if (dev_ctrl == NULL) {
            dms_err("dev_ctrl NULL, (dev_id=%u)\n", notify_queue->sensor_obj_queue[notify_queue->head].dev_id);
            continue;
        }
        psensor_obj_cfg = &notify_queue->sensor_obj_queue[notify_queue->head].notify_sensor_obj;
        dms_sensor_scan_notify_sensor(&dev_ctrl->dev_sensor_cb, psensor_obj_cfg, scan_mode);
        notify_queue->head = (notify_queue->head + 1) % notify_queue->size;

        if (notify_queue->count++ >= notify_queue->size) {
            msleep(1); /* sleep 1 ms to release cpu */
            dms_sensor_notify_count_clear();
        }
    }

    return;
}

void dms_sensor_notify_event_proc(unsigned int scan_mode)
{
    dms_sensor_report_notify_event(scan_mode);
}

int dms_sensor_event_notify(unsigned int dev_id, struct dms_sensor_object_cfg *psensor_obj_cfg)
{
    int ret;

    struct dms_system_ctrl_block *sys_cb = NULL;

    if ((psensor_obj_cfg == NULL) || (dms_check_device_id(dev_id) != 0)) {
        dms_err("invalid para, (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    ret = dms_sensor_add_notify_event(dev_id, psensor_obj_cfg);
    if (ret != 0) {
        dms_err("add notify event failed, (dev_id=%u; sensor_name=%.*s; ret=%d)\n",
            dev_id, DMS_SENSOR_DESCRIPT_LENGTH, psensor_obj_cfg->sensor_name, ret);
        return ret;
    }

    sys_cb = dms_get_sys_ctrl_cb();
    if (sys_cb == NULL) {
        dms_err("sys_cb NULL, (dev_id=%u; sensor_name=%.*s)\n", dev_id,
            DMS_SENSOR_DESCRIPT_LENGTH, psensor_obj_cfg->sensor_name);
        return -EINVAL;
    }

    sys_cb->sensor_scan_task_state = true;
    wake_up(&sys_cb->sensor_scan_wait);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(dms_sensor_event_notify);
