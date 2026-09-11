/*
* Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
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
* Create: 2022-6-23
*/

#include "ka_kernel_def_pub.h"
#include "ka_pci_pub.h"
#include "ka_fs_pub.h"

#include "dbl/runenv_config.h"
#include "docker_query.h"
#include "vm_query.h"
#include "runenv_config_module.h"

MODULE_DESCRIPTION("Ascend runtime environment configuration");
KA_MODULE_LICENSE("GPL");
KA_MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
KA_MODULE_AUTHOR("DAVINCI driver");

#define PCI_VENDOR_ID_HUAWEI            0x19e5
#define DEVDRV_DIVERSITY_PCIE_VENDOR_ID 0xFFFF
static const ka_pci_device_id_t recfg_device_tbl[] = {
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
    { 0x20E9, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x20C6, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x203F, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x20E9, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    {}
};
MODULE_DEVICE_TABLE(pci, recfg_device_tbl);

/* The name cannot be changed because it is exposed to the user. */
static u32 deployment_mode = DBL_DEVICE_DEPLOYMENT;
ka_module_param(deployment_mode, uint, KA_S_IRUSR);

u32 dbl_get_deployment_mode(void)
{
    return deployment_mode;
}
KA_EXPORT_SYMBOL(dbl_get_deployment_mode);

int dbl_get_run_env(void)
{
    int mach_flag = DBL_IN_PHYSICAL_MACH;

    if (run_in_normal_docker() == true) {
        mach_flag = DBL_IN_NORMAL_DOCKER;
    } else if (run_in_admin_docker() == true) {
        mach_flag = DBL_IN_ADMIN_DOCKER;
    } else if (run_in_virtual_mach() == true) {
        mach_flag = DBL_IN_VIRTUAL_MACH;
    }

    return mach_flag;
}
KA_EXPORT_SYMBOL(dbl_get_run_env);

STATIC int __init recfg_init(void)
{
#ifdef CFG_FEATURE_HOST_ENV
    deployment_mode = DBL_HOST_DEPLOYMENT;
#endif
    recfg_info("Module init success.\n");
    return 0;
}

STATIC void __exit recfg_exit(void)
{
    recfg_info("Module exit success.\n");
}

ka_module_init(recfg_init);
ka_module_exit(recfg_exit);
