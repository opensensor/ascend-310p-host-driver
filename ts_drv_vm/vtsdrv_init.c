/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2025. All rights reserved.
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
#ifndef TSDRV_KERNEL_UT
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/version.h>

#include "davinci_interface.h"
#include "davinci_api.h"
#include "dbl/uda.h"

#include "vtsdrv_init.h"
#include "vtsdrv_proc.h"
#include "devdrv_cb.h"
#include "logic_cq.h"
#include "ka_driver_pub.h"
#include "ka_kernel_def_pub.h"

static const struct pci_device_id g_vtsdrv_tbl[] = {
    { KA_PCI_VDEVICE(HUAWEI, 0xd801),           0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd500),           0 },
    { 0x20C6, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x203F, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x20E9, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    {}
};
KA_MODULE_DEVICE_TABLE(pci, g_vtsdrv_tbl);

struct vmng_common_msg_client vtsdrv_common_msg_client = {
    .type = VMNG_MSG_COMMON_TYPE_TSDRV,
    .init = vtsdrv_com_msg_init,
    .common_msg_recv = vtsdrv_com_msg_recv,
};

struct vmng_vpc_client vtsdrv_vpc_client = {
    .vpc_type = VMNG_VPC_TYPE_TSDRV,
    .init = vtsdrv_vpc_init,
    .msg_recv = vtsdrv_vpc_msg_recv,
};

#ifndef TSDRV_UT
static long vtsdrv_ioctl_fake(ka_file_t *file, u32 cmd, unsigned long arg)
{
    TSDRV_PRINT_DEBUG("Fake vtsdrv ioctl.\n");
    return 0;
}

static int vtsdrv_open_fake(ka_inode_t *node, ka_file_t *file)
{
    TSDRV_PRINT_DEBUG("Fake vtsdrv open.\n");
    return 0;
}

static int vtsdrv_release_fake(ka_inode_t *node, ka_file_t *file)
{
    TSDRV_PRINT_DEBUG("Fake vtsdrv release.\n");
    return 0;
}

static int vtsdrv_mmap_fake(ka_file_t *filep, struct vm_area_struct *vma)
{
    TSDRV_PRINT_DEBUG("Fake vtsdrv mmap.\n");
    return 0;
}

const ka_file_operations_t vtsdrv_fops_fake = {
    ka_fs_init_f_owner(KA_THIS_MODULE)
    ka_fs_init_f_unlocked_ioctl(vtsdrv_ioctl_fake)
    ka_fs_init_f_open(vtsdrv_open_fake)
    ka_fs_init_f_release(vtsdrv_release_fake)
    ka_fs_init_f_mmap(vtsdrv_mmap_fake)
};
#else
const ka_file_operations_t vtsdrv_fops_fake;
#endif

const ka_file_operations_t vtsdrv_fops = {
    ka_fs_init_f_owner(KA_THIS_MODULE)
    ka_fs_init_f_unlocked_ioctl(vtsdrv_ioctl)
    ka_fs_init_f_open(vtsdrv_open)
    ka_fs_init_f_release(vtsdrv_release)
    ka_fs_init_f_mmap(vtsdrv_mmap)
};

const struct notifier_operations vtsdrv_notifier_ops = {
    .notifier_call =  vtsdrv_release_prepare,
};

struct vtsdrv_info g_vtsdrv_info;
struct vtsdrv_ctrl *vtsdrv_get_ctrl(u32 dev_id)
{
    if (g_vtsdrv_info.tsdrv_ctrl[dev_id] == NULL) {
        TSDRV_PRINT_ERR("devid(%u) is null.\n", dev_id);
    }

    return g_vtsdrv_info.tsdrv_ctrl[dev_id];
}

static int vtsdrv_inform_dev_ready(struct vtsdrv_ctrl *tsdrv_ctrl)
{
    struct vtsdrv_msg msg;
    u32 tsid = 0;
    s32 ret;

    msg.cmd = TSDRV_DEV_READY_NOTIFY;
    msg.vdev_ready.devid = tsdrv_ctrl->devid;
    ret = vtsdrv_common_msg_send(tsdrv_ctrl->devid, &msg);
    if (ret != 0) {
        TSDRV_PRINT_ERR("notify pm device-%u ready failure.\n", tsdrv_ctrl->devid);
        return ret;
    }

    tsdrv_ctrl->id_capacity[tsid].sq_capacity = msg.vdev_ready.id_capacity[tsid].sq_capacity;
    tsdrv_ctrl->id_capacity[tsid].cq_capacity = msg.vdev_ready.id_capacity[tsid].cq_capacity;

    TSDRV_PRINT_INFO("sq_capacity(%u), cq_capacity(%u)\n", tsdrv_ctrl->id_capacity[tsid].sq_capacity,
        tsdrv_ctrl->id_capacity[tsid].cq_capacity);

    return 0;
}

static char *vtsdrv_devnode(ka_device_t *dev, umode_t *mode)
{
    return NULL;
}

static s32 vtsdrv_create_device(void)
{
    dev_t tsdrv_dev;
    s32 ret;

    ret = ka_fs_alloc_chrdev_region(&tsdrv_dev, 0, TSDRV_MAX_DAVINCI_NUM, "vtsdrv-cdev");
    if (ret != 0) {
        TSDRV_PRINT_ERR("alloc chrdev region failed, ret(%d).\n", ret);
        return ret;
    }

    g_vtsdrv_info.vtsdrv_major = KA_DRIVER_MAJOR(tsdrv_dev);
#ifndef EMU_ST
    g_vtsdrv_info.vtsdrv_class = ka_driver_class_create(KA_THIS_MODULE, "tsdrv-class");
    if (KA_IS_ERR(g_vtsdrv_info.vtsdrv_class)) {
#ifndef TSDRV_UT
        ka_fs_unregister_chrdev_region(tsdrv_dev, TSDRV_MAX_DAVINCI_NUM);
        TSDRV_PRINT_ERR("class create failed.\n");
        return -EINVAL;
#endif
    }
    g_vtsdrv_info.vtsdrv_class->devnode = vtsdrv_devnode;
#endif
    return 0;
}

static void vtsdrv_release_driver_device(void)
{
    if (!KA_IS_ERR_OR_NULL(g_vtsdrv_info.vtsdrv_class)) {
        ka_driver_class_destroy(g_vtsdrv_info.vtsdrv_class);
        ka_fs_unregister_chrdev_region(KA_DRIVER_MKDEV(g_vtsdrv_info.vtsdrv_major, 0), TSDRV_MAX_DAVINCI_NUM);
    }
}

static s32 vtsdrv_create_one_device(struct vtsdrv_ctrl *tsdrv, const ka_file_operations_t *fops)
{
    ka_device_t *vdev = NULL;
    s32 ret;

    tsdrv->dev_no = KA_DRIVER_MKDEV(g_vtsdrv_info.vtsdrv_major, tsdrv->devid);
    ka_fs_cdev_init(&tsdrv->cdev, fops);
    tsdrv->cdev.owner = KA_THIS_MODULE;
    ret = ka_fs_cdev_add(&tsdrv->cdev, tsdrv->dev_no, TSDRV_MAX_DAVINCI_NUM);
    if (ret != 0) {
        TSDRV_PRINT_ERR("cdev add failed, ret(%d), dev_id(%u).\n", ret, tsdrv->devid);
        return ret;
    }
#ifndef EMU_ST
    vdev = ka_driver_device_create(g_vtsdrv_info.vtsdrv_class, NULL, tsdrv->dev_no, NULL, "davinci%d", tsdrv->devid);
    if (KA_IS_ERR(vdev)) {
        ka_fs_cdev_del(&tsdrv->cdev);
        TSDRV_PRINT_ERR("device create failed, dev_id(%u).\n", tsdrv->devid);
        return -ENODEV;
    }
    tsdrv->dev = vdev;
#endif
    return 0;
}

static void vtsdrv_release_one_device(struct vtsdrv_ctrl *tsdrv)
{
    if (tsdrv->dev != NULL) {
#ifndef EMU_ST
        ka_driver_device_destroy(g_vtsdrv_info.vtsdrv_class, tsdrv->dev_no);
        ka_fs_cdev_del(&tsdrv->cdev);
        tsdrv->dev = NULL;
#endif
    }
    return;
}

STATIC s32 vtsdrv_init_instance(u32 dev_id, ka_device_t *dev)
{
    struct vtsdrv_ctrl *tsdrv_ctrl = NULL;
    u32 tsnum = DEVDRV_MAX_TS_NUM;
    s32 ret;

    tsdrv_ctrl = g_vtsdrv_info.tsdrv_ctrl[dev_id];
    if (tsdrv_ctrl != NULL) {
        TSDRV_PRINT_ERR("vtsdrv dev-%u has been initialized.\n", dev_id);
        return -EALREADY;
    }

    tsdrv_ctrl = (struct vtsdrv_ctrl *)tsdrv_vzalloc(sizeof(struct vtsdrv_ctrl));
    if (tsdrv_ctrl == NULL) {
        TSDRV_PRINT_ERR("alloc vtsdrv dev-%u failed.\n", dev_id);
        return -ENOMEM;
    }

    tsdrv_ctrl->devid = dev_id;
    ret = tsdrv_device_init(dev_id);
    if (ret != 0) {
        TSDRV_PRINT_ERR("tsdrv dev init fail, err(%d) devid=%u\n", ret, dev_id);
        goto err_tsdrv_device_init;
    }
    tsdrv_set_dev_tsnum(dev_id, DEVDRV_MAX_TS_NUM);
    ret = vtsdrv_create_one_device(tsdrv_ctrl, &vtsdrv_fops_fake);
    if (ret != 0) {
        TSDRV_PRINT_ERR("vtsdrv create dev-%u fail, ret %d.\n", dev_id, ret);
        goto free_tsdrv;
    }

    ret = vmnga_register_common_msg_client(dev_id, &vtsdrv_common_msg_client);
    if (ret != 0) {
        TSDRV_PRINT_ERR("vtsdrv dev-%u common msg client register failed, ret %d.\n", dev_id, ret);
        goto destroy_dev;
    }

    ret = vmnga_vpc_register_client(dev_id, &vtsdrv_vpc_client);
    if (ret != 0) {
        TSDRV_PRINT_ERR("vtsdrv dev-%u vpc client register failed, ret %d.\n", dev_id, ret);
        goto unregister_client;
    }

    ret = vmnga_get_physicl_addr_info(dev_id, VMNG_GET_ADDR_TYPE_TSDRV, &tsdrv_ctrl->sqcq_hwinfo.sqcq_bar_addr,
        (u64 *)&tsdrv_ctrl->sqcq_hwinfo.size);
    if (ret != 0) {
        TSDRV_PRINT_ERR("vtsdrv dev-%u get addr failed, ret %d.\n", dev_id, ret);
        goto get_phy_addr;
    }

    ret = vtsdrv_inform_dev_ready(tsdrv_ctrl);
    if (ret != 0) {
        TSDRV_PRINT_ERR("vtsdrv inform dev-%u failed, ret %d.\n", dev_id, ret);
        goto get_phy_addr;
    }

    tsdrv_ctrl->unique_num = 0;
    g_vtsdrv_info.tsdrv_ctrl[dev_id] = tsdrv_ctrl;

    /* must be initiated after id */
    ret = callback_dev_init(dev_id, TSDRV_PM_FID, tsnum);
    if (ret != 0) {
        goto get_phy_addr;
    }

    ret = logic_sqcq_dev_init(dev_id, TSDRV_PM_FID, tsnum);
    if (ret != 0) {
        goto callback_exit;
    }
    g_vtsdrv_info.dev_num++;

    TSDRV_PRINT_INFO("vtsdrv dev-%u host_flag-%d init instance success.size=%lu\n", dev_id,
        tsdrv_get_host_flag(dev_id), tsdrv_ctrl->sqcq_hwinfo.size);
    return 0;
callback_exit:
    callback_dev_exit(dev_id, TSDRV_PM_FID, tsnum);
get_phy_addr:
    g_vtsdrv_info.tsdrv_ctrl[dev_id] = NULL;
    (void)vmnga_vpc_unregister_client(dev_id, &vtsdrv_vpc_client);
unregister_client:
    (void)vmnga_unregister_common_msg_client(dev_id, &vtsdrv_common_msg_client);
destroy_dev:
    vtsdrv_release_one_device(tsdrv_ctrl);
free_tsdrv:
    tsdrv_device_exit(dev_id);
err_tsdrv_device_init:
    tsdrv_vfree(tsdrv_ctrl);
    tsdrv_ctrl = NULL;
    return ret;
}

STATIC s32 vtsdrv_uninit_instance(u32 dev_id)
{
    struct vtsdrv_ctrl *tsdrv_ctrl = NULL;
    u32 tsnum = DEVDRV_MAX_TS_NUM;

    tsdrv_ctrl = g_vtsdrv_info.tsdrv_ctrl[dev_id];
    if (tsdrv_ctrl == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("vtsdrv dev-%u has not been initialized.\n", dev_id);
        return -EPERM;
#endif
    }

    tsdrv_ctrl->unique_num = 0;
    logic_sqcq_dev_exit(dev_id, TSDRV_PM_FID, tsnum);
    callback_dev_exit(dev_id, TSDRV_PM_FID, tsnum); /* callback must exit before mailbox exit */

    (void)vmnga_vpc_unregister_client(dev_id, &vtsdrv_vpc_client);
    (void)vmnga_unregister_common_msg_client(dev_id, &vtsdrv_common_msg_client);
    vtsdrv_release_one_device(tsdrv_ctrl);
    tsdrv_vfree(tsdrv_ctrl);
    g_vtsdrv_info.tsdrv_ctrl[dev_id] = NULL;
    g_vtsdrv_info.dev_num--;

    tsdrv_device_exit(dev_id);
    TSDRV_PRINT_INFO("vtsdrv dev-%u uninit instance success.\n", dev_id);
    return 0;
}

#define TSDRV_REMOTE_NOTIFIER "tsdrv_remote"
static int vtsdrv_remote_notifier_func(u32 udevid, enum uda_notified_action action)
{
    int ret = 0;

    if (udevid >= TSDRV_MAX_DAVINCI_NUM) {
        TSDRV_PRINT_ERR("Invalid para. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    if (action == UDA_INIT) {
        ret = vtsdrv_init_instance(udevid, uda_get_device(udevid));
    } else if (action == UDA_UNINIT) {
        ret = vtsdrv_uninit_instance(udevid);
    }

    return ret;
}

s32 __init vtsdrv_init_module(void)
{
    struct uda_dev_type type;
    s32 ret;

    ret = drv_davinci_register_sub_parallel_module(DAVINCI_INTF_MODULE_TSDRV, &vtsdrv_fops);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("register sub module fail, err(%d)\n", ret);
        return -ENODEV;
#endif
    }

    ret = drv_ascend_register_notify(DAVINCI_INTF_MODULE_TSDRV, &vtsdrv_notifier_ops);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("register devdrv_device notify fail, err(%d)\n", ret);
        (void)drv_ascend_unregister_sub_module(DAVINCI_INTF_MODULE_TSDRV);
        return -ENODEV;
#endif
    }

    ret = vtsdrv_create_device();
    if (ret != 0) {
        (void)drv_ascend_unregister_sub_module(DAVINCI_INTF_MODULE_TSDRV);
        return ret;
    }

    uda_davinci_remote_real_entity_type_pack(&type);
    ret = uda_notifier_register(TSDRV_REMOTE_NOTIFIER, &type, UDA_PRI2, vtsdrv_remote_notifier_func);
    if (ret != 0) {
        TSDRV_PRINT_ERR("Register notifier failed. (ret=%d)\n", ret);
        vtsdrv_release_driver_device();
        (void)drv_ascend_unregister_sub_module(DAVINCI_INTF_MODULE_TSDRV);
        return ret;
    }

    TSDRV_PRINT_INFO("vtsdrv init success.\n");
    return 0;
}

void __exit vtsdrv_exit_module(void)
{
    struct uda_dev_type type;

    uda_davinci_remote_real_entity_type_pack(&type);
    (void)uda_notifier_unregister(TSDRV_REMOTE_NOTIFIER, &type);
    vtsdrv_release_driver_device();
    (void)drv_ascend_unregister_sub_module(DAVINCI_INTF_MODULE_TSDRV);

    TSDRV_PRINT_INFO("vtsdrv uninit success.\n");
}

ka_module_init(vtsdrv_init_module);
ka_module_exit(vtsdrv_exit_module);

KA_MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
KA_MODULE_DESCRIPTION("vtsdrv driver");
KA_MODULE_LICENSE("GPL");
#else
void vtsdrv_exit_module(void)
{
    return;
}
#endif
