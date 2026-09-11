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

#ifndef _RES_DRV_H_
#define _RES_DRV_H_

#include "devdrv_pci.h"

#define NETWORK_PF_0 0
#define NETWORK_PF_1 1

#define PCIE_PF_NUM 2
#define PCIE_PF_0 0
#define PCIE_PF_1 1

#define PCIE_BAR_0 0
#define PCIE_BAR_2 2
#define PCIE_BAR_4 4

int devdrv_res_init(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_res_uninit(struct devdrv_pci_ctrl *pci_ctrl);

#endif
