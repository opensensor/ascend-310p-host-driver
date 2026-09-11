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

#include "virtmnghost_pci.h"
#include "virtmnghost_vpc_unit.h"
#include "virtmnghost_unit.h"
#include "virtmnghost_ctrl.h"
#include "virtmnghost_msg.h"
#include "virtmnghost_msg_admin.h"
#include "virtmnghost_external.h"
#include "virtmnghost_vpc.h"
#include "virtmnghost_sysfs.h"
#include "virtmnghost_msg_common.h"
#include "virtmng_msg_pub.h"
#include "virtmng_msg_admin.h"
#include "virtmng_public_def.h"
#include "virtmng_interface.h"
#include "virtmng_resource.h"
#include "devdrv_interface.h"
#include "hw_vdavinci.h"
#include "virtmnghost_proc_fs.h"
#include "vmng_mem_alloc_interface.h"

#include "ka_base_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_barrier_pub.h"
#include "ka_system_pub.h"

#include "dbl/uda.h"
#include "soc_res.h"

static const ka_pci_device_id_t g_vmngh_tbl[] = {{ KA_PCI_VDEVICE(HUAWEI, HISI_EP_DEVICE_ID_MINIV2), 0 },
                                                   { KA_PCI_VDEVICE(HUAWEI, HISI_EP_DEVICE_ID_CLOUD), 0 },
                                                   { KA_PCI_VDEVICE(HUAWEI, HISI_EP_DEVICE_ID_CLOUD_V2), 0 },
                                                   { DEVDRV_DIVERSITY_PCIE_VENDOR_ID, 0xd500,
                                                     KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
                                                   { 0x20C6, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
                                                   { 0x203F, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
                                                   { 0x20C6, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
                                                   { 0x203F, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
                                                   { 0x20E9, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
                                                   { 0x20E9, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
                                                   {}};
KA_MODULE_DEVICE_TABLE(pci, g_vmngh_tbl);

ka_mutex_t g_vmngh_pci_mutex;
u32 procfs_valid;
u32 g_max_msix_num;
u32 g_max_pass_to_vm_msix_num;

#define VMNGH_VF_SRIOV_MDEV_MODE 0
#define VMNGH_PF_MDEV_MODE 1
#define VMNGH_VF_MDEV_MODE 2

STATIC inline u32 vmngh_get_fid_for_host(struct vdavinci_dev *vdev)
{
    return vdev->fid + 1;
}

static inline u32 vmngh_get_dtype_by_aicore(unsigned int aicore_num)
{
    // __builtin_clz return the front 0 bit num of (u32)KA_BASE_BIT(N) equal to 31 - N
    // dtype is equal to log(aicore), this function return the highest 1 bit index
    return (u32)(__builtin_clz((u32)KA_BASE_BIT(0)) - __builtin_clz(aicore_num));
}

STATIC int vmngh_ctrl_msg_send(u32 devid, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len)
{
    return devdrv_common_msg_send(devid, data, in_data_len, out_data_len, real_out_len, DEVDRV_COMMON_MSG_VMNG);
}

int vmngh_vdev_msg_send(struct vmng_ctrl_msg_info *info, int msg_type)
{
    struct vmng_ctrl_msg msg = { 0 };
    int ret;
    int len = 0;

    if (info == NULL) {
        vmng_err("Param NULL\n");
        return VMNG_ERR;
    }

    ret = memcpy_s(&msg.info_msg, sizeof(struct vmng_ctrl_msg_info), info, sizeof(struct vmng_ctrl_msg_info));
    if (ret != EOK) {
        vmng_err("Call memcpy_s err.(dev_id=%u; vfid=%u; msg_type=%d)\n", info->dev_id, info->vfid, msg_type);
        return ret;
    }
    msg.type = msg_type;

    ret = vmngh_ctrl_msg_send(info->dev_id, (void *)&msg, sizeof(msg), sizeof(msg), &len);
    if ((ret != VMNG_OK) || (len != sizeof(msg)) || (msg.error_code != VMNG_OK)) {
        vmng_err("Send message failed. (dev_id=%d; vfid=%d; ret=%d; len=%d; error_code=%d)\n", info->dev_id, info->vfid,
            ret, len, msg.error_code);
        return ret != 0 ? ret : msg.error_code;
    }

    ret = memcpy_s(info, sizeof(struct vmng_ctrl_msg_info), &msg.info_msg, sizeof(struct vmng_ctrl_msg_info));
    if (ret != EOK) {
        vmng_err("Call memcpy_s err.(dev_id=%d; vfid=%d)\n", info->dev_id, info->vfid);
        return ret;
    }

    return VMNG_OK;
}

int vmngh_add_mia_dev(u32 dev_id, u32 vfid, u32 agent_flag)
{
    struct uda_dev_type type;
    struct uda_dev_para para;
    struct uda_mia_dev_para mia_para;
    enum uda_dev_prop prop = devdrv_is_sriov_support(dev_id) ? UDA_REAL : UDA_VIRTUAL;
    enum uda_dev_object object = (agent_flag == 0) ? UDA_ENTITY : UDA_AGENT;
    u32 udevid = dev_id;
    ka_device_t *dev = NULL;
    int ret;

    if (devdrv_is_sriov_support(dev_id)) {
        devdrv_get_devid_by_pfvf_id(dev_id, vfid, &udevid);
        dev = devdrv_get_pci_dev_by_devid(udevid);
    }

    uda_dev_type_pack(&type, UDA_DAVINCI, object, UDA_NEAR, prop);
    uda_mia_dev_para_pack(&mia_para, dev_id, vfid - 1);

    if (agent_flag == 0) {
        uda_dev_para_pack(&para, UDA_INVALID_UDEVID, UDA_INVALID_UDEVID, uda_get_chip_type(dev_id), dev);
        ret = uda_add_mia_dev(&type, &para, &mia_para, &udevid);
    } else {
        (void)uda_mia_devid_to_udevid(&mia_para, &udevid);
        uda_dev_para_pack(&para, udevid, UDA_INVALID_UDEVID, uda_get_chip_type(dev_id), dev);
        ret = uda_add_dev(&type, &para, &udevid);
    }
    if (ret != 0) {
        vmng_err("Add uda mia device failed.(dev_id=%u; vfid=%u; ret=%d; agent_flag=%u)\n",
            dev_id, vfid, ret, agent_flag);
        return ret;
    }

    return 0;
}

int vmngh_remove_mia_dev(u32 dev_id, u32 vfid, u32 agent_flag)
{
    struct uda_dev_type type;
    struct uda_mia_dev_para mia_para;
    enum uda_dev_prop prop = devdrv_is_sriov_support(dev_id) ? UDA_REAL : UDA_VIRTUAL;
    enum uda_dev_object object = (agent_flag == 0) ? UDA_ENTITY : UDA_AGENT;
    u32 udevid;
    int ret;

    uda_mia_dev_para_pack(&mia_para, dev_id, vfid - 1);
    ret = uda_mia_devid_to_udevid(&mia_para, &udevid);
    if (ret != 0) {
        vmng_err("Get udevid failed.(dev_id=%u; vfid=%u; ret=%d)\n", dev_id, vfid, ret);
        return ret;
    }

    uda_dev_type_pack(&type, UDA_DAVINCI, object, UDA_NEAR, prop);
    ret = uda_remove_dev(&type, udevid);
    if (ret != 0) {
        vmng_err("Remove uda mia device failed.(dev_id=%u; vfid=%u; ret=%d)\n", dev_id, vfid, ret);
        return ret;
    }

    return 0;
}

STATIC int vmngh_device_online(const struct vmngh_vd_dev *vd_dev)
{
    int ret;
    u32 dev_id = vd_dev->dev_id;
    u32 vfid = vd_dev->fid;

    if (vfid == 0) { // PF no need to inform device
        return 0;
    }

    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("Input parameter is error. (dev_id=%d)\n", dev_id);
        return VMNG_ERR;
    }

    if (vmngh_get_device_status(dev_id) != VMNG_VALID) {
        vmng_err("Device is not ready. (dev_id=%d)\n", dev_id);
        return VMNG_ERR;
    }

    ret = vmngh_init_instance_client_device(dev_id, vfid);
    if (ret != VMNG_OK) {
        vmng_err("Init_instance_client_device failed. (dev_id=%u; vfid=%u)\n", dev_id, vfid);
        return VMNG_ERR;
    }

    vmng_info("Device online. (dev_id=%u; fid=%u)\n", vd_dev->dev_id, vd_dev->fid);
    return VMNG_OK;
}


STATIC void vmngh_device_offline(const struct vmngh_vd_dev *vd_dev)
{
    int ret;
    u32 dev_id = vd_dev->dev_id;
    u32 vfid = vd_dev->fid;

    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("Input parameter is error. (dev_id=%d)\n", dev_id);
        return;
    }

    if (vmngh_get_device_status(dev_id) != VMNG_VALID) {
        vmng_err("Device is not ready. (dev_id=%d)\n", dev_id);
        return;
    }

    ret = vmngh_uninit_instance_client_device(dev_id, vfid);
    if (ret != VMNG_OK) {
        vmng_err("Unnit_instance_client_device failed. (dev_id=%u; vfid=%u)\n", dev_id, vfid);
        return;
    }

    vmng_info("Device offline. (dev_id=%u; fid=%u)\n", vd_dev->dev_id, vd_dev->fid);
}

/* P2V */
STATIC void vmngh_admin_notify_rm_vdev(struct vmng_msg_dev *msg_dev, u32 flag)
{
    struct vmng_host_rm_vdev_cmd_reply cmd;
    struct vmng_tx_msg_proc_info tx_info;
    u32 dev_id = msg_dev->dev_id;
    u32 fid = msg_dev->fid;
    u32 ret;

    cmd.opcode = VMNGH_ADMIN_OPCODE_RM_VDEV;
    cmd.rm_mode = flag;
    cmd.finish = 0x0;

    tx_info.data = &cmd;
    tx_info.in_data_len = sizeof(struct vmng_host_rm_vdev_cmd_reply);
    tx_info.out_data_len = sizeof(struct vmng_host_rm_vdev_cmd_reply);
    tx_info.real_out_len = 0;

    ret = (u32)vmng_admin_msg_send(msg_dev->admin_tx, &tx_info, (u32)VMNG_MSG_CHAN_TYPE_ADMIN,
        (u32)VMNGH_ADMIN_OPCODE_RM_VDEV);
    if ((ret != 0) || (tx_info.real_out_len != tx_info.out_data_len)) {
        vmng_err("Message send failed. (dev_id=%u; fid=%u; out_len=%u; ret=%d)\n",
                 dev_id, fid, ret, tx_info.real_out_len);
        return;
    }
    if (cmd.finish != VMNG_VM_SUSPEND_SUCCESS) {
        vmng_err("Proc host stop failed. (dev_id=%u; fid=%u; ret=%d)\n", dev_id, fid, cmd.finish);
    }
}


/* V2P */
STATIC int vmngh_admin_para_check(const struct vmng_msg_dev *msg_dev,
    const struct vmng_msg_chan_rx_proc_info *proc_info)
{
    if (msg_dev == NULL) {
        vmng_err("Input parameter is error.\n");
        return -EINVAL;
    }
    if (proc_info == NULL) {
        vmng_err("Input parameter is error. (dev_id=%u; fid=%u)\n", msg_dev->dev_id, msg_dev->fid);
        return -EINVAL;
    }
    if (proc_info->data == NULL) {
        vmng_err("Input parameter is error. (dev_id=%u; fid=%u)\n", msg_dev->dev_id, msg_dev->fid);
        return -EINVAL;
    }
    return 0;
}

STATIC int vmngh_admin_rx_agent_rm_pdev(struct vmng_msg_dev *msg_dev, struct vmng_msg_chan_rx_proc_info *proc_info)
{
    struct vmng_agent_rm_pdev_cmd_reply *cmd = NULL;
    u32 dev_id;
    u32 fid;

    if (vmngh_admin_para_check(msg_dev, proc_info) != 0) {
        vmng_err("Parameter check failed.\n");
        return -EINVAL;
    }
    dev_id = msg_dev->dev_id;
    fid = msg_dev->fid;
    cmd = (struct vmng_agent_rm_pdev_cmd_reply *)proc_info->data;
    if ((cmd->rm_mode == VMNG_VM_SHUTDOWN_WAIT) || (cmd->rm_mode == VMNG_VM_REMOVE_WAIT)) {
        vmng_info("Agent vdevice remove or shutdown. (dev_id=%u; fid=%u; rm_mode=0x%x)\n", dev_id, fid, cmd->rm_mode);
        vmngh_host_stop(dev_id, fid);
        *(proc_info->real_out_len) = sizeof(struct vmng_agent_rm_pdev_cmd_reply);
        cmd->finish = VMNG_VM_SUSPEND_SUCCESS;
        return 0;
    } else {
        cmd->finish = 0;
        return -1;
    }
}

STATIC int vmngh_vdev_start_check(const struct vmngh_vd_dev *vd_dev)
{
    u32 dev_id = vd_dev->dev_id;
    u32 fid = vd_dev->fid;

    if (vmngh_get_ctrl_startup_flag(dev_id, fid) != VMNG_STARTUP_TOP_HALF_OK) {
        vmng_err("Top half not finish. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return -EINVAL;
    }

    if (vd_dev->shr_para->start_flag != VMNG_VM_START_WAIT) {
        vmng_err("start_flag is error. (dev_id=%u; fid=%u; start_flag=0x%x)\n",
                 dev_id, fid, vd_dev->shr_para->start_flag);
        return -EINVAL;
    }

    return 0;
}

STATIC int vmngh_vdev_prepare(struct vmngh_vd_dev *vd_dev)
{
    u32 dev_id = vd_dev->dev_id;
    u32 fid = vd_dev->fid;
    int ret;

    ret = vmngh_vdev_start_check(vd_dev);
    if (ret != 0) {
        vmng_err("Start check failed. (dev_id=%u; fid=%u; ret=%d)\n", dev_id, fid, ret);
        goto DIRECTLY_OUT;
    }
    vmng_info("Start check ok. (dev_id=%u; fid=%u)\n", dev_id, fid);

    ret = vmngh_prepare_msg_chan(vd_dev->msg_dev);
    if (ret != 0) {
        vmng_err("Prepare msg chans error. (dev_id=%u; fid=%u; pid=%u)\n", dev_id, fid, vd_dev->vm_pid);
        goto DIRECTLY_OUT;
    }

    vd_dev->agent_device = vd_dev->shr_para->agent_device;
    vd_dev->agent_bdf = vd_dev->shr_para->agent_bdf;
    vd_dev->agent_dev_id = vd_dev->shr_para->agent_dev_id;
    if (vd_dev->agent_dev_id >= VMNG_PDEV_MAX) {
        vmng_err("Trans virt devid error. (dev_id=%u; fid=%u; pid=%u)\n", dev_id, fid, vd_dev->vm_pid);
        vmngh_unprepare_msg_chan(vd_dev->msg_dev);
        return -EINVAL;
    }
    ret = vmngh_alloc_vm_id(dev_id, fid, vd_dev->vm_pid, vd_dev->agent_dev_id);
    if (ret != 0) {
        vmng_err("Alloc vm id failed. (dev_id=%u; fid=%u; pid=%u)\n", dev_id, fid, vd_dev->vm_pid);
        goto UNPREPARE_MSG_CHAN;
    }

    /* ctrls probe finish. */
    vmngh_ctrl_register_half(vd_dev);
    vd_dev->vm_id = (u32)vmngh_ctrl_get_vm_id(dev_id, fid);

    return 0;
UNPREPARE_MSG_CHAN:
    vmngh_unprepare_msg_chan(vd_dev->msg_dev);

DIRECTLY_OUT:
    return ret;
}

STATIC void vmngh_vdev_unprepare(struct vmngh_vd_dev *vd_dev)
{
    vmngh_ctrl_unregister_half(vd_dev);
    vmngh_ctrl_rm_vm_id(vd_dev->dev_id, vd_dev->fid, vd_dev->agent_dev_id);
    vmngh_unprepare_msg_chan(vd_dev->msg_dev);
}

STATIC int vmngh_notify_vdev_probe_bottom_half(struct vmngh_vd_dev *vd_dev)
{
    struct vmngh_start_dev *start_dev = &(vd_dev->start_dev);
    u32 msix_offset;
    int ret;

    /* update infomation use shr para */
    vd_dev->shr_para->dtype = (u32)vd_dev->dtype.type;
    vd_dev->shr_para->start_flag = VMNG_VM_START_SUCCESS;
    ka_wmb();
    msix_offset = vd_dev->shr_para->msix_offset;
    /* notice vm to start bottom half probe */
    ret = hw_dvt_hypervisor_inject_msix(vd_dev->vdavinci, start_dev->msix_irq + msix_offset);
    if (ret != 0) {
        vmng_err("Notify vm start half probe failed. (devid=%u; fid=%u; ret=%d)\n", vd_dev->dev_id, vd_dev->fid, ret);
        return -EIO;
    }
    return 0;
}

STATIC int vmngh_vm_add_dev(u32 dev_id, u32 vfid)
{
    int ret;

    ret = vmngh_add_mia_dev(dev_id, vfid, 0); /* add entity */
    if (ret != 0) {
        vmng_err("Add entity failed. (devid=%u; vfid=%u; ret=%d)\n", dev_id, vfid, ret);
        return ret;
    }

    ret = vmngh_add_mia_dev(dev_id, vfid, 1); /* add agent */
    if (ret != 0) {
        (void)vmngh_remove_mia_dev(dev_id, vfid, 0);
        vmng_err("Add agent failed. (devid=%u; vfid=%u; ret=%d)\n", dev_id, vfid, ret);
        return ret;
    }

    return 0;
}

STATIC void vmngh_vm_remove_dev(u32 dev_id, u32 vfid)
{
    (void)vmngh_remove_mia_dev(dev_id, vfid, 1);
    (void)vmngh_remove_mia_dev(dev_id, vfid, 0);
}

STATIC int vmngh_add_sec_eh_dev(u32 dev_id, u32 vfid)
{
    struct uda_dev_type type;
    struct uda_dev_para para;
    struct uda_mia_dev_para mia_para;
    u32 udevid = dev_id;

    if (devdrv_is_sriov_support(dev_id)) {
        devdrv_get_devid_by_pfvf_id(dev_id, vfid, &udevid);
    }

    uda_dev_type_pack(&type, UDA_DAVINCI, UDA_ENTITY, UDA_NEAR, UDA_REAL_SEC_EH);
    uda_dev_para_pack(&para, UDA_INVALID_UDEVID,
        UDA_INVALID_UDEVID, uda_get_chip_type(dev_id), devdrv_get_pci_dev_by_devid(udevid));
    uda_mia_dev_para_pack(&mia_para, dev_id, vfid - 1);

    return uda_add_mia_dev(&type, &para, &mia_para, &udevid);
}

STATIC void vmngh_remove_sec_eh_dev(u32 dev_id, u32 vfid)
{
    struct uda_dev_type type;
    struct uda_mia_dev_para mia_para;
    u32 udevid;
    int ret;

    uda_mia_dev_para_pack(&mia_para, dev_id, vfid - 1);
    ret = uda_mia_devid_to_udevid(&mia_para, &udevid);
    if (ret != 0) {
        vmng_err("Get udevid failed.(dev_id=%u; vfid=%u; ret=%d)\n", dev_id, vfid, ret);
        return;
    }

    uda_dev_type_pack(&type, UDA_DAVINCI, UDA_ENTITY, UDA_NEAR, UDA_REAL_SEC_EH);
    (void)uda_remove_dev(&type, udevid);
}

STATIC int vmngh_service_online(struct vmngh_vd_dev *vd_dev)
{
    u32 dev_id = vd_dev->dev_id;
    u32 fid = vd_dev->fid;
    u32 vdev_id;
    int ret;

    if ((dev_id >= VMNG_PDEV_MAX) || (fid >= VMNG_VDEV_MAX_PER_PDEV) || (fid == 0)) {
        vmng_err("dev_id is invalid. (dev_id=%d;fid=%u)\n", dev_id, fid);
        return -EINVAL;
    }

    if (devdrv_is_sriov_support(dev_id)) {
        ret = vmngh_device_online(vd_dev);
        if (ret != 0) {
            vmng_err("Agent online failed. (devid=%u; fid=%u; ret=%d)\n", dev_id, fid, ret);
            return ret;
        }
        vdev_id = vmngh_get_ctrl(dev_id, fid)->vdev_ctrl.dev_id;
        ret = devdrv_sriov_init_instance(vdev_id);
        if (ret != 0) {
            vmng_err("Init pcie initstance failed. (ret=%d;dev_id=%u)\n", ret, vdev_id);
            vmngh_device_offline(vd_dev);
            return ret;
        }

        ret = vmngh_add_sec_eh_dev(dev_id, fid);
        if (ret != 0) {
            (void)devdrv_sriov_uninit_instance(vdev_id);
            vmngh_device_offline(vd_dev);
            return ret;
        }

        ret = vmngh_init_instance_after_probe(dev_id, fid);
        if (ret != 0) {
            vmng_err("Init instance all client failed. (devid=%u; fid=%u; ret=%d)\n", dev_id, fid, ret);
            vmngh_remove_sec_eh_dev(dev_id, fid);
            devdrv_sriov_uninit_instance(vdev_id);
            vmngh_device_offline(vd_dev);
            return ret;
        }
    } else {
        ret = vmngh_vm_add_dev(dev_id, fid);
        if (ret != 0) {
            return ret;
        }

        ret = vmngh_init_instance_after_probe(dev_id, fid);
        if (ret != 0) {
            vmng_err("Init instance all client failed. (devid=%u; fid=%u; ret=%d)\n", dev_id, fid, ret);
            vmngh_vm_remove_dev(dev_id, fid);
            return ret;
        }
        ret = vmngh_device_online(vd_dev);
        if (ret != 0) {
            vmng_err("Agent online failed. (devid=%u; fid=%u; ret=%d)\n", dev_id, fid, ret);
            (void)vmngh_uninit_instance_remove_vdev(dev_id, fid);
            vmngh_vm_remove_dev(dev_id, fid);
            return ret;
        }
    }

    return 0;
}

STATIC void vmngh_service_offline(struct vmngh_vd_dev *vd_dev)
{
    u32 dev_id = vd_dev->dev_id;
    u32 fid = vd_dev->fid;
    u32 vdev_id;
    int ret;

    if ((dev_id >= VMNG_PDEV_MAX) || (fid >= VMNG_VDEV_MAX_PER_PDEV) || (fid == 0)) {
        vmng_err("dev_id is invalid. (dev_id=%d;fid=%u)\n", dev_id, fid);
        return;
    }

    if (devdrv_is_sriov_support(dev_id)) {
        ret = vmngh_uninit_instance_remove_vdev(vd_dev->dev_id, vd_dev->fid);
        if (ret != 0) {
            vmng_err("Uninit instance remove vdev failed. (ret=%d)\n", ret);
        }
        vmngh_remove_sec_eh_dev(dev_id, fid);
        vdev_id = vmngh_get_ctrl(dev_id, fid)->vdev_ctrl.dev_id;
        ret = devdrv_sriov_uninit_instance(vdev_id);
        if (ret != 0) {
            vmng_err("Uninit pcie initstance failed. (ret=%d;dev_id=%u)\n", ret, vdev_id);
        }
        vmngh_device_offline(vd_dev);
    } else {
        vmngh_device_offline(vd_dev);
        ret = vmngh_uninit_instance_remove_vdev(vd_dev->dev_id, vd_dev->fid);
        if (ret != 0) {
            vmng_err("Uninit instance remove vdev failed. (ret=%d)\n", ret);
        }
        vmngh_vm_remove_dev(dev_id, fid);
    }
    return;
}


STATIC int vmngh_vdev_feature(struct vmngh_vd_dev *vd_dev)
{
    u32 dev_id = vd_dev->dev_id;
    u32 fid = vd_dev->fid;
    int ret;

    ret = vmngh_service_online(vd_dev);
    if (ret != 0) {
        vmng_err("Init instance failed. (dev_id=%u;fid=%u;ret=%d)\n", dev_id, fid, ret);
        return ret;
    }
    vmng_info("Init instance ok. (devid=%u; fid=%u)\n", dev_id, fid);

    ret = vmngh_notify_vdev_probe_bottom_half(vd_dev);
    if (ret != 0) {
        vmng_err("Notify vdev start probe bottom half failed. (dev_id=%u; fid=%u; ret=%d)\n", dev_id, fid, ret);
        goto UNINIT_INSTANCE;
    }

    return 0;

UNINIT_INSTANCE:
    vmngh_service_offline(vd_dev);
    return ret;
}

STATIC int vmngh_vdev_unfeature(struct vmngh_vd_dev *vd_dev)
{
    vmngh_service_offline(vd_dev);
    return 0;
}

STATIC void vmngh_start_timer_task(ka_timer_list_t *t)
{
    struct vmngh_start_dev *start_dev = ka_system_from_timer(start_dev, t, wd_timer);
    struct vmngh_vd_dev *vd_dev = ka_container_of(start_dev, struct vmngh_vd_dev, start_dev);
    u32 dev_id = vd_dev->dev_id;
    u32 fid = vd_dev->fid;

    if (ka_base_atomic_read(&start_dev->start_flag) == VMNG_TASK_SUCCESS) {
        vmng_info("Startup success, timer close. (devid=%u; fid=%u)\n", dev_id, fid);
        return;
    }
    if (start_dev->timer_remain > 0) {
        start_dev->timer_remain--;
        start_dev->wd_timer.expires = ka_jiffies + start_dev->timer_cycle;
        ka_system_add_timer(&(start_dev->wd_timer));
        vmng_debug("Startup timer rebuild. (devid=%u; fid=%u)\n", dev_id, fid);
    } else {
        vmng_warn("Startup timeout. (devid=%u; fid=%u)\n", dev_id, fid);
        ka_base_atomic_set(&start_dev->start_flag, VMNG_TASK_TIMEOUT);
    }
}

STATIC void vmngh_set_start_timer(struct vmngh_vd_dev *vd_dev)
{
    struct vmngh_start_dev *start_dev = &(vd_dev->start_dev);

    /* create start check timer, vmngh_vm_load_irq to set status to close timer */
    start_dev->timer_remain = VMNG_START_TIMER_OUT;
    start_dev->timer_cycle = VMNG_START_TIMER_CYCLE;
    ka_system_timer_setup(&start_dev->wd_timer, vmngh_start_timer_task, 0);
    start_dev->wd_timer.expires = ka_jiffies + start_dev->timer_cycle;
    ka_system_add_timer(&(start_dev->wd_timer));
}

STATIC void vmngh_del_start_timer(struct vmngh_vd_dev *vd_dev)
{
    if (vd_dev->start_dev.timer_cycle != 0) {
        ka_system_del_timer_sync(&vd_dev->start_dev.wd_timer);
        vd_dev->start_dev.timer_cycle = 0;
        vmng_info("Delete start timer. (devid=%u; fid=%u)\n", vd_dev->dev_id, vd_dev->fid);
    }
}

#define MAX_PCIE_MSIX_NUM 64
#define MAX_PASS_TO_VM_MSIX_NUM 44
STATIC ka_irqreturn_t msi_inject_interrupt(int irq, void *data)
{
    struct vm_msix_struct *vm_msix = (struct vm_msix_struct *)data;
    int ret;

    if ((vm_msix == NULL) || (vm_msix->vdavinci == NULL)) {
        vmng_err("Vdavinci is null. (irq=%d)\n", irq);
        return KA_IRQ_NONE;
    }

    if ((vm_msix->irq_vector >= g_max_pass_to_vm_msix_num) || (vm_msix->irq_vector < 0)) {
        vmng_err("Invalid irq vector. (vector=%d, irq=%d)\n", vm_msix->irq_vector, irq);
        return KA_IRQ_NONE;
    }

    ret = hw_dvt_hypervisor_inject_msix(vm_msix->vdavinci, (u32)vm_msix->irq_vector);
    if (ret != 0) {
        vmng_err("Inject msix irq fail.(irq=%d;vector=%d;ret=%d)\n", irq, vm_msix->irq_vector, ret);
        return KA_IRQ_NONE;
    }

    return KA_IRQ_HANDLED;
}

int vmngh_hypervisor_inject_msix(unsigned int dev_id, unsigned int irq_vector)
{
    unsigned int pf_id = dev_id;
    unsigned int vf_id = 0;
    void *vdavinci;
    int ret;

    ret = devdrv_get_pfvf_id_by_devid(dev_id, &pf_id, &vf_id);
    if (ret != 0) {
        return ret;
    }
    vdavinci = vmngh_get_vdavinci_by_id(pf_id, vf_id);
    if (vdavinci == NULL) {
        return -EINVAL;
    }
    return hw_dvt_hypervisor_inject_msix(vdavinci, irq_vector);
}
KA_EXPORT_SYMBOL(vmngh_hypervisor_inject_msix);

STATIC int vmngh_passthrough_msix_irq_to_vm(unsigned int dev_id, struct vmngh_vd_dev *vd_dev)
{
    struct vm_msix_struct *vm_msix;
    ka_pci_dev_t *pdev;
    int ret;
    int irq;
    int i;

    pdev = devdrv_get_pci_pdev_by_devid(dev_id);
    if (pdev == NULL) {
        vmng_err("pdev is NULL.\n");
        return -EINVAL;
    }

    vm_msix = vmng_kzalloc(sizeof(struct vm_msix_struct) * g_max_msix_num, KA_GFP_KERNEL);
    if (vm_msix == NULL) {
        vmng_err("Alloc vm msix failed. (dev_id=%u;vfid=%u)\n", vd_dev->dev_id, vd_dev->fid);
        return -ENOMEM;
    }

    vd_dev->vm_msix = vm_msix;
    for (i = 0; i < g_max_pass_to_vm_msix_num; i++) {
        vm_msix[i].vdavinci = vd_dev->vdavinci;
        vm_msix[i].irq_vector = i;
        ka_pci_irq_vector(pdev, (u32)i, dev_id, (u32 *)&irq, devdrv_get_irq_vector);
        ret = ka_system_request_irq((u32)irq, msi_inject_interrupt, 0, "msix_inject", vm_msix + i);
        if (ret < 0) {
            vmng_err("request irq failed, ret : %d\n", ret);
            goto failed;
        }
    }
    return 0;

failed:
    for (--i; i >= 0; --i) {
        ka_pci_irq_vector(pdev, (u32)i, dev_id, (u32 *)&irq, devdrv_get_irq_vector);
        ka_system_free_irq(irq,  vm_msix + i);
    }
    vmng_kfree(vm_msix);
    vd_dev->vm_msix = NULL;
    return ret;
}

STATIC void vmngh_release_passthrough_to_vm_msix_irq(unsigned int dev_id, struct vmngh_vd_dev *vd_dev)
{
    ka_pci_dev_t *pdev;
    int irq;
    int i;

    pdev = devdrv_get_pci_pdev_by_devid(dev_id);
    if (pdev == NULL) {
        vmng_err("pdev is NULL.\n");
        return;
    }
    for (i = 0; i < g_max_pass_to_vm_msix_num; i++) {
        ka_pci_irq_vector(pdev, (u32)i, dev_id, (u32 *)&irq, devdrv_get_irq_vector);
        (void)ka_system_free_irq((u32)irq, vd_dev->vm_msix + i);
    }
    vmng_kfree(vd_dev->vm_msix);
    vd_dev->vm_msix = NULL;
}

STATIC void vmngh_disable_pcie_feature(struct vmngh_vd_dev *vd_dev)
{
    struct vmng_ctrl_msg msg = {0};
    u32 dev_id;
    u32 len;
    int ret;

    if (devdrv_is_sriov_support(vd_dev->dev_id) == false) {
        return;
    }
    ret = devdrv_get_devid_by_pfvf_id(vd_dev->dev_id, vd_dev->fid, &dev_id);
    if (ret != 0) {
        vmng_err("Get devid by pfvf id failed. (ret=%d;dev_id=%u;fid=%u)\n", ret, vd_dev->dev_id, vd_dev->fid);
        return;
    }

    // info device to unset iova
    msg.type = VMNG_CTRL_MSG_TYPE_IOVA_INFO;
    msg.info_msg.iova_info.vfid = vd_dev->fid;
    msg.info_msg.iova_info.action = VMNG_INSTANCE_FLAG_UNINIT;
    (void)vmngh_ctrl_msg_send(vd_dev->dev_id, (void *)&msg, sizeof(msg), sizeof(msg), &len);

    // info pcie
    (void)devdrv_mdev_set_pm_iova_addr_range(dev_id, (ka_dma_addr_t)0, (size_t)0);
    vmngh_release_passthrough_to_vm_msix_irq(dev_id, vd_dev);
    devdrv_mdev_pm_uninit_msi_interrupt(dev_id);
    devdrv_mdev_free_vf_dma_sqcq_on_pm(dev_id);
    return;
}

STATIC int vmngh_enable_pcie_feature(struct vmngh_vd_dev *vd_dev)
{
    struct vmng_ctrl_msg msg = {0};
    u32 dev_id;
    u32 len;
    int ret;

    if (devdrv_is_sriov_support(vd_dev->dev_id) == false) {
        return 0;
    }

    ret = devdrv_get_devid_by_pfvf_id(vd_dev->dev_id, vd_dev->fid, &dev_id);
    if (ret != 0) {
        vmng_err("Get devid by pfvf id failed. (ret=%d;dev_id=%u;fid=%u)\n", ret, vd_dev->dev_id, vd_dev->fid);
        return ret;
    }

    ret = devdrv_mdev_pm_init_msi_interrupt(dev_id);
    if (ret != 0) {
        vmng_err("PCI-e alloc msix irq failed. (ret=%d;dev_id=%u)\n", ret, dev_id);
        return ret;
    }

    ret = vmngh_passthrough_msix_irq_to_vm(dev_id, vd_dev);
    if (ret != 0) {
        vmng_err("Passthrough msix irq to vm failed. (ret=%d;dev_id=%u)\n", ret, dev_id);
        devdrv_mdev_pm_uninit_msi_interrupt(dev_id);
        return ret;
    }

    ret = hw_dvt_hypervisor_get_reserve_iova(vd_dev->resource_dev, &vd_dev->iova_addr, &vd_dev->size);
    if (ret != 0) {
        vmngh_release_passthrough_to_vm_msix_irq(dev_id, vd_dev);
        devdrv_mdev_pm_uninit_msi_interrupt(dev_id);
        vmng_err("Get iova reserver failed. (dev_id=%u;ret=%d)\n", dev_id, ret);
        return ret;
    }

    ret = devdrv_mdev_set_pm_iova_addr_range(dev_id, vd_dev->iova_addr, vd_dev->size);
    if (ret != 0) {
        vmngh_release_passthrough_to_vm_msix_irq(dev_id, vd_dev);
        devdrv_mdev_pm_uninit_msi_interrupt(dev_id);
        vmng_err("Set iova info to pcie failed.(vdev_id=%u;ret=%d)\n", dev_id, ret);
        return ret;
    }

    msg.type = VMNG_CTRL_MSG_TYPE_IOVA_INFO;
    msg.info_msg.iova_info.vfid = vd_dev->fid;
    msg.info_msg.iova_info.iova_base = vd_dev->iova_addr;
    msg.info_msg.iova_info.size = vd_dev->size;
    msg.info_msg.iova_info.action = VMNG_INSTANCE_FLAG_INIT;
    ret = vmngh_ctrl_msg_send(vd_dev->dev_id, (void *)&msg, sizeof(msg), sizeof(msg), &len);
    if ((ret != VMNG_OK) || (len != sizeof(msg)) || (msg.error_code != VMNG_OK)) {
        vmng_err("Send iova msg to device failed.(ret=%d;error_code=%d)\n", ret, msg.error_code);
        devdrv_mdev_set_pm_iova_addr_range(dev_id, (ka_dma_addr_t)0, 0);
        vmngh_release_passthrough_to_vm_msix_irq(dev_id, vd_dev);
        devdrv_mdev_pm_uninit_msi_interrupt(dev_id);
        return VMNG_ERR;
    }

    return 0;
}

STATIC void vmngh_vdev_create_bottom(ka_work_struct_t *p_work)
{
    struct vmngh_vd_dev *vd_dev = ka_container_of(p_work, struct vmngh_vd_dev, start_work);
    int ret;
    u32 dev_id = vd_dev->dev_id;
    u32 fid = vd_dev->fid;

    vd_dev->dfx.create_bottom_cnt++;
    vmng_info("Get create_bottom_cnt. (dev_id=%u; fid=%u; dtype=%u; agent_dev_id=%u; create_bottom_cnt=%u)\n",
              dev_id, fid, vd_dev->dtype.type, vd_dev->shr_para->agent_dev_id, vd_dev->dfx.create_bottom_cnt);

    if ((dev_id >= VMNG_PDEV_MAX) || (fid >= VMNG_VDEV_MAX_PER_PDEV)) {
        vmng_err("dev_id is invalid. (dev_id=%d;fid=%u)\n", dev_id, fid);
        return;
    }

    if (ka_base_atomic_read(&vd_dev->reset_ref_flag) != VMNGH_MDEV_FLR_STATE) {
        ret = hw_dvt_hypervisor_dma_pool_init(vd_dev->vdavinci);
        if (ret != 0) {
            vmng_err("Dma pool init failed. (dev_id=%u; fid=%u; ret=%d)\n", dev_id, fid, ret);
            goto DIRECTLY_OUT;
        }
    } else {
        vmng_info("Skip dma pool init in flr process. (dev_id=%u; fid=%u)\n", dev_id, fid);
    }
    ret = vmngh_enable_pcie_feature(vd_dev);
    if (ret != 0) {
        vmng_err("enable pcie msix inject failed. (dev_id=%u; fid=%u; ret=%d)\n", dev_id, fid, ret);
        goto DMA_POOL_UNINIT;
    }

    ret = vmngh_vdev_prepare(vd_dev);
    if (ret != 0) {
        vmng_err("Prepara vdev failed. (dev_id=%u; fid=%u; ret=%d)\n", dev_id, fid, ret);
        goto MSIX_DISABLE;
    }

    ret = vmngh_vdev_feature(vd_dev);
    if (ret != 0) {
        vmng_err("Initialize feature failed. (dev_id=%u; fid=%u; ret=%d)\n", dev_id, fid, ret);
        goto VDEV_UNPREARE;
    }

    if (devdrv_is_sriov_support(dev_id) == false) {
        ret = vmngh_bw_set_token_limit(dev_id, fid);
        if (ret != 0) {
            vmng_err("Set bandwidth limit failed. (dev_id=%u; fid=%u; ret=%d)\n", dev_id, fid, ret);
            goto VDEV_UNPREARE;
        }
        vmngh_bw_data_clear_timer_init(dev_id, fid);
    }

    /* success */
    vd_dev->init_status = VMNG_STARTUP_BOTTOM_HALF_OK;
    ka_base_atomic_set(&vd_dev->reset_ref_flag, VMNGH_MDEV_RUNNING_STATE);
    vmng_info("Bottom create success. (dev_id=%u; fid=%u)\n", dev_id, fid);

    return;

VDEV_UNPREARE:
    vmngh_vdev_unprepare(vd_dev);
MSIX_DISABLE:
    vmngh_disable_pcie_feature(vd_dev);
DMA_POOL_UNINIT:
    if (ka_base_atomic_read(&vd_dev->reset_ref_flag) != VMNGH_MDEV_FLR_STATE) {
        hw_dvt_hypervisor_dma_pool_uninit(vd_dev->vdavinci);
    }
    ka_base_atomic_set(&vd_dev->reset_ref_flag, VMNGH_MDEV_RUNNING_STATE);
DIRECTLY_OUT:
    vmng_err("Create half bottom failed. (dev_id=%u; fid=%u)\n", dev_id, fid);
    return;
}

STATIC int vmngh_vdev_free_bottom(struct vmngh_vd_dev *vd_dev)
{
    u32 dev_id = vd_dev->dev_id;
    u32 fid = vd_dev->fid;

    vmng_info("Free bottom begin. (dev_id=%u; fid=%u)\n", dev_id, fid);
    vmngh_bw_data_clear_timer_uninit();
    vd_dev->init_status = VMNG_STARTUP_TOP_HALF_OK;

    (void)vmngh_vdev_unfeature(vd_dev);
    vmngh_vdev_unprepare(vd_dev);
    vmngh_disable_pcie_feature(vd_dev);
    if (ka_base_atomic_read(&vd_dev->reset_ref_flag) != VMNGH_MDEV_FLR_STATE) {
        hw_dvt_hypervisor_dma_pool_uninit(vd_dev->vdavinci);
        vmng_info("Dma pool uninit end. (dev_id=%u; fid=%u)\n", dev_id, fid);
    } else {
        vmng_info("Skip dma pool uninit in flr process. (dev_id=%u; fid=%u)\n", dev_id, fid);
    }
    vmngh_del_start_timer(vd_dev);
    vmngh_ctrl_set_startup_flag(vd_dev->dev_id, vd_dev->fid, VMNG_STARTUP_TOP_HALF_OK);
    /* vdavinci will invoke reset to clear create bottom, when vm start */
    ka_base_atomic_set(&(vd_dev->start_dev.start_flag), VMNG_TASK_WAIT);
    vmng_info("Free bottom finish. (dev_id=%u; fid=%u)\n", dev_id, fid);

    return 0;
}

static void vmngh_wait_vm_start(struct vmngh_vd_dev *vd_dev)
{
    /* create half bottom, when vm start, we get a doorbell then schule. */
    KA_TASK_INIT_WORK(&vd_dev->start_work, vmngh_vdev_create_bottom);

    /* set status */
    ka_base_atomic_set(&(vd_dev->start_dev.start_flag), VMNG_TASK_WAIT);
}

static void vmngh_start_exit(struct vmngh_vd_dev *vd_dev)
{
    if (vd_dev->start_work.func != NULL) {
        ka_task_cancel_work_sync(&vd_dev->start_work);
    }
}

static struct vmngh_vd_dev *vmngh_alloc_vdev(u32 fid, void *vdavinci, struct vmngh_pci_dev *vm_pdev,
    struct vdavinci_type *vd_type, ka_uuid_le_t uuid)
{
    struct vmngh_vd_dev *vd_dev = NULL;
    int ret;

    if (vm_pdev->vd_dev[fid] != NULL) {
        vmng_err("Input parameter is error. (dev_id=%u; fid=%u)\n", vm_pdev->dev_id, fid);
        return NULL;
    }

    vd_dev = vmng_kzalloc(sizeof(struct vmngh_vd_dev), KA_GFP_KERNEL);
    if (vd_dev == NULL) {
        vmng_err("Alloc device failed. (dev_id=%u; fid=%u)\n", vm_pdev->dev_id, fid);
        return NULL;
    }
    vd_dev->vm_pdev = vm_pdev;
    vd_dev->fid = fid;
    vd_dev->vdavinci = vdavinci;
    vd_dev->dev_id = vm_pdev->dev_id;
    vd_dev->vascend_uuid = uuid;
    ret = memcpy_s((void *)(&vd_dev->dtype), sizeof(struct vdavinci_type),
        (void *)vd_type, sizeof(struct vdavinci_type));
    if (ret != 0) {
        vmng_err("Call memcpy failed. (dev_id=%u; fid=%u)\n", vm_pdev->dev_id, fid);
        goto FREE_VD_DEV;
    }
    vd_dev->dtype.template_name[HW_DVT_MAX_TYPE_NAME - 1] = '\0';
    vd_dev->dtype.type = (int)vmngh_get_dtype_by_aicore(vd_type->aicore_num);

    ret = hw_dvt_hypervisor_mmio_get(&vd_dev->mm_res.io_base, &vd_dev->mm_res.io_size, vdavinci, IO_REGION_INDEX);
    if ((ret != 0) || (vd_dev->mm_res.io_base == NULL)) {
        vmng_err("Hypervisor get bar2 failed. (dev_id=%u; fid=%u)\n", vm_pdev->dev_id, fid);
        goto FREE_VD_DEV;
    }
    ret = hw_dvt_hypervisor_mmio_get(&vd_dev->mm_res.mem_base, &vd_dev->mm_res.mem_size, vdavinci, MEM_REGION_INDEX);
    if ((ret != 0) || (vd_dev->mm_res.mem_base == NULL)) {
        vmng_err("Hypervisor get bar4 failed. (dev_id=%u; fid=%u)", vm_pdev->dev_id, fid);
        goto FREE_VD_DEV;
    }

    vd_dev->shr_para = (struct vmng_shr_para *)vd_dev->mm_res.io_base + VMNG_SHR_PARA_ADDR_BASE;
    vd_dev->shr_para->dtype = (u32)vd_type->type;
    if (vm_pdev->vm_full_spec_enable) {
        vd_dev->shr_para->aicore_num = vm_pdev->dev_info.aicore_num;
    } else {
        vd_dev->shr_para->aicore_num = vd_type->aicore_num;
    }
    vd_dev->shr_para->hbmmem_size = vd_type->hbmmem_size;
    vd_dev->shr_para->ddrmem_size = vd_type->ddrmem_size;
    ka_base_atomic_set(&vd_dev->reset_ref_flag, VMNGH_MDEV_RUNNING_STATE);

    /* mount at last, to avoid heap use after free, if mmio get failed */
    vm_pdev->vd_dev[fid] = vd_dev;

    return vd_dev;

FREE_VD_DEV:
    vmng_kfree(vd_dev);
    vd_dev = NULL;
    return NULL;
}

STATIC int vmngh_alloc_vpc_unit(struct vmngh_vd_dev *vd_dev)
{
    struct vmngh_pci_dev *vm_pdev = vd_dev->vm_pdev;
    struct vmngh_vpc_unit *vpc_unit;

    vpc_unit = vmng_kzalloc(sizeof(struct vmngh_vpc_unit), KA_GFP_KERNEL);
    if (vpc_unit == NULL) {
        vmng_err("Kzalloc vpc unit failed.\n");
        return -ENOMEM;
    }
    vpc_unit->vdavinci = vd_dev->vdavinci;
    vpc_unit->pdev = vm_pdev->pdev;
    vpc_unit->db_base = 0x0;
    vpc_unit->msg_base = vd_dev->mm_res.io_base + VMNG_MSG_QUEUE_BASE;
    vpc_unit->shr_para = vd_dev->shr_para;
    if (devdrv_is_sriov_support(vd_dev->dev_id)) {
        devdrv_get_devid_by_pfvf_id(vd_dev->dev_id, vd_dev->fid, &vpc_unit->dev_id);
        vpc_unit->fid = 0;
    } else {
        vpc_unit->dev_id = vd_dev->dev_id;
        vpc_unit->fid = vd_dev->fid;
    }
    vd_dev->vpc_unit = vpc_unit;

    return 0;
}

STATIC int vmngh_info_vpc_init(struct vmngh_vd_dev *vd_dev)
{
    unsigned int dev_id;
    int ret;

    if (devdrv_is_sriov_support(vd_dev->dev_id) == false) {
        return 0;
    }

    if (vd_dev->fid == 0) {
        dev_id = vd_dev->dev_id;
    } else {
        ret = devdrv_get_devid_by_pfvf_id(vd_dev->dev_id, vd_dev->fid, &dev_id);
        if (ret != 0) {
            vmng_err("get dev_id by pfvf id failed. (dev_id=%u;fid=%u;ret=%d)\n",
                vd_dev->dev_id, vd_dev->fid, ret);
            return ret;
        }
    }
    ret = devdrv_vpc_client_init(dev_id);
    if (ret != 0) {
        vmng_err("Init pcie vpc failed. (dev_id=%u;ret=%d)\n", dev_id, ret);
    }
    return ret;
}

STATIC int vmngh_create_base_feature(struct vmngh_vd_dev *vd_dev)
{
    u32 dev_id = vd_dev->dev_id;
    u32 fid = vd_dev->fid;
    int ret;

    ret = vmngh_alloc_vpc_unit(vd_dev);
    if (ret != 0) {
        vmng_err("Alloc vpc unit failed. (ret=%d)\n", ret);
        goto OUT;
    }

    /* message chan top half initial, bottom half initial after VM returned */
    ret = vmngh_init_vpc_msg((void *)vd_dev->vpc_unit);
    if (ret != 0) {
        vmng_err("Message device init failed. (dev_id=%u; fid=%u; ret=%d)\n", dev_id, fid, ret);
        goto FREE_VPC_UNIT;
    }
    vd_dev->msg_dev = vd_dev->vpc_unit->msg_dev;
    vmngh_register_extended_common_msg_client(vd_dev->msg_dev);

    /* alloc external db */
    ret = vmngh_alloc_external_db_entries(vd_dev);
    if (ret != 0) {
        vmng_err("Alloc external db failed. (dev_id=%u; fid=%u)\n", dev_id, fid);
        goto UNREGISTER_EXTENDED_COMMON_MSG_CLIENT;
    }
    ret = vmngh_info_vpc_init(vd_dev);
    if (ret != 0) {
        vmng_err("Info vpc init failed. (dev_id=%u;fid=%u;ret=%d)\n", dev_id, fid, ret);
        goto FREE_EXTERNAL_DB_ENTRIES;
    }
    return 0;

FREE_EXTERNAL_DB_ENTRIES:
    vmngh_free_external_db_entries(vd_dev);
UNREGISTER_EXTENDED_COMMON_MSG_CLIENT:
    vmngh_unregister_extended_common_msg_client(vd_dev->msg_dev);
    vmngh_uninit_vpc_msg((void *)vd_dev->vpc_unit);
FREE_VPC_UNIT:
    if (vd_dev->vpc_unit != NULL) {
        vmng_kfree(vd_dev->vpc_unit);
        vd_dev->vpc_unit = NULL;
    }
OUT:
    return ret;
}

STATIC void vmngh_destroy_base_feature(struct vmngh_vd_dev *vd_dev)
{
    vmngh_free_external_db_entries(vd_dev);
    vmngh_unregister_extended_common_msg_client(vd_dev->msg_dev);
    vmngh_uninit_vpc_msg((void *)vd_dev->vpc_unit);
}

STATIC int vmngh_vdev_free_top(struct vmngh_vd_dev *vd_dev)
{
    struct vmngh_ctrl_ops *ops = vmngh_get_ctrl_ops(vd_dev->dev_id);
    u32 dev_id = vd_dev->dev_id;
    u32 fid = vd_dev->fid;

    vmngh_del_start_timer(vd_dev);
    vmngh_start_exit(vd_dev);
    if ((ops->free_vf != NULL) && (ops->free_vf(dev_id, fid) != VMNG_OK)) {
        vmng_err("Send msg failed. (dev_id=%d; vfid=%d)\n", dev_id, fid);
    }
    vmngh_destroy_base_feature(vd_dev);
    vmngh_unregister_ctrls(dev_id, fid);
    vmngh_free_vdev(vd_dev);
    vmng_info("Free top finish. (dev_id=%u; fid=%u)\n", dev_id, fid);
    return 0;
}

STATIC int vmngh_vdev_create_para_check(const struct vdavinci_dev *vdev, const void *vdavinci)
{
    if (vdev == NULL) {
        vmng_err("vdev is error.\n");
        return -EINVAL;
    }
    if (vdev->dev == NULL) {
        vmng_err("vdev->dev is error.\n");
        return -EINVAL;
    }
    if (vdavinci == NULL) {
        vmng_err("Input parameter is error. (fid=%u)\n", vdev->fid);
        return -EINVAL;
    }
    if (vdev->fid + 1 >= VMNG_VDEV_MAX_PER_PDEV) {
        vmng_err("Input parameter is error. (fid=%u)\n", vdev->fid);
        return -EINVAL;
    }
    return 0;
}

void vmngh_free_vdev_host_stop(u32 dev_id, u32 fid)
{
    struct vmngh_vd_dev *vd_dev = NULL;

    vd_dev = vmngh_get_vdev_from_unit(dev_id, fid);
    if (vd_dev != NULL) {
        vmngh_vdev_free_bottom(vd_dev);
    }
}
#define SHARE_WITH_FOUR_DEVICES(share_num) ((share_num) == 4)
#define SHARE_WITH_TWO_DEVICES(share_num) ((share_num) == 2)
#define VMNG_SHARE_CORE_NUM 0xFFFF  // (u16)(-1) means share a computing core

STATIC int vmngh_adapt_vfresource_for_vm(struct vmng_vf_res_info *vf_resource, struct vdavinci_type *type)
{
    int ret;
    vf_resource->vfg.vfg_id = KA_U32_MAX; // -1
    vf_resource->stars_refresh.device_aicpu = type->aicpu_num;
    if (SHARE_WITH_FOUR_DEVICES(type->share)) {
        vf_resource->stars_refresh.device_aicpu = VMNG_SHARE_CORE_NUM;
    } else if (SHARE_WITH_TWO_DEVICES(type->share)) {
        vf_resource->stars_refresh.device_aicpu = VMNG_SHARE_CORE_NUM;
    }

    vf_resource->memory.size = 0;
    vf_resource->stars_refresh.jpegd = type->jpegd_num;
    vf_resource->stars_refresh.jpege = type->jpege_num;
    vf_resource->stars_refresh.vpc = type->vpc_num;
    vf_resource->stars_refresh.vdec = type->vdec_num;
    vf_resource->stars_refresh.pngd = KA_U32_MAX;
    vf_resource->stars_refresh.venc = type->venc_num;
    ret = strcpy_s(vf_resource->name, VMNG_VF_TEMP_NAME_LEN, type->template_name);
    if (ret != 0) {
        vmng_err("strcpy_s failed. (ret=%d)\n", ret);
        return ret;
    }
    return VMNG_OK;
}

STATIC void vmngh_adapt_vfresource_for_vf_vnic_support(u32 dev_id, struct vmng_vf_res_info *vf_resource)
{
    struct vmngh_pci_dev *vm_pdev = NULL;

    vm_pdev = vmngh_get_pdev_from_unit(dev_id);
    if (vm_pdev->vm_full_spec_enable == 0) {
        return;
    }
    vf_resource->vm_full_spec_enable = vm_pdev->vm_full_spec_enable;
    vf_resource->stars_refresh.device_aicpu = vm_pdev->dev_info.aicpu_num;
    vf_resource->memory.size = 0;
    vf_resource->stars_refresh.jpegd = vm_pdev->dev_info.jpegd_num;
    vf_resource->stars_refresh.jpege = vm_pdev->dev_info.jpege_num;
    vf_resource->stars_refresh.vpc = vm_pdev->dev_info.vpc_num;
    vf_resource->stars_refresh.vdec = vm_pdev->dev_info.vdec_num;
    vf_resource->stars_refresh.pngd = KA_U32_MAX;
    vf_resource->stars_refresh.venc = vm_pdev->dev_info.venc_num;
    vf_resource->stars_static.aic = vm_pdev->dev_info.aicore_num;
    memset_s(vf_resource->name, VMNG_VF_TEMP_NAME_LEN, 0, VMNG_VF_TEMP_NAME_LEN);
    return;
}

STATIC int vmngh_alloc_vf(u32 dev_id, u32 fid, struct vdavinci_dev *vdev, struct vdavinci_type *type)
{
    struct vmng_vf_res_info vf_resource;
    struct vmngh_ctrl_ops *ops = NULL;
    u32 dtype;
    int ret;

    if (fid == 0) { // pf no need to send msg to device to alloc resource
        return 0;
    }

    (void)memset_s(&vf_resource, sizeof(struct vmng_vf_res_info), 0, sizeof(struct vmng_vf_res_info));
    vf_resource.dev_id = dev_id;
    vf_resource.vfid = fid;
    vf_resource.stars_static.aic = type->aicore_num;
    dtype = vmngh_get_dtype_by_aicore(type->aicore_num);
    ret = vmngh_adapt_vfresource_for_vm(&vf_resource, type);
    if (ret != VMNG_OK) {
        return ret;
    }

    vmngh_adapt_vfresource_for_vf_vnic_support(dev_id, &vf_resource);
    ops = vmngh_get_ctrl_ops(dev_id);
    if (ops->alloc_vf != NULL) {
        ret = ops->alloc_vf(dev_id, &fid, dtype, &vf_resource);
        if (ret != VMNG_OK) {
            vmng_err("Send alloc vf msg to device failed, (ret=%d).\n", ret);
            return ret;
        }
    }
    return 0;
}

STATIC void vmngh_free_vf(u32 dev_id, u32 fid)
{
    struct vmngh_ctrl_ops *ops = NULL;

    ops = vmngh_get_ctrl_ops(dev_id);
    if (ops->free_vf != NULL) {
        (void)ops->free_vf(dev_id, fid);
    }
}

STATIC int _vmngh_vdev_create(struct vdavinci_dev *vdev, void *vdavinci,
    struct vdavinci_type *type, ka_uuid_le_t uuid, u32 fid)
{
    struct vmngh_pci_dev *vm_pdev = NULL;
    struct vmngh_vd_dev *vd_dev = NULL;
    ka_pci_dev_t *pdev = NULL;
    u8 bus_num, device_num, func_num;
    u32 dev_id;
    int ret;

    pdev = ka_pci_to_pci_dev(vdev->dev);
    bus_num = ka_pci_get_bus_number(pdev);
    device_num = KA_PCI_SLOT(pdev->devfn);
    func_num = KA_PCI_FUNC(pdev->devfn);
    vmng_info("Create mdev through pf. (bdf=%02x:%02x.%d)\n", bus_num, device_num, func_num);

    dev_id = (u32)devdrv_get_dev_id_by_pdev_with_dev_index(pdev, (int)vdev->dev_index);
    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("device_id is invalid. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    vm_pdev = vmngh_get_pdev_from_unit(dev_id);

    /* alloc vdev and assign its bar, shr */
    vd_dev = vmngh_alloc_vdev(fid, vdavinci, vm_pdev, type, uuid);
    if (vd_dev == NULL) {
        vmng_err("Alloc dev failed. (dev_id=%d; fid=%u; bdf=%02x:%02x.%d)\n",
                 dev_id, fid, bus_num, device_num, func_num);
        return -ENOMEM;
    }
    vd_dev->init_status = VMNG_STARTUP_PROBED;
    vd_dev->resource_dev = vdev->resource_dev;

    ret = vmngh_alloc_vf(dev_id, fid, vdev, type);
    if (ret != 0) {
        vmng_err("Vm alloc vf resource failed. (ret=%d)\n", ret);
        goto FREE_VDEV;
    }

    /* register vdev ctrl */
    ret = vmngh_register_ctrls(vd_dev);
    if (ret != 0) {
        vmng_err("Register ctrls failed. (dev_id=%d; fid=%u; bdf=%02x:%02x.%d, ret=%d)\n",
                 dev_id, fid, bus_num, device_num, func_num, ret);
        goto FREE_VF;
    }

    ret = vmngh_create_base_feature(vd_dev);
    if (ret != 0) {
        vmng_err("Create base feature error. (dev_id=%d; fid=%u; bdf=%02x:%02x.%d; ret=%d)\n",
                 dev_id, fid, bus_num, device_num, func_num, ret);
        goto UNREGISTER_CTRL;
    }

    vmngh_wait_vm_start(vd_dev);

    vmngh_ctrl_set_startup_flag(vd_dev->dev_id, fid, VMNG_STARTUP_TOP_HALF_OK);
    vd_dev->init_status = VMNG_STARTUP_TOP_HALF_OK;
    vmng_info("Create top half finish. (dev_id=%d; fid=%u; bdf=%02x:%02x.%d)\n",
              dev_id, fid, bus_num, device_num, func_num);
    return 0;

UNREGISTER_CTRL:
    vmngh_unregister_ctrls(dev_id, fid);
FREE_VF:
    vmngh_free_vf(dev_id, fid);
FREE_VDEV:
    vmngh_free_vdev(vd_dev);

    return ret;
}

STATIC int vmngh_check_device_is_amp(bool *is_amp)
{
#ifdef CFG_FEATURE_VDEV_NOT_SUPPROT_SMP
    struct vmngh_pci_dev *vm_pdev = NULL;
    u32 master_id;
    u32 dev_id;
    int ret;

    for (dev_id = 0; dev_id < VMNG_PDEV_MAX; ++dev_id) {
        vm_pdev = vmngh_get_pdev_from_unit(dev_id);
        if (vm_pdev->dev_id == (u32)VMNG_CTRL_DEVICE_ID_INIT) { // device not init
            continue;
        }
        ret = devdrv_get_master_devid_in_the_same_os(dev_id, &master_id);
        if (ret != 0) {
            vmng_err("Get master id failed. (dev_id=%u;ret=%d)\n", dev_id, ret);
            return ret;
        }
        if (dev_id != master_id) {
            *is_amp = false;
            return 0;
        }
    }
#endif
    *is_amp = true;
    return 0;
}

STATIC bool vmngh_check_is_support_vdev(void)
{
    bool is_amp = true;
    int ret;

    ret = vmngh_check_device_is_amp(&is_amp);
    if (ret != 0) {
        vmng_err("Get device amp or smp mode failed. (ret=%d)\n", ret);
        return false;
    }

    if (is_amp == false) {  // smp is not support vdevice
        return false;
    }

    return true;
}

STATIC int vmngh_vdev_create(struct vdavinci_dev *vdev, void *vdavinci,
    struct vdavinci_type *type, ka_uuid_le_t uuid)
{
    struct vmngh_pci_dev *vm_pdev = NULL;
    u32 fid;
    u32 dev_id;
    int ret;

    if (vmngh_vdev_create_para_check(vdev, vdavinci) != 0) {
        vmng_err("Parameter check failed.\n");
        return -EINVAL;
    }

    if (!vmngh_check_is_support_vdev()) {
        vmng_err("Not support to create vdevice in smp mode.\n");
        return -EOPNOTSUPP;
    }

    dev_id = (u32)devdrv_get_dev_id_by_pdev_with_dev_index(ka_pci_to_pci_dev(vdev->dev), (int)vdev->dev_index);
    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("dev_id is invalid. (dev_id=%d)\n", dev_id);
        return -EINVAL;
    }

    fid = vmngh_get_fid_for_host(vdev);
    vm_pdev = vmngh_get_pdev_from_unit(dev_id);
    ka_task_mutex_lock(&vm_pdev->vpdev_mutex);
    if (vm_pdev->vdev_ref == 0) {
        ret = uda_dev_ctrl(dev_id, UDA_CTRL_TO_MIA);
        if (ret != 0) {
            ka_task_mutex_unlock(&vm_pdev->vpdev_mutex);
            vmng_err("To mia mode failed. (dev_id=%u)\n", dev_id);
            return ret;
        }

        vmng_set_device_split_mode(dev_id, VMNG_VIRTUAL_SPLIT_MODE);
    }

    vm_pdev->vdev_ref++;

    ret = _vmngh_vdev_create(vdev, vdavinci, type, uuid, fid);
    if (ret != 0) {
        vm_pdev->vdev_ref--;
        if (vm_pdev->vdev_ref == 0) {
            vmng_set_device_split_mode(dev_id, VMNG_NORMAL_NONE_SPLIT_MODE);
            (void)uda_dev_ctrl(dev_id, UDA_CTRL_TO_SIA);
        }
    }

    ka_task_mutex_unlock(&vm_pdev->vpdev_mutex);
    return ret;
}

STATIC int vmngh_vdev_check_vdev(const struct vdavinci_dev *vdev)
{
    if (vdev == NULL || vdev->dev == NULL) {
        vmng_err("dev is invalid.\n");
        return -EINVAL;
    }

    if (vdev->fid + 1 >= VMNG_VDEV_MAX_PER_PDEV) {
        vmng_err("fid is invalid. (fid=%u)\n", vdev->fid);
        return -EINVAL;
    }

    return 0;
}

STATIC void vmngh_vdev_destroy(struct vdavinci_dev *vdev)
{
    u32 dev_id;
    struct vmngh_vd_dev *vd_dev = NULL;
    ka_pci_dev_t *pdev = NULL;
    u32 fid;
    u8 bus_num, device_num, func_num;
    int ret;

    ret = vmngh_vdev_check_vdev(vdev);
    if (ret != 0) {
        vmng_err("vdev is invalid.\n");
        return;
    }

    fid = vmngh_get_fid_for_host(vdev);
    pdev = ka_pci_to_pci_dev(vdev->dev);
    dev_id = (u32)devdrv_get_dev_id_by_pdev_with_dev_index(pdev, (int)vdev->dev_index);
    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("device_id is invalid. (dev_id=%u)\n", dev_id);
        return;
    }
    bus_num = ka_pci_get_bus_number(pdev);
    device_num = KA_PCI_SLOT(pdev->devfn);
    func_num = KA_PCI_FUNC(pdev->devfn);
    vmng_debug("Destroy begin. (dev_id=%d; fid=%u; bdf=%02x:%02x.%d)\n",
               dev_id, fid, bus_num, device_num, func_num);

    // Prevent vd_dev from being used concurrently
    ka_task_mutex_lock(vmngh_get_vdev_lock_from_unit(dev_id, fid));
    vd_dev = vmngh_get_vdev_from_unit(dev_id, fid);
    if (vd_dev != NULL) {
        struct vmngh_pci_dev *vm_pdev = vmngh_get_pdev_from_unit(dev_id);
        if (vd_dev->init_status == VMNG_STARTUP_BOTTOM_HALF_OK) {
            vmngh_suspend_instance_remove_vdev(dev_id, fid);
            vmngh_vdev_free_bottom(vd_dev);
        }
        vmngh_vdev_free_top(vd_dev);
        ka_task_mutex_lock(&vm_pdev->vpdev_mutex);
        vm_pdev->vdev_ref--;
        if (vm_pdev->vdev_ref == 0) {
            vmng_set_device_split_mode(dev_id, VMNG_NORMAL_NONE_SPLIT_MODE);
            (void)uda_dev_ctrl(dev_id, UDA_CTRL_TO_SIA);
        }
        ka_task_mutex_unlock(&vm_pdev->vpdev_mutex);
    }
    ka_task_mutex_unlock(vmngh_get_vdev_lock_from_unit(dev_id, fid));
    vmng_info("Destroy finish. (dev_id=%d; fid=%u; bdf=%02x:%02x.%d)\n",
              dev_id, fid, bus_num, device_num, func_num);
}

STATIC void vmngh_vdev_release(struct vdavinci_dev *vdev)
{
    u32 dev_id, vdev_id;
    struct vmngh_vd_dev *vd_dev = NULL;
    ka_pci_dev_t *pdev = NULL;
    u32 fid;
    u8 bus_num, device_num, func_num;
    int ret;

    ret = vmngh_vdev_check_vdev(vdev);
    if (ret != 0) {
        vmng_err("vdev is invalid.\n");
        return;
    }

    fid = vmngh_get_fid_for_host(vdev);
    pdev = ka_pci_to_pci_dev(vdev->dev);
    dev_id = (u32)devdrv_get_dev_id_by_pdev_with_dev_index(pdev, (int)vdev->dev_index);
    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("dev_id is invalid. (dev_id=%u)\n", dev_id);
        return;
    }

    bus_num = ka_pci_get_bus_number(pdev);
    device_num = KA_PCI_SLOT(pdev->devfn);
    func_num = KA_PCI_FUNC(pdev->devfn);
    // Prevent vd_dev from being used concurrently
    ka_task_mutex_lock(vmngh_get_vdev_lock_from_unit(dev_id, fid));
    vd_dev = vmngh_get_vdev_from_unit(dev_id, fid);
    if (vd_dev != NULL) {
        if (vd_dev->init_status == VMNG_STARTUP_BOTTOM_HALF_OK) {
            vmngh_suspend_instance_remove_vdev(dev_id, fid);
            vmngh_vdev_free_bottom(vd_dev);
        } else {
            vmng_info("init_status is not half ok. (init_status=%u)\n", vd_dev->init_status);
        }
        if (devdrv_is_sriov_support(dev_id)) {
            devdrv_get_devid_by_pfvf_id(dev_id, fid, &vdev_id);
            (void)devdrv_hot_reset_device(vdev_id);
        }
    }
    ka_task_mutex_unlock(vmngh_get_vdev_lock_from_unit(dev_id, fid));
    vmng_info("Release finish. (dev_id=%d; fid=%u; bdf=%02x:%02x.%d)\n",
              dev_id, fid, bus_num, device_num, func_num);
}

STATIC int vmngh_vdev_reset(struct vdavinci_dev *vdev)
{
    u32 dev_id, vdev_id;
    struct vmngh_vd_dev *vd_dev = NULL;
    ka_pci_dev_t *pdev = NULL;
    u32 fid;
    u8 bus_num, device_num, func_num;
    int ret;

    ret = vmngh_vdev_check_vdev(vdev);
    if (ret != 0) {
        vmng_err("vdev is invalid.\n");
        return -EINVAL;
    }

    fid = vmngh_get_fid_for_host(vdev);
    pdev = ka_pci_to_pci_dev(vdev->dev);
    dev_id = (u32)devdrv_get_dev_id_by_pdev_with_dev_index(pdev, (int)vdev->dev_index);
    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("device_id is invalid. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    bus_num = ka_pci_get_bus_number(pdev);
    device_num = KA_PCI_SLOT(pdev->devfn);
    func_num = KA_PCI_FUNC(pdev->devfn);
    vmng_debug("Reset begin. (dev_id=%d; fid=%u; bdf=%02x:%02x.%d)\n",
               dev_id, fid, bus_num, device_num, func_num);
    // Prevent vd_dev from being used concurrently
    ka_task_mutex_lock(vmngh_get_vdev_lock_from_unit(dev_id, fid));
    vd_dev = vmngh_get_vdev_from_unit(dev_id, fid);
    if (vd_dev != NULL) {
        ka_base_atomic_set(&vd_dev->reset_ref_flag, VMNGH_MDEV_VM_RESET_STATE);
        vd_dev->vm_pid = (u32)ka_task_get_current_tgid();
        if (vd_dev->init_status == VMNG_STARTUP_BOTTOM_HALF_OK) {
            vmngh_suspend_instance_remove_vdev(dev_id, fid);
            vmngh_vdev_free_bottom(vd_dev);
        }
        if (devdrv_is_sriov_support(dev_id)) {
            devdrv_get_devid_by_pfvf_id(dev_id, fid, &vdev_id);
            (void)devdrv_hot_reset_device(vdev_id);
        }
        vmngh_del_start_timer(vd_dev);
        vmngh_set_start_timer(vd_dev);
        vd_dev->dfx.reset_cnt++;
        vmng_info("Reset finish. (dev_id=%d; fid=%u; bdf=%02x:%02x.%d; reset_cnt=%u; vm_pid=%u)\n",
                  dev_id, fid, bus_num, device_num, func_num, vd_dev->dfx.reset_cnt, vd_dev->vm_pid);
    } else {
        vmng_err("vd_dev is NULL. (dev_id=%d; fid=%u; bdf=%02x:%02x.%d)\n",
                 dev_id, fid, bus_num, device_num, func_num);
    }
    ka_task_mutex_unlock(vmngh_get_vdev_lock_from_unit(dev_id, fid));
    return 0;
}

STATIC int vmngh_vdev_flr(struct vdavinci_dev *vdev)
{
    u32 dev_id;
    struct vmngh_vd_dev *vd_dev = NULL;
    ka_pci_dev_t *pdev = NULL;
    u32 fid;
    u8 bus_num, device_num, func_num;
    int ret;

    ret = vmngh_vdev_check_vdev(vdev);
    if (ret != 0) {
        vmng_err("vdev is invalid.\n");
        return -EINVAL;
    }

    fid = vmngh_get_fid_for_host(vdev);
    pdev = ka_pci_to_pci_dev(vdev->dev);
    dev_id = (u32)devdrv_get_dev_id_by_pdev_with_dev_index(pdev, (int)vdev->dev_index);
    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("device_id is invalid. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    bus_num = ka_pci_get_bus_number(pdev);
    device_num = KA_PCI_SLOT(pdev->devfn);
    func_num = KA_PCI_FUNC(pdev->devfn);
    vmng_debug("Reset begin. (dev_id=%d; fid=%u; bdf=%02x:%02x.%d)\n",
               dev_id, fid, bus_num, device_num, func_num);
    // Prevent vd_dev from being used concurrently
    ka_task_mutex_lock(vmngh_get_vdev_lock_from_unit(dev_id, fid));
    vd_dev = vmngh_get_vdev_from_unit(dev_id, fid);
    if (vd_dev != NULL) {
        vd_dev->vm_pid = (u32)ka_task_get_current_tgid();
        if (vd_dev->init_status == VMNG_STARTUP_BOTTOM_HALF_OK) {
            ka_base_atomic_set(&vd_dev->reset_ref_flag, VMNGH_MDEV_FLR_STATE);
            vmngh_suspend_instance_remove_vdev(dev_id, fid);
            vmngh_vdev_free_bottom(vd_dev);
        }
        vmngh_del_start_timer(vd_dev);
        vmngh_set_start_timer(vd_dev);
        vd_dev->dfx.reset_cnt++;
        vmng_info("Reset finish. (dev_id=%d; fid=%u; bdf=%02x:%02x.%d; reset_cnt=%u; vm_pid=%u)\n",
                  dev_id, fid, bus_num, device_num, func_num, vd_dev->dfx.reset_cnt, vd_dev->vm_pid);
    } else {
        vmng_err("vd_dev is NULL. (dev_id=%d; fid=%u; bdf=%02x:%02x.%d)\n",
                 dev_id, fid, bus_num, device_num, func_num);
    }
    ka_task_mutex_unlock(vmngh_get_vdev_lock_from_unit(dev_id, fid));
    return 0;
}

static int vmngh_vm_startup_irq(void *data)
{
    struct vmngh_vd_dev *vd_dev = NULL;

    vd_dev = (struct vmngh_vd_dev *)data;
    if (vd_dev->shr_para->start_flag == VMNG_VM_START_WAIT) {
        vmng_info("Start wait. (dev_id=%u; fid=%u)\n", vd_dev->dev_id, vd_dev->fid);
        ka_base_atomic_set(&(vd_dev->start_dev.start_flag), VMNG_TASK_SUCCESS);
        ka_task_schedule_work_on(ka_system_raw_smp_processor_id(), &vd_dev->start_work);
    }
    return 0;
}

STATIC void vmngh_vdev_notify(struct vdavinci_dev *vdev, int db_index)
{
    struct vmngh_vd_dev *vd_dev = NULL;
    ka_pci_dev_t *pdev = NULL;
    u32 dev_id;
    int ret;
    u32 fid;
    u8 bus_num, device_num, func_num;

    ret = vmngh_vdev_check_vdev(vdev);
    if (ret != 0) {
        vmng_err("vdev is invalid.\n");
        return;
    }

    fid = vmngh_get_fid_for_host(vdev);
    pdev = ka_pci_to_pci_dev(vdev->dev);
    dev_id = (u32)devdrv_get_dev_id_by_pdev_with_dev_index(pdev, (int)vdev->dev_index);
    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("dev_id is invalid. (dev_id=%d)\n", dev_id);
        return;
    }

    bus_num = ka_pci_get_bus_number(pdev);
    device_num = KA_PCI_SLOT(pdev->devfn);
    func_num = KA_PCI_FUNC(pdev->devfn);
    vd_dev = vmngh_get_vdev_from_unit(dev_id, fid);
    if (vd_dev == NULL) {
        vmng_err("vd_dev is NULL. (dev_id=%d; fid=%u; bdf=%02x:%02x.%d; db_index=%u)\n",
            dev_id, fid, bus_num, device_num, func_num, db_index);
        return;
    }

    if (db_index == VMNG_DB_BASE_LOAD) {
        ret = vmngh_vm_startup_irq(vd_dev);
    } else if ((db_index >= VMNG_DB_BASE_MSG) && (db_index < VMNG_DB_BASE_MSG_BLOCK_TX_FINISH)) {
        ret = vmngh_msg_db_hanlder(vd_dev->msg_dev, (u32)db_index);
    } else if ((db_index >= VMNG_DB_BASE_MSG_BLOCK_TX_FINISH) && (db_index < VMNG_DB_BASE_EXTERNAL)) {
        ret = vmngh_tx_finish_db_hander(vd_dev->msg_dev, (u32)db_index);
    } else if ((db_index >= VMNG_DB_BASE_EXTERNAL) && (db_index < VMNG_DB_BASE_MAX)) {
        ret = vmngh_external_db_handler(vd_dev, db_index);
    } else {
        vmng_err("Doorbell index is out of range. (dev_id=%d; fid=%u; bdf=%02x:%02x.%d; db_index=%u)\n",
                 dev_id, fid, bus_num, device_num, func_num, db_index);
        return;
    }
    if (ret != 0) {
        vmng_err("Hanlder is error. (dev_id=%d; fid=%u; bdf=%02x:%02x.%d; db_index=%u; ret=%d)\n",
                 dev_id, fid, bus_num, device_num, func_num, db_index, ret);
    } else {
        vmng_debug("Doorbell finish. (dev_id=%d; fid=%u; bdf=%02x:%02x.%d; db_index=%u)\n",
                   dev_id, fid, bus_num, device_num, func_num, db_index);
    }
}

STATIC int vmngh_vdev_get_map_info(struct vdavinci_dev *vdev, struct vdavinci_type *type,
    u32 bar_id, struct vdavinci_mapinfo *map_info)
{
    struct vmngh_map_info client_map_info = {0};
    struct vmngh_client_instance instance = {0};
    struct vmng_vdev_ctrl dev_ctrl = {0};
    ka_pci_dev_t *pdev = NULL;
    u32 vfid;
    u32 dev_id;
    u64 i;

    if ((vdev == NULL) || (vdev->dev == NULL) || (type == NULL) || (map_info == NULL)) {
        vmng_err("Input parameter is error. (vdev=%s; type=%s; map_info=%s)\n",
                 vdev == NULL ? "NULL" : "OK", type == NULL ? "NULL" : "OK",
                 map_info == NULL ? "NULL" : "OK");
        return -EINVAL;
    }

    vfid = vmngh_get_fid_for_host(vdev);
    pdev = ka_pci_to_pci_dev(vdev->dev);
    dev_id = (u32)devdrv_get_dev_id_by_pdev_with_dev_index(pdev, (int)vdev->dev_index);
    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("dev_id is invalid. (dev_id=%d)\n", dev_id);
        return -EINVAL;
    }

    dev_ctrl.dev_id = dev_id;
    dev_ctrl.vfid = vfid;
    dev_ctrl.dtype = vmngh_get_dtype_by_aicore(type->aicore_num);
    dev_ctrl.bar0_size = type->bar0_size;
    dev_ctrl.bar2_size = type->bar2_size;
    dev_ctrl.bar4_size = type->bar4_size;
    instance.dev_ctrl = &dev_ctrl;
    instance.type = VMNG_CLIENT_TYPE_TSDRV;

    if (vmngh_get_map_info_client(&instance, VMNG_CLIENT_TYPE_TSDRV, &client_map_info) != 0) {
        vmng_err("Get map info client failed. (dev_id=%d; fid=%u)\n", dev_id, vfid);
        return -EINVAL;
    }

    if (client_map_info.num > HW_BAR_SPARSE_MAP_MAX) {
        vmng_err("Too many map info. (dev_id=%d; fid=%u; num=%llu)\n", dev_id, vfid, client_map_info.num);
        return -EINVAL;
    }

    for (i = 0; i < client_map_info.num; i++) {
        map_info->map_info[i].offset = client_map_info.map_info[i].offset;
        map_info->map_info[i].paddr = client_map_info.map_info[i].paddr;
        map_info->map_info[i].size = client_map_info.map_info[i].size;
        vmng_info("Get map info (dev_id=%d; fid=%u; i=%llu; offset=0x%lx; size=0x%lx)\n",
            dev_id, vfid, i, map_info->map_info[i].offset, map_info->map_info[i].size);
    }
    map_info->num = client_map_info.num;

    vmng_info("Get map info success. (dev_id=%d; fid=%u; num=%llu)\n", dev_id, vfid, map_info->num);
    return 0;
}

STATIC int vmngh_vdev_put_map_info(struct vdavinci_dev *vdev)
{
    struct vmngh_client_instance instance = {0};
    struct vmng_vdev_ctrl dev_ctrl = {0};
    ka_pci_dev_t *pdev = NULL;
    u32 vfid;
    u32 dev_id;
    int ret;

    ret = vmngh_vdev_check_vdev(vdev);
    if (ret != 0) {
        vmng_err("vdev is invalid.\n");
        return -EINVAL;
    }

    vfid = vmngh_get_fid_for_host(vdev);
    pdev = ka_pci_to_pci_dev(vdev->dev);
    dev_id = (u32)devdrv_get_dev_id_by_pdev_with_dev_index(pdev, (int)vdev->dev_index);
    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("dev_id is invalid. (dev_id=%d)\n", dev_id);
        return -EINVAL;
    }

    dev_ctrl.dev_id = dev_id;
    dev_ctrl.vfid = vfid;
    instance.dev_ctrl = &dev_ctrl;
    instance.type = VMNG_CLIENT_TYPE_TSDRV;

    if (vmngh_put_map_info_client(&instance, VMNG_CLIENT_TYPE_TSDRV) != 0) {
        vmng_err("Put map info client. (dev_id=%d; fid=%u)\n", dev_id, vfid);
        return -EINVAL;
    }

    vmng_info("Put map info success. (dev_id=%d; fid=%u)\n", dev_id, vfid);
    return 0;
}

STATIC int vmngh_vdev_get_devnum_by_pdev(ka_device_t *dev)
{
    ka_pci_dev_t *pdev = NULL;
    int dev_num;

    if (dev == NULL) {
        vmng_err("Input parameter is error.\n");
        return -EINVAL;
    }
    pdev = ka_pci_to_pci_dev(dev);
    dev_num = devdrv_get_davinci_dev_num_by_pdev(pdev);

    return dev_num;
}
STATIC int vmngh_vdev_wait_aicore_ready(u32 dev_id)
{
    int timeout = VMNG_WAIT_INIT_TIME;
    u32 aicore_num;

    while (timeout > 0) {
        aicore_num = vmngh_get_total_core_num(dev_id);
        if (aicore_num != 0) {
            break;
        }
        ka_rmb();
        ka_system_usleep_range(100000, 200000);  // 100000us ~ 200000us
        timeout--;
    }

    if (timeout <= 0) {
        vmng_err("Wait for set total aicore num timeout. (dev_id=%d)\n", dev_id);
        return VMNG_ERR;
    }

    return 0;
}

#define MAX_CPU_NUM 32
#define GET_MEMORY_MODULE_SIZE(mem) KA_BASE_DIV_ROUND_UP((mem) / 1024, 8) * 8
STATIC int vmngh_vdev_wait_dev_resource_ready(u32 dev_id, struct dvt_devinfo *dev_info)
{
    struct vmng_soc_resource_enquire info;
    unsigned long aicpu_bitmap;
    int ret;

#define MILAN_BIN3_AIC_NUM 25
#define MILAN_BIN0_AIC_NUM 24
    if (dev_info->aicore_num == MILAN_BIN3_AIC_NUM) {
        dev_info->aicore_num = MILAN_BIN0_AIC_NUM;
    }

    (void)memset_s(&info, sizeof(struct vmng_soc_resource_enquire), 0, sizeof(struct vmng_soc_resource_enquire));
    ret = vmngh_enquire_soc_resource(dev_id, 0, &info);
    if (ret != 0) {
        vmng_err("Get device resource failed. (ret=%d;dev_id=%u)\n", ret, dev_id);
        return ret;
    }
    aicpu_bitmap = (unsigned long)info.total.stars_refresh.device_aicpu;
    dev_info->aicpu_num = (u32)ka_base_bitmap_weight(&aicpu_bitmap, MAX_CPU_NUM);
    dev_info->mem_size = GET_MEMORY_MODULE_SIZE(info.total.base.memory_spec);
    dev_info->jpegd_num = info.total.stars_refresh.jpegd;
    dev_info->jpege_num = info.total.stars_refresh.jpege;
    dev_info->vpc_num = info.total.stars_refresh.vpc;
    dev_info->vdec_num = info.total.stars_refresh.vdec;
    dev_info->venc_num = info.total.stars_refresh.venc;
    return 0;
}

STATIC int vmngh_vdev_get_devinfo(ka_device_t *dev, u32 dev_index, struct dvt_devinfo *dev_info)
{
    struct vmngh_pci_dev *vm_pdev = NULL;
    ka_pci_dev_t *pdev = NULL;
    u8 bus_num, device_num, func_num;
    u32 dev_id;
    int ret;

    if ((dev == NULL) || (dev_info == NULL)) {
        vmng_err("Input parameter is error.\n");
        return -EINVAL;
    }
    pdev = ka_pci_to_pci_dev(dev);
    dev_id = (u32)devdrv_get_dev_id_by_pdev_with_dev_index(pdev, (int)dev_index);
    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("device_id is invalid. (dev_id=%d)\n", dev_id);
        return -EINVAL;
    }
    bus_num = ka_pci_get_bus_number(pdev);
    device_num = KA_PCI_SLOT(pdev->devfn);
    func_num = KA_PCI_FUNC(pdev->devfn);
    vm_pdev = vmngh_get_pdev_from_unit(dev_id);
    if (vm_pdev == NULL) {
        vmng_err("vm_pdev is NULL. (dev_id=%d; bdf=%02x:%02x.%d)\n", dev_id, bus_num, device_num, func_num);
        return -EINVAL;
    }

    ret = vmngh_vdev_wait_aicore_ready(dev_id);
    if (ret != 0) {
        return -EINVAL;
    }
    ret = vmngh_vdev_wait_dev_resource_ready(dev_id, &vm_pdev->dev_info);
    if (ret != 0) {
        vmng_err("Get dvt info failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = memcpy_s(dev_info, sizeof(struct dvt_devinfo), &vm_pdev->dev_info, sizeof(struct dvt_devinfo));
    if (ret != 0) {
        vmng_err("Memcpy dev_info failed. (ret=%d)\n", ret);
        return ret;
    }

    return 0;
}

void *vmngh_dma_alloc_coherent(ka_device_t *dev, size_t size, ka_dma_addr_t *dma_handle, ka_gfp_t gfp)
{
    return hw_dvt_hypervisor_dma_alloc_coherent(dev, size, dma_handle, gfp);
}
KA_EXPORT_SYMBOL(vmngh_dma_alloc_coherent);

void vmngh_dma_free_coherent(ka_device_t *dev, size_t size, void *cpu_addr, ka_dma_addr_t dma_handle)
{
    hw_dvt_hypervisor_dma_free_coherent(dev, size, cpu_addr, dma_handle);
}
KA_EXPORT_SYMBOL(vmngh_dma_free_coherent);

static void set_sriov_mode(u32 dev_id, int sriov_mode)
{
    struct vmngh_vdev_ctrl *ctrl = vmngh_get_ctrl(dev_id, 0);
    if (ctrl == NULL) {
        vmng_err("Set sriov mode failed. (dev_id=%u)\n", dev_id);
        return;
    }
    ctrl->sriov_mode = sriov_mode;
}

#define VMNG_SUPPORT_SRIOV_VF_NUM 8
STATIC int vmngh_vm_enable_sriov(ka_pci_dev_t *pdev, int numvfs)
{
    unsigned int dev_id;
    int ret;

    if (pdev == NULL) {
        vmng_err("Input pdev is NULL.\n");
        return -EINVAL;
    }

    dev_id = (u32)devdrv_get_dev_id_by_pdev(pdev);
    if (numvfs == VMNG_SUPPORT_SRIOV_VF_NUM) {
        ret = devdrv_set_sriov_and_mdev_mode(dev_id, DEVDRV_BOOT_MDEV_AND_SRIOV);
        if (ret != 0) {
            vmng_err("Set pcie mdev sriov mode failed. (ret=%d;dev_id=%u)\n", ret, dev_id);
            return ret;
        }
        set_sriov_mode(dev_id, DEVDRV_BOOT_MDEV_AND_SRIOV);
        vmng_info("vmng enable sriov. (dev_id=%u)\n", dev_id);
        return vmngh_common_enable_sriov(dev_id);
    } else if (numvfs == 0) {
        vmng_info("vmng disable sriov. (dev_id=%u)\n", dev_id);
        return vmngh_disable_sriov(dev_id);
    } else {
        vmng_err("Invalid vfs nums. (numvfs=%d)\n", numvfs);
        return -EINVAL;
    }
    return 0;
}

struct vdavinci_priv_ops g_vmngh_ops = {
    .vdavinci_create = vmngh_vdev_create,
    .vdavinci_destroy = vmngh_vdev_destroy,
    .vdavinci_release = vmngh_vdev_release,
    .vdavinci_reset = vmngh_vdev_reset,
    .vdavinci_flr = vmngh_vdev_flr,
    .vdavinci_notify = vmngh_vdev_notify,
    .vdavinci_getmapinfo = vmngh_vdev_get_map_info,
    .vdavinci_putmapinfo = vmngh_vdev_put_map_info,
    .davinci_getdevnum = vmngh_vdev_get_devnum_by_pdev,
    .davinci_getdevinfo = vmngh_vdev_get_devinfo,
    .vascend_enable_sriov = vmngh_vm_enable_sriov,
};

static void vmngh_init_pci_pdev(struct vmngh_pci_dev *vmngh_pcidev, u32 dev_id, ka_device_t *dev)
{
    int i;
    /* add info to unit */
    vmngh_pcidev->dev_id = dev_id;
    vmngh_pcidev->pdev = ka_pci_to_pci_dev(dev);
    vmngh_pcidev->dev = dev;
    vmngh_pcidev->vdev_num = VMNG_FID_BEGIN; /* vdev_num begin as 1. */
    vmngh_pcidev->ep_devic_id = ka_pci_get_device_id(vmngh_pcidev->pdev);
    ka_task_mutex_init(&vmngh_pcidev->vpdev_mutex);
    for (i = 0; i < VMNG_VDEV_MAX_PER_PDEV; i++) {
        ka_task_mutex_init(&vmngh_pcidev->vddev_mutex[i]);
    }
    vmngh_pcidev->vdev_ref = 0;
}

static void vmngh_uninit_pci_pdev(struct vmngh_pci_dev *vmngh_pcidev)
{
    /* add info to unit */
    vmngh_pcidev->dev_id = (u32)VMNG_CTRL_DEVICE_ID_INIT;
    vmngh_pcidev->pdev = NULL;
    vmngh_pcidev->dev = NULL;
    vmngh_pcidev->vdev_num = 0; /* vdev_num begin as 1. */
    vmngh_pcidev->ep_devic_id = 0;
    vmngh_pcidev->vdev_ref = 0;
}

static bool vmngh_all_devices_are_valid(struct vmngh_pci_dev *vmngh_dev)
{
    struct vmngh_ctrl_ops *ctrl_ops;
    int dev_num;
    int start_dev_id;
    int i;

    dev_num = devdrv_get_davinci_dev_num_by_pdev(vmngh_dev->pdev);
    start_dev_id = ka_base_rounddown((int)vmngh_dev->dev_id, dev_num);
    vmng_info("Get ka_base_rounddown dev_id = %u;dev_num=%d\n", start_dev_id, dev_num);
    for (i = 0; i < dev_num; ++i) {
        ctrl_ops = vmngh_get_ctrl_ops((u32)(start_dev_id + i));
        if (ctrl_ops->valid == VMNG_INVALID) {
            return false;
        }
    }
    return true;
}

STATIC int vmngh_dvt_init(struct vmngh_pci_dev *vmngh_dev)
{
    struct vdavinci_priv *vdavinci_set = NULL;
    int ret;

    vdavinci_set = (struct vdavinci_priv *)devdrv_get_devdrv_priv(vmngh_dev->pdev);
    if (vdavinci_set == NULL) {
        vmng_err("vdavinci_set is null. (dev_id=%u)", vmngh_dev->dev_id);
        return VMNG_ERR;
    }

    ka_task_mutex_lock(&g_vmngh_pci_mutex);
    if (!vmngh_all_devices_are_valid(vmngh_dev)) {
        ka_task_mutex_unlock(&g_vmngh_pci_mutex);
        return 0;
    }

    if (vdavinci_set->ops != NULL) {
        ka_task_mutex_unlock(&g_vmngh_pci_mutex);
        return 0;
    }

    vdavinci_set->dev = vmngh_dev->dev;
    vdavinci_set->ops = &g_vmngh_ops;

    ret = hw_dvt_init((void *)vdavinci_set);
    if (ret != 0) {
        vmng_err("Register vdavinci failed. (dev_id=%u; ret=%d)\n", vmngh_dev->dev_id, ret);
        vdavinci_set->ops = NULL;
        ka_task_mutex_unlock(&g_vmngh_pci_mutex);
        return VMNG_ERR;
    }

    ka_task_mutex_unlock(&g_vmngh_pci_mutex);
    return VMNG_OK;
}

STATIC void vmngh_sync_udevid_to_remote(u32 udevid)
{
    struct vmng_ctrl_msg msg = {0};
    u32 phyid = 0;
    u32 vfid = 0;
    u32 len = 0;
    int ret;

    ret = devdrv_get_pfvf_id_by_devid(udevid, &phyid, &vfid);
    if (ret != 0) {
        vmng_err("Trans udevid to phy devid failed. (udevid=%u;ret=%d)\n", udevid, ret);
        return;
    }
    /* sync udevid to remote */
    msg.type = VMNG_CTRL_MSG_TYPE_SYNC_ID;
    msg.info_msg.id_info.udevid = udevid;
    msg.info_msg.vfid = vfid;
    msg.error_code = VMNG_OK;
    ret = vmngh_ctrl_msg_send(phyid, (void *)&msg, sizeof(msg), sizeof(msg), &len);
    if ((ret != VMNG_OK) || (msg.error_code != VMNG_OK)) {
        vmng_err("Sync udevid to remote failed. (dev_id=%u;ret=%d;error_code=%d)\n", udevid, ret, msg.error_code);
        return;
    }
}

STATIC int vmngh_init_vpc_msg_chan(struct vmngh_pci_dev *vmngh_dev)
{
    struct vmng_ctrl_msg msg;
    u32 len = 0;
    int ret;

    /* wait for device init */
    msg.type = VMNG_CTRL_MSG_TYPE_SYNC;
    msg.sync_msg.dev_id = (int)vmngh_dev->dev_id;
    msg.error_code = VMNG_OK;
    vmng_info("Device work begin. (dev_id=%d)\n", vmngh_dev->dev_id);
    ret = vmngh_ctrl_msg_send(vmngh_dev->dev_id, (void *)&msg, sizeof(msg), sizeof(msg), &len);
    if ((ret != VMNG_OK) || (len != sizeof(msg)) || (msg.error_code != VMNG_OK)) {
        vmng_err("Sync failed. (dev_id=%d)\n", vmngh_dev->dev_id);
        return -EFAULT;
    }

    vmng_info("Device work begin. (dev_id=%u)\n", vmngh_dev->dev_id);

    /* Adapt cloudv2 get pf aic problem sequence issues.
    Send msg to device to get aic num instead of waiting for devmng to set.*/
    vmngh_set_total_core_num(vmngh_dev->dev_id, msg.sync_msg.aicore_num);

    ret = vmngh_dvt_init(vmngh_dev);
    if (ret != VMNG_OK) {
        return ret;
    }
    vmngh_set_peer_dev_id(vmngh_dev->dev_id, msg.sync_msg.dev_id);

    vmngh_set_device_status(vmngh_dev->dev_id, VMNG_VALID);

    ka_task_mutex_lock(&g_vmngh_pci_mutex);
    vmngh_dev->status = VMNG_STARTUP_PROBED;
    ka_task_mutex_unlock(&g_vmngh_pci_mutex);
    vmngh_sync_udevid_to_remote(vmngh_dev->dev_id);
    vmng_info("Device work end. (dev_id=%d; peer_dev_id=%d)\n", vmngh_dev->dev_id, vmngh_dev->peer_dev_id);

    return 0;
}

// Chip hardware limiations
#define MAX_HCCS_MSIX_NUM 33
#define MAX_HCCS_VM_MSIX_NUM 29
STATIC void vmngh_init_msix_num(u32 dev_id)
{
    if (devdrv_get_connect_protocol(dev_id) == CONNECT_PROTOCOL_HCCS) {
        g_max_msix_num = MAX_HCCS_MSIX_NUM;
        g_max_pass_to_vm_msix_num = MAX_HCCS_VM_MSIX_NUM;
    } else {
        g_max_msix_num = MAX_PCIE_MSIX_NUM;
        g_max_pass_to_vm_msix_num = MAX_PASS_TO_VM_MSIX_NUM;
    }
    vmng_info("Get connect protocol. (dev_id=%u; max_msix_num=%u; max_vm_msix_num=%u)\n",
        dev_id, g_max_msix_num, g_max_pass_to_vm_msix_num);
}

/* pcie_host => vmngh_pdev => vdavinci */
STATIC int vmngh_pci_init_instance(u32 dev_id, ka_device_t *dev)
{
    struct vmngh_pci_dev *vmngh_pdev = NULL;
    ka_pci_dev_t *pdev = ka_pci_to_pci_dev(dev);
    int ret, sysfs_ret;

    if (devdrv_get_pfvf_type_by_devid(dev_id) != DEVDRV_SRIOV_TYPE_PF) {
        vmng_debug("Skip vf pci init instance. (dev_id=%u)\n", dev_id);
        return 0;
    }

    vmng_info("Call vmngh_pci_init_instance start. (bdf=%02x:%02x.%d; vendor=0x%x%x; dev_id=%u)\n",
        ka_pci_get_bus_number(pdev), KA_PCI_SLOT(pdev->devfn), KA_PCI_FUNC(pdev->devfn),
        ka_pci_get_vendor_id(pdev), ka_pci_get_device_id(pdev), dev_id);

    if (vmngh_init_ctrl_ops(dev_id) != VMNG_OK) {
        vmng_err("Init ctrl ops error. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    // 910A3 use HCCS protocol, msix_num less than PCIe
    vmngh_init_msix_num(dev_id);

    /* get vmngh from unit, then initialize it by instance */
    vmngh_pdev = vmngh_get_pdev_from_unit(dev_id);
    if (vmngh_pdev == NULL) {
        vmng_err("vmngh_pdev is NULL. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    vmngh_init_pci_pdev(vmngh_pdev, dev_id, dev);

    sysfs_ret = vmngh_sysfs_init(dev_id, vmngh_pdev->pdev);
    if (sysfs_ret != 0) {
        vmng_warn("Init sysfs incomplete. (dev_id=%u;ret=%d)\n", dev_id, sysfs_ret);
    }

    ret = vmngh_init_vpc_msg_chan(vmngh_pdev);
    if (ret != 0) {
        if (sysfs_ret == 0) {
            vmngh_sysfs_exit(dev_id, vmngh_pdev->pdev);
        }
        return ret;
    }

    if (devdrv_is_sriov_support(dev_id) == false) {
        vmngh_bw_ctrl_info_init(dev_id);
    }

    vmng_info("Instance init finish. (dev_id=%u)\n", dev_id);
    return 0;
}

static int vmngh_free_all_vdev(u32 dev_id)
{
    u32 fid;
    struct vmngh_vd_dev *vd_dev = NULL;
    int vd_dev_alive_cnt = 0;

    vmng_debug("Free all vdev begin. (dev_id=%u)\n", dev_id);
    for (fid = 0; fid < VMNG_VDEV_MAX_PER_PDEV; fid++) {
        vd_dev = vmngh_get_vdev_from_unit(dev_id, fid);
        if (vd_dev != NULL) {
            /* find the number of vd_dev alive in this pdev. */
            vd_dev_alive_cnt++;

            if (vd_dev->init_status == VMNG_STARTUP_BOTTOM_HALF_OK) {
                vmngh_suspend_instance_remove_vdev(dev_id, fid);
                vmngh_admin_notify_rm_vdev(vd_dev->msg_dev, VMNG_VM_RM_HOST_PDEV_WAIT);
                vmngh_vdev_free_bottom(vd_dev);
            }
            vmngh_vdev_free_top(vd_dev);
        }
    }

    vmng_info("Free all vdev finish. (dev_id=%u; free_cnt=%d)\n", dev_id, vd_dev_alive_cnt);
    return vd_dev_alive_cnt;
}

STATIC int vmngh_pci_uninit_instance(u32 dev_id)
{
    struct vdavinci_priv *vdavinci_set = NULL;
    struct vmngh_pci_dev *vmngh_pdev = NULL;
    int vd_dev_alive_cnt;
    u32 dev_num;
    u32 fid;
    int ret;

    if (devdrv_get_pfvf_type_by_devid(dev_id) != DEVDRV_SRIOV_TYPE_PF) {
        vmng_debug("Skip vf pci uninit instance. (dev_id=%u)\n", dev_id);
        return 0;
    }

    vmngh_pdev = vmngh_get_pdev_from_unit(dev_id);

    if (devdrv_is_sriov_support(dev_id) == false) {
        for (fid = 1; fid < VMNG_VDEV_MAX_PER_PDEV; ++fid) {
            vmngh_bw_data_clear_timer_uninit();
        }
        vmngh_bw_ctrl_info_uninit(dev_id);
    }

    vmngh_suspend_instance_remove_pdev(dev_id);
    vd_dev_alive_cnt = vmngh_free_all_vdev(dev_id);
    if (vd_dev_alive_cnt != 0) {
        vmng_warn("mdevs are maybe alive when phy pci uninit.(dev_id=%u; free_cnt=%d)\n", dev_id, vd_dev_alive_cnt);
    }

    /* Register to vdavinci dev */
    vdavinci_set = (struct vdavinci_priv *)devdrv_get_devdrv_priv(vmngh_pdev->pdev);
    if (vdavinci_set == NULL) {
        vmng_err("Get devdrv priv NULL. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    dev_num = devdrv_get_davinci_dev_num_by_pdev(vmngh_pdev->pdev);
    ka_task_mutex_lock(&g_vmngh_pci_mutex);
    if (vmngh_pdev->status == VMNG_STARTUP_PROBED) {
        vmngh_pdev->status = VMNG_STARTUP_UNPROBED;
        ka_task_mutex_unlock(&g_vmngh_pci_mutex);
        if (dev_id % dev_num == 0) {
            ret = hw_dvt_uninit((void *)vdavinci_set);
            if (ret != 0) {
                vmng_err("Call hw_dvt_uninit failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
            } else {
                vmng_info("Call hw_dvt_uninit success. (dev_id=%u)\n", dev_id);
            }
        }
    } else {
        ka_task_mutex_unlock(&g_vmngh_pci_mutex);
    }

    vmngh_sysfs_exit(dev_id, vmngh_pdev->pdev);

    vmngh_uninit_pci_pdev(vmngh_pdev);

    (void)vmngh_uninit_ctrl_ops(dev_id);

    vmng_info("Call instance uninit finish. (dev_id=%u)\n", dev_id);
    return 0;
}

#define MIA_MNG_HOST_NOTIFIER "mia_mng"
static int vmngh_sriov_dev_notifier_func(u32 udevid, enum uda_notified_action action)
{
    int ret = 0;

    if (action == UDA_INIT) {
        vmngh_sync_udevid_to_remote(udevid);
    } else if (action == UDA_REMOVE) {
        struct uda_dev_type type;
        uda_davinci_near_real_entity_type_pack(&type);
        (void)uda_remove_dev(&type, udevid);
    }

    vmng_info("notifier action. (udevid=%u; action=%d; ret=%d)\n", udevid, action, ret);

    return 0;
}

static int vmngh_notifier_func(u32 udevid, enum uda_notified_action action)
{
    int ret = 0;

    if (udevid >= VMNG_PDEV_MAX) {
        return vmngh_sriov_dev_notifier_func(udevid, action);
    }

    if (action == UDA_INIT) {
        ret = vmngh_pci_init_instance(udevid, uda_get_device(udevid));
    } else if (action == UDA_UNINIT) {
        ret = vmngh_pci_uninit_instance(udevid);
    }

    vmng_info("notifier action. (udevid=%u; action=%d; ret=%d)\n", udevid, action, ret);

    return ret;
}

/* version 0: obp, 1: others */
static int vmngh_set_mia_dev_res(u32 udevid, u32 version)
{
    struct vmngh_vdev_ctrl *ctrl = NULL;
    struct vmng_vdev_ctrl *vdev_ctrl = NULL;
    struct uda_mia_dev_para mia_para;
    u32 fid;
    u32 ret;

    ret = (u32)uda_udevid_to_mia_devid(udevid, &mia_para);
    fid = mia_para.sub_devid + 1;
    if (ret != 0) {
        vmng_err("Invalid para. (udevid=%u)\n", udevid);
        return (int)ret;
    }

    ctrl = vmngh_get_ctrl(mia_para.phy_devid, fid);
    vdev_ctrl = &ctrl->vdev_ctrl;

    if (version == 0) {
        u64 aic_bitmap = 0;
        u32 i;

         /* no aicore bitmap, just start from bit0, later only used for calc aicore num */
        for (i = 0; i < vdev_ctrl->core_num; i++) {
            aic_bitmap |= (0x1 << i);
        }
        ret = (u32)soc_resmng_dev_set_mia_res(udevid, MIA_AC_AIC, aic_bitmap, 1);
    } else {
        vf_ac_info_t *ac = &vdev_ctrl->vf_cfg.accelerator;
        struct res_inst_info inst;

        soc_resmng_inst_pack(&inst, udevid, TS_SUBSYS, 0);
        /* mia mng query num from device later */
        ret = (u32)soc_resmng_dev_set_mia_res(udevid, MIA_AC_AIC, (u64)ac->aic_bitmap, 1);
        ret |= (u32)soc_resmng_set_mia_res(&inst, MIA_STARS_RTSQ, (u64)ac->rtsq_slice_bitmap, 128); /* 128 num */
        ret |= (u32)soc_resmng_set_mia_res(&inst, MIA_STARS_EVENT, (u64)ac->event_slice_bitmap, 4096); /* 4096 num */
        ret |= (u32)soc_resmng_set_mia_res(&inst, MIA_STARS_NOTIFY, (u64)ac->notify_slice_bitmap, 512); /* 512 num */
    }

    return (int)ret;
}

static int vmngh_mia_dev_notifier_func(u32 udevid, enum uda_notified_action action)
{
    int ret = 0;

    if (action == UDA_INIT) {
        ret = vmngh_set_mia_dev_res(udevid, 0);
    }

    vmng_info("notifier action. (udevid=%u; action=%d; ret=%d)\n", udevid, action, ret);

    return ret;
}

static int vmngh_sec_eh_notifier_func(u32 udevid, enum uda_notified_action action)
{
    int ret = 0;

    if (action == UDA_INIT) {
        vmngh_sync_udevid_to_remote(udevid);
        ret = vmngh_set_mia_dev_res(udevid, 1);
    }

    vmng_info("notifier action. (udevid=%u; action=%d; ret=%d)\n", udevid, action, ret);

    return ret;
}

STATIC int __init vmngh_init_module(void)
{
    struct uda_dev_type type;
    int ret;

    vmng_info("Call vmngh_init_module start.\n");

    ret = vmngh_init_unit();
    if (ret != 0) {
        vmng_err("Call vmngh_init_unit failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = vmngh_init_ctrl();
    if (ret != 0) {
        vmng_err("Call vmngh_init_ctrl failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = vmngh_register_admin_rx_func(VMNGA_ADMIN_OPCODE_RM_PDEV, vmngh_admin_rx_agent_rm_pdev);
    if (ret != 0) {
        vmng_err("Register admin rx func failed. (ret=%d)\n", ret);
        vmngh_uninit_ctrl();
        return ret;
    }

    ka_task_mutex_init(&g_vmngh_pci_mutex);

    /* vmnghost <=> pcie host
     * description: register client, wait for pcie host module callback init_intance
     * next function: vmngh_init_instance
     */
    uda_davinci_near_real_entity_type_pack(&type);
    ret = uda_notifier_register(MIA_MNG_HOST_NOTIFIER, &type, UDA_PRI3, vmngh_notifier_func);
    if (ret != 0) {
        vmng_err("Call uda_notifier_register failed. (ret=%d)\n", ret);
        goto uda_register_failed;
    }

    uda_davinci_near_virtual_entity_type_pack(&type);
    ret = uda_notifier_register(MIA_MNG_HOST_NOTIFIER, &type, UDA_PRI0, vmngh_mia_dev_notifier_func);
    if (ret != 0) {
        vmng_err("Call uda_notifier_register failed. (ret=%d)\n", ret);
        goto mia_register_failed;
    }

    uda_dev_type_pack(&type, UDA_DAVINCI, UDA_ENTITY, UDA_NEAR, UDA_REAL_SEC_EH);
    ret = uda_notifier_register(MIA_MNG_HOST_NOTIFIER, &type, UDA_PRI0, vmngh_sec_eh_notifier_func);
    if (ret != 0) {
        vmng_err("Call uda_notifier_register failed. (ret=%d)\n", ret);
        goto sec_eh_register_failed;
    }

    ret = vmngh_proc_fs_init();
    if (ret == 0) {
        procfs_valid = VMNG_PROCFS_VALID;
        vmng_info("Proc fs init finish.\n");
    }

    vmng_info("Init module finish.\n");
    return 0;

sec_eh_register_failed:
    uda_davinci_near_virtual_entity_type_pack(&type);
    (void)uda_notifier_unregister(MIA_MNG_HOST_NOTIFIER, &type);
mia_register_failed:
    uda_davinci_near_real_entity_type_pack(&type);
    (void)uda_notifier_unregister(MIA_MNG_HOST_NOTIFIER, &type);
uda_register_failed:
    vmngh_unregister_admin_rx_func(VMNGA_ADMIN_OPCODE_RM_PDEV);
    vmngh_uninit_ctrl();
    return ret;
}
ka_module_init(vmngh_init_module);

STATIC void __exit vmngh_exit_module(void)
{
    struct uda_dev_type type;
    u32 alive_status;

    if (procfs_valid == VMNG_PROCFS_VALID) {
        vmngh_proc_fs_uninit();
        procfs_valid = VMNG_PROCFS_INVALID;
        vmng_info("Proc fs uninit finish.\n");
    }
    alive_status = vmngh_vd_dev_alive();
    if (alive_status == VMNGA_VD_DEV_ALIVE_OK) {
        vmng_warn("There are mdevs alive, rmmod ko would cause mistake.\n");
    }

    uda_dev_type_pack(&type, UDA_DAVINCI, UDA_ENTITY, UDA_NEAR, UDA_REAL_SEC_EH);
    (void)uda_notifier_unregister(MIA_MNG_HOST_NOTIFIER, &type);

    uda_davinci_near_virtual_entity_type_pack(&type);
    (void)uda_notifier_unregister(MIA_MNG_HOST_NOTIFIER, &type);

    uda_davinci_near_real_entity_type_pack(&type);
    (void)uda_notifier_unregister(MIA_MNG_HOST_NOTIFIER, &type);

    vmngh_unregister_admin_rx_func(VMNGA_ADMIN_OPCODE_RM_PDEV);
    vmngh_uninit_ctrl();
    vmng_info("Exit module finish.\n");
}
ka_module_exit(vmngh_exit_module);

KA_MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
KA_MODULE_DESCRIPTION("virt mng host driver");
KA_MODULE_LICENSE("GPL");
