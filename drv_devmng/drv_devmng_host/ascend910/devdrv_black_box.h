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

#ifndef DEVDRV_BLACK_BOX_H
#define DEVDRV_BLACK_BOX_H

#include "devdrv_interface.h"
#include "devdrv_user_common.h"
#include "dms_kernel_version_adapt.h"
#include "kernel_version_adapt.h"

void devdrv_host_black_box_init(void);
void devdrv_host_black_box_exit(void);
void devdrv_host_black_box_close_check(pid_t pid);
int devdrv_host_black_box_add_exception(u32 devid, u32 code, struct timespec stamp, const void *data);
void devdrv_host_black_box_get_exception(struct devdrv_black_box_user *black_box_user, int index);
extern int devdrv_register_black_callback(struct devdrv_black_callback *black_callback);
extern void devdrv_unregister_black_callback(const struct devdrv_black_callback *black_callback);
#endif
