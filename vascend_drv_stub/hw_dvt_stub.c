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

#include "hw_vdavinci.h"
#include "ka_kernel_def_pub.h"
#include "ka_dfx_pub.h"
#include "ka_fs_pub.h"
#include "ka_errno_pub.h"
#include "ka_task_pub.h"

bool hw_dvt_check_host_is_uvp(void);

#define PCI_VENDOR_ID_HUAWEI 0x19e5
#define HISI_EP_DEVICE_ID_MINIV1 0xd100
#define HISI_EP_DEVICE_ID_MINIV2 0xd500
#define HISI_EP_DEVICE_ID_CLOUD 0xd801
#define HISI_EP_DEVICE_ID_CLOUD_V2 0x802
#define DEVDRV_DIVERSITY_PCIE_VENDOR_ID 0xFFFF
#define UVP_VERSION_INFO_SIZE 256

static const ka_pci_device_id_t g_vmngh_tbl[] = {{ KA_PCI_VDEVICE(HUAWEI, HISI_EP_DEVICE_ID_MINIV2), 0 },
                                                   { KA_PCI_VDEVICE(HUAWEI, HISI_EP_DEVICE_ID_CLOUD), 0 },
                                                   { KA_PCI_VDEVICE(HUAWEI, HISI_EP_DEVICE_ID_CLOUD_V2), 0 },
                                                   { KA_PCI_VDEVICE(HUAWEI, 0xd805), 0 },
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

struct vdavinci_mode_info g_vdev_mode = { 0 };
struct vdavinci_drv_ops vascend_virtual_ops = { 0 };

int register_vdavinci_virtual_ops(struct vdavinci_drv_ops *ops)
{
    if (!ops) {
        ka_dfx_pr_err("Invalid para. ops is NULL\n");
        return -EINVAL;
    }

    vascend_virtual_ops.vdavinci_init = ops->vdavinci_init;
    vascend_virtual_ops.vdavinci_uninit = ops->vdavinci_uninit;
    vascend_virtual_ops.vdavinci_hypervisor_inject_msix = ops->vdavinci_hypervisor_inject_msix;
    vascend_virtual_ops.vdavinci_hypervisor_read_gpa = ops->vdavinci_hypervisor_read_gpa;
    vascend_virtual_ops.vdavinci_hypervisor_write_gpa = ops->vdavinci_hypervisor_write_gpa;
    vascend_virtual_ops.vdavinci_hypervisor_gfn_to_mfn = ops->vdavinci_hypervisor_gfn_to_mfn;
    vascend_virtual_ops.vdavinci_hypervisor_dma_pool_init = ops->vdavinci_hypervisor_dma_pool_init;
    vascend_virtual_ops.vdavinci_hypervisor_dma_pool_uninit = ops->vdavinci_hypervisor_dma_pool_uninit;
    vascend_virtual_ops.vdavinci_hypervisor_dma_map_guest_page = ops->vdavinci_hypervisor_dma_map_guest_page;
    vascend_virtual_ops.vdavinci_hypervisor_dma_unmap_guest_page = ops->vdavinci_hypervisor_dma_unmap_guest_page;
    vascend_virtual_ops.vdavinci_hypervisor_dma_pool_active = ops->vdavinci_hypervisor_dma_pool_active;
    vascend_virtual_ops.vdavinci_hypervisor_dma_map_guest_page_batch =
        ops->vdavinci_hypervisor_dma_map_guest_page_batch;
    vascend_virtual_ops.vdavinci_hypervisor_dma_unmap_guest_page_batch =
        ops->vdavinci_hypervisor_dma_unmap_guest_page_batch;
    vascend_virtual_ops.vdavinci_hypervisor_is_valid_gfn = ops->vdavinci_hypervisor_is_valid_gfn;
    vascend_virtual_ops.vdavinci_hypervisor_mmio_get = ops->vdavinci_hypervisor_mmio_get;
    vascend_virtual_ops.vdavinci_hypervisor_dma_alloc_coherent = ops->vdavinci_hypervisor_dma_alloc_coherent;
    vascend_virtual_ops.vdavinci_hypervisor_dma_free_coherent = ops->vdavinci_hypervisor_dma_free_coherent;
    vascend_virtual_ops.vdavinci_hypervisor_dma_map_single = ops->vdavinci_hypervisor_dma_map_single;
    vascend_virtual_ops.vdavinci_hypervisor_dma_unmap_single = ops->vdavinci_hypervisor_dma_unmap_single;
    vascend_virtual_ops.vdavinci_hypervisor_dma_map_page = ops->vdavinci_hypervisor_dma_map_page;
    vascend_virtual_ops.vdavinci_hypervisor_dma_unmap_page = ops->vdavinci_hypervisor_dma_unmap_page;
    vascend_virtual_ops.vdavinci_get_reserve_iova_for_check = ops->vdavinci_get_reserve_iova_for_check;
    ka_dfx_pr_info("register virtual ops success!\n");
    return 0;
}
EXPORT_SYMBOL_GPL(register_vdavinci_virtual_ops);

void unregister_vdavinci_virtual_ops(void)
{
    struct vdavinci_drv_ops ops = {0};
    vascend_virtual_ops = ops;
}
EXPORT_SYMBOL_GPL(unregister_vdavinci_virtual_ops);

struct vdavinci_drv_ops *get_vdavinci_virtual_ops(void)
{
    return &vascend_virtual_ops;
}

int hw_dvt_set_mode(int mode)
{
    if (mode < 0 || mode >= VDAVINCI_MODE_MAX) {
        ka_dfx_pr_err("Invalid para. mode is %d\n", mode);
        return -EINVAL;
    }

    ka_task_down_write(&g_vdev_mode.rw_sem);
    g_vdev_mode.mode = mode;
    ka_task_up_write(&g_vdev_mode.rw_sem);
    ka_dfx_pr_info("set mode success! mode=%d\n", mode);
    return 0;
}
EXPORT_SYMBOL_GPL(hw_dvt_set_mode);

int hw_dvt_get_mode(int *mode)
{
    if (mode == NULL) {
        ka_dfx_pr_err("Invalid para. mode is NULL\n");
        return -EINVAL;
    }

    *mode = g_vdev_mode.mode;
    return 0;
}
EXPORT_SYMBOL_GPL(hw_dvt_get_mode);

bool hw_dvt_check_is_vm_mode(void)
{
    int ret;
    int mode;

    ret = hw_dvt_get_mode(&mode);
    if (ret != 0) {
        ka_dfx_pr_err("hw_dvt_get_mode fail, ret: %d\n", ret);
        return false;
    }

    if (mode == VDAVINCI_VM) {
        return true;
    }

    return false;
}

int hw_dvt_init(void *vdavinci_priv)
{
    if (get_vdavinci_virtual_ops()->vdavinci_init != NULL) {
        return get_vdavinci_virtual_ops()->vdavinci_init(vdavinci_priv);
    }

    return 0;
}
EXPORT_SYMBOL_GPL(hw_dvt_init);

int hw_dvt_uninit(void *vdavinci_priv)
{
    if (get_vdavinci_virtual_ops()->vdavinci_uninit != NULL) {
        return get_vdavinci_virtual_ops()->vdavinci_uninit(vdavinci_priv);
    }

    return 0;
}
EXPORT_SYMBOL_GPL(hw_dvt_uninit);

MODULE_DESCRIPTION("Ascend virtualization interface stub");
KA_MODULE_LICENSE("GPL");
