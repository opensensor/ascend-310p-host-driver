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

#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/wait.h>

#ifndef DEVMNG_UT
#include "devdrv_common.h"
#include "devdrv_manager.h"
#include "devdrv_driver_pm.h"

void devdrv_driver_hardware_exception(struct devdrv_info *info)
{
    if ((info == NULL) || (info->drv_ops == NULL)) {
        devdrv_drv_err("invalid input argument.\n");
    }
}
#else
void devdrv_driver_host_hardware_exception(struct devdrv_info *info)
{
}
#endif
