/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * Description: ascend manager driver
 * Author: huawei
 * Create: 2021-06-30
 */

#ifndef __DEVDRV_MANAGER_DEV_SHARE_H__
#define __DEVDRV_MANAGER_DEV_SHARE_H__

#define DEVICE_UNSHARE 0
#define DEVICE_SHARE   1

#ifdef CFG_FEATURE_DEVICE_SHARE
int set_device_share_flag(unsigned int device_id, unsigned value);
#else
void set_device_share_flag(unsigned int device_id, unsigned value);
#endif
int get_device_share_flag(unsigned int device_id);
int devdrv_manager_config_device_share(struct file *filep, unsigned int cmd, unsigned long arg);

#endif
