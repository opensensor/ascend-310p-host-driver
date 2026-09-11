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

#ifndef DEVDRV_MANAGER_PID_MAP_H
#define DEVDRV_MANAGER_PID_MAP_H

#include "ka_list_pub.h"
#include "ka_hashtable_pub.h"
#include "ka_system_pub.h"
#include "ka_fs_pub.h"

#include "devdrv_manager_comm.h"
#include "devdrv_user_common.h"
#include "devdrv_manager_common.h"

#if defined(CFG_SOC_PLATFORM_MDC_V51) || defined(CFG_FEATURE_VF_USE_DEVID)
#define VFID_NUM_MAX 1
#else
#define VFID_NUM_MAX 32  /* virtual function id max num */
#endif

#define DEVMNG_PID_INVALID (-1)
#define DEVMNG_PID_START_ONCE (-2)

enum tagAicpufwPlat {
    AICPUFW_ONLINE_PLAT = 0,
    AICPUFW_OFFLINE_PLAT,
    AICPUFW_MAX_PLAT,
};

/* side: host 1 device 0 */
#define DEVICE_SIDE 0
#define HOST_SIDE 1
#define DEVMNG_USER_PROC_MAX 256

#define HAL_BIND_ALL_DEVICE 0xffffffffU

#define DELETE_PID 0
#define ADD_PID 1

#ifdef CFG_HOST_ENV
#define PID_MAP_DEVNUM 64
#else
#define PID_MAP_DEVNUM DEVDRV_MAX_NODE_NUM
#endif

struct devdrv_process_user_info {
    u32 valid;
    u32 devid; /* local udevid */
    u32 vfid;
    u32 mode;
    pid_t pid;
    u64 start_time;
    u32 dev_num; /* devid为HAL_BIND_ALL_DEVICE时，需要将对应的udevid存起来 */
    u32 udevids[PID_MAP_DEVNUM];
};

struct devdrv_slave_info {
    u32 mode;
    pid_t slave_pid;
    u64 pid_start_time;
};

struct devdrv_process_sign {
    pid_t hostpid;
    u64 hostpid_start_time;
    u32 host_process_status;
    struct devdrv_process_user_info user_proc_host[DEVMNG_USER_PROC_MAX];
    struct devdrv_process_user_info user_proc_device[DEVMNG_USER_PROC_MAX];
    struct devdrv_slave_info devpid[PID_MAP_DEVNUM][VFID_NUM_MAX][DEVDRV_PROCESS_CPTYPE_MAX];
    char sign[PROCESS_SIGN_LENGTH];
    u32 cp_count; /* count cp1 & dev_only devpid num for releasing sign */
    u32 sync_proc_cnt;
    u32 docker_id;
    u32 in_use_count;
    ka_list_head_t list;
#ifndef DEVMNG_UT
    ka_hlist_node_t link; /* hash find link */
#endif
};

struct bind_cost_statistics {
    /* total cost time */
    ka_ktime_t bind_start;
    ka_ktime_t bind_end;

    /* Sub-process cost time */
    /* check master form host */
    ka_ktime_t check_master_start;
    ka_ktime_t check_master_end;

    /* check slave process is initialized */
    ka_ktime_t check_slave_start;
    ka_ktime_t check_slave_end;

    /* sync msg to host  */
    ka_ktime_t sync_start;
    ka_ktime_t sync_end;

    /* update msg to hash */
    ka_ktime_t update_hash_start;
    ka_ktime_t update_hash_end;
};

int devdrv_bind_hostpid(struct devdrv_ioctl_para_bind_host_pid para_info, struct bind_cost_statistics *cost_stat);
int devdrv_unbind_hostpid(struct devdrv_ioctl_para_bind_host_pid para_info);
void devdrv_manager_process_sign_release(pid_t devpid);
void devdrv_manager_free_hashtable(void);
int devdrv_fop_query_host_pid(ka_file_t *filep, unsigned int cmd, unsigned long arg);
int devdrv_notice_process_exit(u32 dev_id, u32 host_pid);
#ifdef CFG_HOST_ENV
int devdrv_pid_map_sync_proc(void *msg, u32 *ack_len);
#else
int devdrv_pid_map_sync_proc(u32 devid, void *msg, u32 in_len, u32 *ack_len);
#endif

int devdrv_query_slave_by_map_info(const devdrv_pid_map_info_t *q_info, pid_t *slave_pid, int *mode);
void devdrv_put_dev_process(pid_t devpid);
void devdrv_release_pid_with_start_time(struct devdrv_process_sign *d_sign, pid_t devpid, u64 start_time,
    ka_list_head_t *free_list, int *release_flag);
void devdrv_release_try_to_sync_to_peer(pid_t slave_pid);
void bind_cost_print(struct bind_cost_statistics *cost_stat);
#endif
