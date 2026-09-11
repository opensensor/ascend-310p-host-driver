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

#include <linux/sysfs.h>
#include <linux/cred.h>

#include "devdrv_pci.h"
#include "devdrv_ctrl.h"
#include "devdrv_sysfs.h"
#include "devdrv_util.h"
#include "devdrv_interface.h"
#include "devdrv_mem_alloc.h"
#ifdef CFG_FEATURE_SYSFS_DUMP_DFX
#include "devdrv_pcie_dump_dfx.h"
#endif

#ifdef DRV_UT
#define STATIC
#else
#define STATIC static
#endif

STATIC int g_sysfs_davinci_dev_id = 0;

STATIC struct devdrv_pci_ctrl *devdrv_sysfs_get_pci_ctrl_by_dev(struct device *dev)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct pci_dev *pdev = to_pci_dev(dev);
    struct devdrv_pdev_ctrl *pdev_ctrl = (struct devdrv_pdev_ctrl *)pci_get_drvdata(pdev);
    if (pdev_ctrl == NULL) {
        devdrv_err("Input parameter is invalid.\n");
        return NULL;
    }
    pci_ctrl = pdev_ctrl->pci_ctrl[g_sysfs_davinci_dev_id];

    return pci_ctrl;
}

STATIC u32 devdrv_sysfs_get_devid_by_dev(struct device *dev)
{
    struct devdrv_pci_ctrl *pci_ctrl = devdrv_sysfs_get_pci_ctrl_by_dev(dev);

    if (pci_ctrl == NULL) {
        devdrv_err("Input parameter is invalid.\n");
        return MAX_DEV_CNT;
    }

    return pci_ctrl->dev_id;
}

STATIC int devdrv_sysfs_common_msg_send(u32 devid, void *data, u32 *real_out_len)
{
    return devdrv_common_msg_send(devid, data, DEVDRV_SYSFS_MSG_IN_BYTES, (u32)sizeof(struct devdrv_sysfs_msg),
        real_out_len, DEVDRV_COMMON_MSG_SYSFS);
}

STATIC ssize_t devdrv_sysfs_link_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct devdrv_pci_ctrl *pci_ctrl = devdrv_sysfs_get_pci_ctrl_by_dev(dev);
    struct devdrv_sysfs_msg *msg = NULL;
    u32 real_out_len = 0;
    int ret;

    if ((pci_ctrl == NULL) || (pci_ctrl->pdev == NULL)) {
        devdrv_err("Pci_ctrl or pdev is null \n");
        return 0;
    }

    msg = devdrv_kzalloc(sizeof(struct devdrv_sysfs_msg), GFP_KERNEL | __GFP_ACCOUNT);
    if (msg == NULL) {
        devdrv_err("sysfs_msg malloc failed.\n");
        return 0;
    }

    msg->type = DEVDRV_SYSFS_LINK_INFO;

    devdrv_info("devdrv sysfs show link info.(dev_id=%u)\n", pci_ctrl->dev_id);

    if (pci_ctrl->ops.get_peh_link_info != NULL) {
        ret = pci_ctrl->ops.get_peh_link_info(pci_ctrl->pdev, &(msg->link_info.link_speed),
                                              &(msg->link_info.link_width), &(msg->link_info.link_status));
    } else {
        ret = devdrv_sysfs_common_msg_send(pci_ctrl->dev_id, (void *)msg, &real_out_len);
    }

    if (ret != 0) {
        devdrv_err("Common msg send failed.\n");
        ret = snprintf_s(buf, PAGE_SIZE, PAGE_SIZE - 1, "read link_info fail, ret:%d\n", ret);
        devdrv_kfree(msg);
        return ret == -1 ? 0 : ret;
    }

    ret = snprintf_s(buf, PAGE_SIZE, PAGE_SIZE - 1, "link speed: %d, link width: %d, link_status: 0x%x\n",
                     msg->link_info.link_speed, msg->link_info.link_width, msg->link_info.link_status);
    devdrv_kfree(msg);
    return ret == -1 ? 0 : ret;
}

STATIC ssize_t devdrv_sysfs_rx_para_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    u32 devid;
    u32 real_out_len = 0;
    u32 offset = 0;
    u32 i;
    int ret;

    struct devdrv_sysfs_msg *msg = devdrv_kzalloc(sizeof(struct devdrv_sysfs_msg), GFP_KERNEL | __GFP_ACCOUNT);
    if (msg == NULL) {
        devdrv_err("sysfs_msg malloc failed.\n");
        return 0;
    }

    msg->type = DEVDRV_SYSFS_RX_PARA;

    devdrv_info("devdrv sysfs show rx para.\n");

    devid = devdrv_sysfs_get_devid_by_dev(dev);
    if (devid >= MAX_DEV_CNT) {
        devdrv_err("Get dev_id failed. (dev_id=%u)\n", devid);
        devdrv_kfree(msg);
        return 0;
    }

    devdrv_info("Get dev_id success. (dev_id=%u)\n", devid);

    ret = devdrv_sysfs_common_msg_send(devid, (void *)msg, &real_out_len);
    if (ret != 0) {
        devdrv_err("Common msg send failed. (dev_id=%u)\n", devid);
        ret = snprintf_s(buf, PAGE_SIZE, PAGE_SIZE - 1, "read rx_para fail, ret:%d\n", ret);
        if (ret != -1) {
            offset = (u32)ret;
        }
        devdrv_kfree(msg);
        return offset;
    }

    for (i = 0; i < DEVDRV_SYSFS_RX_LANE_MAX; i++) {
        ret = snprintf_s(buf + offset, PAGE_SIZE - offset, PAGE_SIZE - offset - 1,
                         "lane%d  att:%-10d gain:%-10d boost:%-10d tap1:%-10d tap2:%-10d\n", i,
                         msg->rx_para.lane_rx_para[i].att, msg->rx_para.lane_rx_para[i].gain,
                         msg->rx_para.lane_rx_para[i].boost, msg->rx_para.lane_rx_para[i].tap1,
                         msg->rx_para.lane_rx_para[i].tap2);
        if (ret != -1) {
            offset += (u32)ret;
        } else {
            devdrv_err("snprintf_s failed.\n");
            devdrv_kfree(msg);
            return 0;
        }
    }

    devdrv_kfree(msg);
    return offset;
}

STATIC ssize_t devdrv_sysfs_tx_para_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    u32 devid;
    u32 real_out_len = 0;
    u32 offset = 0;
    u32 i;
    int ret;

    struct devdrv_sysfs_msg *msg = devdrv_kzalloc(sizeof(struct devdrv_sysfs_msg), GFP_KERNEL | __GFP_ACCOUNT);
    if (msg == NULL) {
        devdrv_err("sysfs_msg malloc failed.\n");
        return 0;
    }

    msg->type = DEVDRV_SYSFS_TX_PARA;

    devdrv_info("devdrv sysfs show tx para.\n");

    devid = devdrv_sysfs_get_devid_by_dev(dev);
    if (devid >= MAX_DEV_CNT) {
        devdrv_err("Get dev_id failed. (dev_id=%u)\n", devid);
        devdrv_kfree(msg);
        return 0;
    }

    devdrv_info("Get devid success. (dev_id=%u)\n", devid);

    ret = devdrv_sysfs_common_msg_send(devid, (void *)msg, &real_out_len);
    if (ret != 0) {
        devdrv_err("Common msg send failed. (dev_id=%u)\n", devid);
        ret = snprintf_s(buf, PAGE_SIZE, PAGE_SIZE - 1, "read tx_para fail, ret:%d\n", ret);
        if (ret != -1) {
            offset = (u32)ret;
        }
        devdrv_kfree(msg);
        return offset;
    }

    for (i = 0; i < DEVDRV_SYSFS_TX_LANE_MAX; i++) {
        ret = snprintf_s(buf + offset, PAGE_SIZE - offset, PAGE_SIZE - offset - 1,
                         "lane%d  pre:%-10d main:%-10d post:%-10d\n", i, msg->tx_para.lane_tx_para[i].pre,
                         msg->tx_para.lane_tx_para[i].main, msg->tx_para.lane_tx_para[i].post);
        if (ret != -1) {
            offset += (u32)ret;
        } else {
            devdrv_err("snprintf_s failed. (dev_id=%u)\n", devid);
            devdrv_kfree(msg);
            return 0;
        }
    }

    devdrv_kfree(msg);
    return offset;
}

STATIC ssize_t devdrv_sysfs_aer_cnt_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    u32 devid;
    u32 real_out_len = 0;
    int ret;

    struct devdrv_sysfs_msg *msg = devdrv_kzalloc(sizeof(struct devdrv_sysfs_msg), GFP_KERNEL | __GFP_ACCOUNT);
    if (msg == NULL) {
        devdrv_err("sysfs_msg malloc failed.\n");
        return 0;
    }

    msg->type = DEVDRV_SYSFS_AER_COUNT;

    devdrv_info("devdrv sysfs show aer count.\n");

    devid = devdrv_sysfs_get_devid_by_dev(dev);
    if (devid >= MAX_DEV_CNT) {
        devdrv_err("Get dev_id failed. (dev_id=%u)", devid);
        devdrv_kfree(msg);
        return 0;
    }

    devdrv_info("Get dev_id success. (dev_id=%u)", devid);

    ret = devdrv_sysfs_common_msg_send(devid, (void *)msg, &real_out_len);
    if (ret != 0) {
        devdrv_err("Common msg send failed.\n");
        ret = snprintf_s(buf, PAGE_SIZE, PAGE_SIZE - 1, "read aer count fail, ret:%d\n", ret);
        devdrv_kfree(msg);
        return ret == -1 ? 0 : ret;
    }

    ret = snprintf_s(buf, PAGE_SIZE, PAGE_SIZE - 1, "AER count: %d\n", msg->data[0]);
    devdrv_kfree(msg);
    return ret == -1 ? 0 : ret;
}

STATIC ssize_t devdrv_sysfs_aer_cnt_store(struct device *dev, struct device_attribute *attr, const char *buf,
                                          size_t count)
{
    u32 devid;
    u32 real_out_len = 0;
    int ret;

    struct devdrv_sysfs_msg *msg = devdrv_kzalloc(sizeof(struct devdrv_sysfs_msg), GFP_KERNEL | __GFP_ACCOUNT);
    if (msg == NULL) {
        devdrv_err("Clear aer count failed, sysfs_msg malloc failed. (uid=%u)\n", __kuid_val(current_uid()));
        return -EINVAL;
    }

    msg->type = DEVDRV_SYSFS_AER_CLEAR;

    devid = devdrv_sysfs_get_devid_by_dev(dev);
    if (devid >= MAX_DEV_CNT) {
        devdrv_err("Clear aer count failed, get dev_id failed. (dev_id=%u; uid=%u)\n",
            devid, __kuid_val(current_uid()));
        devdrv_kfree(msg);
        return -EINVAL;
    }

    if (strcmp(buf, "clear") == 0) {
        ret = devdrv_sysfs_common_msg_send(devid, (void *)msg, &real_out_len);
        if (ret != 0) {
            devdrv_err("Clear aer count failed, Common msg send failed. (dev_id=%u; uid=%u)\n",
                devid, __kuid_val(current_uid()));
            devdrv_kfree(msg);
            return -EINVAL;
        }
    }

    devdrv_info("Clear aer count success. (dev_id=%u; uid=%u)\n", devid, __kuid_val(current_uid()));

    devdrv_kfree(msg);
    return (ssize_t)count;
}

STATIC ssize_t devdrv_sysfs_bdf_to_devid_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct devdrv_ctrl *ctrl = NULL;
    struct pci_dev *pdev = NULL;
    ssize_t offset = 0;
    u8 bus_num = 0;
    u8 device_num = 0;
    u8 func_num = 0;
    int ret;
    u32 i;

    devdrv_info("devdrv sysfs show bdf and dev_id.\n");

    for (i = 0; i < MAX_DEV_CNT; i++) {
        ctrl = devdrv_get_devctrl_by_id(i);
        if (ctrl == NULL) {
            continue;
        }
        if ((ctrl->dev_id < MAX_DEV_CNT) && (ctrl->pdev != NULL) &&
            (ctrl->startup_flg != DEVDRV_DEV_STARTUP_UNPROBED)) {
            pdev = ctrl->pdev;
            bus_num = pdev->bus->number;
            device_num = PCI_SLOT(pdev->devfn);
            func_num = PCI_FUNC(pdev->devfn);
            ret = snprintf_s(buf + offset, PAGE_SIZE - offset, PAGE_SIZE - offset - 1, "%02x:%02x.%d ---> %d\n",
                             bus_num, device_num, func_num, ctrl->dev_id);
            if (ret != -1) {
                offset += ret;
            }
        }
    }

    return offset;
}

STATIC ssize_t devdrv_sysfs_bios_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct devdrv_pci_ctrl *pci_ctrl = devdrv_sysfs_get_pci_ctrl_by_dev(dev);
    ssize_t offset = 0;
    int ret = 0;

    if (pci_ctrl == NULL) {
        devdrv_err("Get pci_ctrl failed.\n");
        return 0;
    }

    if (!(pci_ctrl->device_bios_version.valid)) {
        ret = snprintf_s(buf, PAGE_SIZE, PAGE_SIZE - 1, "device_fw_unsupported\n");
        if (ret != -1) {
            offset = ret;
        }
        return offset;
    }

    ret = snprintf_s(buf + offset, PAGE_SIZE - offset, PAGE_SIZE - offset - 1, "bootrom:%x.%x.%x.%x\n",
                     pci_ctrl->device_bios_version.bootrom[DEVDRV_INDEX_0],
                     pci_ctrl->device_bios_version.bootrom[DEVDRV_INDEX_1],
                     pci_ctrl->device_bios_version.bootrom[DEVDRV_INDEX_2],
                     pci_ctrl->device_bios_version.bootrom[DEVDRV_INDEX_3]);
    if (ret != -1) {
        offset += ret;
    }

    ret = snprintf_s(buf + offset, PAGE_SIZE - offset, PAGE_SIZE - offset - 1, "xloader:%x.%x.%x.%05x\n",
                     pci_ctrl->device_bios_version.xloader[DEVDRV_INDEX_0],
                     pci_ctrl->device_bios_version.xloader[DEVDRV_INDEX_1],
                     pci_ctrl->device_bios_version.xloader[DEVDRV_INDEX_2],
                     pci_ctrl->device_bios_version.xloader[DEVDRV_INDEX_3]);
    if (ret != -1) {
        offset += ret;
    }

    ret = snprintf_s(buf + offset, PAGE_SIZE - offset, PAGE_SIZE - offset - 1, "nve:%05x\n",
                     pci_ctrl->device_bios_version.nve);
    if (ret != -1) {
        offset += ret;
    }

    return offset;
}

STATIC ssize_t devdrv_sysfs_get_dev_id(struct device *dev, struct device_attribute *attr, char *buf)
{
    u32 dev_id;
    int ret;
    ssize_t offset = 0;

    (void)attr;

    dev_id = devdrv_sysfs_get_devid_by_dev(dev);
    if (dev_id >= MAX_DEV_CNT) {
        devdrv_err("Invalid dev_id. (dev_id=%u)\n", dev_id);
        return offset;
    }

    ret = snprintf_s(buf, PAGE_SIZE, PAGE_SIZE - 1, "%d\n", dev_id);
    if (ret >= 0) {
        offset += ret;
    }

    return offset;
}

STATIC ssize_t devdrv_sysfs_get_davinci_dev_id(struct device *dev, struct device_attribute *attr, char *buf)
{
    int ret;
    ssize_t offset = 0;

    (void)attr;

    ret = snprintf_s(buf, PAGE_SIZE, PAGE_SIZE - 1, "%d\n", g_sysfs_davinci_dev_id);
    if (ret >= 0) {
        offset += ret;
    }

    return offset;
}

STATIC ssize_t devdrv_sysfs_set_davinci_dev_id(struct device *dev,
    struct device_attribute *attr,
    const char *buf,
    size_t count)
{
    u32 val = 0;
    int davinci_dev_num = devdrv_get_davinci_dev_num_by_pdev(to_pci_dev(dev));
    if (davinci_dev_num < 0) {
        devdrv_err("davinci_dev_num is invalid.\n");
        return (ssize_t)count;
    }
    if (kstrtou32(buf, 0, &val) < 0) {
        devdrv_err("Call kstrtou32 failed.\n");
        return  (ssize_t)count;
    }

    if (val >= davinci_dev_num) {
        devdrv_err("Input parameter is invalid. (val=%u; devid max in pdev=%d)\n", val, davinci_dev_num);
        return  (ssize_t)count;
    }

    g_sysfs_davinci_dev_id = (int)val;

    return  (ssize_t)count;
}

STATIC ssize_t devdrv_sysfs_get_davinci_dev_num(struct device *dev, struct device_attribute *attr, char *buf)
{
    int ret;
    ssize_t offset = 0;

    (void)attr;

    ret = snprintf_s(buf, PAGE_SIZE, PAGE_SIZE - 1, "%d\n", devdrv_get_davinci_dev_num_by_pdev(to_pci_dev(dev)));
    if (ret >= 0) {
        offset += ret;
    }

    return offset;
}

STATIC ssize_t devdrv_sysfs_get_chip_id(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct devdrv_pci_ctrl *pci_ctrl = devdrv_sysfs_get_pci_ctrl_by_dev(dev);
    u32 chip_id;
    int ret;
    ssize_t offset = 0;

    (void)attr;

    if (pci_ctrl == NULL) {
        devdrv_err("Get pci_ctrl failed.\n");
        return 0;
    }

    chip_id = (u32)pci_ctrl->shr_para->chip_id;

    ret = snprintf_s(buf, PAGE_SIZE, PAGE_SIZE - 1, "%d\n", chip_id);
    if (ret >= 0) {
        offset += ret;
    }

    return offset;
}

STATIC ssize_t devdrv_sysfs_get_bus_name(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct devdrv_ctrl *ctrl = NULL;
    struct pci_bus *bus = NULL;
    struct pci_dev *pdev = NULL;
    u32 dev_id;
    int ret;
    ssize_t offset = 0;

    (void)attr;

    dev_id = devdrv_sysfs_get_devid_by_dev(dev);
    if (dev_id >= MAX_DEV_CNT) {
        devdrv_err("Invalid dev_id. (dev_id=%u)\n", dev_id);
        return offset;
    }

    ctrl = devdrv_get_bottom_half_devctrl_by_id(dev_id);
    if (ctrl == NULL) {
        devdrv_err("Get dev_ctrl failed. (dev_id=%u)\n", dev_id);
        return offset;
    }

    bus = ctrl->bus;
    if (bus != NULL) {
        pdev = bus->self;
        if (pdev == NULL) {
            devdrv_err("Get bus fail, bus->self is NULL. (dev_id=%u)\n", dev_id);
            return offset;
        }
        ret = snprintf_s(buf, PAGE_SIZE, PAGE_SIZE - 1, "%04x:%02x:%02x.%d\n", pci_domain_nr(bus), pdev->bus->number,
                         PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));
        if (ret >= 0) {
            offset += ret;
        }
    }

    return offset;
}

STATIC ssize_t devdrv_sysfs_set_hotreset_flag(struct device *dev,
    struct device_attribute *attr,
    const char *buf,
    size_t count)
{
    struct devdrv_pci_ctrl *pci_ctrl = devdrv_sysfs_get_pci_ctrl_by_dev(dev);
    unsigned long val = 0;
    ssize_t result;
    u32 devid;

    (void)attr;

    if (pci_ctrl == NULL) {
        devdrv_err("Set hotreset flag failed, get pci_ctrl failed. (uid=%u)\n", __kuid_val(current_uid()));
        return -EINVAL;
    }
    devid = pci_ctrl->dev_id;

    result = kstrtoul(buf, 0, &val);
    if (result < 0) {
        devdrv_err("Set hotreset flag failed, kstrtoul failed. (dev_id=%u; uid=%u; result=%ld)\n",
            devid, __kuid_val(current_uid()), result);
        return result;
    }

    if (val == 1) {
        if (pci_ctrl != NULL) {
            if (pci_ctrl->shr_para != NULL) {
                pci_ctrl->shr_para->hot_reset_pcie_flag = DEVDRV_PCIE_HOT_RESET_FLAG;
                devdrv_info("Set hotreset flag success. (dev_id=%u; uid=%u; val=%lu)\n",
                    devid, __kuid_val(current_uid()), val);
                return (ssize_t)count;
            }
        }
    }

    devdrv_info("No need to set hotreset flag. (dev_id=%u; uid=%u; val=%lu)\n", devid, __kuid_val(current_uid()), val);

    return (ssize_t)count;
}

STATIC ssize_t devdrv_sysfs_get_common_msg(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct devdrv_pci_ctrl *pci_ctrl = devdrv_sysfs_get_pci_ctrl_by_dev(dev);
    u32 dev_id;
    int ret;
    ssize_t offset = 0;
    struct devdrv_sysfs_msg *msg = NULL;
    u32 real_out_len = 0;
    int i;
    u64 tx_total_cnt;
    u64 tx_success_cnt;
    u64 tx_failed_cnt1;
    u64 tx_failed_cnt2;
    u64 tx_failed_cnt3;
    u64 tx_failed_cnt4;
    u64 tx_failed_cnt5;
    u64 rx_total_cnt;
    u64 rx_success_cnt;
    u64 rx_work_max_time;
    u64 rx_work_delay_cnt;
    char *common_str[DEVDRV_COMMON_MSG_TYPE_MAX] = {DEVDRV_PCIVNIC, DEVDRV_SMMU, DEVDRV_DEVMM, DEVDRV_VMNG,
                                                    DEVDRV_PROFILE, DEVDRV_DEVMAN, DEVDRV_TSDRV, DEVDRV_HDC,
                                                    DEVDRV_SYSFS, DEVDRV_ESCHED};

    (void)attr;

    if (pci_ctrl == NULL) {
        devdrv_err("Get pci_ctrl failed.\n");
        return 0;
    }

    dev_id = pci_ctrl->dev_id;

    msg = (struct devdrv_sysfs_msg *)devdrv_kzalloc(sizeof(struct devdrv_sysfs_msg), GFP_KERNEL | __GFP_ACCOUNT);
    if (msg == NULL) {
            devdrv_err("devdrv_kzalloc failed.\n");
            return 0;
    }

    msg->type = DEVDRV_SYSFS_COMMON_MSG;

    ret = devdrv_sysfs_common_msg_send(dev_id, (void *)msg, &real_out_len);
    if (ret != 0) {
        devdrv_kfree(msg);
        msg = NULL;
        devdrv_err("Common msg send failed.\n");
        ret = snprintf_s(buf, PAGE_SIZE, PAGE_SIZE - 1, "read common msg fail, ret:%d\n", ret);
        return ret == -1 ? 0 : ret;
    }

    ret = snprintf_s(buf + offset, PAGE_SIZE - offset, PAGE_SIZE - offset - 1,
                     "\nhost chan|    tx_total    |    tx_success  |  inval |  nodev |"
                     " enosys |timedout| default|"
                     "  rx_total_cnt  | rx_success_cnt | cost_time|work_delay\n");
    if (ret != -1) {
        offset += ret;
    }
    for (i = 0; i < DEVDRV_COMMON_MSG_TYPE_MAX; i++) {
        tx_total_cnt = pci_ctrl->msg_dev->common_msg.com_msg_stat[i].tx_total_cnt;
        tx_success_cnt = pci_ctrl->msg_dev->common_msg.com_msg_stat[i].tx_success_cnt;
        tx_failed_cnt1 = pci_ctrl->msg_dev->common_msg.com_msg_stat[i].tx_einval_err;
        tx_failed_cnt2 = pci_ctrl->msg_dev->common_msg.com_msg_stat[i].tx_enodev_err;
        tx_failed_cnt3 = pci_ctrl->msg_dev->common_msg.com_msg_stat[i].tx_enosys_err;
        tx_failed_cnt4 = pci_ctrl->msg_dev->common_msg.com_msg_stat[i].tx_etimedout_err;
        tx_failed_cnt5 = pci_ctrl->msg_dev->common_msg.com_msg_stat[i].tx_default_err;
        rx_total_cnt = pci_ctrl->msg_dev->common_msg.com_msg_stat[i].rx_total_cnt;
        rx_success_cnt = pci_ctrl->msg_dev->common_msg.com_msg_stat[i].rx_success_cnt;
        rx_work_max_time = pci_ctrl->msg_dev->common_msg.com_msg_stat[i].rx_work_max_time;
        rx_work_delay_cnt = pci_ctrl->msg_dev->common_msg.com_msg_stat[i].rx_work_delay_cnt;

        ret = snprintf_s(buf + offset, PAGE_SIZE - offset, PAGE_SIZE - offset - 1, "  %s|%-16llu|%-16llu|%-8llu|"
                         "%-8llu|%-8llu|%-8llu|%-8llu|%-16llu|%-16llu|%-10llu|%-10llu\n",
                         common_str[i], tx_total_cnt, tx_success_cnt, tx_failed_cnt1, tx_failed_cnt2,
                         tx_failed_cnt3, tx_failed_cnt4, tx_failed_cnt5, rx_total_cnt, rx_success_cnt,
                         rx_work_max_time, rx_work_delay_cnt);
        if (ret != -1) {
            offset += ret;
        }
    }

    ret = snprintf_s(buf + offset, PAGE_SIZE - offset, PAGE_SIZE - offset - 1,
                     "\ndev  chan|    tx_total    |    tx_success  |  inval |  nodev |"
                     " enosys |timedout| default|"
                     "  rx_total_cnt  | rx_success_cnt | cost_time|work_delay\n");
    if (ret != -1) {
        offset += ret;
    }
    for (i = 0; i < DEVDRV_COMMON_MSG_TYPE_MAX; i++) {
        tx_total_cnt = msg->common_stat[i].tx_total_cnt;
        tx_success_cnt = msg->common_stat[i].tx_success_cnt;
        tx_failed_cnt1 = msg->common_stat[i].tx_einval_err;
        tx_failed_cnt2 = msg->common_stat[i].tx_enodev_err;
        tx_failed_cnt3 = msg->common_stat[i].tx_enosys_err;
        tx_failed_cnt4 = msg->common_stat[i].tx_etimedout_err;
        tx_failed_cnt5 = msg->common_stat[i].tx_default_err;
        rx_total_cnt = msg->common_stat[i].rx_total_cnt;
        rx_success_cnt = msg->common_stat[i].rx_success_cnt;
        rx_work_max_time = msg->common_stat[i].rx_work_max_time;
        rx_work_delay_cnt = msg->common_stat[i].rx_work_delay_cnt;

        ret = snprintf_s(buf + offset, PAGE_SIZE - offset, PAGE_SIZE - offset - 1, "  %s|%-16llu|%-16llu|%-8llu|"
                         "%-8llu|%-8llu|%-8llu|%-8llu|%-16llu|%-16llu|%-10llu|%-10llu\n",
                         common_str[i], tx_total_cnt, tx_success_cnt, tx_failed_cnt1, tx_failed_cnt2,
                         tx_failed_cnt3, tx_failed_cnt4, tx_failed_cnt5, rx_total_cnt, rx_success_cnt,
                         rx_work_max_time, rx_work_delay_cnt);
        if (ret != -1) {
            offset += ret;
        }
    }

    devdrv_kfree(msg);
    msg = NULL;
    return offset;
}

STATIC int devdrv_sysfs_fill_buf(struct devdrv_msg_dev *msg_dev, struct devdrv_sysfs_msg *msg, char *buf)
{
    int ret;
    u32 i;
    u64 tx_total_cnt;
    u64 tx_success_cnt;
    u64 tx_no_callback;
    u64 tx_failed_cnt1;
    u64 tx_failed_cnt2;
    u64 tx_failed_cnt3;
    u64 tx_failed_cnt4;
    u64 tx_failed_cnt5;
    u64 tx_failed_cnt6;
    u64 rx_total_cnt;
    u64 rx_success_cnt;
    u64 rx_para_err;
    u64 rx_work_max_time;
    u64 rx_work_delay_cnt;
    u32 msg_type;
    ssize_t offset = 0;
    struct devdrv_msg_chan *chan = NULL;
    char *non_str[devdrv_msg_client_max] = {DEVDRV_PCIVNIC, DEVDRV_SMMU, DEVDRV_DEVMM, DEVDRV_COMMON,
                                            DEVDRV_DEVMAN, DEVDRV_TSDRV, DEVDRV_HDC, DEVDRV_QUEUE};

    ret = snprintf_s(buf + offset, PAGE_SIZE - offset, PAGE_SIZE - offset - 1,
                     "\nhost type|   tx_total   |  tx_success  |no_callback|len_err|stat_abno|irqtimeout|"
                     "timeout|proc_err|inva_para|"
                     "   rx_total   |  rx_success  |para_err|cost_time|work_delay\n");
    if (ret != -1) {
        offset += ret;
    }
    for (i = 0; i < msg_dev->chan_cnt; i++) {
        chan = &(msg_dev->msg_chan[i]);

        if ((chan->status == AGENTDRV_ENABLE) && (chan->queue_type == NON_TRANSPARENT_MSG_QUEUE)) {
            tx_total_cnt = chan->chan_stat.tx_total_cnt;
            tx_success_cnt = chan->chan_stat.tx_success_cnt;
            tx_no_callback = chan->chan_stat.tx_no_callback;
            tx_failed_cnt1 = chan->chan_stat.tx_len_check_err;
            tx_failed_cnt2 = chan->chan_stat.tx_status_abnormal_err;
            tx_failed_cnt3 = chan->chan_stat.tx_irq_timeout_err;
            tx_failed_cnt4 = chan->chan_stat.tx_timeout_err;
            tx_failed_cnt5 = chan->chan_stat.tx_process_err;
            tx_failed_cnt6 = chan->chan_stat.tx_invalid_para_err;
            rx_total_cnt = chan->chan_stat.rx_total_cnt;
            rx_success_cnt = chan->chan_stat.rx_success_cnt;
            rx_para_err = chan->chan_stat.rx_para_err;
            rx_work_max_time = chan->chan_stat.rx_work_max_time;
            rx_work_delay_cnt = chan->chan_stat.rx_work_delay_cnt;

            ret = snprintf_s(buf + offset, PAGE_SIZE - offset, PAGE_SIZE - offset - 1,
                             "  %s|%-14llu|%-14llu|%-11llu|%-7llu|%-9llu|%-10llu|%-7llu|%-8llu|%-9llu|%-14llu|"
                             "%-14llu|%-8llu|%-9llu|%-10llu\n",
                             non_str[chan->msg_type], tx_total_cnt, tx_success_cnt, tx_no_callback, tx_failed_cnt1,
                             tx_failed_cnt2, tx_failed_cnt3, tx_failed_cnt4, tx_failed_cnt5, tx_failed_cnt6,
                             rx_total_cnt, rx_success_cnt, rx_para_err, rx_work_max_time, rx_work_delay_cnt);
            if (ret != -1) {
                offset += ret;
            }
        }
    }

    ret = snprintf_s(buf + offset, PAGE_SIZE - offset, PAGE_SIZE - offset - 1,
                     "\ndev  type|   tx_total   |  tx_success  |no_callback|len_err| dma_copy|          |"
                     "timeout|proc_err|inva_para|"
                     "   rx_total   |  rx_success  |para_err|cost_time|work_delay\n");
    if (ret != -1) {
        offset += ret;
    }
    for (i = 0; i < devdrv_msg_client_max; i++) {
        if ((msg->non_trans_stat[i].msg_type == 0) || (msg->non_trans_stat[i].msg_type >= devdrv_msg_client_max)) {
            continue;
        }

        msg_type = (u32)msg->non_trans_stat[i].msg_type;
        tx_total_cnt = msg->non_trans_stat[i].tx_total_cnt;
        tx_success_cnt = msg->non_trans_stat[i].tx_success_cnt;
        tx_no_callback = msg->non_trans_stat[i].tx_no_callback;
        tx_failed_cnt1 = msg->non_trans_stat[i].tx_len_check_err;
        tx_failed_cnt2 = msg->non_trans_stat[i].tx_dma_copy_err;
        tx_failed_cnt3 = msg->non_trans_stat[i].tx_timeout_err;
        tx_failed_cnt4 = msg->non_trans_stat[i].tx_process_err;
        tx_failed_cnt5 = msg->non_trans_stat[i].tx_invalid_para_err;
        rx_total_cnt = msg->non_trans_stat[i].rx_total_cnt;
        rx_success_cnt = msg->non_trans_stat[i].rx_success_cnt;
        rx_para_err = msg->non_trans_stat[i].rx_para_err;
        rx_work_max_time = msg->non_trans_stat[i].rx_work_max_time;
        rx_work_delay_cnt = msg->non_trans_stat[i].rx_work_delay_cnt;

        ret = snprintf_s(buf + offset, PAGE_SIZE - offset, PAGE_SIZE - offset - 1,
                         "  %s|%-14llu|%-14llu|%-11llu|%-7llu|%-9llu|%-10llu|%-7llu|%-8llu|%-9llu|%-14llu|"
                         "%-14llu|%-8llu|%-9llu|%-10llu\n", non_str[msg_type], tx_total_cnt, tx_success_cnt,
                         tx_no_callback, tx_failed_cnt1, tx_failed_cnt2, 0, tx_failed_cnt3, tx_failed_cnt4,
                         tx_failed_cnt5, rx_total_cnt, rx_success_cnt, rx_para_err, rx_work_max_time,
                         rx_work_delay_cnt);
        if (ret != -1) {
            offset += ret;
        }
    }

    return (int)offset;
}

STATIC ssize_t devdrv_sysfs_get_non_trans_msg(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct devdrv_pci_ctrl *pci_ctrl = devdrv_sysfs_get_pci_ctrl_by_dev(dev);
    u32 dev_id;
    int ret;
    ssize_t offset = 0;
    struct devdrv_sysfs_msg *msg = NULL;
    struct devdrv_msg_dev *msg_dev = NULL;
    u32 real_out_len = 0;

    (void)attr;

    if (pci_ctrl == NULL) {
        devdrv_err("Get pci_ctrl failed.\n");
        return 0;
    }
    dev_id = pci_ctrl->dev_id;
    msg_dev = pci_ctrl->msg_dev;

    msg = (struct devdrv_sysfs_msg *)devdrv_kzalloc(sizeof(struct devdrv_sysfs_msg), GFP_KERNEL | __GFP_ACCOUNT);
    if (msg == NULL) {
            devdrv_err("malloc failed.\n");
            return 0;
    }

    msg->type = DEVDRV_SYSFS_NON_TRANS_MSG;

    ret = devdrv_sysfs_common_msg_send(dev_id, (void *)msg, &real_out_len);
    if (ret != 0) {
        devdrv_kfree(msg);
        msg = NULL;
        devdrv_err("Common msg send failed.\n");
        ret = snprintf_s(buf, PAGE_SIZE, PAGE_SIZE - 1, "read non trans fail, ret:%d\n", ret);
        return ret == -1 ? 0 : ret;
    }

    offset = devdrv_sysfs_fill_buf(msg_dev, msg, buf);

    devdrv_kfree(msg);
    msg = NULL;
    return offset;
}

STATIC ssize_t devdrv_sysfs_sync_dma_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        struct devdrv_pci_ctrl *pci_ctrl = devdrv_sysfs_get_pci_ctrl_by_dev(dev);
        struct devdrv_sysfs_msg *msg = NULL;
        struct devdrv_msg_dev *msg_dev = NULL;
        struct devdrv_dma_dev *dma_dev = NULL;
        u32 dev_id;
        u32 real_out_len = 0;
        int ret;
        ssize_t offset = 0;
        u32 i;

        if (pci_ctrl == NULL) {
            devdrv_err("Get pci_ctrl failed.\n");
            return 0;
        }

        dev_id = pci_ctrl->dev_id;

        msg_dev = pci_ctrl->msg_dev;
        dma_dev = pci_ctrl->dma_dev;

        msg = (struct devdrv_sysfs_msg *)devdrv_kzalloc(sizeof(struct devdrv_sysfs_msg), GFP_KERNEL | __GFP_ACCOUNT);
        if (msg == NULL) {
                devdrv_err("malloc failed.\n");
                return 0;
        }

        msg->type = DEVDRV_SYSFS_SYNC_DMA_INFO;

        ret = devdrv_sysfs_common_msg_send(dev_id, (void *)msg, &real_out_len);
        if (ret != 0) {
                devdrv_err("Common msg send failed.\n");
                ret = snprintf_s(buf, PAGE_SIZE, PAGE_SIZE - 1,
                                 "read dma info fail, ret:%d\n", ret);
                devdrv_kfree(msg);
                msg = NULL;
                return (ret == -1) ? 0 : ret;
        }

        ret = snprintf_s(buf + offset, PAGE_SIZE - offset, PAGE_SIZE - offset - 1,
            "\ndev |syn_submit|asy_submit|sml_submit|tot_sync|trg_remot|trg_128|done_int|"
            "re_sched|done_work|syn_proc|max_cb|new_cb|cb_10s|cb_10ms|asy_proc|err_int|err_work|"
            "max_task|sq_idle\n");
        if (ret != -1) {
                offset += ret;
        }
        // max device chan num is DEVDRV_SYSFS_DMA_CHAN_NUM-16
        for (i = 0; i < DEVDRV_SYSFS_DMA_CHAN_NUM; i++) {
            if (msg->sync_dma_stat[i].flag == DEVDRV_DISABLE) {
                continue;
            }
            ret = snprintf_s(buf + offset, PAGE_SIZE - offset, PAGE_SIZE - offset - 1,
                                " %-4d|%-10llu|%-10llu|%-10llu|%-8llu|%-9llu|%-7llu|%-8llu|%-8llu|%-9llu|"
                                "%-8llu|%-6llu|%-6llu|%-6llu|%-7llu|%-8llu|%-7llu|%-8llu|%-8llu|%-7llu\n",
                                i,
                                msg->sync_dma_stat[i].sync_submit_cnt,
                                msg->sync_dma_stat[i].async_submit_cnt,
                                msg->sync_dma_stat[i].sml_submit_cnt,
                                msg->sync_dma_stat[i].sync_submit_cnt + msg->sync_dma_stat[i].sml_submit_cnt,
                                msg->sync_dma_stat[i].trigger_remot_int_cnt,
                                msg->sync_dma_stat[i].trigger_local_128,
                                msg->sync_dma_stat[i].done_int_cnt,
                                msg->sync_dma_stat[i].re_schedule_cnt,
                                msg->sync_dma_stat[i].done_tasklet_in_cnt,
                                msg->sync_dma_stat[i].sync_sem_up_cnt,
                                msg->sync_dma_stat[i].max_callback_time,
                                msg->sync_dma_stat[i].new_callback_time,
                                msg->sync_dma_stat[i].callback_time_over10s,
                                msg->sync_dma_stat[i].callback_time_over10ms,
                                msg->sync_dma_stat[i].async_proc_cnt,
                                msg->sync_dma_stat[i].err_int_cnt,
                                msg->sync_dma_stat[i].err_work_cnt,
                                msg->sync_dma_stat[i].max_task_op_time,
                                msg->sync_dma_stat[i].sq_idle_bd_cnt);
            if (ret != -1) {
                offset += ret;
            }
        }

        ret = snprintf_s(buf + offset, PAGE_SIZE - offset, PAGE_SIZE - offset - 1, "\nhost\n");
        if (ret != -1) {
            offset += ret;
        }
        for (i = 0; i < dma_dev->remote_chan_num; i++) {
            ret = snprintf_s(buf + offset, PAGE_SIZE - offset, PAGE_SIZE - offset - 1,
                                " %-4d|%-10llu|%-10llu|%-10llu|%-8llu|%-9llu|%-7llu|%-8llu|%-8llu|%-9llu|"
                                "%-8llu|%-6llu|%-6llu|%-6llu|%-7llu|%-8llu|%-7llu|%-8llu|%-8llu|%-7llu\n",
                                i,
                                dma_dev->dma_chan[i].status.sync_submit_cnt,
                                dma_dev->dma_chan[i].status.async_submit_cnt,
                                dma_dev->dma_chan[i].status.sml_submit_cnt,
                                dma_dev->dma_chan[i].status.sync_submit_cnt +
                                    dma_dev->dma_chan[i].status.sml_submit_cnt,
                                dma_dev->dma_chan[i].status.trigger_remot_int_cnt,
                                dma_dev->dma_chan[i].status.trigger_local_128,
                                dma_dev->dma_chan[i].status.done_int_cnt,
                                dma_dev->dma_chan[i].status.re_schedule_cnt,
                                dma_dev->dma_chan[i].status.done_tasklet_in_cnt,
                                dma_dev->dma_chan[i].status.sync_sem_up_cnt,
                                dma_dev->dma_chan[i].status.max_callback_time,
                                dma_dev->dma_chan[i].status.new_callback_time,
                                dma_dev->dma_chan[i].status.callback_time_over10s,
                                dma_dev->dma_chan[i].status.callback_time_over10ms,
                                dma_dev->dma_chan[i].status.async_proc_cnt,
                                dma_dev->dma_chan[i].status.err_int_cnt,
                                dma_dev->dma_chan[i].status.err_work_cnt,
                                dma_dev->dma_chan[i].status.max_task_op_time,
                                dma_dev->dma_chan[i].status.sq_idle_bd_cnt);
            if (ret != -1) {
                offset += ret;
            }
        }

        devdrv_kfree(msg);
        msg = NULL;

        return offset;
}

#define PCIE_AER_ROOT_ERR 1U
#define PCIE_AER_CFG_SPACE_UNCORRECT_ERR 2U
#define PCIE_AER_CFG_SPACE_CORRECT_ERR 3U

STATIC struct pcie_aer_info_item g_pcie_aer_info_table[] = {
    PCIE_AER_NODE(PCIE_AER_ROOT_ERR, 0U, "correct err msg recv"),
    PCIE_AER_NODE(PCIE_AER_ROOT_ERR, 1U, "multi correct err msg recv"),
    PCIE_AER_NODE(PCIE_AER_ROOT_ERR, 2U, "uncorrect err msg recv"),
    PCIE_AER_NODE(PCIE_AER_ROOT_ERR, 3U, "multi uncorrect err msg recv"),
    PCIE_AER_NODE(PCIE_AER_ROOT_ERR, 4U, "first uncorrect err"),
    PCIE_AER_NODE(PCIE_AER_ROOT_ERR, 5U, "noncritical msg recv"),
    PCIE_AER_NODE(PCIE_AER_ROOT_ERR, 6U, "critical err msg recv"),
    PCIE_AER_NODE(PCIE_AER_CFG_SPACE_UNCORRECT_ERR, 4U, "data link protocol err status"),
    PCIE_AER_NODE(PCIE_AER_CFG_SPACE_UNCORRECT_ERR, 5U, "surprise down status"),
    PCIE_AER_NODE(PCIE_AER_CFG_SPACE_UNCORRECT_ERR, 12U, "poisoned TLP status"),
    PCIE_AER_NODE(PCIE_AER_CFG_SPACE_UNCORRECT_ERR, 13U, "credit protocol err status"),
    PCIE_AER_NODE(PCIE_AER_CFG_SPACE_UNCORRECT_ERR, 14U, "completion timeout status"),
    PCIE_AER_NODE(PCIE_AER_CFG_SPACE_UNCORRECT_ERR, 15U, "completion abort status"),
    PCIE_AER_NODE(PCIE_AER_CFG_SPACE_UNCORRECT_ERR, 16U, "unexpect completion status"),
    PCIE_AER_NODE(PCIE_AER_CFG_SPACE_UNCORRECT_ERR, 17U, "receive overflow status"),
    PCIE_AER_NODE(PCIE_AER_CFG_SPACE_UNCORRECT_ERR, 18U, "malformed TLP err status"),
    PCIE_AER_NODE(PCIE_AER_CFG_SPACE_UNCORRECT_ERR, 19U, "ECRC err status"),
    PCIE_AER_NODE(PCIE_AER_CFG_SPACE_UNCORRECT_ERR, 20U, "unsupported req err status"),
    PCIE_AER_NODE(PCIE_AER_CFG_SPACE_UNCORRECT_ERR, 21U, "ACS violation status"),
    PCIE_AER_NODE(PCIE_AER_CFG_SPACE_UNCORRECT_ERR, 22U, "uncorrect internal err status"),
    PCIE_AER_NODE(PCIE_AER_CFG_SPACE_UNCORRECT_ERR, 23U, "multicast block status"),
    PCIE_AER_NODE(PCIE_AER_CFG_SPACE_UNCORRECT_ERR, 24U, "atomicop egress block status"),
    PCIE_AER_NODE(PCIE_AER_CFG_SPACE_UNCORRECT_ERR, 25U, "TLP prefix block err status"),
    PCIE_AER_NODE(PCIE_AER_CFG_SPACE_UNCORRECT_ERR, 26U, "poisoned TLP block status"),
    PCIE_AER_NODE(PCIE_AER_CFG_SPACE_CORRECT_ERR, 0U, "rx err status"),
    PCIE_AER_NODE(PCIE_AER_CFG_SPACE_CORRECT_ERR, 6U, "bad tlp status"),
    PCIE_AER_NODE(PCIE_AER_CFG_SPACE_CORRECT_ERR, 7U, "bad dllp status"),
    PCIE_AER_NODE(PCIE_AER_CFG_SPACE_CORRECT_ERR, 8U, "reply num rollover status"),
    PCIE_AER_NODE(PCIE_AER_CFG_SPACE_CORRECT_ERR, 12U, "reply timer timeout status"),
    PCIE_AER_NODE(PCIE_AER_CFG_SPACE_CORRECT_ERR, 13U, "advisory noncritical err status"),
    PCIE_AER_NODE(PCIE_AER_CFG_SPACE_CORRECT_ERR, 14U, "correct internal err status"),
    PCIE_AER_NODE(PCIE_AER_CFG_SPACE_CORRECT_ERR, 15U, "header log overflow status"),
};

STATIC ssize_t devdrv_sysfs_aer_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    u32 num;
    u32 i;
    ssize_t offset = 0;
    int ret;

    num = sizeof(g_pcie_aer_info_table) / sizeof(struct pcie_aer_info_item);

    for (i = 0; i < num; i++) {
        if (g_pcie_aer_info_table[i].count != 0) {
            ret = snprintf_s(buf + offset, PAGE_SIZE - offset, PAGE_SIZE - offset - 1, "%-31s:%u\n",
                g_pcie_aer_info_table[i].describe, g_pcie_aer_info_table[i].count);
            if (ret != -1) {
                offset += ret;
            }
        }
    }
    return offset;
}

void devdrv_sysfs_update_aer_count(unsigned char aer_regs, unsigned char aer_bits)
{
    u32 num;
    u32 i;
    bool update_finish = false;

    num = sizeof(g_pcie_aer_info_table) / sizeof(struct pcie_aer_info_item);
    for (i = 0; i < num; i++) {
        if ((g_pcie_aer_info_table[i].regs == aer_regs) && (g_pcie_aer_info_table[i].bits == aer_bits)) {
            g_pcie_aer_info_table[i].count++;
            update_finish = true;
            break;
        }
    }

    if (!update_finish) {
        devdrv_err("update aer info failed. (aer_regs=%u, aer_bits=%u).\n", (u32)aer_regs, (u32)aer_bits);
    }
    return;
}

EXPORT_SYMBOL(devdrv_sysfs_update_aer_count);

STATIC ssize_t devdrv_sysfs_dump_dfx_part1_show(struct device *dev, struct device_attribute *attr, char *buf)
{
#ifndef CFG_FEATURE_SYSFS_DUMP_DFX
    return -1;
#else
    return soc_misc_pcie_sysfs_dump_dfx_info(buf, 0);
#endif
}

STATIC ssize_t devdrv_sysfs_dump_dfx_part2_show(struct device *dev, struct device_attribute *attr, char *buf)
{
#ifndef CFG_FEATURE_SYSFS_DUMP_DFX
    return -1;
#else
    return soc_misc_pcie_sysfs_dump_dfx_info(buf, 1);
#endif
}

static DEVICE_ATTR(devdrv_sysfs_link_info, S_IRUSR | S_IRGRP, devdrv_sysfs_link_info_show, NULL);
static DEVICE_ATTR(devdrv_sysfs_rx_para_info, S_IRUSR | S_IRGRP, devdrv_sysfs_rx_para_show, NULL);
static DEVICE_ATTR(devdrv_sysfs_tx_para_info, S_IRUSR | S_IRGRP, devdrv_sysfs_tx_para_show, NULL);
static DEVICE_ATTR(devdrv_sysfs_aer_cnt_info, S_IRUSR | S_IRGRP | S_IWUSR | S_IWGRP, devdrv_sysfs_aer_cnt_show,
                   devdrv_sysfs_aer_cnt_store);
static DEVICE_ATTR(devdrv_sysfs_bdf_to_devid, S_IRUSR | S_IRGRP, devdrv_sysfs_bdf_to_devid_show, NULL);
static DEVICE_ATTR(devdrv_sysfs_bios_version, S_IRUSR | S_IRGRP, devdrv_sysfs_bios_version_show, NULL);

static DEVICE_ATTR(davinci_dev_id, S_IRUSR | S_IRGRP | S_IWUSR | S_IWGRP, devdrv_sysfs_get_davinci_dev_id,
                   devdrv_sysfs_set_davinci_dev_id);
static DEVICE_ATTR(davinci_dev_num, S_IRUSR | S_IRGRP, devdrv_sysfs_get_davinci_dev_num, NULL);

static DEVICE_ATTR(dev_id, S_IRUSR | S_IRGRP, devdrv_sysfs_get_dev_id, NULL);
static DEVICE_ATTR(chip_id, S_IRUSR | S_IRGRP, devdrv_sysfs_get_chip_id, NULL);
static DEVICE_ATTR(bus_name, S_IRUSR | S_IRGRP, devdrv_sysfs_get_bus_name, NULL);
static DEVICE_ATTR(hotreset_flag, S_IWUSR | S_IWGRP, NULL, devdrv_sysfs_set_hotreset_flag);
static DEVICE_ATTR(common_msg, S_IRUSR | S_IRGRP, devdrv_sysfs_get_common_msg, NULL);
static DEVICE_ATTR(non_trans_msg, S_IRUSR | S_IRGRP, devdrv_sysfs_get_non_trans_msg, NULL);
static DEVICE_ATTR(dma_info, S_IRUGO, devdrv_sysfs_sync_dma_info_show, NULL);
static DEVICE_ATTR(aer_info, S_IRUSR | S_IRGRP, devdrv_sysfs_aer_info_show, NULL);
static DEVICE_ATTR(dump_dfx_part1, S_IRUSR | S_IRGRP, devdrv_sysfs_dump_dfx_part1_show, NULL);
static DEVICE_ATTR(dump_dfx_part2, S_IRUSR | S_IRGRP, devdrv_sysfs_dump_dfx_part2_show, NULL);

static struct attribute *g_devdrv_sysfs_attrs[] = {
    &dev_attr_devdrv_sysfs_link_info.attr,
    &dev_attr_devdrv_sysfs_rx_para_info.attr,
    &dev_attr_devdrv_sysfs_tx_para_info.attr,
    &dev_attr_devdrv_sysfs_aer_cnt_info.attr,
    &dev_attr_devdrv_sysfs_bdf_to_devid.attr,
    &dev_attr_devdrv_sysfs_bios_version.attr,
    &dev_attr_davinci_dev_id.attr,
    &dev_attr_davinci_dev_num.attr,
    &dev_attr_dev_id.attr,
    &dev_attr_chip_id.attr,
    &dev_attr_bus_name.attr,
    &dev_attr_hotreset_flag.attr,
    &dev_attr_aer_info.attr, /* mdc_v11 */
    &dev_attr_dump_dfx_part1.attr, /* mdc_v51lite */
    &dev_attr_dump_dfx_part2.attr, /* mdc_v51lite */
    NULL,
};

static const struct attribute_group g_devdrv_sysfs_group = {
    .attrs = g_devdrv_sysfs_attrs,
};

static struct attribute *g_devdrv_sysfs_msg_attrs[] = {
    &dev_attr_common_msg.attr,
    &dev_attr_non_trans_msg.attr,
    &dev_attr_dma_info.attr,
    NULL,
};

static const struct attribute_group g_devdrv_sysfs_msg_group = {
    .attrs = g_devdrv_sysfs_msg_attrs,
    .name = "msg",
};

int devdrv_sysfs_init(struct pci_dev *pdev)
{
    int ret;

    ret =  sysfs_create_group(&pdev->dev.kobj, &g_devdrv_sysfs_group);
    if (ret != 0) {
        devdrv_err("Call sysfs_create_group failed. (ret=%d)\n", ret);
        return -1;
    }

    ret =  sysfs_create_group(&pdev->dev.kobj, &g_devdrv_sysfs_msg_group);
    if (ret != 0) {
        devdrv_err("Call sysfs_create_group failed. (ret=%d)\n", ret);
        return -1;
    }

    return 0;
}

void devdrv_sysfs_exit(struct pci_dev *pdev)
{
    sysfs_remove_group(&pdev->dev.kobj, &g_devdrv_sysfs_group);
    sysfs_remove_group(&pdev->dev.kobj, &g_devdrv_sysfs_msg_group);
}
