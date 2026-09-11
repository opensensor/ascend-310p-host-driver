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

#ifndef VIRTMNGHOST_SYSFS_H
#define VIRTMNGHOST_SYSFS_H

#include "ka_pci_pub.h"

#define VMNG_VDEV_AGENT_BDF_SHIFT_8 8

int vmngh_sysfs_init(u32 dev_id, ka_pci_dev_t *pcidev);
void vmngh_sysfs_exit(u32 dev_id, ka_pci_dev_t *pcidev);
#endif
