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


#ifdef CONFIG_GENERIC_BUG
#undef CONFIG_GENERIC_BUG
#endif
#ifdef CONFIG_BUG
#undef CONFIG_BUG
#endif
#ifdef CONFIG_DEBUG_BUGVERBOSE
#undef CONFIG_DEBUG_BUGVERBOSE
#endif

#include "ka_task_pub.h"
#include "ka_memory_pub.h"
#include "ka_common_pub.h"

#include "dbl/uda.h"

#include "devdrv_manager_common.h"
#include "devdrv_common.h"
#include "vdevmng_init.h"
#include "dms_event_distribute.h"
#include "dbl_mem_alloc_interface.h"
#include "hvdevmng_common.h"
#include "virtmng_interface.h"

#ifndef DEVDRV_MANAGER_HOST_UT_TEST
int vdevmng_vpc_msg_send(u32 devid, struct vdevmng_ioctl_msg *iomsg)
{
    struct vmng_tx_msg_proc_info tx_info;
    int ret;

    if ((devid >= VMNG_PDEV_MAX) || (iomsg == NULL)) {
        devdrv_drv_err("invalid dev_id(%u) or iomsg(%pK) is NULL.\n", devid, iomsg);
        return -EINVAL;
    }

    iomsg->result = -EINVAL;

    tx_info.data = (void *)iomsg;
    tx_info.in_data_len = sizeof(struct vdevmng_ioctl_msg);
    tx_info.out_data_len = sizeof(struct vdevmng_ioctl_msg);
    tx_info.real_out_len = 0;
    ret = vpc_msg_send(devid, VPC_VM_FID, VMNG_VPC_TYPE_DEVMNG, &tx_info, VPC_DEFAULT_TIMEOUT);
    if ((ret != 0) || (iomsg->result != 0)) {
        devdrv_drv_err("Vpc send msg failed. (devid=%u; main_cmd=%u; sub_cmd=%u; result=%d; ret=%d)\n",
                       devid, iomsg->main_cmd, iomsg->sub_cmd, iomsg->result, ret);
    }

    return ret;
}
EXPORT_SYMBOL_UNRELEASE(vdevmng_vpc_msg_send);

int vdevmng_common_msg_send(u32 devid, enum VDEVMNG_CTRL_MSG_TYPE type,
    struct vdevmng_ctrl_msg* ctrl_msg)
{
    struct vmng_tx_msg_proc_info tx_info;
    int ret;

    if ((devid >= VMNG_PDEV_MAX) || (ctrl_msg == NULL)) {
        devdrv_drv_err("invalid dev_id(%u) or ctrl_msg(%pK) is NULL.\n", devid, ctrl_msg);
        return -EINVAL;
    }

    ctrl_msg->type = type;
    ctrl_msg->error_code = -EINVAL;

    tx_info.data = (void *)ctrl_msg;
    tx_info.in_data_len = sizeof(struct vdevmng_ctrl_msg);
    tx_info.out_data_len = sizeof(struct vdevmng_ctrl_msg);
    tx_info.real_out_len = 0;
    ret = vmnga_common_msg_send(devid, VMNG_MSG_COMMON_TYPE_DEVMNG, &tx_info);
    if ((ret != 0) || (ctrl_msg->error_code != 0)) {
        devdrv_drv_err("common msg send failed, dev(%u) cmd %u,ret = %d,error_code = %d.\n",
                       devid, type, ret, ctrl_msg->error_code);
    }

    return ret;
}
EXPORT_SYMBOL_UNRELEASE(vdevmng_common_msg_send);

STATIC void vdevmng_mdev_info_set(struct devdrv_info *vdev_info, struct vdevdrv_info_msg *ready_info)
{
    int ret;

    /* set device info that from phy machine to vdev_info */
    ka_task_spin_lock(&vdev_info->spinlock);
    vdev_info->ctrl_cpu_ip = ready_info->ctrl_cpu_ip;
    vdev_info->ctrl_cpu_id = ready_info->ctrl_cpu_id;
    vdev_info->ctrl_cpu_core_num = ready_info->ctrl_cpu_core_num;
    vdev_info->ctrl_cpu_occupy_bitmap = ready_info->ctrl_cpu_occupy_bitmap;
    vdev_info->ctrl_cpu_endian_little = ready_info->ctrl_cpu_endian_little;
    vdev_info->ai_core_num = ready_info->ai_core_num;
    vdev_info->ai_core_id = ready_info->ai_core_id;
    vdev_info->inuse.ai_core_num = ready_info->inuse_ai_core_num;
    vdev_info->inuse.ai_core_error_bitmap = ready_info->inuse_ai_core_error_bitmap;
    vdev_info->ai_cpu_core_num = ready_info->ai_cpu_core_num;
    vdev_info->ai_cpu_core_id = ready_info->ai_cpu_core_id;
    vdev_info->aicpu_occupy_bitmap = ready_info->aicpu_occupy_bitmap;
    vdev_info->ai_subsys_ip_broken_map = ready_info->ai_subsys_ip_broken_map;
    vdev_info->hardware_version = ready_info->hardware_version;
    vdev_info->env_type = ready_info->env_type;
    vdev_info->inuse.ai_cpu_num = ready_info->inuse_ai_cpu_num;
    vdev_info->inuse.ai_cpu_error_bitmap = ready_info->inuse_ai_cpu_error_bitmap;
    vdev_info->ts_num = ready_info->ts_num;
    vdev_info->aicore_freq = ready_info->aicore_freq;
    vdev_info->vector_core_num = ready_info->vector_core_num;
    vdev_info->vector_core_bitmap = ready_info->vector_core_bitmap;
    vdev_info->vector_core_freq = ready_info->vector_core_freq;
    vdev_info->chip_name = ready_info->chip_name;
    vdev_info->chip_version = ready_info->chip_version;

    ret = memcpy_s(vdev_info->template_name, TEMPLATE_NAME_LEN, ready_info->template_name, TEMPLATE_NAME_LEN);
    if (ret != 0) {
        ka_task_spin_unlock(&vdev_info->spinlock);
        devdrv_drv_err("Copy name length failed.(dev_id=%u; ret=%d)\n", vdev_info->dev_id, ret);
        return;
    }

    ka_task_spin_unlock(&vdev_info->spinlock);
}

STATIC void vdevmng_mdev_pdata_set(struct devdrv_info *vdev_info, struct vdevdrv_info_pdata *pdata)
{
    ka_task_spin_lock(&vdev_info->spinlock);
    vdev_info->pdata->ts_pdata[0].ts_cpu_core_num = pdata->ts_cpu_core_num;
    vdev_info->pdata->ts_num = pdata->ts_num;
    vdev_info->pdata->ai_core_num_level = pdata->ai_core_num_level;
    vdev_info->pdata->ai_core_freq_level = pdata->ai_core_freq_level;
    ka_task_spin_unlock(&vdev_info->spinlock);
}

STATIC int vdevmng_mdev_info_get(struct devdrv_info *vdev_info)
{
    struct vdevmng_ctrl_msg *ctrl_msg = NULL;
    int ret;

    ctrl_msg = (struct vdevmng_ctrl_msg *)dbl_kzalloc(sizeof(struct vdevmng_ctrl_msg), GFP_KERNEL | __GFP_ACCOUNT);
    if (ctrl_msg == NULL) {
        devdrv_drv_err("kzalloc ctrl_msg fail, dev(%u).\n", vdev_info->dev_id);
        return -ENOMEM;
    }

    ret = vdevmng_common_msg_send(vdev_info->dev_id, VDEVMNG_CTRL_MSG_TYPE_GET_DEVINFO, ctrl_msg);
    if (ret) {
        devdrv_drv_err("vdevmng_common_msg_send fail ret(%d), dev(%u).\n", ret, vdev_info->dev_id);
        dbl_kfree(ctrl_msg);
        ctrl_msg = NULL;
        return ret;
    }

    vdevmng_mdev_info_set(vdev_info, &ctrl_msg->ctrl_data.ready_info);
    dbl_kfree(ctrl_msg);
    ctrl_msg = NULL;
    return 0;
}

STATIC int vdevmng_mdev_pdata_get(struct devdrv_info *vdev_info)
{
    struct vdevmng_ctrl_msg *ctrl_msg = NULL;
    int ret;

    ctrl_msg = (struct vdevmng_ctrl_msg *)dbl_kzalloc(sizeof(struct vdevmng_ctrl_msg), GFP_KERNEL | __GFP_ACCOUNT);
    if (ctrl_msg == NULL) {
        devdrv_drv_err("kzalloc ctrl_msg fail, dev(%u).\n", vdev_info->dev_id);
        return -ENOMEM;
    }

    ret = vdevmng_common_msg_send(vdev_info->dev_id, VDEVMNG_CTRL_MSG_TYPE_GET_PDATA, ctrl_msg);
    if (ret) {
        devdrv_drv_err("vdevmng_common_msg_send fail ret(%d), dev(%u).\n", ret, vdev_info->dev_id);
        dbl_kfree(ctrl_msg);
        ctrl_msg = NULL;
        return ret;
    }

    vdevmng_mdev_pdata_set(vdev_info, &ctrl_msg->ctrl_data.ready_pdata);
    dbl_kfree(ctrl_msg);
    ctrl_msg = NULL;
    return 0;
}

STATIC void vdevmng_mdev_ready_work(struct work_struct *work)
{
    struct devdrv_info *vdev_info = NULL;
    int ret;

    vdev_info = ka_container_of(work, struct devdrv_info, work);
    if (vdev_info == NULL) {
        devdrv_drv_err("vdev_info is NULL.\n");
        return;
    }

    ret = vdevmng_mdev_info_get(vdev_info);
    if (ret) {
        devdrv_drv_err("mdev(%u) get info error.\n", vdev_info->dev_id);
        return;
    }

    ret = vdevmng_mdev_pdata_get(vdev_info);
    if (ret) {
        devdrv_drv_err("mdev(%u) get pdata error.\n", vdev_info->dev_id);
        return;
    }
    vdev_info->dev_ready = DEVDRV_DEV_READY_WORK;

    devdrv_drv_info("dev(%u) ready work succ...\n", vdev_info->dev_id);
}

STATIC void vdevmng_com_msg_init(u32 dev_id, u32 fid, s32 status)
{
    return;
}

int vdevmng_com_msg_recv(u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info)
{
    return -EPERM;
}
EXPORT_SYMBOL_UNRELEASE(vdevmng_com_msg_recv);
#endif

STATIC int vdevmng_vpc_init(void)
{
    return 0;
}

#ifndef DEVDRV_MANAGER_HOST_UT_TEST
int vdevmng_vpc_msg_recv(u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info)
{
    return -EPERM;
}
EXPORT_SYMBOL_UNRELEASE(vdevmng_vpc_msg_recv);


STATIC struct vmng_common_msg_client vdevmng_common_msg_client = {
    .type = VMNG_MSG_COMMON_TYPE_DEVMNG,
    .init = vdevmng_com_msg_init,
    .common_msg_recv = vdevmng_com_msg_recv,
};

STATIC struct vmng_vpc_client vdevmng_vpc_client = {
    .vpc_type = VMNG_VPC_TYPE_DEVMNG,
    .init = vdevmng_vpc_init,
    .msg_recv = vdevmng_vpc_msg_recv,
};

STATIC int vdevmng_init_instance(u32 dev_id, struct device *dev)
{
    struct devdrv_manager_info *vmanager_info = NULL;
    struct devdrv_platform_data *pdata = NULL;
    struct devdrv_info *vdev_info = NULL;
    int ret = 0;

    devdrv_drv_info("vdevdrv_manager_init_instance IN dev(%u).\n", dev_id);

    vmanager_info = vdevdrv_get_manager_info();
    if (vmanager_info == NULL) {
        devdrv_drv_err("vdev_manager_info is NULL, dev_id(%u)\n", dev_id);
        return -EINVAL;
    }

    ret = vmnga_register_common_msg_client(dev_id, &vdevmng_common_msg_client);
    if (ret) {
        devdrv_drv_err("vdevmng dev-%u common msg client register failed, ret %d.\n", dev_id, ret);
        goto register_common_fail;
    }

    ret = vmnga_vpc_register_client(dev_id, &vdevmng_vpc_client);
    if (ret) {
        devdrv_drv_err("vdevmng dev-%u vpc client register failed, ret %d.\n", dev_id, ret);
        goto register_vpc_fail;
    }

    vdev_info = dbl_kzalloc(sizeof(struct devdrv_info), GFP_KERNEL | __GFP_ACCOUNT);
    if (vdev_info == NULL) {
        devdrv_drv_err("kzalloc vdev_info failed. dev_id(%u)\n", dev_id);
        ret = -ENOMEM;
        goto info_kzalloc_fail;
    }

    pdata = dbl_kzalloc(sizeof(struct devdrv_platform_data), GFP_KERNEL | __GFP_ACCOUNT);
    if (pdata == NULL) {
        devdrv_drv_err("kzalloc pdata failed. dev_id(%u)\n", dev_id);
        ret = -ENOMEM;
        goto pdata_kzalloc_fail;
    }
    pdata->ts_num = 1;

    ka_task_mutex_init(&vdev_info->lock);
    ka_task_spin_lock_init(&vdev_info->spinlock);
    vdev_info->dev_id = dev_id;
    vdev_info->pci_dev_id = dev_id;
    vdev_info->cce_ops.cce_dev = NULL;
    vdev_info->dev_ready = 0;
    vdev_info->dev = dev;
    vdev_info->plat_type = (u8)DEVDRV_MANAGER_HOST_ENV;
    vdev_info->pdata = pdata;

    ret = vdevmng_set_devinfo_inc_devnum(dev_id, vdev_info);
    if (ret) {
        devdrv_drv_err("vdevmng_set_devinfo_inc_devnum dev_id :%u failed !\n", dev_id);
        goto set_devinfo_fail;
    }

    KA_TASK_INIT_WORK(&vdev_info->work, vdevmng_mdev_ready_work); //lint !e613
    ka_task_queue_work(vmanager_info->dev_rdy_work, &vdev_info->work);
    devdrv_drv_info("devdrv_manager_init_instance dev_id :%u SUCC ! "
                    "dev_num: %u\n", dev_id, vmanager_info->num_dev);
    return 0;

set_devinfo_fail:
    dbl_kfree(pdata);
    pdata = NULL;
pdata_kzalloc_fail:
    dbl_kfree(vdev_info);
    vdev_info = NULL;
info_kzalloc_fail:
    (void)vmnga_vpc_unregister_client(dev_id, &vdevmng_vpc_client);
register_vpc_fail:
    (void)vmnga_unregister_common_msg_client(dev_id, &vdevmng_common_msg_client);
register_common_fail:
    devdrv_drv_err("devdrv_manager_init_instance dev_id :%u FAIL ! "
        "dev_num: %u\n", dev_id, vmanager_info->num_dev);
    return ret;
}

STATIC int vdevmng_uninit_instance(u32 dev_id)
{
    struct devdrv_info *vdev_info = NULL;

    vdev_info = vdevdrv_manager_get_devdrv_info(dev_id);
    if (vdev_info == NULL) {
        devdrv_drv_err("the ready of device is not ok, dev_id:%u.\n", dev_id);
        return -EINVAL;
    }

    ka_task_cancel_work_sync(&vdev_info->work);
    if (vdevdrv_manager_reset_devinfo_dec_devnum(dev_id)) {
        devdrv_drv_err("reset device is not ok, dev_id(%u).\n", dev_id);
    }

    if (vdev_info->pdata != NULL) {
        dbl_kfree(vdev_info->pdata);
        vdev_info->pdata = NULL;
    } else {
        devdrv_drv_err("vdev_info->pdata is NULL, dev_id(%u).\n", dev_id);
    }
    dbl_kfree(vdev_info);
    vdev_info = NULL;

    devdrv_drv_info("dev_id(%u) uninit instance.\n", dev_id);
    return 0;
}

#define DEVMNG_REMOTE_NOTIFIER "dms_mng_remote"
static int vdevmng_remote_notifier_func(u32 udevid, enum uda_notified_action action)
{
    int ret = 0;

    if (udevid >= VMNG_PDEV_MAX) {
        devdrv_drv_err("Invalid para. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    if (action == UDA_INIT) {
        ret = vdevmng_init_instance(udevid, uda_get_device(udevid));
    } else if (action == UDA_UNINIT) {
        ret = vdevmng_uninit_instance(udevid);
    }

    return ret;
}

int vdevmng_register_client(void)
{
    struct uda_dev_type type;
    int ret;

    uda_davinci_remote_real_entity_type_pack(&type);
    ret = uda_notifier_register(DEVMNG_REMOTE_NOTIFIER, &type, UDA_PRI0, vdevmng_remote_notifier_func);
    if (ret != 0) {
        devdrv_drv_err("Register notifier failed. (ret=%d)\n", ret);
        return ret;
    }

    devdrv_drv_info("vdevmng register vmna client success\n");
    return 0;
}

void vdevmng_unregister_client(void)
{
    struct uda_dev_type type;

    uda_davinci_remote_real_entity_type_pack(&type);
    (void)uda_notifier_unregister(DEVMNG_REMOTE_NOTIFIER, &type);
    devdrv_drv_info("vdevmng unregister vmna client success\n");
}
#endif
