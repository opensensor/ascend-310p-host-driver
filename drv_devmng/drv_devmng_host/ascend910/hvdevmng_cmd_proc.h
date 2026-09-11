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

#ifndef DEVDRV_HVDEVMNG_CMD_PROC__HOST_H
#define DEVDRV_HVDEVMNG_CMD_PROC__HOST_H


int hvdevmng_get_h2d_devinfo(u32 dev_id, u32 fid, struct vdevmng_ioctl_msg *iomsg);
int hvdevmng_get_device_status(u32 dev_id, u32 fid, struct vdevmng_ioctl_msg *iomsg);
int hvdevmng_get_device_boot_status(u32 dev_id, u32 fid, struct vdevmng_ioctl_msg *iomsg);
int hvdevmng_get_device_startup_status(u32 dev_id, u32 fid, struct vdevmng_ioctl_msg *iomsg);
int hvdevmng_get_device_health_status(u32 dev_id, u32 fid, struct vdevmng_ioctl_msg *iomsg);
int hvdevmng_mdev_info_get(u32 dev_id, u32 fid, struct vdevdrv_info_msg *ready_info);
int hvdevmng_mdev_pdata_get(u32 dev_id, struct vdevdrv_info_pdata *pdata);
int hvdevmng_vpc_get_dev_resource_info(u32 dev_id, u32 fid, struct vdevmng_ioctl_msg *iomsg);
int hvdevmng_get_error_code(u32 dev_id, u32 fid, struct vdevmng_ioctl_msg *iomsg);
int hvdevmng_get_osc_freq(u32 dev_id, u32 fid, struct vdevmng_ioctl_msg *iomsg);
int hvdevmng_get_current_aic_freq(u32 dev_id, u32 fid, struct vdevmng_ioctl_msg *iomsg);
#endif

