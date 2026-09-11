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

#ifndef __DMS_HOTRESET_H
#define __DMS_HOTRESET_H
#include <linux/rwsem.h>
#include "dms_template.h"
#include "devdrv_common.h"

extern int devdrv_manager_container_logical_id_to_physical_id(u32 logical_dev_id, u32 *physical_dev_id, u32 *vfid);
struct devdrv_info *devdrv_manager_get_devdrv_info(u32 dev_id);

#define DMS_MODULE_BASIC_POWER_INFO "dms_basic_power"
INIT_MODULE_FUNC(DMS_MODULE_BASIC_POWER_INFO);
EXIT_MODULE_FUNC(DMS_MODULE_BASIC_POWER_INFO);

#define HOTRESET_TASK_FLAG_BIT 0
#define ALL_DEVICE_HOTRESET_FLAG 0xff

#ifdef CFG_FEATURE_OLD_ALL_DEVICE_RESET_FLAG
#define DEVDRV_RESET_ALL_DEVICE_ID 0xff
#else
#define DEVDRV_RESET_ALL_DEVICE_ID 0xffffffff
#endif

struct hotreset_task_info {
    unsigned long task_flag;
    unsigned long task_ref_cnt;
    struct rw_semaphore task_rw_sema;
};

int dms_power_hotreset_init(void);
void dms_power_hotreset_exit(void);
int dms_hotreset_task_init(unsigned int dev_id);
#ifdef CFG_FEATURE_SRIOV
void dms_hotreset_vf_task_exit(unsigned int dev_id);
#endif
void dms_hotreset_task_exit(void);

int dms_hotreset_task_cnt_increase(unsigned int dev_id);
void dms_hotreset_task_cnt_decrease(unsigned int dev_id);

int dms_notify_device_hotreset(unsigned int dev_id);
int dms_notify_pre_device_hotreset(unsigned int dev_id);
int dms_power_pre_hotreset(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len);
int dms_power_pcie_hot_reset(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len);
int dms_power_pcie_pre_reset(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len);

void dms_notify_single_device_cancel_hotreset(unsigned int dev_id);
int dms_notify_all_device_pre_hotreset(void);
int dms_notify_single_device_pre_hotreset(unsigned int dev_id);
int dms_power_check_phy_mach(unsigned int dev_id);
int devdrv_uda_one_dev_ctrl_hotreset(u32 udevid);
void devdrv_uda_one_dev_ctrl_hotreset_cancel(u32 udevid);

#endif
