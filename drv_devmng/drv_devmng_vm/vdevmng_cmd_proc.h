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

#ifndef __DEVDRV_VDEVMNG_CMD_PROC__HOST_H
#define __DEVDRV_VDEVMNG_CMD_PROC__HOST_H

int vdevdrv_manager_ioctl_get_devnum(struct file *filep, unsigned int cmd, unsigned long arg);
int vdevdrv_manager_get_devids(struct file *filep, unsigned int cmd, unsigned long arg);
int vdevdrv_manager_container_cmd(struct file *filep, unsigned int cmd, unsigned long arg);
int vdrv_get_device_boot_status(struct file *filep, unsigned int cmd, unsigned long arg);
int vdevdrv_manager_get_host_phy_mach_flag(struct file *filep, unsigned int cmd, unsigned long arg);
int vdevdrv_manager_get_probe_num(struct file *filep, unsigned int cmd, unsigned long arg);
int vdevdrv_manager_get_container_flag(struct file *filep, unsigned int cmd, unsigned long arg);
int vdevdrv_manager_get_process_sign(struct file *filep, unsigned int cmd, unsigned long arg);
int vdevdrv_get_core_inuse(struct file *filep, unsigned int cmd, unsigned long arg);
int vdevdrv_get_core_spec(struct file *filep, unsigned int cmd, unsigned long arg);
int vdevdrv_manager_get_tsdrv_dev_com_info(struct file *filep,
    unsigned int cmd, unsigned long arg);
int vdevmng_get_devinfo_from_vdev_info(struct file *filep, unsigned int cmd, unsigned long arg);
int vdevmng_get_devinfo_from_phy_machine(struct file *filep, unsigned int cmd, unsigned long arg);
int vdevdrv_manager_get_pci_info(struct file *filep, unsigned int cmd, unsigned long arg);
int vdevdrv_get_pcie_id(struct file *filep, unsigned int cmd, unsigned long arg);
int vdevdrv_manager_get_device_status(struct file *filep, unsigned int cmd, unsigned long arg);
int vdevdrv_manager_ioctl_get_plat_info(struct file *filep, unsigned int cmd, unsigned long arg);
int vdevdrv_manager_get_console_loglevel(struct file *filep, unsigned int cmd, unsigned long arg);
int vdevdrv_manager_get_device_startup_status(struct file *filep, unsigned int cmd, unsigned long arg);
int vdevdrv_manager_get_device_health_status(struct file *filep, unsigned int cmd, unsigned long arg);
int vdevdrv_manager_get_dev_resource_info(struct file *filep, unsigned int cmd, unsigned long arg);
int vdevdrv_get_error_code(struct file *filep, unsigned int cmd, unsigned long arg);
int vdevdrv_manager_not_support(struct file *filep, unsigned int cmd, unsigned long arg);
int vdevdrv_manager_not_permit(struct file *filep, unsigned int cmd, unsigned long arg);
int vdevdrv_manager_ioctl_get_chip_info(struct file *filep, unsigned int cmd, unsigned long arg);
int vdevdrv_manager_get_osc_freq(struct file *filep, unsigned int cmd, unsigned long arg);
int vdevdrv_manager_get_current_aic_freq(struct file *filep, unsigned int cmd, unsigned long arg);
int vdevdrv_get_host_kern_log(struct file *filep, unsigned int cmd, unsigned long arg);

#endif

