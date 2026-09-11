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

#include "virtmnghost_unit.h"
#include "virtmng_public_def.h"
#include "vmng_mem_alloc_interface.h"

#include "ka_kernel_def_pub.h"
#include "ka_base_pub.h"

int vmngh_dev_id_check(u32 dev_id, u32 fid)
{
    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("Input parameter is error. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return -EINVAL;
    }
    if (fid >= VMNG_VDEV_MAX_PER_PDEV) {
        vmng_err("Input parameter is error. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return -EINVAL;
    }
    return 0;
}

int vmngh_vm_id_check(u32 vm_id, u32 vm_devid)
{
    if (vm_id >= VMNG_VM_MAX) {
        vmng_err("Input parameter is error. (vm_id=%u; vm_devid=%u)\n", vm_id, vm_devid);
        return -EINVAL;
    }
    if (vm_devid >= VMNG_PDEV_MAX) {
        vmng_err("Input parameter is error. (vm_id=%u; vm_devid=%u)\n", vm_id, vm_devid);
        return -EINVAL;
    }

    return 0;
}

struct vmngh_unit g_vmngh_unit;

struct vmngh_vd_dev *vmngh_get_vddev_by_id(u32 dev_id, u32 fid)
{
    if (vmngh_dev_id_check(dev_id, fid) != 0) {
        vmng_err("Parameter check failed. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return NULL;
    }

    return g_vmngh_unit.vmngh_pcidev[dev_id].vd_dev[fid];
}

void *vmngh_get_vdavinci_by_id(u32 dev_id, u32 fid)
{
    struct vmngh_vd_dev *vd_dev = NULL;

    if (vmngh_dev_id_check(dev_id, fid) != 0) {
        vmng_err("Parameter check failed. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return NULL;
    }

    vd_dev = vmngh_get_vddev_by_id(dev_id, fid);
    if (vd_dev == NULL) {
        return NULL;
    }
    return vd_dev->vdavinci;
}
KA_EXPORT_SYMBOL(vmngh_get_vdavinci_by_id);

ka_mutex_t *vmngh_get_vdev_lock_from_unit(u32 dev_id, u32 fid)
{
    return &g_vmngh_unit.vmngh_pcidev[dev_id].vddev_mutex[fid];
}

struct vmngh_vd_dev *vmngh_get_vdev_from_unit(u32 dev_id, u32 fid)
{
    if (vmngh_dev_id_check(dev_id, fid) != 0) {
        vmng_err("Parameter check failed. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return NULL;
    }

    return g_vmngh_unit.vmngh_pcidev[dev_id].vd_dev[fid];
}

struct vmngh_pci_dev *vmngh_get_pdev_from_unit(u32 dev_id)
{
    return &(g_vmngh_unit.vmngh_pcidev[dev_id]);
}

void vmngh_set_device_status(u32 dev_id, u32 valid)
{
    struct vmngh_pci_dev *dev = &g_vmngh_unit.vmngh_pcidev[dev_id];
    dev->valid = valid;
    vmng_info("Set status finished. (devid=%d; status=%d)\n", dev_id, valid);
}

u32 vmngh_get_device_status(u32 dev_id)
{
    return g_vmngh_unit.vmngh_pcidev[dev_id].valid;
}

void vmngh_set_peer_dev_id(u32 dev_id, int peer_dev_id)
{
    g_vmngh_unit.vmngh_pcidev[dev_id].peer_dev_id = (u32)peer_dev_id;
}

void vmngh_free_vdev(struct vmngh_vd_dev *vd_dev)
{
    u32 dev_id;
    u32 fid;

    if (vd_dev == NULL) {
        vmng_err("Input parameter is error.\n");
        return;
    }
    dev_id = vd_dev->dev_id;
    fid = vd_dev->fid;
    if (vmngh_dev_id_check(dev_id, fid) == 0) {
        g_vmngh_unit.vmngh_pcidev[dev_id].vd_dev[fid] = NULL;
    }

    if (vd_dev->vpc_unit != NULL) {
        vmng_kfree(vd_dev->vpc_unit);
        vd_dev->vpc_unit = NULL;
    }
    vmng_kfree(vd_dev);
    vd_dev = NULL;
}

int vmngh_init_unit(void)
{
    u32 dev_id;

    if (memset_s(&g_vmngh_unit, sizeof(g_vmngh_unit), 0, sizeof(g_vmngh_unit)) != EOK) {
        vmng_err("Call memset_s failed.\n");
        return -EINVAL;
    }

    for (dev_id = 0; dev_id < VMNG_PDEV_MAX; dev_id++) {
        g_vmngh_unit.vmngh_pcidev[dev_id].dev_id = (u32)VMNG_CTRL_DEVICE_ID_INIT;
    }

    return 0;
}

u32 vmngh_vd_dev_alive(void)
{
    u32 dev_id;
    u32 fid;

    for (dev_id = 0; dev_id < VMNG_PDEV_MAX; dev_id++) {
        for (fid = 0; fid < VMNG_VDEV_MAX_PER_PDEV; fid++) {
            if (g_vmngh_unit.vmngh_pcidev[dev_id].vd_dev[fid] != NULL) {
                return VMNGA_VD_DEV_ALIVE_OK;
            }
        }
    }

    return VMNGA_VD_DEV_ALIVE_NOK;
}
