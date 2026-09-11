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

#include "ka_task_pub.h"
#include "ka_base_pub.h"
#include "ka_list_pub.h"
#include "ka_memory_pub.h"

#include "dms_define.h"
#include "dms_common.h"
#include "dms_notifier.h"
#include "dms_sensor_type.h"
#include "soft_fault_define.h"
#include "dbl_mem_alloc_interface.h"
#include "davinci_interface.h"

void os_reset_report(u32 devid)
{
    int ret;
    int data_len;
    struct soft_fault os_init;
    const char *info = "OS init";

    data_len = ka_base_strlen(info) + 1;
    ret = strcpy_s(os_init.data, DMS_MAX_EVENT_DATA_LENGTH, info);
    if (ret != 0) {
        soft_drv_err("String copy failed! (device=%u) \n", devid);
        return;
    }

    os_init.data_len = data_len;
    os_init.dev_id = devid;
    os_init.sensor_type = DMS_SEN_TYPE_GENERAL_SOFTWARE_FAULT;
    os_init.user_id = SF_SENSOR_OS;
    os_init.node_type = HAL_DMS_DEV_TYPE_OS_LINUX;
    os_init.node_id = SF_USER_NODE_OS;
    os_init.sub_id = SF_SUB_ID0;
    os_init.err_type = OS_INIT;
    os_init.assertion = GENERAL_EVENT_TYPE_ONE_TIME;

    ret = soft_fault_event_handler(&os_init);
    if (ret != 0) {
        soft_drv_err("Soft fault report failed! (device=%u) \n", devid);
    }

    return;
}

STATIC int os_dev_node_config(unsigned int user_id, struct soft_dev *s_dev)
{
    int pid = current->tgid;
    struct dms_node_operations *soft_ops = soft_get_ops();
    struct dms_node s_node = SOFT_NODE_DEF(HAL_DMS_DEV_TYPE_OS_LINUX, "OS Linux", s_dev->dev_id,
        s_dev->node_id, soft_ops);
    struct dms_sensor_object_cfg s_cfg = SOFT_SENSOR_DEF(DMS_SEN_TYPE_GENERAL_SOFTWARE_FAULT, "OS init", 0UL,
        user_id, SF_SUB_ID0, 0xFFFF, 0xFDFF, SF_SENSOR_SCAN_TIME, soft_fault_event_scan, pid);

    s_cfg.private_data =
        soft_combine_private_data(s_dev->dev_id, user_id, HAL_DMS_DEV_TYPE_OS_LINUX, s_dev->node_id, SF_SUB_ID0);
    /* for kernel space, not used the pid, set default value -1 */
    s_cfg.pid = -1;
    s_node.pid = -1;
    s_dev->sensor_obj_num++;
    s_dev->sensor_obj_table[SF_SUB_ID0] = s_cfg;
    s_dev->dev_node = s_node;

    return 0;
}

int os_dev_register(u32 devid)
{
    int ret;
    unsigned int user_id = SF_SENSOR_OS;
    struct soft_dev_client *client = NULL;
    struct soft_dev *s_dev = NULL;
    struct drv_soft_ctrl *soft_ctrl = soft_get_ctrl();

    ka_task_mutex_lock(&soft_ctrl->mutex[devid]);

    client = soft_ctrl->s_dev_t[devid][user_id];
    if (client->registered == 1) {
        soft_drv_warn("Node OS-Linux already registered, return. (dev_id=%u; registered=%u)\n",
            devid, client->registered);
        ka_task_mutex_unlock(&soft_ctrl->mutex[devid]);
        return 0;
    }

    s_dev = dbl_kzalloc(sizeof(struct soft_dev), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (s_dev == NULL) {
        soft_drv_err("kzalloc soft_dev failed.\n");
        ka_task_mutex_unlock(&soft_ctrl->mutex[devid]);
        return -ENOMEM;
    }

    soft_one_dev_init(s_dev);
    s_dev->dev_id = devid;
    s_dev->node_id = SF_USER_NODE_OS;

    ret = os_dev_node_config(user_id, s_dev);
    if (ret != 0) {
        soft_drv_err("soft_fault configurate one node failed. (dev_id=%u; ret=%d)\n", devid, ret);
        goto ERROR;
    }

    ret = soft_register_one_node(s_dev);
    if (ret != 0) {
        soft_drv_err("soft_fault register one node failed. (dev_id=%u; ret=%d)\n", devid, ret);
        goto ERROR;
    }

    ka_list_add(&s_dev->list, &client->head);
    client->user_id = user_id;
    client->registered = 1;
    client->node_num++;
    soft_ctrl->user_num[devid]++;
    ka_task_mutex_unlock(&soft_ctrl->mutex[devid]);

    return 0;
ERROR:
    dbl_kfree(s_dev);
    s_dev = NULL;
    ka_task_mutex_unlock(&soft_ctrl->mutex[devid]);
    return ret;
}

void os_dev_unregister(void)
{
    int i;
    struct soft_dev_client *client = NULL;
    struct drv_soft_ctrl *soft_ctrl = soft_get_ctrl();

    for (i = 0; i < DEV_ID_MAX; i++) {
        client = soft_ctrl->s_dev_t[i][SF_SENSOR_OS];
        soft_free_one_node(client, HAL_DMS_DEV_TYPE_OS_LINUX);
        soft_ctrl->user_num[i]--;
    }

    return;
}

int os_device_notifier_func(u32 devid)
{
    int ret = 0;

    if (devid >= DEVDRV_PF_DEV_MAX_NUM) {
        return 0;
    }
    ret = os_dev_register(devid);
    os_reset_report(devid);

    return ret;
}
