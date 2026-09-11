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
#include "devdrv_interface.h"
#include "virtmnghost_unit.h"
#include "virtmng_public_def.h"
#include "virtmnghost_sysfs.h"

#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_fs_pub.h"
#include "ka_driver_pub.h"

STATIC struct vmngh_pci_dev *vmngh_sysfs_get_valid_pdev(ka_device_t *dev)
{
    struct vmngh_pci_dev *vmngh_pdev = NULL;
    u32 dev_id;

    dev_id = (u32)devdrv_get_dev_id_by_pdev(ka_pci_to_pci_dev(dev));
    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("Device id is invalid. (dev_id=%u)\n", dev_id);
        return NULL;
    }
    vmngh_pdev = vmngh_get_pdev_from_unit(dev_id);
    if (vmngh_pdev == NULL) {
        vmng_err("vmngh_pdev is invalid. (dev_id=%u)\n", dev_id);
        return NULL;
    }
    if (vmngh_pdev->status != VMNG_STARTUP_PROBED) {
        vmng_err("pdev_status is invalid. (dev_id=%u; status=%u)\n", dev_id, vmngh_pdev->status);
        return NULL;
    }
    return vmngh_pdev;
}

STATIC ssize_t vmngh_sysfs_mdev_info(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    struct vmngh_pci_dev *vmngh_pdev = NULL;
    struct vmngh_vd_dev *vd_dev = NULL;
    ssize_t offset = 0;
    ka_uuid_le_t uuid;
    u32 dev_id;
    u32 fid;
    int ret;

    (void)attr;
    vmngh_pdev = vmngh_sysfs_get_valid_pdev(dev);
    if (vmngh_pdev == NULL) {
        vmng_err("Get sysfs pdev error.\n");
        return offset;
    }
    dev_id = vmngh_pdev->dev_id;
    ret = snprintf_s(buf, KA_MM_PAGE_SIZE, KA_MM_PAGE_SIZE - 1, "dev %u all mdev info.\n", dev_id);
    if (ret >= 0) {
        offset += ret;
    }

    for (fid = 0; fid < VMNG_VDEV_MAX_PER_PDEV; fid++) {
        vd_dev = vmngh_pdev->vd_dev[fid];
        if ((vd_dev != NULL) && (vd_dev->init_status != VMNG_STARTUP_UNPROBED)) {
            uuid = vd_dev->vascend_uuid;

            ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
                "[chip] 19e5:0x%04x %02x:%02x.%u id %2u: "
                "[vd] uuid %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x fid %2u dtype "
                "d%u: ",
                vmngh_pdev->ep_devic_id, ka_pci_get_bus_number(vmngh_pdev->pdev), 
                KA_PCI_SLOT(vmngh_pdev->pdev->devfn), KA_PCI_FUNC(vmngh_pdev->pdev->devfn), 
                dev_id, uuid.b[3], uuid.b[2], uuid.b[1], uuid.b[0], uuid.b[5], uuid.b[4], uuid.b[7], uuid.b[6], 
                uuid.b[8], uuid.b[9], uuid.b[10], uuid.b[11], uuid.b[12], uuid.b[13], uuid.b[14], uuid.b[15], 
                vd_dev->fid, vd_dev->dtype);
            if (ret != -1) {
                offset += ret;
            }

            if (vd_dev->init_status == VMNG_STARTUP_BOTTOM_HALF_OK) {
                ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
                    "[vm] pid %6d vmid %2d [agent] 19e5:%04x %02x:%02x.%d id %2u.\n", vd_dev->vm_pid, vd_dev->vm_id,
                    vd_dev->agent_device, (vd_dev->agent_bdf >> VMNG_VDEV_AGENT_BDF_SHIFT_8),
                    ((vd_dev->agent_bdf >> 3) & 0x1f), (vd_dev->agent_bdf & 0x7), vd_dev->agent_dev_id);
            } else {
                ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "[agent] not build.\n");
            }
            if (ret != -1) {
                offset += ret;
            }
        }
    }
    return offset;
}

static KA_DRIVER_DEVICE_ATTR(mdev_info, (KA_S_IRUSR | KA_S_IRGRP), vmngh_sysfs_mdev_info, NULL);

#ifdef CFG_SOC_PLATFORM_CLOUD_V2
STATIC ssize_t vmngh_sysfs_vm_full_vf_enable_show(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    struct vmngh_pci_dev *vmngh_pdev = NULL;
    ssize_t offset = 0;
    u32 dev_id;
    int ret;

    vmngh_pdev = vmngh_sysfs_get_valid_pdev(dev);
    if (vmngh_pdev == NULL) {
        vmng_err("Get sysfs pdev error.\n");
        return offset;
    }
    dev_id = vmngh_pdev->dev_id;

    ret = snprintf_s(buf, KA_MM_PAGE_SIZE, KA_MM_PAGE_SIZE - 1, "dev %u vm_full_spec_enable is %u.\n",
        dev_id, vmngh_pdev->vm_full_spec_enable);
    if (ret >= 0) {
        offset += ret;
    }

    return offset;
}

STATIC ssize_t vmngh_sysfs_vm_full_vf_enable_store(ka_device_t *dev, ka_device_attribute_t *attr,
    const char *buf, size_t count)
{
    struct vmngh_pci_dev *vmngh_pdev = NULL;
    int input;
    int ret;
#ifndef DRV_UT
    vmngh_pdev = vmngh_sysfs_get_valid_pdev(dev);
    if (vmngh_pdev == NULL) {
        vmng_err("Get sysfs pdev error. (uid=%u)\n", ka_task_get_current_cred_uid());
        return -1;
    }
    ret = ka_base_kstrtoint(buf, 0, &input);
    if (ret != 0) {
        vmng_err("ka_base_kstrtoint failed. (devid=%u; ret=%d; uid=%u)\n",
            vmngh_pdev->dev_id, ret, ka_task_get_current_cred_uid());
        return -1;
    }

    ka_task_mutex_lock(&vmngh_pdev->vpdev_mutex);
    if (input == 0) {
        vmngh_pdev->vm_full_spec_enable = 0;
        vmng_event("Disable full spec support, all the vf template can select. (devid=%u; uid=%u)\n",
            vmngh_pdev->dev_id, ka_task_get_current_cred_uid());
    } else if (input == 1) {
        if (vmngh_pdev->vdev_ref == 0) {
            vmngh_pdev->vm_full_spec_enable = 1;
            vmng_event("Enable full spec support, only full spec vf can select. (devid=%u; uid=%u)\n",
                vmngh_pdev->dev_id, ka_task_get_current_cred_uid());
        } else {
            vmng_err("Mdevs are may be alive, please remove all mdevs.(devid=%u; uid=%u)\n",
                vmngh_pdev->dev_id, ka_task_get_current_cred_uid());
            ka_task_mutex_unlock(&vmngh_pdev->vpdev_mutex);
            return -1;
        }
    } else {
        vmng_err("Invalid input vf_vnic enbale. (devid=%u; input=%d; uid=%u)\n",
            vmngh_pdev->dev_id, input, ka_task_get_current_cred_uid());
    }
    ka_task_mutex_unlock(&vmngh_pdev->vpdev_mutex);
#endif

    return count;
}
static KA_DRIVER_DEVICE_ATTR(vm_full_vf_enable, (KA_S_IWUSR | KA_S_IRUSR | KA_S_IWGRP | KA_S_IRGRP),
    vmngh_sysfs_vm_full_vf_enable_show, vmngh_sysfs_vm_full_vf_enable_store);
#endif

static ka_attribute_t *g_vmngh_sysfs_attrs[] = {
    ka_fs_get_dev_attr(dev_attr_mdev_info)
#ifdef CFG_SOC_PLATFORM_CLOUD_V2
    ka_fs_get_dev_attr(dev_attr_vm_full_vf_enable)
#endif
    NULL,
};

static const ka_attribute_group_t g_vmngh_sysfs_group = {
    ka_fs_init_ag_attrs(g_vmngh_sysfs_attrs)
};

static bool vmng_get_sysfs_creat_group_capbility(ka_pci_dev_t *pcidev, int dev_id)
{
    int devnum_by_pdev;

    devnum_by_pdev = devdrv_get_davinci_dev_num_by_pdev(pcidev);
    if (dev_id % devnum_by_pdev == 0) {
        return true;
    }

    return false;
}

int vmngh_sysfs_init(u32 dev_id, ka_pci_dev_t *pcidev)
{
    int ret;

    if (vmng_get_sysfs_creat_group_capbility(pcidev, (int)dev_id)) {
        ret = ka_sysfs_create_group(&pcidev->dev.kobj, &g_vmngh_sysfs_group);
        if (ret != 0) {
            vmng_err("Create group failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
            return ret;
        }
    }

    return 0;
}

void vmngh_sysfs_exit(u32 dev_id, ka_pci_dev_t *pcidev)
{
    if (vmng_get_sysfs_creat_group_capbility(pcidev, (int)dev_id)) {
        ka_sysfs_remove_group(&pcidev->dev.kobj, &g_vmngh_sysfs_group);
    }
}
