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

#ifndef DEVDRV_PCIE_LINK_INFO_H
#define DEVDRV_PCIE_LINK_INFO_H

#include "devdrv_interface.h"

void devdrv_set_pcie_channel_status(u32 status);
u32 devdrv_get_pcie_channel_status(void);
void devdrv_set_peer_pcie_status(u32 status);
u32 devdrv_get_peer_pcie_status(void);
int devdrv_set_err_out_gpio(void);
void devdrv_peer_ctrl_init(void);

typedef enum {
    DEVDRV_PCIE_LINK_STATUS_OK          = 0,
    DEVDRV_PCIE_LINK_STATUS_DOWN        = 1,
    DEVDRV_PCIE_LINK_STATUS_CHANNEL_ERR = 2,
} DEVDRV_PCIE_LINK_STATUS;

typedef enum {
    DEVDRV_PCIE_COMMON_CHANNEL_INIT  = 0,
    DEVDRV_PCIE_COMMON_CHANNEL_LINKDOWN = 1, /* linkdown must be 1 same as soc_misc */
    DEVDRV_PCIE_COMMON_CHANNEL_SUSPEND = 2,
    DEVDRV_PCIE_COMMON_CHANNEL_RESUME = 3,
    DEVDRV_PCIE_COMMON_CHANNEL_DEAD = 4,
    DEVDRV_PCIE_COMMON_CHANNEL_HALF_PROBE = 5,
    DEVDRV_PCIE_COMMON_CHANNEL_OK = 6,
} DEVDRV_PCIE_CHANNEL_STATUS;

/*             pcie mac link            */
/****************************************/
#ifdef CFG_SOC_PLATFORM_MDC_V51_LITE
#define PCIE_BASE_ADDR 0xA2800000U
#define PCIE_REG_SIZE 0x200000U
#define CORE_NUM 1
#define PORT_NUM 0
#define PCIE_MAC_REG_BASE (0x80000 * (CORE_NUM + 1) + 0x7000 + (0x4000 * PORT_NUM * 2))
#define PCIE_CORE_GLOBAL_REG_BASE (0x80000 * (CORE_NUM + 1) + (0x4000 * PORT_NUM * 2))
#define PCIE_PORT_EN_OFFSET 0x4U
#else
#define PCIE_BASE_ADDR 0xA0200000U
#define PCIE_REG_SIZE 0x100000
#define PCIE_MAC_REG_BASE 0x70000U
#define PCIE_CORE_GLOBAL_REG_BASE 0x5E000U
#define PCIE_PORT_EN_OFFSET 0x400U
#endif
#define PCIE_MAC_REG_LINK_ADDR 0x60U
#define PCIE_MAC_REG_LINK_LTSSM_ST_OFFSET 24U
#define PCIE_MAC_REG_LINK_LTSSM_L0 16U
#define PCIE_MAC_REG_LINK_SPEED_OFFSET 8U
#define PCIE_MAC_REG_MAC_INT_MASK_OFFSET 0x58U

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0)
#define PCIE_ERR_OUT_GPIO5_00   432
#define PCIE_ERR_IN_GPIO5_01    433
#else
#define PCIE_ERR_OUT_GPIO5_00   572U
#define PCIE_ERR_IN_GPIO5_01    573U
#endif

#endif