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

#ifndef __DEVDRV_VDEVMNG_INIT__HOST_H
#define __DEVDRV_VDEVMNG_INIT__HOST_H

#include "devdrv_user_common.h"

#define PCI_VENDOR_ID_HUAWEI 0x19e5

struct devdrv_manager_info *vdevdrv_get_manager_info(void);
u32 vdevdrv_manager_get_devnum(void);
struct devdrv_info *vdevdrv_manager_get_devdrv_info(u32 dev_id);
int vdevdrv_manager_set_devdrv_info(u32 dev_id, struct devdrv_info *vdev_info);
int vdevmng_set_devinfo_inc_devnum(u32 dev_id, struct devdrv_info *vdev_info);
int vdevdrv_manager_reset_devinfo_dec_devnum(u32 dev_id);
int vdevdrv_manager_get_accounting_pid(struct devdrv_resource_info *dinfo);

#endif

