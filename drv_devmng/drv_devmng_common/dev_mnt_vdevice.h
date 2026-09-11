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

#ifndef DEVDRV_MANAGER_VDEVICE_H
#define DEVDRV_MANAGER_VDEVICE_H

#include <linux/fs.h>
#include "devdrv_user_common.h"

#define VDAVINCI_IDLE 0
#define VDAVINCI_INIT 2
#define VDAVINCI_USED 3
#define VDAVINCI_STATE_INUSED 1
#define VDAVINCI_STATE_IDLE 0

#define NONE_ROOT_ACCESS 0660

enum MNT_VDAVINCI_TYPE {
    MNT_VDAVINCI_TYPE_C1,
    MNT_VDAVINCI_TYPE_C2,
    MNT_VDAVINCI_TYPE_C4,
    MNT_VDAVINCI_TYPE_C8,
    MNT_VDAVINCI_TYPE_C16,
    MNT_VDAVINCI_TYPE_C1_4 = 10, /* It is for reserved between MNT_VDAVINCI_TYPE_C16 and 10. */
    MNT_VDAVINCI_TYPE_C2_4,
    MNT_VDAVINCI_TYPE_MAX
};

struct dev_mnt_vdev_action {
    unsigned int vdev_id;
    vdev_action action;
    struct mnt_namespace *ns;
    u64 container_id;
    struct list_head list_node;
};

struct dev_mnt_vdev_inform {
    struct list_head vdev_action_head;
    spinlock_t spinlock;
};

struct vdavinci_info {
    void *ns;              /* ns namespace pointer */
    u32 used;              /* whether the vdevice is used */
    u32 phy_devid;         /* which physical device alloc the vdevice */
    u32 vfid;              /* alloced by vmanager */
    u32 container_devid;   /* container logic_devid */
    struct devdrv_vdev_spec_info spec_info; /* specification of the vdevice */
    u64 container_id;
};

typedef struct dev_mnt_vdev_engine {
    u32 alloc_state;
    struct mutex lock;
    struct cdev cdev;
    struct device *dev;
    struct vdavinci_info info;
} DEV_MNT_VDEV_ENGINE_T;

typedef struct dev_mnt_vdev_cb {
    unsigned int vdev_major;
    struct class *vdev_class;
    struct mutex global_lock;
    DEV_MNT_VDEV_ENGINE_T vdev[MAX_VDEV_NUM];
} DEV_MNT_VDEV_CB_T;

int dev_mnt_vdevice_init(void);
int dev_mnt_vdevice_add_inform(unsigned int vdev_id, vdev_action action, struct mnt_namespace *ns, u64 container_id);
int dev_mnt_create_one_vm_vdevice(unsigned int phy_id, unsigned int fid);
void dev_mnt_vdevice_uninit(void);
void dev_mnt_vdevice_inform(void);
void dev_mnt_released_one_vm_vdevice(unsigned int phy_id, unsigned int fid);

int devdrv_manager_ioctl_create_vdev(struct file *filep, unsigned int cmd, unsigned long arg);
int devdrv_manager_ioctl_destroy_vdev(struct file *filep, unsigned int cmd, unsigned long arg);
int devdrv_manager_ioctl_get_vdevinfo(struct file *filep, unsigned int cmd, unsigned long arg);
#ifndef CFG_FEATURE_VFIO_SOC
int devdrv_manager_get_vdev_resource_info(struct devdrv_resource_info *dinfo);
#endif
#endif
