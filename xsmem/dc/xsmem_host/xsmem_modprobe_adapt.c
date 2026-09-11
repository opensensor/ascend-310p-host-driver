/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
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
 * Create: 2022-08-13
 */
#ifndef EMU_ST
#include "ka_kernel_def_pub.h"
#include "ka_pci_pub.h"

#define PCI_DEVICE_CLOUD                0xa126U
#define PCI_VENDOR_ID_HUAWEI            0x19e5
#define DEVDRV_DIVERSITY_PCIE_VENDOR_ID 0xFFFF

static const ka_pci_device_id_t xsmem_device_tbl[] = {
    { KA_PCI_VDEVICE(HUAWEI, 0xd100), 0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd105), 0 },
    { KA_PCI_VDEVICE(HUAWEI, PCI_DEVICE_CLOUD), 0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd801), 0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd500), 0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd501), 0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd802), 0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd803), 0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd804), 0 },
    { DEVDRV_DIVERSITY_PCIE_VENDOR_ID, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x20C6, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x203F, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x20C6, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x203F, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x20E9, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x20E9, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    {}
};
KA_MODULE_DEVICE_TABLE(pci, xsmem_device_tbl);
#else
int xsmem_modprobe_adapt(void)
{
    return 0;
}
#endif