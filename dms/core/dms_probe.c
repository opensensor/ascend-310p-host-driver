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

#include "ka_kernel_def_pub.h"

#include "dms_define.h"
#include "dms_init.h"
#include "devdrv_common.h"
#include "dms_notifier.h"
#include "dms_probe.h"
#include "dms_sysfs.h"
#include "dms_interface.h"

int dms_get_devid_from_data(void *data)
{
    int dev_id;
    if (data == NULL) {
        dms_err("Data is NULL\n");
        return -EINVAL;
    }
    dev_id = ((struct devdrv_info *)data)->dev_id;
    return dev_id;
}
KA_EXPORT_SYMBOL_GPL(dms_get_devid_from_data);

static int dms_create_device(struct devdrv_info* dev_info)
{
    int ret;
    int dev_id;
    struct dms_system_ctrl_block* cb = dms_get_sys_ctrl_cb();

    dev_id = dev_info->dev_id;
    ret = dms_check_device_id(dev_id);
    if (ret != 0) {
        dms_err("dev_id out of range. (ret=%d; dev_id=%d)\n", ret, dev_id);
        return ret;
    }
    cb->dev_cb_table[dev_id].state = DMS_IN_USED;
    cb->dev_cb_table[dev_id].dev_info = (void*)dev_info;
    return 0;
}

static int dms_destroy_device(struct devdrv_info* dev_info)
{
    struct dms_system_ctrl_block* cb = dms_get_sys_ctrl_cb();
    int ret;
    int dev_id;
    dev_id = dev_info->dev_id;

    if (cb == NULL) {
        return 0;
    }

    ret = dms_check_device_id(dev_id);
    if (ret != 0) {
        dms_err("dev_id out of range. (ret=%d; dev_id=%d)\n", ret, dev_id);
        return ret;
    }
    cb->dev_cb_table[dev_id].state = DMS_NOT_USED;
    cb->dev_cb_table[dev_id].dev_info = NULL;

    return 0;
}

static int dms_notify_device_up(struct devdrv_info* dev)
{
    int ret;
    int dev_id = dev->dev_id;

    ret = dms_notifyer_call(DMS_DEVICE_UP0, dev);
    if (ret != 0) {
        dms_err("notify call failed. (ret=%d; dev_id=%d)\n", ret, dev_id);
        return ret;
    }

    ret = dms_notifyer_call(DMS_DEVICE_UP1, dev);
    if (ret != 0) {
        dms_err("notify call failed. (ret=%d; dev_id=%d)\n", ret, dev_id);
        return ret;
    }
    ret = dms_notifyer_call(DMS_DEVICE_UP2, dev);
    if (ret != 0) {
        dms_err("notify call failed. (ret=%d; dev_id=%d)\n", ret, dev_id);
        return ret;
    }
    ret = dms_notifyer_call(DMS_DEVICE_UP3, dev);
    if (ret != 0) {
        dms_err("notify call failed. (ret=%d; dev_id=%d)\n", ret, dev_id);
        return ret;
    }
    return 0;
}

static int dms_notify_device_down(struct devdrv_info* dev)
{
    int ret;
    int dev_id = dev->dev_id;
    ret = dms_notifyer_call(DMS_DEVICE_DOWN3, dev);
    if (ret != 0) {
        dms_err("notify call failed. (ret=%d; dev_id=%d)\n", ret, dev_id);
        return ret;
    }

    ret = dms_notifyer_call(DMS_DEVICE_DOWN2, dev);
    if (ret != 0) {
        dms_err("notify call failed. (ret=%d; dev_id=%d)\n", ret, dev_id);
        return ret;
    }
    ret = dms_notifyer_call(DMS_DEVICE_DOWN1, dev);
    if (ret != 0) {
        dms_err("notify call failed. (ret=%d; dev_id=%d)\n", ret, dev_id);
        return ret;
    }
    ret = dms_notifyer_call(DMS_DEVICE_DOWN0, dev);
    if (ret != 0) {
        dms_err("notify call failed. (ret=%d; dev_id=%d)\n", ret, dev_id);
        return ret;
    }
    return 0;
}

int dms_device_register(struct devdrv_info* dev)
{
    int ret;
    int dev_id;
    if (dev == NULL) {
        dms_err("dev is null.\n");
        return -EINVAL;
    }
    dev_id = dev->dev_id;
    ret = dms_check_device_id(dev_id);
    if (ret != 0) {
        dms_err("dev_id out of range. (ret=%d; dev_id=%d)\n", ret, dev_id);
        return ret;
    }

    ret = dms_create_device(dev);
    if (ret != 0) {
        dms_err("Create device control block failed.  (ret=%d; dev_id=%d)\n", ret, dev_id);
        return ret;
    }

    ret = dms_notify_device_up(dev);
    if (ret != 0) {
        dms_err("notify call failed.  (ret=%d; dev_id=%d)\n", ret, dev_id);
        return ret;
    }
    return 0;
}
KA_EXPORT_SYMBOL_GPL(dms_device_register);

void dms_device_unregister(struct devdrv_info* dev)
{
    int ret;
    int dev_id;
    if (dev == NULL) {
        dms_err("dev is null.\n");
        return;
    }
    dev_id = dev->dev_id;
    ret = dms_check_device_id(dev_id);
    if (ret != 0) {
        dms_err("dev_id out of range. (ret=%d; dev_id=%d)\n", ret, dev_id);
        return;
    }

    ret = dms_notify_device_down(dev);
    if (ret != 0) {
        dms_err("notify call failed. (ret=%d; dev_id=%d)\n", ret, dev_id);
        return;
    }

    ret = dms_destroy_device(dev);
    if (ret != 0) {
        dms_err("Destroy device control block failed.  (ret=%d; dev_id=%d)\n", ret, dev_id);
        return;
    }
    return;
}
KA_EXPORT_SYMBOL_GPL(dms_device_unregister);
#ifdef AOS_LLVM_BUILD
struct devdrv_info *dms_get_devinfo(u32 devid)
{
    struct dms_dev_ctrl_block *dev_cb = NULL;

    dev_cb = dms_get_dev_cb(devid);
    if (dev_cb == NULL) {
        dms_err("Get dev_ctrl block failed. (dev_id=%u)\n", devid);
        return NULL;
    }

    return (struct devdrv_info *)dev_cb->dev_info;
}
KA_EXPORT_SYMBOL_GPL(dms_get_devinfo);
#endif
