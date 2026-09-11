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

#ifndef DEVMM_PM_VPC_H
#define DEVMM_PM_VPC_H

#include "virtmng_interface.h"
#include "devmm_proc_info.h"

#define DEVMM_VALID 1
#define DEVMM_INVALID 0

#define DEVMM_VM_MAX_NUM 128
#define DEVMM_VM_DEV_MAX_NUM VMNG_PDEV_MAX
#define DEVMM_VM_MAX_SVM_PROC_NUM 2048

#define DEVMM_PM_DEV_MAX_NUM DEVMM_MAX_DEVICE_NUM
#define DEVMM_PM_DEV_FID_MAX_NUM VMNG_VDEV_MAX_PER_PDEV

#define DEVMM_VDEV_MAX_CONVERT_LEN_DEFAULT 0x2000000000UL /* 128G */
#define DEVMM_SET_ALL_DEV_CONVERT_LEN 0xFFFFFFFFUL

struct devmm_vm_map {
    u32 valid;
    u32 pm_dev_id;
    u32 pm_fid;
};

struct devmm_vm_info {
    struct devmm_vm_map map[DEVMM_VM_DEV_MAX_NUM];
    u32 init_cnt;
    ka_rw_semaphore_t channel_lock;
    ka_mutex_t proc_lock;
    ka_atomic_t svm_proc_num;
};

struct devmm_dev_fid_map {
    u32 valid;
    u32 vm_dev_id;
};

struct devmm_pm_dev_info {
    struct devmm_dev_fid_map map[DEVMM_PM_DEV_FID_MAX_NUM];
};

extern struct devmm_vm_info svm_vm_info[DEVMM_VM_MAX_NUM];

struct devmm_pm_vpc_handlers {
    int (*const msg_process)(struct devmm_svm_process *svm_proc, u32 fid, struct vmng_rx_msg_proc_info *proc_info);
    u32 msg_size;
    u32 extend_size;
};

int vdevmmh_init(void);
void vdevmmh_uninit(void);

bool devmm_is_pm_dev_fid_valid(u32 dev_id, u32 fid);
bool devmm_is_vm_dev_valid(u32 vm_id, u32 vm_dev_id);
u32 devmm_get_dev_id_from_vm_dev_id(u32 vm_id, u32 vm_dev_id);
u32 devmm_get_vm_dev_id(u32 dev_id, u32 fid);

int devmm_pm_vpc_msg_send(u32 dev_id, u32 fid, void *msg, u32 len, u32 out_len);
int devmm_pm_vpc_mng_msg_send(u32 dev_id, u32 fid, void *msg, u32 len, u32 out_len);

int devmm_pm_svm_proc_test_and_inc(u32 process_vm_id);
void devmm_pm_svm_proc_sub(u32 process_vm_id);

int devmm_pm_idle_convert_len_sub(u32 process_vm_id, u32 devid, u32 vfid, u64 len);
void devmm_pm_idle_convert_len_add(u32 process_vm_id, u32 devid, u32 vfid, u64 len);

u32 devmm_get_vm_id_by_process_vm_id(u32 process_vm_id);
u32 devmm_get_process_vm_id_by_vm_id(u32 vm_id);

#endif /* _DEVMM_PM_VPC_H_ */
