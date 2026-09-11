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

#ifndef __DEVDRV_MANAGER_MSG_H
#define __DEVDRV_MANAGER_MSG_H

#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/idr.h>
#include <linux/errno.h>
#include <linux/fs.h>

#include "devdrv_common.h"
#include "devdrv_manager_common.h"
#include "devdrv_pm.h"
#include "devdrv_manager.h"
#include "devdrv_interface.h"

typedef struct devdrv_core_utilization {
    u32 dev_id;
    u32 vfid;
    u32 core_type; /* 0: aicore  1: aivector 2:aicpu */
    u32 utilization;
} devdrv_core_utilization_t;

typedef enum devdrv_core_type {
    DEV_DRV_TYPE_AICORE = 0,
    DEV_DRV_TYPE_AIVECTOR,
    DEV_DRV_TYPE_AICPU,
    DEV_DRV_TYPE_MAX,
} devdrv_core_type_t;

int devdrv_manager_h2d_sync_get_devinfo(struct devdrv_info *dev_info);
int devdrv_manager_h2d_query_resource_info(u32 devid, struct devdrv_manager_msg_resource_info *dinfo);
u32 devdrv_manager_h2d_query_dmp_started(u32 devid);
int devdrv_manager_h2d_sync_get_core_utilization(struct devdrv_core_utilization *core_util);

#endif /* __DEVDRV_MANAGER_MSG_H */
