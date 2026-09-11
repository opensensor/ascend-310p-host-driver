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

#ifndef SOFT_FAULT_CONFIG_H
#define SOFT_FAULT_CONFIG_H

#include "devdrv_manager_container.h"

#define DAVINCI_FID 0

#ifdef CFG_HOST_ENV
#include "host_config.h"
#define VDAVINCI_NUM VDAVINCI_MAX_VFID_NUM
#else
#include "device_config.h"
#define VDAVINCI_NUM 0
#endif

#endif /* SOFT_FAULT_CONFIG_H */
