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

#ifndef __DEVDRV_MANAGER_CONTAINER_NOPLUGIN_H
#define __DEVDRV_MANAGER_CONTAINER_NOPLUGIN_H

#include "devdrv_user_common.h"
#include "kernel_version_adapt.h"

/* used for no plugin container */
struct devdrv_manager_container_para {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
    u64 start_time;
    u64 real_start_time;
#else
    struct timespec start_time;
    struct timespec real_start_time;
#endif
    u32 dev_num;
    u32 dev_id[DEVDRV_MAX_DAVINCI_NUM]; /* device list read from dev directory */
    u64 mnt_ns;
    struct pid_namespace *pid_ns;
};

u64 devdrv_manager_get_host_mnt_ns(void);
int devdrv_manager_container_table_init(struct devdrv_manager_info *manager_info);
void devdrv_manager_container_table_exit(struct devdrv_manager_info *manager_info);
int devdrv_manager_container_get_davinci_devlist(struct devdrv_container_para *cmd);
int devdrv_manager_container_table_devlist_add_ns(u32 *physical_devlist, u32 physical_dev_num,
    struct mnt_namespace *mnt_ns);
int devdrv_manager_container_table_devlist_del_ns(u32 *physical_devlist, u32 physical_dev_num,
    struct mnt_namespace *mnt_ns);
int devdrv_manager_container_get_bare_pid(struct devdrv_container_para *cmd);
int devdrv_manager_container_get_bare_tgid(struct devdrv_container_para *cmd);
u32 devdrv_manager_container_get_devnum_ns(u64 mnt_ns);
int devdrv_manager_container_get_devids(u32 *devlist, u32 devlist_len, u32 *devnum, u64 mnt_ns);
int devdrv_manager_container_logical_id_to_physical_id(u32 logical_dev_id, u32 *physical_dev_id, u32 *vfid);
int devdrv_is_in_assignment(void);
int devdrv_manager_get_current_mnt_ns(u64 *current_mnt_ns);
int devdrv_manager_container_get_docker_id(u32 *docker_id);
int devdrv_manager_container_check_devid_in_container_ns(u32 devid, struct task_struct *tsk);
int devdrv_manager_container_check_devid_in_container(u32 devid, pid_t hostpid);
int devdrv_manager_container_task_struct_check(struct task_struct *tsk);
bool devdrv_manager_container_is_admin(void);
int devdrv_manager_container_table_overlap(struct devdrv_manager_context *dev_manager_context, u32 *docker_id);

#endif /* __DEVDRV_MANAGER_CONTAINER_NOPLUGIN_H */
