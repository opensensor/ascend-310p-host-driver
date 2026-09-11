/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2021. All rights reserved.
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
#ifndef DEVDRV_ATU_INTERFACE_H
#define DEVDRV_ATU_INTERFACE_H

#include <linux/types.h>
#include "devdrv_atu.h"
#include "devdrv_pci.h"

int devdrv_get_atu_info(struct devdrv_pci_ctrl *pci_ctrl, int atu_type, struct devdrv_iob_atu **atu,
    u64 *host_phy_base);
int devdrv_atu_base_to_target(struct devdrv_pci_ctrl *pci_ctrl, struct devdrv_iob_atu atu[], int num,
    u64 base_addr, u64 *target_addr);
int devdrv_atu_target_to_base(struct devdrv_pci_ctrl *pci_ctrl, struct devdrv_iob_atu atu[], int num,
    u64 target_addr, u64 *base_addr);
int devdrv_devmem_addr_d2h(u32 devid, phys_addr_t device_phy_addr, phys_addr_t *host_bar_addr);
int devdrv_devmem_addr_h2d(u32 devid, phys_addr_t host_bar_addr, phys_addr_t *device_phy_addr);
#endif
