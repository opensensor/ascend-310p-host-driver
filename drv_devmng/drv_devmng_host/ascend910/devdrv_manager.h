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

#ifndef DEVDRV_MANAGER_H
#define DEVDRV_MANAGER_H

#include "devdrv_manager_common.h"
#include "devdrv_interface.h"
#include "securec.h"
#include <linux/gfp.h>
#include "kernel_version_adapt.h"

#ifndef __GFP_ACCOUNT
#ifdef __GFP_KMEMCG
#define __GFP_ACCOUNT __GFP_KMEMCG /* for linux version 3.10 */
#endif

#ifdef __GFP_NOACCOUNT
#define __GFP_ACCOUNT 0 /* for linux version 4.1 */
#endif
#endif

void *devdrv_manager_get_no_trans_chan(u32 dev_id);

struct devdrv_exception {
    struct timespec stamp;
    struct list_head list;
    u32 code;
    u32 devid;

    /* private data for some exception code */
    void *data;
};

#define DEVDRV_GET_DEVNUM_STARTUP_TIMEOUT 120
#define DEVDRV_GET_DEVNUM_SYNC_TIMEOUT 60

#define DEVDRV_IPC_EVENT_DEFAULT 0
#define DEVDRV_IPC_EVENT_CREATED (1 << 1)
#define DEVDRV_IPC_EVENT_RECORDED (1 << 2)
#define DEVDRV_IPC_EVENT_RECORD_COMPLETED (1 << 3)
#define DEVDRV_IPC_EVENT_DESTROYED (1 << 4)

#define DEVDRV_IPC_NODE_DEFAULT 0
#define DEVDRV_IPC_NODE_OPENED (1 << 3)
#define DEVDRV_IPC_NODE_CLOSED (1 << 4)

#define DEVDRV_INIT_INSTANCE_TIMEOUT (4 * HZ)
#define DEVDRV_INIT_RESOURCE_TIMEOUT (180 * (HZ))

#define DEVDRV_VMCORE_MAX_SIZE   0xFFFFFFFF   /* 4GB - 1 */

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
    u32 docker_id;
    struct mnt_namespace *mnt_ns;
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

typedef enum {
    DEVDRV_CAP_IMU_REG_EXPORT = 0,
    DEVDRV_CAP_MAX,
} devdrv_capability_type;

typedef enum {
    VDEV_BIND = 0,
    VDEV_UNBIND,
    VDEV_INVALID_ACTION,
} vdev_action;

#define DEVDRV_BB_DEVICE_LOAD_TIMEOUT 0x68020001
#define DEVDRV_BB_DEVICE_HEAT_BEAT_LOST 0x68020002
#define DEVDRV_BB_DEVICE_RESET_INFORM 0x68020003

/* flag for amp or smp */
#define DEVMNG_AMP_MODE 0
#define DEVMNG_SMP_MODE 1

int devdrv_manager_ipc_notify_create(void *context, unsigned long arg, void *notify_ioctl_info);
int devdrv_manager_ipc_notify_open(void *context, unsigned long arg, void *notify_ioctl_info);
int devdrv_manager_ipc_notify_close(void *context, void *notify_ioctl_info);
int devdrv_manager_ipc_notify_destroy(void *context, void *notify_ioctl_info);
int devdrv_manager_ipc_notify_set_pid(void *context, void *notify_ioctl_info);

struct devdrv_manager_info *devdrv_get_manager_info(void);
u32 devdrv_manager_get_ts_num(struct devdrv_info *dev_info);
int devdrv_manager_send_msg(struct devdrv_info *dev_info, struct devdrv_manager_msg_info *dev_manager_msg_info,
    int *out_len);
extern struct task_struct init_task;
extern int devdrv_get_pcie_id_info(u32 devid, struct devdrv_pcie_id_info *pcie_id_info);
int devdrv_agent_sync_msg_send(u32 dev_id, struct devdrv_manager_msg_info *msg_info, u32 payload_len, u32 *out_len);
u32 devdrv_manager_get_devnum(void);
struct tsdrv_drv_ops *devdrv_manager_get_drv_ops(void);
struct devdrv_info *devdrv_get_devdrv_info_array(u32 dev_id);
int devmng_get_vdavinci_info(u32 vdev_id, u32 *phy_id, u32 *vfid);
int dev_mnt_vdevice_add_inform(unsigned int vdev_id,
    vdev_action action, struct mnt_namespace *ns, u64 container_id);
void dev_mnt_vdevice_inform(void);

int devdrv_manager_check_capability(u32 dev_id, devdrv_capability_type type);
void devdrv_manager_ops_sem_down_write(void);
void devdrv_manager_ops_sem_up_write(void);
void devdrv_manager_ops_sem_down_read(void);
void devdrv_manager_ops_sem_up_read(void);
int devdrv_manager_register(struct devdrv_info *dev_info);
void devdrv_manager_unregister(struct devdrv_info *dev_info);

int devdrv_manager_get_amp_smp_mode(u32 *amp_or_smp);
int devdrv_manager_shm_info_check(struct devdrv_info *dev_info);

int devmng_dms_get_event_code(u32 devid, u32 *health_code, u32 health_len,
    struct shm_event_code *event_code, u32 event_len);
int devmng_dms_get_health_code(u32 devid, u32 *health_code, u32 health_len);
#ifdef CFG_FEATURE_CHIP_DIE
int devdrv_manager_get_random_from_dev_info(u32 devid, char *random_number, u32 random_len);
#endif

int dms_device_register(struct devdrv_info* dev);
void dms_device_unregister(struct devdrv_info* dev);
int hw_dvt_get_mode(int *mode);
int hw_dvt_set_mode(int mode);
int devdrv_wait_device_ready(u32 dev_id, u32 timeout_second);
int devdrv_manager_devlog_dump(struct devdrv_bbox_logdump *in);
void devdrv_check_pid_map_process_sign(pid_t tgid, u64 start_time);
#endif /* __DEVDRV_MANAGER_H */
