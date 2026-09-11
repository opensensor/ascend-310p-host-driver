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

#ifndef __DMS_PROBE_H__
#define __DMS_PROBE_H__

int dms_device_register(struct devdrv_info* dev);
void dms_device_unregister(struct devdrv_info* dev);

/* dms state flag */
#define DMS_NOT_USED 0x1
#define DMS_IN_USED 0x2

#endif