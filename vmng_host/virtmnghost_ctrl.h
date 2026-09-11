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

#ifndef VIRTMNGHOST_CTRL_H
#define VIRTMNGHOST_CTRL_H

#include "virtmng_interface.h"

#include "ka_task_pub.h"
#include "ka_system_pub.h"

#define VMNGH_VM_DEV_VALID 1
#define VMNGH_VM_DEV_INVALID 0

#define VMNGH_DESTORY_ALL_VDEV -1

#define VMNGH_DTYPE_TO_AICORE_NUM(dtype) (0x01 << (dtype))

struct vmngh_vm_info {
    u32 devid;
    u32 fid;
    u32 valid;
};

struct vmngh_vm_ctrl {
    struct vmngh_vm_info vm_info[VMNG_PDEV_MAX];
    u32 vm_pid;
    u32 pdev_cnt;
    u32 vm_id;
};

struct vmngh_clear_timer {
    ka_hrtimer_t timer;
    ka_ktime_t kt;
    int vaild_dev;
    ka_spinlock_t bandwidth_update_lock;
};

struct vmngh_ctrl_ops {
    int (*enable_sriov)(u32 dev_id);
    int (*disable_sriov)(u32 dev_id);
    int (*alloc_vfid)(u32 dev_id, u32 dtype, u32 *fid);
    void (*free_vfid)(u32 dev_id, u32 vfid);
    int (*alloc_vf)(u32 dev_id, u32 *fid, u32 dtype, struct vmng_vf_res_info *vf_resource);
    int (*free_vf)(u32 dev_id, u32 vfid);
    int (*reset_vf)(u32 dev_id, u32 vfid);
    int (*enquire_vf)(u32 dev_id, u32 vfid, struct vmng_soc_resource_enquire *info);
    int (*refresh_vf)(u32 dev_id, u32 vfid, struct vmng_soc_resource_refresh *info);
    int (*container_client_online)(u32 dev_id, u32 vfid);
    int (*container_client_offline)(u32 dev_id, u32 vfid);
    int (*is_vf)(u32 dev_id);
    int (*unify_devid_to_pfvf_id)(u32 dev_id, u32 *pf_id, u32 *vf_id);
    int valid;
};

struct vmngh_vdev_ctrl {
    void *vd_dev;
    void *vdavinci;
    enum vmng_startup_flag_type startup_flag;
    enum vmng_pf_sriov_status sriov_status;
    int sriov_mode;
    struct vmng_vdev_ctrl vdev_ctrl;
    ka_mutex_t reset_mutex;
    struct vmng_vf_memory_info memory;
    struct vmngh_client_instance client_instance[VMNG_CLIENT_TYPE_MAX];
};

struct vmngh_device_ctrl {
    enum vmng_split_mode split_mode;
    u32 total_core_num;
    struct vmngh_ctrl_ops ctrl_ops;
    struct vmngh_vdev_ctrl vdev_ctrl[VMNG_VDEV_MAX_PER_PDEV];
};

struct vmngh_vd_dev *vmngh_get_top_half_vdev_by_id(u32 dev_id, u32 fid);
struct vmngh_vd_dev *vmngh_get_bottom_half_vdev_by_id(u32 dev_id, u32 fid);

void vmngh_ctrl_set_startup_flag(u32 dev_id, u32 fid, enum vmng_startup_flag_type flag);
int vmngh_get_ctrl_startup_flag(u32 dev_id, u32 fid);
void vmngh_ctrl_register_half(struct vmngh_vd_dev *vd_dev);
void vmngh_ctrl_unregister_half(struct vmngh_vd_dev *vd_dev);
int vmngh_register_ctrls(struct vmngh_vd_dev *vd_dev);
void vmngh_unregister_ctrls(u32 dev_id, u32 fid);

int vmngh_init_instance_after_probe(u32 dev_id, u32 fid);
int vmngh_uninit_instance_remove_vdev(u32 dev_id, u32 fid);
void vmngh_suspend_instance_remove_vdev(u32 dev_id, u32 fid);
void vmngh_suspend_instance_remove_pdev(u32 dev_id);
void vmngh_host_stop(u32 dev_id, u32 fid);

int vmngh_init_ctrl(void);
void vmngh_uninit_ctrl(void);
int vmngh_alloc_vm_id(u32 dev_id, u32 fid, u32 vm_pid, u32 vm_devid);
void vmngh_ctrl_set_vm_id(u32 dev_id, u32 fid, u32 vm_id);
int vmngh_rm_vm_id(u32 dev_id, u32 fid);
void vmngh_ctrl_rm_vm_id(u32 dev_id, u32 fid, u32 vm_devid);
u32 vmngh_get_total_core_num(u32 dev_id);
void vmngh_bw_ctrl_info_init(u32 dev_id);
void vmngh_bw_ctrl_info_uninit(u32 dev_id);
int vmngh_bw_set_token_limit(u32 dev_id, u32 vfid);
int vmngh_get_map_info_client(struct vmngh_client_instance *instance_para,
    enum vmng_client_type client_type, struct vmngh_map_info *client_map_info);
int vmngh_put_map_info_client(struct vmngh_client_instance *instance_para,
    enum vmng_client_type client_type);
int vmngh_init_instance_client_device(u32 dev_id, u32 vfid);
int vmngh_uninit_instance_client_device(u32 dev_id, u32 vfid);
void vmngh_bw_data_clear_timer_init(u32 dev_id, u32 vfid);
void vmngh_bw_data_clear_timer_uninit(void);
int vmngh_init_ctrl_ops(u32 dev_id);
int vmngh_uninit_ctrl_ops(u32 dev_id);
int vmngh_alloc_vfid(u32 dev_id, u32 *fid);
void vmngh_free_vdev_ctrl(u32 dev_id, u32 vfid);
int vmngh_init_instance_all_client(u32 dev_id, u32 fid, u32 vdev_type);
int vmngh_uninit_instance_all_client(u32 dev_id, u32 fid);
struct vmngh_ctrl_ops *vmngh_get_ctrl_ops(u32 dev_id);
struct vmngh_vdev_ctrl *vmngh_get_ctrl(u32 dev_id, u32 vfid);
bool is_sriov_enable(u32 dev_id);
ka_mutex_t *vmngh_get_ctrl_mutex(void);
int vmngh_common_enable_sriov(u32 dev_id);
void vmng_set_device_split_mode(u32 dev_id, enum vmng_split_mode split_mode);

#endif
