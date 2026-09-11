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
#ifndef AOS_LLVM_BUILD
#include <linux/nsproxy.h>
#endif
#include "devdrv_manager_common.h"
#include "devdrv_user_common.h"
#include "devdrv_manager.h"
#include "kernel_version_adapt.h"

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
    struct mnt_namespace *mnt_ns;
    struct pid_namespace *pid_ns;
    u64 container_id;
};
extern struct devdrv_manager_info *dev_manager_info;
struct mnt_namespace *devdrv_manager_get_host_mnt_ns(void);
int devdrv_manager_container_table_init(struct devdrv_manager_info *manager_info);
void devdrv_manager_container_table_exit(struct devdrv_manager_info *manager_info);
int devdrv_manager_container_logical_id_to_physical_id(u32 logical_dev_id, u32 *physical_dev_id, u32 *vfid);
int devdrv_manager_container_get_docker_id(u32 *docker_id);
int devdrv_manager_container_process(struct file *filep, unsigned long arg);
#define devdrv_manager_container_is_in_normal_container() devdrv_manager_container_is_in_container()
int devdrv_manager_container_is_in_container(void);
int devdrv_manager_container_is_in_admin_container(void);
int devdrv_manager_container_check_devid_in_container(u32 devid, pid_t hostpid);
bool devdrv_manager_container_is_admin(void);
int devdrv_devpid_container_convert(int *ipid);
bool devdrv_manager_container_is_host_system(struct mnt_namespace *mnt_ns);
#if defined(CFG_HOST_ENV) || defined(CFG_FEATURE_BIND_TGID)
int devdrv_get_tgid_by_pid(int pid, int *tgid);
#endif

#endif
