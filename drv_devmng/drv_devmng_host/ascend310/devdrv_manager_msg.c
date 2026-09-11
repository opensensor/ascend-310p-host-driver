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

#include "devdrv_manager_msg.h"

int devdrv_manager_h2d_sync_get_devinfo(struct devdrv_info *dev_info)
{
    struct devdrv_manager_msg_info dev_manager_msg_info = { { 0 } };
    struct devmng_msg_h2d_info *h2d_info = NULL;
    u32 dev_id = dev_info->dev_id;
    void *no_trans_chan = NULL;
    u32 out_len, in_len;
    int ret;

    dev_manager_msg_info.header.dev_id = dev_id;
    dev_manager_msg_info.header.msg_id = DEVDRV_MANAGER_CHAN_H2D_SYNC_GET_DEVINFO;
    dev_manager_msg_info.header.valid = DEVDRV_MANAGER_MSG_H2D_MAGIC;
    dev_manager_msg_info.header.result = DEVDRV_MANAGER_MSG_INVALID_RESULT;

    no_trans_chan = devdrv_manager_get_no_trans_chan(dev_id);
    if (no_trans_chan == NULL) {
        devdrv_drv_err("get no trans chan failed, dev_id = %u\n", dev_id);
        return -ENODEV;
    }

    in_len = sizeof(struct devdrv_manager_msg_head) + sizeof(struct devmng_msg_h2d_info);
    ret = devdrv_sync_msg_send(no_trans_chan, &dev_manager_msg_info, in_len, in_len, &out_len);
    if (ret || dev_manager_msg_info.header.result != 0 ||
        dev_manager_msg_info.header.valid != DEVDRV_MANAGER_MSG_D2H_MAGIC) {
        devdrv_drv_err("msg chan alloc stream d2h failed\n");
        devdrv_drv_err("ret = %d, result = %d, valid = 0x%x\n", ret, dev_manager_msg_info.header.result,
            dev_manager_msg_info.header.valid);
        return -EFAULT;
    }

    h2d_info = (struct devmng_msg_h2d_info *)dev_manager_msg_info.payload;

    /* just get dynamic data from device at present */
    dev_info->cpu_system_count = h2d_info->cpu_system_count;
    dev_info->monotonic_raw_time_ns = h2d_info->monotonic_raw_time_ns;

    return 0;
}

int devdrv_manager_h2d_query_resource_info(u32 devid, struct devdrv_manager_msg_resource_info *dinfo)
{
    struct devdrv_manager_msg_info dev_manager_msg_info = {{0}};
    struct devdrv_manager_msg_resource_info *resource_info = NULL;
    void *no_trans_chan = NULL;
    u32 out_len;
    int ret;

    dev_manager_msg_info.header.dev_id = devid;
    dev_manager_msg_info.header.msg_id = DEVDRV_MANAGER_CHAN_H2D_GET_RESOURCE_INFO;
    dev_manager_msg_info.header.valid = DEVDRV_MANAGER_MSG_H2D_MAGIC;
    dev_manager_msg_info.header.result = DEVDRV_MANAGER_MSG_INVALID_RESULT;

    resource_info = (struct devdrv_manager_msg_resource_info *)dev_manager_msg_info.payload;

    resource_info->vfid = dinfo->vfid;
    resource_info->owner_id = dinfo->owner_id;
    resource_info->info_type = dinfo->info_type;

    no_trans_chan = devdrv_manager_get_no_trans_chan(devid);
    if (no_trans_chan == NULL) {
        devdrv_drv_warn("The devdrv_manager_get_no_trans_chan failed. (device_id=%u)\n", devid);
        return -ENODEV;
    }

    ret = devdrv_sync_msg_send(no_trans_chan, &dev_manager_msg_info, sizeof(dev_manager_msg_info),
                               sizeof(dev_manager_msg_info), &out_len);
    if (ret || (dev_manager_msg_info.header.result == DEVDRV_MANAGER_MSG_INVALID_RESULT) ||
        (dev_manager_msg_info.header.valid != DEVDRV_MANAGER_MSG_H2D_MAGIC)) {
        devdrv_drv_err("Device msg chan query resource info failed. (device_id=%u)\n", devid);
        devdrv_drv_err("Other parameter. (ret=%d; result=%d; valid=0x%x)\n", ret,
                       dev_manager_msg_info.header.result,
                       dev_manager_msg_info.header.valid);
        return -EFAULT;
    }

    dinfo->value = resource_info->value;
    return 0;
}

u32 devdrv_manager_h2d_query_dmp_started(u32 devid)
{
    struct devdrv_manager_msg_info dev_manager_msg_info = {{0}};
    void *no_trans_chan = NULL;
    u32 *dmp_started = NULL;
    u32 out_len;
    int ret;

    dev_manager_msg_info.header.dev_id = devid;
    dev_manager_msg_info.header.msg_id = DEVDRV_MANAGER_CHAN_H2D_QUERY_DMP_STARTED;
    dev_manager_msg_info.header.valid = DEVDRV_MANAGER_MSG_H2D_MAGIC;
    dev_manager_msg_info.header.result = DEVDRV_MANAGER_MSG_INVALID_RESULT;

    no_trans_chan = devdrv_manager_get_no_trans_chan(devid);
    if (no_trans_chan == NULL) {
        return false;
    }

    dmp_started = (u32 *)dev_manager_msg_info.payload;
    *dmp_started = false;

    ret = devdrv_sync_msg_send(no_trans_chan, &dev_manager_msg_info, sizeof(dev_manager_msg_info),
                               sizeof(dev_manager_msg_info), &out_len);
    if (ret || (dev_manager_msg_info.header.result == DEVDRV_MANAGER_MSG_INVALID_RESULT) ||
        (dev_manager_msg_info.header.valid != DEVDRV_MANAGER_MSG_H2D_MAGIC)) {
        devdrv_drv_warn("device(%u) msg chan query dmp_started: ", devid);
        devdrv_drv_warn("ret = %d, result = %d, valid = 0x%x\n", ret,
                        dev_manager_msg_info.header.result,
                        dev_manager_msg_info.header.valid);
        return false;
    }

    return *dmp_started;
}

int devdrv_manager_h2d_sync_get_core_utilization(struct devdrv_core_utilization *core_util)
{
    struct devdrv_manager_msg_info dev_manager_msg_info = {{0}};
    struct devdrv_core_utilization *h2d_info = NULL;
    u32 dev_id = 0;
    void *no_trans_chan = NULL;
    u32 out_len, in_len;
    int ret;

    if (core_util == NULL) {
        devdrv_drv_err("Parameter is NULL.\n");
        return -EINVAL;
    }
    dev_id = core_util->dev_id;
    dev_manager_msg_info.header.dev_id = dev_id;
    dev_manager_msg_info.header.msg_id = DEVDRV_MANAGER_CHAN_H2D_SYNC_GET_CORE_UTILIZATION;
    dev_manager_msg_info.header.valid = DEVDRV_MANAGER_MSG_H2D_MAGIC;
    dev_manager_msg_info.header.result = DEVDRV_MANAGER_MSG_INVALID_RESULT;

    ret = memcpy_s(dev_manager_msg_info.payload, DEVDRV_MANAGER_INFO_PAYLOAD_LEN,
                   core_util, sizeof(struct devdrv_core_utilization));
    if (ret != 0) {
        devdrv_drv_err("Memcpy failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return -EFAULT;
    }

    no_trans_chan = devdrv_manager_get_no_trans_chan(dev_id);
    if (no_trans_chan == NULL) {
        devdrv_drv_err("Get no trans chan failed. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    in_len = sizeof(struct devdrv_manager_msg_head) + sizeof(struct devdrv_core_utilization);
    ret = devdrv_sync_msg_send(no_trans_chan, &dev_manager_msg_info, in_len, in_len, &out_len);
    if (ret != 0 || (dev_manager_msg_info.header.result != 0) ||
        (dev_manager_msg_info.header.valid != DEVDRV_MANAGER_MSG_D2H_MAGIC)) {
        devdrv_drv_err("Device msg chan alloc stream d2h failed. (dev_id=%u, ret=%d)\n", dev_id, ret);
        return -EFAULT;
    }

    h2d_info = (struct devdrv_core_utilization*)dev_manager_msg_info.payload;

    core_util->utilization = h2d_info->utilization;

    return 0;
}