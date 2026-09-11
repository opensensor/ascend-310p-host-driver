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

#ifndef DEVMM_VM_VPC_H
#define DEVMM_VM_VPC_H

#include <linux/types.h>
#include <linux/device.h>
#include "virtmng_interface.h"
#include "devmm_proc_info.h"


#define DEVMM_VALID 1
#define DEVMM_INVALID 0

struct devmm_vm_dev_info {
    u32 valid;
    u32 pm_dev_id;
    u32 pm_fid;
    ka_device_t *dev;
};

extern struct devmm_vm_dev_info svm_vm_dev_info[DEVMM_MAX_DEVICE_NUM];

int devmm_register_ops_init(void);

void devmm_unregister_ops_uninit(void);

int devmm_add_pid_into_business(uint32_t devid, ka_pid_t pid);
void devmm_remove_pid_from_business(uint32_t devid, ka_pid_t pid);
void devmm_remove_pid_from_all_business(ka_pid_t pid);

ka_device_t *devmm_device_get_by_devid(u32 dev_id);
void devmm_device_put_by_devid(u32 dev_id);

bool devmm_device_is_ready(u32 dev_id);
int devmm_vm_vpc_mng_msg_send(void *msg, unsigned int len, unsigned int out_len);

#endif /* _DEVMM_VM_VPC_H_ */
