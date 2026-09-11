/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
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
 * Description: suspend and resume in the pm system
 * Author: huawei
 * Create: 2023-12-22
 */
#ifndef DEVDRV_PM_H
#define DEVDRV_PM_H

#define WAIT_SUSPEND_SLEEP 10

#define WAIT_RESUME_NOTIFY_TIMEOUT 5000

#define DEVDRV_SENTRY_CHECK_EP_SUSPEND 0
#define DEVDRV_HANDSHAKE_GET_EP_SUSPEND 1

int drv_pcie_suspend(struct device *dev);
void devdrv_resume_device(void);
int drv_pcie_resume_notify(struct device *dev);
void devdrv_load_half_resume(struct devdrv_pci_ctrl *pci_ctrl);
int devdrv_dev_get_suspend_status(struct devdrv_pci_ctrl *pci_ctrl);
bool devdrv_is_sentry_work_mode(void);
bool devdrv_get_fault_proc_status(u32 flag);

#endif