/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2023. All rights reserved.
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
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pci.h>
#include <linux/delay.h>

#include "dbl/uda.h"

#include "devdrv_common.h"
#include "devdrv_interface.h"
#include "devdrv_devinit.h"
#include "tsdrv_h2d_chan.h"
#include "tsdrv_plat_dev.h"
#include "tsdrv_parse.h"
#include "devdrv_manager.h"
#include "tsdrv_status.h"
#include "tsdrv_sync.h"
#include "tsdrv_log.h"

#include "hvtsdrv_init.h"
#include "hvtsdrv_cqsq.h"
#include "devdrv_interface.h"
#include "drv_log.h"
#include "ka_kernel_def_pub.h"

#define PCI_VENDOR_ID_HUAWEI 0x19e5

static const struct pci_device_id tsdrv_driver_tbl[] = {
    {KA_PCI_VDEVICE(HUAWEI, 0xd100), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd105), 0},
    {KA_PCI_VDEVICE(HUAWEI, PCI_DEVICE_CLOUD), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd801), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd500), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd501), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd802), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd803), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd804), 0},
    {DEVDRV_DIVERSITY_PCIE_VENDOR_ID, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    { 0x20C6, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x203F, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x20E9, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x20C6, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x203F, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x20E9, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    {}
};
KA_MODULE_DEVICE_TABLE(pci, tsdrv_driver_tbl);

static struct devdrv_info *tsdrv_devinfo_init(u32 devid, ka_device_t *dev)
{
    struct devdrv_platform_data *pdata = NULL;
    struct devdrv_info *dev_info = NULL;

    dev_info = tsdrv_kzalloc(sizeof(struct devdrv_info), KA_GFP_KERNEL);
    if (dev_info == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("kmalloc dev_info fail, devid=%u size=%lu\n", devid, sizeof(struct devdrv_info));
        return NULL;
#endif
    }
    pdata = tsdrv_kzalloc(sizeof(struct devdrv_platform_data), KA_GFP_KERNEL);
    if (pdata == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("kmalloc pdata fail, devid=%u size=%lu\n", devid, sizeof(struct devdrv_platform_data));
        goto err_pdata_alloc;
#endif
    }
    dev_info->dev_id = devid;
    dev_info->dev = dev;
    dev_info->plat_type = (u8)DEVDRV_MANAGER_HOST_ENV;
    dev_info->pdata = pdata;
    dev_info->drv_ops = devdrv_manager_get_drv_ops();
    return dev_info;
#ifndef TSDRV_UT
err_pdata_alloc:
    tsdrv_kfree(dev_info);
    return NULL;
#endif
}

static void tsdrv_devinfo_exit(u32 devid, struct devdrv_info *dev_info)
{
    if (dev_info == NULL || dev_info->pdata == NULL) {
        TSDRV_PRINT_ERR("invalid dev_info, devid=%u\n", devid);
        return;
    }
    tsdrv_kfree(dev_info->pdata);
    dev_info->pdata = NULL;
    tsdrv_kfree(dev_info);
}

STATIC int tsdrv_handshake_handler(u32 devid, struct tsdrv_msg_info *msg, u32 *ack_len)
{
    struct devdrv_info *dev_info = NULL;
    int err;

    TSDRV_PRINT_DEBUG("Start to handle handshake. (devid=%u)\n", devid);
    dev_info = plat_dev_set_handshake(devid);
    if (dev_info == NULL) {
        return -ENODEV;
    }
    err = tsdrv_h2d_fast_chan_init(devid);
    if (err != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("h2d fast chan init fail, devid=%u err=%d\n", devid, err);
        goto err_h2d_fast_chan_init;
#endif
    }

    dev_info->ts_num = 1;
    dev_info->dev_id = devid;
    dev_info->chip_id = devid;
    dev_info->die_id = 0;
    tsdrv_set_ts_status(devid, 0, TS_WORK);
    tsdrv_set_send_msg_proc(tsdrv_h2d_slow_msg_send);
    err = devdrv_drv_register(dev_info);
    if (err != 0) {
#ifndef TSDRV_UT
        goto err_tsdrv_register;
#endif
    }

#ifdef CFG_FEATURE_SUPPORT_VSQ_MNG
    if (tsdrv_get_pfvf_type_by_devid(devid) == DEVDRV_SRIOV_TYPE_PF) {
        err = tsdrv_notice_tsagent_dev_create(devid, TSDRV_PM_FID);
        if (err != 0) {
            devdrv_drv_unregister(dev_info);
            goto err_notice_tsagent;
        }
    }
#endif

    plat_dev_reset_handshake(devid);
    /* ack len must be set. if not ack data will not be send back to device side */
    *ack_len = sizeof(struct tsdrv_msg_info);
    msg->header.result = 0;
    msg->header.valid = 0xA5A5;
    TSDRV_PRINT_DEBUG("Success to handle handshake. (devid=%u)\n", devid);
    return 0;
#ifdef CFG_FEATURE_SUPPORT_VSQ_MNG
err_notice_tsagent:
#endif
err_tsdrv_register:
    (void)tsdrv_h2d_fast_chan_exit(devid);
err_h2d_fast_chan_init:
    plat_dev_reset_handshake(devid);
    return -ENOMEM;
}

STATIC int tsdrv_init_instance(u32 devid, ka_device_t *dev)
{
    struct devdrv_info *dev_info = NULL;
    struct tsdrv_msg_info msg;
    u32 ack_len;
    int err;

    dev_info = tsdrv_devinfo_init(devid, dev);
    if (dev_info == NULL) {
        return -ENOMEM;
    }
    err = plat_dev_init(devid, dev_info);
    if (err != 0) {
#ifndef TSDRV_UT
        goto err_plat_dev_init;
#endif
    }
    err = tsdrv_parse_init(devid, dev_info);
    if (err != 0) {
#ifndef TSDRV_UT
        goto err_parse_init;
#endif
    }
    err = tsdrv_h2d_slow_chan_init(devid);
    if (err != 0) {
#ifndef TSDRV_UT
        goto err_h2d_slow_chan_init;
#endif
    }

    err = tsdrv_handshake_handler(devid, &msg, &ack_len);
    if (err != 0) {
#ifndef TSDRV_UT
        goto err_handshake_init;
#endif
    }

    TSDRV_PRINT_DEBUG("Success to tsdrv_init_instance. (devid=%u)\n", devid);
    return 0;
#ifndef TSDRV_UT
err_handshake_init:
    (void)tsdrv_h2d_slow_chan_exit(devid);
err_h2d_slow_chan_init:
    tsdrv_parse_exit(devid, dev_info);
err_parse_init:
    (void)plat_dev_uninit(devid);
err_plat_dev_init:
    tsdrv_devinfo_exit(devid, dev_info);
    return -ENODEV;
#endif
}

STATIC int tsdrv_uninit_instance(u32 devid)
{
    struct devdrv_info *dev_info = NULL;
    int err;

    /* prevent hot reset when the heartbeat comes and run heartbeat handler */
    dev_info = plat_dev_uniniting(devid);
    if (dev_info == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("set dev uniniting fail, devid=%u\n", devid);
        return -ENODEV;
#endif
    }

#ifdef CFG_FEATURE_SUPPORT_VSQ_MNG
    if (tsdrv_get_pfvf_type_by_devid(devid) == DEVDRV_SRIOV_TYPE_PF) {
        tsdrv_notice_tsagent_dev_destroy(devid, TSDRV_PM_FID);
    }
#endif

    /* Destroy the vm recycle work before releasing the IDs resource */
    hvtsdrv_uninit_device(devid);

    devdrv_drv_unregister(dev_info);
    err = tsdrv_h2d_slow_chan_exit(devid);
    if (err != 0) {
        TSDRV_PRINT_ERR("h2d slow chan exit fail, devid=%u err=%d\n", devid, err);
    }
    err = tsdrv_h2d_fast_chan_exit(devid);
    if (err != 0) {
        TSDRV_PRINT_ERR("h2d fast chan exit fail, devid=%u err=%d\n", devid, err);
    }
    tsdrv_parse_exit(devid, dev_info);
    err = plat_dev_uninit(devid);
    if (err != 0) {
        TSDRV_PRINT_ERR("plat dev uninit fail, devid=%u\n", devid);
    }
    tsdrv_devinfo_exit(devid, dev_info);
    TSDRV_PRINT_DEBUG("Success to tsdrv_uninit_instance. (devid=%u)\n", devid);

    return 0;
}

#define TSDRV_HOST_NOTIFIER "tsdrv_host"
static int tsdrv_host_notifier_func(u32 udevid, enum uda_notified_action action)
{
    int ret = 0;

    if (udevid >= TSDRV_MAX_DAVINCI_NUM) {
        TSDRV_PRINT_ERR("Invalid para. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    if (action == UDA_INIT) {
        ret = tsdrv_init_instance(udevid, uda_get_device(udevid));
    } else if (action == UDA_UNINIT) {
        ret = tsdrv_uninit_instance(udevid);
#ifndef TSDRV_UT
    } else if (action == UDA_HOTRESET) {
        ret = tsdrv_device_pre_hotreset(udevid);
    } else if (action == UDA_HOTRESET_CANCEL) {
        ret = tsdrv_device_cancel_hotreset(udevid);
    } else if (action == UDA_PRE_HOTRESET) {
        ret = tsdrv_device_pre_hotreset(udevid);
    } else if (action == UDA_PRE_HOTRESET_CANCEL) {
        ret = tsdrv_device_cancel_hotreset(udevid);
#endif
    }

    TSDRV_PRINT_INFO("notifier action. (udevid=%u; action=%d; ret=%d)\n", udevid, action, ret);

    return ret;
}


#define CHIP_NOTIFY_ADDR_OFFSET 0xE000UL
int devdrv_get_notify_base_addr(u32 devid, u64 *phy_addr)
{
    size_t size;
    u64 addr;
    int ret;

    if ((phy_addr == NULL) || (devid >= TSDRV_MAX_DAVINCI_NUM)) {
        TSDRV_PRINT_ERR("notify base: invalid null pointer.devid = %u\n", devid);
        return -EINVAL;
    }
    ret = devdrv_get_addr_info(devid, DEVDRV_ADDR_HWTS, 0, &addr, &size);
    if (ret != 0) {
        TSDRV_PRINT_ERR("notify base: get base addr err.devid = %u, ret = %d\n", devid, ret);
        return ret;
    }

    *phy_addr = addr + CHIP_NOTIFY_ADDR_OFFSET;

    return 0;
}
KA_EXPORT_SYMBOL_GPL(devdrv_get_notify_base_addr);

STATIC int __init tsdrv_platform_init(void)
{
    struct uda_dev_type type;
    int err;

    TSDRV_PRINT_INFO("tsdrv_platform init start\n");
    plat_dev_setup();
    err = tsdrv_h2d_chan_setup();
    if (err != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("h2d chan setup fail, err=%d\n", err);
        goto err_h2d_chan_setup;
#endif
    }

    uda_davinci_near_real_entity_type_pack(&type);
    err = uda_notifier_register(TSDRV_HOST_NOTIFIER, &type, UDA_PRI2, tsdrv_host_notifier_func);
    if (err != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("startup register fail, err=%d\n", err);
        goto err_h2d_chan_init;
#endif
    }

    hvtsdrv_init();
    TSDRV_PRINT_INFO("tsdrv_platform init end\n");

    return 0;
#ifndef TSDRV_UT
err_h2d_chan_init:
    tsdrv_h2d_chan_cleanup();
err_h2d_chan_setup:
    plat_dev_cleanup();
    return err;
#endif
}
ka_module_init(tsdrv_platform_init);

STATIC void __exit tsdrv_platform_exit(void)
{
    struct uda_dev_type type;

    TSDRV_PRINT_INFO("tsdrv_platform exit start\n");
    hvtsdrv_uninit();
    uda_davinci_near_real_entity_type_pack(&type);
    (void)uda_notifier_unregister(TSDRV_HOST_NOTIFIER, &type);
    tsdrv_h2d_chan_cleanup();

    plat_dev_cleanup();
    TSDRV_PRINT_INFO("tsdrv_platform exit end\n");
}
ka_module_exit(tsdrv_platform_exit);

KA_MODULE_LICENSE("GPL");
KA_MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
KA_MODULE_DESCRIPTION("DAVINCI driver");
