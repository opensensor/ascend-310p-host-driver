/*
* Copyright (c) Huawei Technologies Co., Ltd. 2019-2023. All rights reserved.
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

#ifndef ESCHED_HOST_MSG_H
#define ESCHED_HOST_MSG_H

int esched_drv_remote_add_pid(u32 dev_id, u32 host_ctrl_pid, u32 pid_type, u32 pid);
int esched_drv_remote_del_pid(u32 dev_id, u32 host_ctrl_pid, u32 pid);
int esched_drv_remote_add_pool(u32 dev_id, u32 cpu_type);
int esched_drv_remote_get_cpu_mbid(u32 dev_id, u32 cpu_type, u32 *mb_id, u32 *wait_mb_id);
int esched_drv_remote_add_mb(u32 dev_id, u32 vf_id);
int esched_drv_remote_config_intr(u32 dev_id, u32 irq);

#endif
