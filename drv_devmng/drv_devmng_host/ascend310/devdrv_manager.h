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

#ifndef __DEVDRV_MANAGER_H
#define __DEVDRV_MANAGER_H

#include "devdrv_manager_common.h"
#include "devdrv_interface.h"
#include <linux/gfp.h>
#include "dms_kernel_version_adapt.h"
#include "kernel_version_adapt.h"

#ifndef __GFP_ACCOUNT
#ifdef __GFP_KMEMCG
#define __GFP_ACCOUNT __GFP_KMEMCG /* for linux version 3.10 */
#endif

#ifdef __GFP_NOACCOUNT
#define __GFP_ACCOUNT 0 /* for linux version 4.1 */
#endif
#endif

#define DEVMNG_AMP_MODE 0
#define DEVMNG_SMP_MODE 1

void *devdrv_manager_get_no_trans_chan(u32 dev_id);

struct devdrv_exception {
    struct timespec stamp;
    struct list_head list;
    u32 code;
    u32 devid;

    /* private data for some exception code */
    void *data;
};

struct devdrv_process_sign {
    pid_t hostpid;
    u32 docker_id;
    char sign[PROCESS_SIGN_LENGTH];
    struct list_head list;
};

#define DEVDRV_IPC_EVENT_DEFAULT 0
#define DEVDRV_IPC_EVENT_CREATED (1 << 1)
#define DEVDRV_IPC_EVENT_RECORDED (1 << 2)
#define DEVDRV_IPC_EVENT_RECORD_COMPLETED (1 << 3)
#define DEVDRV_IPC_EVENT_DESTROYED (1 << 4)

#define DEVDRV_IPC_NODE_DEFAULT 0
#define DEVDRV_IPC_NODE_OPENED (1 << 3)
#define DEVDRV_IPC_NODE_CLOSED (1 << 4)

#define DEVDRV_GET_DEVNUM_STARTUP_TIMEOUT 120
#define DEVDRV_GET_DEVNUM_SYNC_TIMEOUT 60

struct ipc_notify_info {
    u32 open_fd_num;
    u32 create_fd_num;

    /* created node list head */
    struct list_head create_list_head;
    struct list_head open_list_head;

    struct mutex info_mutex;
};

struct devdrv_manager_context {
    pid_t pid;
    pid_t tgid;
    pid_t current_tgid;
    u64 mnt_ns;
    struct pid_namespace *pid_ns;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
    u64 start_time;
    u64 real_start_time;
#else
#ifdef DEVDRV_MANAGER_HOST_UT_TEST
    u64 start_time;
    u64 real_start_time;
#else
    struct timespec start_time;
    struct timespec real_start_time;
#endif
#endif
    struct task_struct *task;
    struct ipc_notify_info *ipc_notify_info;
};

#define DEVDRV_BB_DEVICE_LOAD_TIMEOUT 0x68020001
#define DEVDRV_BB_DEVICE_HEAT_BEAT_LOST 0x68020002
#define DEVDRV_BB_DEVICE_RESET_INFORM 0x66020003

#define DEVDRV_FIRST_TASK_PID 1
#define DEVDRV_INIT_INSTANCE_TIMEOUT (4 * (HZ))
#define DEVDRV_INIT_RESOURCE_TIMEOUT (20 * (HZ))

void devdrv_host_black_box_init(void);
void devdrv_host_black_box_exit(void);

void devdrv_host_black_box_close_check(pid_t pid);
int devdrv_host_black_box_add_exception(u32 devid, u32 code, struct timespec stamp, const void *data);
void devdrv_host_black_box_get_exception(struct devdrv_black_box_user *black_box_user, int index);
u32 devdrv_manager_get_devnum(void);
extern int devdrv_get_pcie_id_info(u32 devid, struct devdrv_pcie_id_info *pcie_id_info);

struct devdrv_manager_info *devdrv_get_manager_info(void);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
ssize_t devdrv_load_file_read(struct file *file, loff_t *pos, char *addr, size_t count);
#endif
int devdrv_manager_send_msg(struct devdrv_info *dev_info,
    struct devdrv_manager_msg_info *dev_manager_msg_info, int *out_len);
void devdrv_manager_ops_sem_down_write(void);
void devdrv_manager_ops_sem_up_write(void);
void devdrv_manager_ops_sem_down_read(void);
void devdrv_manager_ops_sem_up_read(void);
int devdrv_manager_register(struct devdrv_info *dev_info);
void devdrv_manager_unregister(struct devdrv_info *dev_info);
struct tsdrv_drv_ops *devdrv_manager_get_drv_ops(void);

int devmng_get_vdavinci_info(u32 vdev_id, u32 *phy_id, u32 *vfid);
int hvdevmng_get_aicore_num(u32 devid, u32 fid, u32 *aicore_num);
int devdrv_manager_shm_info_check(struct devdrv_info *dev_info);

int devmng_dms_get_event_code(u32 devid, u32 *health_code, u32 health_len,
    struct shm_event_code *event_code, u32 event_len);
int devmng_dms_get_health_code(u32 devid, u32 *health_code, u32 health_len);

int dms_device_register(struct devdrv_info* dev);
void dms_device_unregister(struct devdrv_info* dev);
int devdrv_manager_devlog_dump(struct devdrv_bbox_logdump *in);

#endif /* __DEVDRV_MANAGER_H */
