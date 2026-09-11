/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
* Create: 2025-08-06
*/

#include "ascend_kernel_hal.h"

#include "ka_kernel_def_pub.h"
#include "ka_memory_pub.h"

#include "dms_define.h"
#include "dms_common.h"
#include "dms_notifier.h"
#include "dms_sensor_type.h"
#include "soft_fault_define.h"
#include "dbl_mem_alloc_interface.h"
#include "davinci_interface.h"
#include "drv_kernel_soft.h"

STATIC int drv_kernel_soft_fault_node_config(unsigned int user_id, struct soft_dev *s_dev)
{
    int pid = -1;
    struct dms_node_operations *soft_ops = soft_get_ops();
    struct dms_node s_node = SOFT_NODE_DEF(HAL_DMS_DEV_TYPE_DRV_KERNEL, "DRV_KERNEL", s_dev->dev_id,
        s_dev->node_id, soft_ops);
    struct dms_sensor_object_cfg s_cfg = SOFT_SENSOR_DEF(DMS_SEN_TYPE_GENERAL_SOFTWARE_FAULT, "drv soft", 0UL,
        user_id, SF_SUB_ID0, 0x1000, 0x1000, SF_SENSOR_SCAN_TIME, soft_fault_event_scan, pid);

    s_cfg.private_data =
        soft_combine_private_data(s_dev->dev_id, user_id, HAL_DMS_DEV_TYPE_DRV_KERNEL, s_dev->node_id, SF_SUB_ID0);
    /* for kernel space, not used the pid, set default value -1 */
    s_cfg.pid = -1;
    s_node.pid = -1;
    s_dev->sensor_obj_num++;
    s_dev->sensor_obj_table[SF_SUB_ID0] = s_cfg;
    s_dev->dev_node = s_node;

    return 0;
}

STATIC int drv_kernel_soft_fault_register(u32 devid)
{
    struct drv_soft_ctrl *soft_ctrl = soft_get_ctrl();
    struct soft_dev_client *client = NULL;
    unsigned int user_id = SF_SENSOR_DRV;
    struct soft_dev *s_dev = NULL;
    int ret;

    mutex_lock(&soft_ctrl->mutex[devid]);
    client = soft_ctrl->s_dev_t[devid][user_id];
    if (client->registered == 1) {
        mutex_unlock(&soft_ctrl->mutex[devid]);
        return 0;
    }

    s_dev = dbl_kzalloc(sizeof(struct soft_dev), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (s_dev == NULL) {
        soft_drv_err("kzalloc soft_dev failed.\n");
        mutex_unlock(&soft_ctrl->mutex[devid]);
        return -ENOMEM;
    }

    soft_one_dev_init(s_dev);
    s_dev->dev_id = devid;
    s_dev->node_id = SF_SENSOR_DRV;

    ret = drv_kernel_soft_fault_node_config(user_id, s_dev);
    if (ret != 0) {
        soft_drv_err("soft_fault configurate one node failed. (dev_id=%u; ret=%d)\n", devid, ret);
        goto ERROR;
    }

    ret = soft_register_one_node(s_dev);
    if (ret != 0) {
        soft_drv_err("soft_fault register one node failed. (dev_id=%u; ret=%d)\n", devid, ret);
        goto ERROR;
    }

    list_add(&s_dev->list, &client->head);
    client->user_id = user_id;
    client->registered = 1;
    client->node_num++;
    soft_ctrl->user_num[devid]++;
    mutex_unlock(&soft_ctrl->mutex[devid]);

    return 0;
ERROR:
    dbl_kfree(s_dev);
    s_dev = NULL;
    mutex_unlock(&soft_ctrl->mutex[devid]);
    return ret;
}

void drv_kernel_soft_fault_unregister(void)
{
    struct drv_soft_ctrl *soft_ctrl = soft_get_ctrl();
    struct soft_dev_client *client = NULL;
    u32 i;

    for (i = 0; i < DEV_ID_MAX; i++) {
        mutex_lock(&soft_ctrl->mutex[i]);
        client = soft_ctrl->s_dev_t[i][SF_SENSOR_DRV];
        if (client->registered == 1) {
            soft_free_one_node(client, HAL_DMS_DEV_TYPE_DRV_KERNEL);
            soft_ctrl->user_num[i]--;
        }
        mutex_unlock(&soft_ctrl->mutex[i]);
    }

    return;
}

static void drv_kernel_soft_fault_report(u32 devid)
{
    int ret;
    int data_len;
    struct soft_fault drv_soft;
    const char *info = "drv soft";

    data_len = strlen(info) + 1;
    ret = strcpy_s(drv_soft.data, DMS_MAX_EVENT_DATA_LENGTH, info);
    if (ret != 0) {
        soft_drv_err("String copy failed! (dev_id=%u) \n", devid);
        return;
    }

    drv_soft.data_len = data_len;
    drv_soft.dev_id = devid;
    drv_soft.sensor_type = DMS_SEN_TYPE_GENERAL_SOFTWARE_FAULT;
    drv_soft.user_id = SF_SENSOR_DRV;
    drv_soft.node_type = HAL_DMS_DEV_TYPE_DRV_KERNEL;
    drv_soft.node_id = SF_USER_NODE_DRV;
    drv_soft.sub_id = SF_SUB_ID0;
    drv_soft.err_type = SOFT_FAIL_CANNOT_RECOVER;
    drv_soft.assertion = GENERAL_EVENT_TYPE_OCCUR;

    soft_fault_event_handler(&drv_soft);

    return;
}

void hal_kernel_drv_soft_fault_report(u32 devid)
{
    u32 i;

    if ((devid != DRV_SOFT_FAULT_REPORT_ALL_DEV) && ((devid >= DEV_ID_MAX) || (devid >= DEVDRV_PF_DEV_MAX_NUM))) {
        return;
    }

    if (devid == DRV_SOFT_FAULT_REPORT_ALL_DEV) {
        for (i = 0; (i < DEVDRV_PF_DEV_MAX_NUM) && (i < DEV_ID_MAX); i++) {
            if (drv_kernel_soft_fault_register(i) == 0) {
                drv_kernel_soft_fault_report(i);
            }
        }
    } else {
        if (drv_kernel_soft_fault_register(devid) == 0) {
            drv_kernel_soft_fault_report(devid);
        }
    }
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_drv_soft_fault_report);

