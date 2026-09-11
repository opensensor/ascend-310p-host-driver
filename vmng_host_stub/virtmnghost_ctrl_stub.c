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

#include "ka_base_pub.h"
#include "ka_pci_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_memory_pub.h"

#include "virtmng_interface.h"
#include "virtmng_public_def.h"

#define PCI_VENDOR_ID_HUAWEI 0x19e5
#define HISI_EP_DEVICE_ID_MINIV1 0xd100
#define HISI_EP_DEVICE_ID_MINIV2 0xd500
#define HISI_EP_DEVICE_ID_CLOUD 0xd801
#define HISI_EP_DEVICE_ID_CLOUD_V2 0xd802
#define DEVDRV_DIVERSITY_PCIE_VENDOR_ID 0xFFFF

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


int vmngh_get_virtual_addr_info(u32 dev_id, u32 fid, enum vmng_get_addr_type type, u64 *addr, u64 *size)
{
    return 0;
}
KA_EXPORT_SYMBOL(vmngh_get_virtual_addr_info);

ka_dma_addr_t vmngh_dma_map_guest_page(u32 dev_id, u32 fid, unsigned long addr, unsigned long size,
    ka_sg_table_t **dma_sgt)
{
    return 0;
}
KA_EXPORT_SYMBOL(vmngh_dma_map_guest_page);

void vmngh_dma_unmap_guest_page(u32 dev_id, u32 fid, ka_sg_table_t *dma_sgt)
{

}
KA_EXPORT_SYMBOL(vmngh_dma_unmap_guest_page);


int vmngh_ctrl_get_vm_id(u32 dev_id, u32 fid)
{
    return 0;
}
KA_EXPORT_SYMBOL(vmngh_ctrl_get_vm_id);

int vmngh_ctrl_get_devid_fid(u32 vm_id, u32 vm_devid, u32 *dev_id, u32 *fid)
{
    return 0;
}
KA_EXPORT_SYMBOL(vmngh_ctrl_get_devid_fid);

void vmngh_set_dev_info(u32 dev_id, enum vmngh_dev_info_type type, u64 val)
{
    return;
}
KA_EXPORT_SYMBOL(vmngh_set_dev_info);

int vmngh_create_container_vdev(u32 dev_id, u32 dtype, u32 *vfid, struct vmng_vf_res_info *vf_resource)
{
    return 0;
}
KA_EXPORT_SYMBOL(vmngh_create_container_vdev);

int vmngh_destory_container_vdev(u32 dev_id, u32 vfid)
{
    return 0;
}
KA_EXPORT_SYMBOL(vmngh_destory_container_vdev);

void vmngh_set_total_core_num(u32 dev_id, u32 total_core_num)
{
    return;
}
KA_EXPORT_SYMBOL(vmngh_set_total_core_num);

int vmng_bandwidth_limit_check(struct vmng_bandwidth_check_info *info)
{
    return 0;
}
KA_EXPORT_SYMBOL(vmng_bandwidth_limit_check);
bool vmngh_dma_pool_active(u32 dev_id, u32 fid)
{
    return true;
}
KA_EXPORT_SYMBOL(vmngh_dma_pool_active);

int vmngh_dma_map_guest_page_batch(u32 dev_id, u32 fid, unsigned long *gfn,
    unsigned long *dma_addr, unsigned long count)
{
    return 0;
}
KA_EXPORT_SYMBOL(vmngh_dma_map_guest_page_batch);

void vmngh_dma_unmap_guest_page_batch(u32 dev_id, u32 fid,
    unsigned long *gfn, unsigned long *dma_addr, unsigned long count)
{
    return;
}
KA_EXPORT_SYMBOL(vmngh_dma_unmap_guest_page_batch);

int vmngh_enable_sriov(u32 dev_id)
{
    return 0;
}
KA_EXPORT_SYMBOL(vmngh_enable_sriov);

int vmngh_disable_sriov(u32 dev_id)
{
    return 0;
}
KA_EXPORT_SYMBOL(vmngh_disable_sriov);

int vmngh_enquire_soc_resource(u32 dev_id, u32 vfid, struct vmng_soc_resource_enquire *info)
{
    return 0;
}
KA_EXPORT_SYMBOL(vmngh_enquire_soc_resource);

enum vmng_split_mode vmng_get_device_split_mode(u32 dev_id)
{
    return VMNG_NORMAL_NONE_SPLIT_MODE;
}
KA_EXPORT_SYMBOL(vmng_get_device_split_mode);

int vmngh_sriov_reset_vdev(u32 dev_id, u32 vfid)
{
    return 0;
}
KA_EXPORT_SYMBOL(vmngh_sriov_reset_vdev);
