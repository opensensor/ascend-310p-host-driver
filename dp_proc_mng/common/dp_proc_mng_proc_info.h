/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
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
 * Create: 2022-08-13
 */
#ifndef DP_PROC_MNG_PROC_INFO_H
#define DP_PROC_MNG_PROC_INFO_H

#include <linux/types.h>
#include <linux/fs.h>
#include <linux/hashtable.h>

#include "dp_proc_mng_cmd.h"
#include "dp_proc_mng_log.h"
#include "ascend_hal_define.h"
#include "devdrv_manager_comm.h"

#include "ka_list_pub.h"
#include "ka_task_pub.h"
#include "ka_fs_pub.h"
#include "ka_ioctl_pub.h"
#include "ka_base_pub.h"
#include "ka_errno_pub.h"

#ifndef __GFP_ACCOUNT
#ifdef __GFP_KMEMCG
#define GFP_ACCOUNT __GFP_KMEMCG /* for linux version 3.10 */
#endif

#ifdef __GFP_NOACCOUNT
#define GFP_ACCOUNT 0 /* for linux version 4.1 */
#endif
#else
#define GFP_ACCOUNT __GFP_ACCOUNT
#endif

struct dp_proc_mng_info {
    ka_list_head_t bind_cgroup_list;
    ka_mutex_t bind_cgroup_list_lock;
    struct work_struct bind_cgroup_work;
    struct workqueue_struct *bind_cgroup_wq;
};

struct dp_proc_mng_ctx {
    int pid;
    s64 unique_id;
};

extern int (* const dp_proc_mng_ioctl_handlers[DP_PROC_MNG_CMD_MAX_CMD])(ka_file_t *file,
    struct dp_proc_mng_ioctl_arg *arg);
extern int (* const dp_proc_mng_exit_handlers)(struct dp_proc_mng_ctx *proc_ctx);

int dp_proc_mng_davinci_module_init(const ka_file_operations_t *ops);
void dp_proc_mng_davinci_module_uninit(void);

struct dp_proc_mng_info *dp_proc_get_manager_info(void);

int dp_proc_mng_info_init(void);
void dp_proc_mng_info_unint(void);

int dp_proc_mng_create_work(void);
void dp_proc_mng_destroy_work(void);

extern int devdrv_manager_container_logical_id_to_physical_id(u32 logical_dev_id, u32 *physical_dev_id, u32 *vfid);
extern int devdrv_manager_container_is_in_container(void);
extern int devdrv_manager_container_get_docker_id(u32 *docker_id);

int dp_proc_mng_convert_id_from_vir_to_phy(struct dp_proc_mng_ioctl_arg *buffer);

#endif
