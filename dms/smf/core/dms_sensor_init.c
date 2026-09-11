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

#include <linux/slab.h>
#include <linux/version.h>
#include <linux/kthread.h>
#include <linux/timer.h>
#include <linux/delay.h>

#include "ka_system_pub.h"
#include "ka_task_pub.h"
#include "ka_memory_pub.h"

#include "dms_kernel_version_adapt.h"
#include "dms_define.h"
#include "dms_interface.h"
#include "dms_sensor_notify.h"
#include "dms_sensor.h"
#include "kernel_version_adapt.h"
#include "dbl_mem_alloc_interface.h"
#ifndef CFG_HOST_ENV
#include "timer_affinity.h"
#endif
#ifdef CFG_SOC_PLATFORM_MDC_V51
#include "kthread_affinity.h"
#endif
#include "dms_template.h"
#include "dms_cmd_def.h"
#include "urd_acc_ctrl.h"
#include "devdrv_pm.h"
#include "dms_event.h"
#include "dms_sensor_init.h"
#include "ka_kernel_def_pub.h"

INIT_MODULE_FUNC(DMS_SENSOR_CMD_NAME);
EXIT_MODULE_FUNC(DMS_SENSOR_CMD_NAME);
BEGIN_DMS_MODULE_DECLARATION(DMS_SENSOR_CMD_NAME)
BEGIN_FEATURE_COMMAND()
#if defined(CFG_SOC_PLATFORM_MDC_V51) || defined(CFG_SOC_PLATFORM_MDC_V11)
ADD_FEATURE_COMMAND(DMS_SENSOR_CMD_NAME, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_INJECT_FAULT, NULL, "dmp_daemon",
                    DMS_SUPPORT_ALL, dms_sensor_inject_fault)
ADD_FEATURE_COMMAND(DMS_SENSOR_CMD_NAME, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_FAULT_INJECT_INFO, NULL, NULL,
                    DMS_SUPPORT_ALL, dms_sensor_get_fault_inject_info)
#endif
END_FEATURE_COMMAND()
END_MODULE_DECLARATION()

#define SLEEP_1MS 1
static int dms_sensor_scan_task(void *arg)
{
    int i;
    int ret = 0;
    unsigned long timeout, default_timeout;
    ka_ktime_t time_notify_start, time_notify_end;
    struct dms_dev_ctrl_block *dev_ctrl = NULL;
    struct dms_system_ctrl_block *sys_cb = NULL;
    sys_cb = dms_get_sys_ctrl_cb();
    default_timeout = msecs_to_jiffies(DMS_SENSOR_CHECK_TIMER_LEN);
    timeout = default_timeout;

    dms_debug("dms sensor scan task start\n");
    time_notify_start = ka_system_ktime_get();

    while (!kthread_should_stop()) {
        /* Waiting for synchronization semaphore lock */
        ret = ka_task_wait_event_interruptible_timeout(sys_cb->sensor_scan_wait,
                                               sys_cb->sensor_scan_task_state == true, timeout);
        if (ret == 0) {
        /* wait event timeout */
        ret = -ETIMEDOUT;
        } else if (ret == -ERESTARTSYS) {
            /* wait event is interrupted */
            dms_err("wait event interrupted\n");
            ret = -ERESTARTSYS;
        } else if (ret > 0) {
            /* wait event is awakened */
            ret = 0;
        }
        if (sys_cb->sensor_scan_suspend_flag == true) {
#ifndef DMS_UT
            sys_cb->sensor_scan_task_state = false;
            msleep(SLEEP_1MS); /* sleep 1 ms to avoid infinite loop which leads to soft lockup */
#endif
            continue;
        }
        time_notify_end = ka_system_ktime_get();
        dms_sensor_notify_event_proc(DMS_SERSOR_SCAN_NOTIFY);
        /* if exceeded scan time, need to go on scan all */
        if (msecs_to_jiffies(dms_get_time_change_ms(time_notify_start, time_notify_end)) < default_timeout) {
            timeout = default_timeout - msecs_to_jiffies(dms_get_time_change_ms(time_notify_start, time_notify_end));
            sys_cb->sensor_scan_task_state = false;
            continue;
        }
        for (i = 0; i < DEVICE_NUM_MAX; i++) {
            dev_ctrl = dms_get_dev_cb(i);
            if (dev_ctrl != NULL) {
                dms_sensor_scan_proc(&dev_ctrl->dev_sensor_cb);
            }
        }
        sys_cb->sensor_scan_task_state = false;
        timeout = default_timeout;
        dms_sensor_notify_count_clear();
        time_notify_start = ka_system_ktime_get();
    }

    dms_debug("dms sensor scan task exit\n");
    return ret;
}

static unsigned int dms_sensor_task_init(void)
{
    struct dms_system_ctrl_block *sys_cb = NULL;
    sys_cb = dms_get_sys_ctrl_cb();

    ka_task_init_waitqueue_head(&sys_cb->sensor_scan_wait);
    sys_cb->sensor_scan_task_state = false;
    sys_cb->sensor_scan_suspend_flag = false;

    /* Start the sensor scan task */
    sys_cb->sensor_scan_task = ka_task_kthread_create(dms_sensor_scan_task, NULL, "dms_sensor_scan_task");
    if (IS_ERR(sys_cb->sensor_scan_task)) {
        dms_err("sensor scan start failed.\n");
        return DRV_ERROR_INNER_ERR;
    }
#ifdef CFG_SOC_PLATFORM_MDC_V51
    kthread_bind_to_ctrl_cpu(sys_cb->sensor_scan_task);
#endif
    (void)wake_up_process(sys_cb->sensor_scan_task);
    dms_info("create sensor scan task success\n");
    return DRV_ERROR_NONE;
}

static void dms_release_sensor_scan_task_sem(struct timer_list *t)
{
    struct dms_system_ctrl_block *sys_cb = NULL;
    sys_cb = dms_get_sys_ctrl_cb();
    sys_cb->sensor_scan_task_state = true;
    ka_task_wake_up(&sys_cb->sensor_scan_wait);
    return;
}

static unsigned int dms_init_sensor_timer(void)
{
    struct dms_system_ctrl_block *sys_cb = NULL;
    sys_cb = dms_get_sys_ctrl_cb();
    ka_system_timer_setup(&sys_cb->dms_sensor_check_timer, dms_release_sensor_scan_task_sem, 0);
    sys_cb->dms_sensor_check_timer.expires = jiffies + msecs_to_jiffies(DMS_SENSOR_CHECK_TIMER_LEN);
#ifndef CFG_HOST_ENV
    add_timer_affinity(&sys_cb->dms_sensor_check_timer);
#else
    add_timer(&sys_cb->dms_sensor_check_timer);
#endif
    dms_info("create sensor scan task timer success\n");
    return 0;
}

static void dms_start_sensor_scan(void)
{
    (void)dms_init_sensor_timer();
}

static void dms_clean_sensor_timer(void)
{
    struct dms_system_ctrl_block *sys_cb = NULL;
    sys_cb = dms_get_sys_ctrl_cb();
    (void)del_timer_sync(&sys_cb->dms_sensor_check_timer);
}

static void dms_stop_sensor_scan(void)
{
    struct dms_system_ctrl_block *sys_cb = NULL;
    dms_info("stop sensor task scan \n");
    sys_cb = dms_get_sys_ctrl_cb();
    sys_cb->sensor_scan_task_state = true;
    ka_task_wake_up(&sys_cb->sensor_scan_wait);
    dms_clean_sensor_timer();
}

static unsigned int dms_sensor_task_exit(void)
{
    struct dms_system_ctrl_block *sys_cb = NULL;
    dms_info("sensor task exit \n");
    sys_cb = dms_get_sys_ctrl_cb();
    if (!IS_ERR(sys_cb->sensor_scan_task)) {
        (void)kthread_stop(sys_cb->sensor_scan_task);
    }
    sys_cb->sensor_scan_task = NULL;
    return 0;
}

void dms_sensor_init(void)
{
    CALL_INIT_MODULE(DMS_SENSOR_CMD_NAME);
    if (dms_sen_init_sensor() != (unsigned int)DRV_ERROR_NONE) {
        dms_err("dms_sensor_init.\n");
    }
    (void)dms_mgnt_clockid_init();
    dms_sensor_notify_init();
    (void)dms_sensor_task_init();
    dms_start_sensor_scan();
}

unsigned int dms_sensor_exit(void)
{
    CALL_EXIT_MODULE(DMS_SENSOR_CMD_NAME);
    dms_stop_sensor_scan();
    (void)dms_sensor_task_exit();
    dms_sensor_notify_exit();
    return 0;
}

int dms_sensor_inject_fault(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret = 0;
    dms_fault_inject_t info;
    struct dms_node *node = NULL;
    struct dms_dev_ctrl_block *dev_cb = NULL;

    if ((in == NULL) || (in_len != sizeof(dms_fault_inject_t))) {
        dms_err("Invalid parameter. (input=%s; length=%u)\n", (in == NULL) ? "NULL" : "OK", in_len);
        return -EINVAL;
    }

    ret = memcpy_s((void *)&info, sizeof(dms_fault_inject_t), (void *)in, in_len);
    if (ret != 0) {
        dms_err("Memcpy failed. (ret=%d)\n", ret);
        return ret;
    }

    dev_cb = dms_get_dev_cb(info.device_id);
    if (dev_cb == NULL) {
        return -ENODEV;
    }
    node = dms_get_devnode_cb(info.device_id, info.node_type, info.node_id);
    if (node == NULL) {
        dms_err("Node not found. (device=%u; node_type=%u; node_id=%u; fault_index=%u)\n",
            info.device_id, info.node_type, info.node_id, info.fault_index);
        return -EINVAL;
    }
    mutex_lock(&dev_cb->node_lock);
    if ((node->ops == NULL) || (node->ops->fault_inject == NULL)) {
        dms_err("Invalid parameter. (option=%s; fault_inject=NULL)\n", (node->ops == NULL) ? "NULL" : "OK");
        mutex_unlock(&dev_cb->node_lock);
        return -EINVAL;
    }

    ret = node->ops->fault_inject(info);
    if (ret != 0) {
        dms_err("Fault inject failed. (device=%u; node=%u,%u,%u,%u; fault_index=%u; ret=%d)\n",
            info.device_id, info.node_type, info.node_id, info.sub_node_type, info.sub_node_id, info.fault_index, ret);
        mutex_unlock(&dev_cb->node_lock);
        return ret;
    }
    mutex_unlock(&dev_cb->node_lock);
    dms_event("Fault inject ok. (device:%u; node=%u,%u,%u,%u; fault_index=%u)\n",
        info.device_id, info.node_type, info.node_id, info.sub_node_type, info.sub_node_id, info.fault_index);
    return 0;
}

#define FAULT_INJECT_INFO_NUM_MAX (64U)
#define INVALID_NODE_TYPE (0xFFFFFFFFU)
STATIC int dms_sensor_check_param_valid(char *in, u32 in_len, char *out, u32 out_len)
{
    unsigned int out_num;
    out_num = out_len / sizeof(dms_fault_inject_t);
    if ((in == NULL) || (in_len != sizeof(unsigned int)) || (out == NULL) || (out_num == 0) ||
        (out_num > FAULT_INJECT_INFO_NUM_MAX)) {
        dms_err("Invalid parameter. (in=%s; in_length=%u; out=%s; out_length=%u; out_num=%u)\n",
            (in == NULL) ? "NULL" : "OK", in_len, (out == NULL) ? "NULL" : "OK", out_len, out_num);
        return -EINVAL;
    }
    return 0;
}
STATIC int dms_sensor_get_info_of_node(struct dms_dev_ctrl_block *dev_cb, dms_fault_inject_t *out_arr, u32 out_num)
{
    int ret;
    dms_fault_inject_t *drv_buf;
    unsigned int drv_info_num;
    unsigned int drv_index;
    unsigned int out_index = 0;
    struct dms_node *node = NULL;
    struct dms_node *next = NULL;

    /* 1 set node_type to invalid value, to compute valid info number in function DmsGetFaultInjectInfo */
    for (out_index = 0; out_index < out_num; out_index++) {
        out_arr[out_index].node_type = INVALID_NODE_TYPE;
    }
    out_index = 0;

    /* 2 malloc a tmp buf for driver to put their info, rather than operate the out_arr directly */
    drv_buf = (dms_fault_inject_t *)dbl_kzalloc(sizeof(dms_fault_inject_t) * FAULT_INJECT_INFO_NUM_MAX,
                                            KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (drv_buf == NULL) {
        dms_err("Malloc memory fail.\n");
        return -EINVAL;
    }

    /* 3 search the all the nodes to get info */
    mutex_lock(&dev_cb->node_lock);
    if (list_empty_careful(&dev_cb->dev_node_list) != 0) {
        goto out;
    }
    list_for_each_entry_safe(node, next, &dev_cb->dev_node_list, list)
    {
        if ((node->ops != NULL) && (node->ops->get_fault_inject_info != NULL)) {
            drv_info_num = 0;
            ret = node->ops->get_fault_inject_info(node, drv_buf, FAULT_INJECT_INFO_NUM_MAX, &drv_info_num);
            if (ret != 0) {
                dms_info("Do not get info. (node=%d,%d; return=%d)\n", node->node_type, node->node_id, ret);
                continue;
            }
            if (drv_info_num > FAULT_INJECT_INFO_NUM_MAX) {
                dms_warn("drv_info_num (%u) larger than (%d).\n", drv_info_num, FAULT_INJECT_INFO_NUM_MAX);
                drv_info_num = FAULT_INJECT_INFO_NUM_MAX;
            }
            for (drv_index = 0; ((drv_index < drv_info_num) && (out_index < out_num)); (drv_index++, out_index++)) {
                dms_info("Get info. (index=%u; device=%u; node=%u,%u,%u,%u; fault=%u; event=0x%08X; reserve=%u,%u)\n",
                    out_index, drv_buf[drv_index].device_id, drv_buf[drv_index].node_type,
                    drv_buf[drv_index].node_id, drv_buf[drv_index].sub_node_type,
                    drv_buf[drv_index].sub_node_id, drv_buf[drv_index].fault_index,
                    drv_buf[drv_index].event_id, drv_buf[drv_index].reserve1, drv_buf[drv_index].reserve2);
                (void)memcpy_s(&(out_arr[out_index]), sizeof(dms_fault_inject_t),
                    &(drv_buf[drv_index]), sizeof(dms_fault_inject_t));
            }
        }
    }
out:
    mutex_unlock(&dev_cb->node_lock);
    dbl_kfree(drv_buf);
    drv_buf = NULL;
    return 0;
}

int dms_sensor_get_fault_inject_info(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    int dev_id;
    struct dms_dev_ctrl_block *dev_cb;
    dms_fault_inject_t *out_arr;
    int out_num;

    /* 1 check parameter */
    if (dms_sensor_check_param_valid(in, in_len, out, out_len) != 0) {
        return -EINVAL;
    }

    /* 2 get dev_cb by device_id, so that we can search all the nodes to get info */
    ret = memcpy_s((void *)&dev_id, sizeof(unsigned int), (void *)in, in_len);
    if (ret != 0) {
        dms_err("Call memcpy_s failed. (return=%d)\n", ret);
        return -EINVAL;
    }
    dev_cb = dms_get_dev_cb(dev_id);
    if (dev_cb == NULL) {
        dms_err("Get device control block failed.\n");
        return -EINVAL;
    }

    /* 3 main process: search all nodes' info and put info into out_arr */
    out_arr = (dms_fault_inject_t *)out;
    out_num = out_len / sizeof(dms_fault_inject_t);
    ret = dms_sensor_get_info_of_node(dev_cb, out_arr, out_num);
    if (ret != 0) {
        return -EINVAL;
    }
    return 0;
}

int dms_smf_suspend(unsigned int devid)
{
    struct dms_system_ctrl_block *sys_cb = dms_get_sys_ctrl_cb();

    dms_info("dms_smf suspend start.\n");
    sys_cb->sensor_scan_suspend_flag = true;
    (void)dms_event_clear_by_phyid(devid);
    dms_info("dms_smf suspend finish.\n");
    return 0;
}
KA_EXPORT_SYMBOL(dms_smf_suspend);

int dms_smf_resume(unsigned int devid)
{
    struct dms_system_ctrl_block *sys_cb = dms_get_sys_ctrl_cb();

    (void)devid;
    sys_cb->sensor_scan_suspend_flag = false;
    dms_info("dms_smf resume finish.\n");
    return 0;
}
KA_EXPORT_SYMBOL(dms_smf_resume);