/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "dvt.h"
#include "priv_ops.h"

#define SHIFT 8
#define DEVFN_MASK 0xff

STATIC void hw_dvt_clean_vdavinci_vfs(struct hw_dvt *dvt)
{
    unsigned int i = 0;
 
    if (dvt->sriov.vf_array != NULL) {
        for (i = 0; i < dvt->sriov.vf_num; i++) {
            /* for pci_get_domain_bus_and_slot */
            pci_dev_put(dvt->sriov.vf_array[i].vf);
            dvt->sriov.vf_array[i].vf = NULL;
        }
        kfree(dvt->sriov.vf_array);
        dvt->sriov.vf_array = NULL;
    }
    dvt->sriov.vf_num = 0;
    dvt->sriov.vf_used = 0;
    dvt->is_sriov_enabled = false;
}

STATIC int sriov_virtfn_bus(struct pci_dev *dev, int vf_id)
{
    if (!dev->is_physfn) {
        return -EINVAL;
    }
    return dev->bus->number + ((dev->devfn + dev->sriov->offset +
        dev->sriov->stride * vf_id) >> SHIFT);
}

STATIC int sriov_virtfn_devfn(struct pci_dev *dev, int vf_id)
{
    if (!dev->is_physfn) {
        return -EINVAL;
    }
    return (dev->devfn + dev->sriov->offset +
        dev->sriov->stride * vf_id) & DEVFN_MASK;
}

STATIC int hw_dvt_sriov_disable(struct pci_dev *dev)
{
    struct device *kdev = &dev->dev;
    struct hw_dvt *dvt;
    int ret = 0;

    dvt = kdev_to_davinci(kdev)->dvt;
    if (!dvt) {
        return -EINVAL;
    }

    mutex_lock(&dvt->lock);
    ret = vdavinci_priv_vdev_enable_sriov(dvt->vdavinci_priv, dev, 0);
    if (ret != 0) {
        vascend_err(kdev, "Failed to disable sriov\n");
        mutex_unlock(&dvt->lock);
        return ret;
    }
    hw_dvt_clean_vdavinci_vfs(dvt);
    hw_dvt_uninit_dev_pf_info(dvt);
    ret = hw_dvt_set_mmio_ops(dvt, vdavinci_mmio_pf_devices_ops);
    if (ret) {
        vascend_err(kdev, "Failed to set mmio ops\n");
        mutex_unlock(&dvt->lock);
        return -EINVAL;
    }
    vascend_info(kdev, "Sriov disable success\n");
    mutex_unlock(&dvt->lock);
    return 0;
}

int hw_dvt_sriov_enable(struct pci_dev *dev, int num_vfs)
{
    struct device *kdev = &dev->dev;
    struct hw_dvt *dvt;
    unsigned int i = 0;
    int ret = 0;

    dvt = kdev_to_davinci(kdev)->dvt;
    if (!dvt) {
        return -EINVAL;
    }

    if (num_vfs > VDAVINCI_VF_MAX || num_vfs < 0) {
        vascend_err(kdev, "Unsupported vf num:%u\n", num_vfs);
        return -EINVAL;
    }

    if (!hw_vdavinci_vf_used_num_zero(dvt)) {
        vascend_err(kdev, "the count of mdev has not been reset to zero\n");
        return -EINVAL;
    }

    if (num_vfs == 0) {
        return hw_dvt_sriov_disable(dev);
    }
    ret = vdavinci_priv_vdev_enable_sriov(dvt->vdavinci_priv, dev, num_vfs);
    if (ret != 0) {
        vascend_err(kdev, "Failed to enable sriov\n");
        return ret;
    }

    mutex_lock(&dvt->lock);
    ret = hw_dvt_init_dev_pf_info(dvt);
    if (ret) {
        vascend_err(kdev, "init dev pf info failed.\n");
        goto out_free_vf;
    }

    dvt->sriov.vf_array = kzalloc(num_vfs * sizeof(struct vf_used_map), GFP_KERNEL);
    if (!dvt->sriov.vf_array) {
        vascend_err(kdev, "Failed to allocate vf arrary\n");
        goto out_free_vf;
    }
    for (i = 0; i < num_vfs; i++) {
        dvt->sriov.vf_array[i].vf = pci_get_domain_bus_and_slot(pci_domain_nr(dev->bus),
            sriov_virtfn_bus(dev, i),
            sriov_virtfn_devfn(dev, i));
        if (!dvt->sriov.vf_array[i].vf) {
            vascend_err(kdev, "Failed to enable vf\n");
            goto out_free_vf;
        }
        dvt->sriov.vf_array[i].vdavinci = NULL;
        dvt->sriov.vf_array[i].used = false;
    }
    dvt->sriov.vf_used = 0;
    dvt->sriov.vf_num = num_vfs;
    dvt->is_sriov_enabled = true;
    ret = hw_dvt_set_mmio_ops(dvt, vdavinci_mmio_vf_devices_ops);
    if (ret) {
        vascend_err(kdev, "Failed to set mmio ops\n");
        goto out_free_vf;
    }
    mutex_unlock(&dvt->lock);
    return num_vfs;

out_free_vf:
    mutex_unlock(&dvt->lock);
    return hw_dvt_sriov_disable(dev);
}
