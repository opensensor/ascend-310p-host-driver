/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2012-2019. All rights reserved.
 * Description: Host Pci vnic
 * Author: Huawei Technologies Co., Ltd.
 * Create: 2020/7/4
 */

#include "pcivnic_host.h"
#include "pcivnic_main.h"

#include "ka_pci_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_driver_pub.h"

#define PCI_VENDOR_ID_HUAWEI 0x19e5

/*lint -e508 */
static const ka_pci_device_id_t g_devdrv_pcivnic_tbl[] = {{ KA_PCI_VDEVICE(HUAWEI, 0xd100), 0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd105), 0 },
    { KA_PCI_VDEVICE(HUAWEI, PCI_DEVICE_CLOUD), 0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd801), 0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd500), 0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd501), 0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd802), 0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd803), 0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd804), 0 },
    { DEVDRV_DIVERSITY_PCIE_VENDOR_ID, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x20C6, 0xd500, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
    { 0x203F, 0xd500, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
    { 0x20C6, 0xd802, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
    { 0x203F, 0xd802, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
    { 0x20E9, 0xd802, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
    { 0x20E9, 0xd500, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
    {}};
KA_MODULE_DEVICE_TABLE(pci, g_devdrv_pcivnic_tbl);
/*lint +e508 */

STATIC int __init pcivnic_init_module(void)
{
    struct pcivnic_netdev *vnic_dev = NULL;
    int ret;

    devdrv_info("pcivnic_init_module start\n");
    vnic_dev = pcivnic_alloc_vnic_dev();
    if (vnic_dev == NULL) {
        devdrv_err("alloc vnic dev failed!\n");
        return -ENOMEM;
    }

    ret = pcivnic_register_netdev(vnic_dev);
    if (ret) {
        pcivnic_free_vnic_dev();
        devdrv_err("register netdev fail ret = %d\n", ret);
        return ret;
    }

    ret = pcivnic_register_client();
    if (ret) {
        devdrv_err("pcivnic_register_client fail ret = %d\n", ret);
        pcivnic_free_vnic_dev();
        return ret;
    }

    return 0;
}

STATIC void __exit pcivnic_exit_module(void)
{
    int ret = 0;

    ret = pcivnic_unregister_client();
    if (ret) {
        devdrv_err("pcivnic_unregister_client is fail ret = %d\n", ret);
        return;
    }

    pcivnic_free_vnic_dev();
    devdrv_info("pcivnic_exit_module success\n");
}

ka_module_init(pcivnic_init_module);
ka_module_exit(pcivnic_exit_module);

KA_MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
KA_MODULE_DESCRIPTION("pcivnic host driver");
KA_MODULE_LICENSE("GPL");
