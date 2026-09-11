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
#include <linux/time.h>
#include <linux/delay.h>

#ifndef AOS_LLVM_BUILD
#include "drv_type.h"
#include "devdrv_common.h"
#include "devdrv_info.h"
#include "devdrv_manager.h"
#include "device/bbox_pub_cloud.h"
#include "device/bbox_pub_mini.h"
#else
#include "devdrv_common.h"
#endif

#include "dms_define.h"
#include "dms_event.h"
#include "dms_event_device.h"
#include "dms_event_distribute.h"
#include "dms_event_converge.h"
#include "dbl_mem_alloc_interface.h"
#include "dms_kernel_version_adapt.h"
#include "dms_notifier.h"
#include "kernel_version_adapt.h"
#include "devmng_dms_adapt.h"
#include "dms_event_adapt.h"
#include "dms_event_device.h"

#include "ka_memory_pub.h"

int devdrv_host_black_box_add_exception(u32 devid, u32 code,
    struct timespec stamp, const void *data)
{
    return DRV_ERROR_NOT_SUPPORT;
}

int dms_event_subscribe_from_device(u32 phyid)
{
    return DRV_ERROR_NONE;
}

int dms_event_get_exception_from_device(void *msg, u32 *ack_len)
{
    return DRV_ERROR_NOT_SUPPORT;
}

int dms_event_clean_to_device(u32 phyid)
{
    return DRV_ERROR_NONE;
}

int dms_get_event_code_from_bar(u32 devid, u32 *health_code, u32 health_len,
    struct shm_event_code *event_code, u32 event_len)
{
    return DRV_ERROR_NONE;
}

int dms_get_health_code_from_bar(u32 devid, u32 *health_code, u32 health_len)
{
    return DRV_ERROR_NONE;
}

int dms_save_exception_in_local(void)
{
    return DRV_ERROR_NONE;
}

int dms_remote_event_save_in_local_init(void)
{
    return DRV_ERROR_NONE;
}

void dms_remote_event_save_in_local_exit(void)
{
    return;
}

void dms_event_host_init(void)
{
    return;
}

void dms_event_host_uninit(void)
{
    return;
}
#ifndef AOS_LLVM_BUILD
void devdrv_device_black_box_add_exception(u32 devid, u32 code)
{
    struct timespec os_stamp = {0};
    excep_time execp_stamp = {0};

    os_stamp = current_kernel_time();
    execp_stamp.tv_sec = os_stamp.tv_sec;
    execp_stamp.tv_usec = os_stamp.tv_nsec / 1000; /* divide 1000 to us */
#if (defined(CFG_SOC_PLATFORM_CLOUD) || defined(CFG_SOC_PLATFORM_MINIV2))
    bbox_system_error(devid, code, &execp_stamp, 0);
#else
    mntn_system_error(code, execp_stamp, 0);
#endif
}

int dms_event_box_add_exception(u32 devid, u32 code, struct timespec stamp)
{
    devdrv_device_black_box_add_exception(devid, code);
    return 0;
}

#ifdef CFG_SURPPORT_PCIE_HOST_DEVICE_COMM
static int dms_event_distribute_to_host(DMS_EVENT_NODE_STRU *exception_node)
{
    struct devdrv_manager_msg_info dev_manager_msg_info = {{0}, {0}};
    DMS_EVENT_NODE_STRU *exception_buf = NULL;
    u32 out_len;
    int ret;

    dev_manager_msg_info.header.msg_id = DEVDRV_MANAGER_CHAN_D2H_DMS_EVENT_DISTRIBUTE;
    dev_manager_msg_info.header.valid = (u16)DEVDRV_MANAGER_MSG_D2H_MAGIC;
    dev_manager_msg_info.header.result = (u16)DEVDRV_MANAGER_MSG_INVALID_RESULT;

    exception_buf = (DMS_EVENT_NODE_STRU *)dev_manager_msg_info.payload;
    if (memcpy_s(exception_buf, sizeof(dev_manager_msg_info.payload),
        exception_node, sizeof(DMS_EVENT_NODE_STRU)) != 0) {
        dms_err("Calling memcpy_s failed. (dev_id=%u; event_code=0x%x)\n",
                exception_node->event.deviceid, exception_node->event.event_code);
        return DRV_ERROR_INNER_ERR;
    }

    ret = devdrv_manager_wait_host_ready_event(exception_node->event.deviceid);
    if (ret != 0) {
        return ret;
    }

    ret = agentdrv_common_msg_send(exception_node->event.deviceid, &dev_manager_msg_info, sizeof(dev_manager_msg_info),
                                   sizeof(dev_manager_msg_info), &out_len, AGENTDRV_COMMON_MSG_DEVDRV_MANAGER);
    if (ret || (dev_manager_msg_info.header.result != DEVDRV_MANAGER_MSG_VALID)) {
        dms_err("Send message to host failed. (dev_id=%u; event_code=0x%x; ret=%d; result=%u)\n",
                exception_node->event.deviceid, exception_node->event.event_code,
                ret, dev_manager_msg_info.header.result);
        return DRV_ERROR_SEND_MESG;
    }
    dms_debug("Send event to host success. (dev_id=%u; event_id=0x%x)\n",
              exception_node->event.deviceid, exception_node->event.event_id);
    return DRV_ERROR_NONE;
}
#endif

static int dms_event_check_msg_and_ack_len(void *msg, u32 in_len, u32 *ack_len)
{
    struct devdrv_manager_msg_info *dev_manager_msg_info = NULL;

    if ((msg == NULL) || (ack_len == NULL)) {
        dms_err("Invalid parameter. (msg=%d; ack_len=%d)\n", msg != NULL, ack_len != NULL);
        return DRV_ERROR_PARA_ERROR;
    }

    dev_manager_msg_info = (struct devdrv_manager_msg_info *)msg;
    if ((dev_manager_msg_info->header.valid != DEVDRV_MANAGER_MSG_H2D_MAGIC) ||
        (in_len < sizeof(struct devdrv_manager_msg_info))) {
        dms_err("The verification of message is invalid. (valid=%u)\n", dev_manager_msg_info->header.valid);
        return DRV_ERROR_INVALID_VALUE;
    }

    return  DRV_ERROR_NONE;
}

int dms_event_subscribe_from_host(u32 devid, void *msg, u32 in_len, u32 *ack_len)
{
#ifdef CFG_SURPPORT_PCIE_HOST_DEVICE_COMM
    int ret;
    struct devdrv_manager_msg_info *dev_manager_msg_info = NULL;
    static bool first_subscribe = true;

    ret = dms_event_check_msg_and_ack_len(msg, in_len, ack_len);
    if (ret != 0) {
        dms_err("Check msg and ack_len failed.(device id=%u; ret=%d)\n", devid, ret);
        return ret;
    }

    dev_manager_msg_info = (struct devdrv_manager_msg_info *)msg;
    *ack_len = sizeof(*dev_manager_msg_info);

    if (first_subscribe == true) {
        if (dms_event_subscribe_register(dms_event_distribute_to_host, DMS_DISTRIBUTE_PRIORITY3) != 0) {
            dms_err("Distribute handle failed. (dev_id=%u)\n", devid);
            return DRV_ERROR_RECV_MESG;
        }
        dms_event_set_poll_task_flag(EVENT_POLL_WORK);
        first_subscribe = false;
    }

    dev_manager_msg_info->header.result = DEVDRV_MANAGER_MSG_VALID;

    dms_notifyer_call(DMS_H2D_EVENT, &devid);

    dms_info("Subscribe from host success. (dev_id=%u)\n", devid);
    return DRV_ERROR_NONE;
#else
    return -ENOTSUPP;
#endif
}
#endif
int dms_event_distribute_to_bar(u32 phyid)
{
    u32 sensor_health[VMNG_VDEV_MAX_PER_PDEV] = {0};
    struct shm_event_code *sensor_event = NULL;
    int ret;

    if (dms_get_rc_ep_mode() == DMS_RC_MODE) {
        return 0;
    }
    sensor_event = (struct shm_event_code *)dbl_kzalloc(sizeof(struct shm_event_code) * \
                                                    DEVMNG_SHM_INFO_EVENT_CODE_LEN, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (sensor_event == NULL) {
        dms_err("Call kzalloc sensor_event failed.\n");
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    if (dms_event_is_converge() != 0) {
        ret = dms_get_event_code_from_event_cb(phyid, sensor_health, VMNG_VDEV_MAX_PER_PDEV,
                                               sensor_event, DEVMNG_SHM_INFO_EVENT_CODE_LEN);
        if (ret != 0) {
            dms_err("Get event code from event cb failed. (dev_id=%u; ret=%d)\n",
                    phyid, ret);
            goto out;
        }
    } else {
        ret = dms_get_event_code_from_sensor(phyid, sensor_health, VMNG_VDEV_MAX_PER_PDEV,
                                             sensor_event, DEVMNG_SHM_INFO_EVENT_CODE_LEN);
        if (ret != 0) {
            dms_err("Get event code from sensor manager failed. (dev_id=%u; ret=%d)\n",
                    phyid, ret);
            goto out;
        }
    }
#ifndef AOS_LLVM_BUILD
    ret = devdrv_fresh_event_code_to_shm(phyid, sensor_health, VMNG_VDEV_MAX_PER_PDEV,
                                         sensor_event, DEVMNG_SHM_INFO_EVENT_CODE_LEN);
    if (ret != 0) {
        dms_warn("Can not fresh event code to bar zone. (dev_id=%u; ret=%d)\n", phyid, ret);
        goto out;
    }
#endif
out:
    dbl_kfree(sensor_event);
    sensor_event = NULL;
    return ret;
}

/* wait for device ready at most 180s */
#define TIMEOUT 180
int dms_distribute_all_devices_event_to_bar(void)
{
#ifndef AOS_LLVM_BUILD
    int ret;
    u32 dev_id = 0;
    u32 dev_num = 0;
    u32 try_times = 0;
    struct devdrv_info *dev_info = NULL;
    struct dms_dev_ctrl_block *dev_cb = NULL;
    static U_SHM_INFO_STATUS *shm_status = NULL;

    if (dms_get_rc_ep_mode() == DMS_RC_MODE) {
        return DRV_ERROR_NONE;
    }

    /* shm_status == NULL indicates device is not inline yet */
    while (shm_status == NULL) {
        ssleep(1);
        dev_cb = dms_get_dev_cb(dev_id);
        if (dev_cb == NULL) {
            dms_err("Get dev_ctrl block failed. (dev_id=%u)\n", dev_id);
            return -ENODEV;
        }

        dev_info = (struct devdrv_info *)dev_cb->dev_info;
        if (dev_info != NULL) {
            shm_status = dev_info->shm_status;
        }

        if (try_times == TIMEOUT) {
            dms_err("Wait for device inline timeout. (dev_id=%u)\n", dev_id);
            return -ETIMEDOUT;
        }

        try_times++;
    }

    /* until device is inline, devdrv_get_devnum is callable */
    ret = devdrv_get_devnum(&dev_num);
    if (ret != DRV_ERROR_NONE) {
        dms_err("Get device number failed. (ret=%d)\n", ret);
        return ret;
    }

    for (dev_id = 0; dev_id < dev_num; dev_id++) {
        ret  = dms_event_distribute_to_bar(dev_id);
        if (ret != DRV_ERROR_NONE) {
            dms_warn("Cannot distribute event to bar. (device id=%u; ret=%d)\n", dev_id, ret);
            continue;
        }
        dms_debug("Distribute event to bar succeed. (device id=%u)\n", dev_id);
    }
#endif

    return DRV_ERROR_NONE;
}

#ifndef AOS_LLVM_BUILD
int dms_event_clean_from_host(u32 devid, void *msg, u32 in_len, u32 *ack_len)
{
    int ret;
    struct devdrv_manager_msg_info *dev_manager_msg_info = NULL;

    ret = dms_event_check_msg_and_ack_len(msg, in_len, ack_len);
    if (ret != 0) {
        dms_err("Check msg and ack_len failed. (device id=%u; ret=%d)\n", devid, ret);
        return ret;
    }

    dev_manager_msg_info = (struct devdrv_manager_msg_info *)msg;
    *ack_len = sizeof(*dev_manager_msg_info);

    if (dms_event_clear_by_phyid(devid) != 0) {
        dms_err("Clear exception failed. (devid=%u)\n", devid);
        return DRV_ERROR_INNER_ERR;
    }

    dev_manager_msg_info->header.result = DEVDRV_MANAGER_MSG_VALID;
    dms_event("Clean from host success. (dev_id=%u)\n", devid);
    return DRV_ERROR_NONE;
}

int dms_event_mask_from_host(u32 devid, void *msg, u32 in_len, u32 *ack_len)
{
    int ret;
    u8 mask;
    u32 event_code;
    u32 *data_load = NULL;
    struct devdrv_manager_msg_info *dev_manager_msg_info = NULL;

    ret = dms_event_check_msg_and_ack_len(msg, in_len, ack_len);
    if (ret != 0) {
        dms_err("Check msg and ack_len failed.(dev_id=%u; ret=%d)\n", devid, ret);
        return ret;
    }

    dev_manager_msg_info = (struct devdrv_manager_msg_info *)msg;
    data_load = (u32 *)dev_manager_msg_info->payload;
    event_code = *data_load++;
    mask = (u8)*data_load;

    if (dms_event_mask_by_phyid(devid, event_code, mask) != 0) {
        dms_err("Mask event_code failed. (dev_id=%u; event_code=0x%x; mask=%u)\n",
                devid, event_code, mask);
        return DRV_ERROR_INNER_ERR;
    }

    dev_manager_msg_info->header.result = DEVDRV_MANAGER_MSG_VALID;
    *ack_len = sizeof(*dev_manager_msg_info);
    dms_event("Mask from host success. (dev_id=%u; event_code=0x%x; mask=%u)\n",
              devid, event_code, mask);
    return DRV_ERROR_NONE;
}
#endif
int dms_event_mask_event_code(u32 phyid, u32 event_code, u8 mask)
{
    int ret;

    ret = dms_event_mask_by_phyid(phyid, event_code, mask);
    if (ret != 0) {
        dms_err("Mask event code in device failed. (physical id=%u; event_code=0x%x; mask=%u; ret=%d)\n",
                phyid, event_code, mask, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

int dms_get_event_para(int dev_id, struct dms_event_para *dms_event, unsigned int in_cnt, unsigned int *event_num)
{
    return DRV_ERROR_NONE;
}
