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

#ifndef DEVDRV_MANAGER_CONTAINER_H
#define DEVDRV_MANAGER_CONTAINER_H

#include <linux/mutex.h>
#include <linux/nsproxy.h>

#include "devdrv_manager_common.h"
#include "devdrv_user_common.h"

#define UUID_NUM_SIZE (16)

int devdrv_manager_container_process(struct file *filep, unsigned long arg);
int devdrv_virtual_id_to_physical_id(u32 virtual_id, u32 *physical_id);
int devdrv_is_in_container(void);
int devdrv_manager_container_is_in_container(void);
int devdrv_manager_container_get_docker_id(u32 *docker_id);
int devdrv_manager_container_is_in_admin_container(void);
int devdrv_manager_container_logical_id_to_physical_id(u32 logical_dev_id, u32 *physical_dev_id, u32 *vfid);

#endif
