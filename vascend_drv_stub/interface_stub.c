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

/**
 * hw_dvt_hypervisor_inject_msix - inject a MSIX interrupt into vdavinci
 *
 * Returns:
 * Zero on success, negative error code if failed.
 */
int hw_dvt_hypervisor_inject_msix(void *__vdavinci, u32 vector)
{
    if (hw_dvt_check_is_vm_mode() && get_vdavinci_virtual_ops()->vdavinci_hypervisor_inject_msix != NULL) {
        return get_vdavinci_virtual_ops()->vdavinci_hypervisor_inject_msix(__vdavinci, vector);
    }

    return 0;
}
EXPORT_SYMBOL_GPL(hw_dvt_hypervisor_inject_msix);

/**
 * hw_dvt_hypervisor_read_gpa - copy data from GPA to host data buffer
 * @vdavinci: a vdavinci
 * @gpa: guest physical address
 * @buf: host data buffer
 * @len: data length
 *
 * Returns:
 * Zero on success, negative error code if failed.
 */
int hw_dvt_hypervisor_read_gpa(void *__vdavinci,
		unsigned long gpa, void *buf, unsigned long len)
{
    if (hw_dvt_check_is_vm_mode() && get_vdavinci_virtual_ops()->vdavinci_hypervisor_read_gpa != NULL) {
        return get_vdavinci_virtual_ops()->vdavinci_hypervisor_read_gpa(__vdavinci, gpa, buf, len);
    }

    return 0;
}
EXPORT_SYMBOL_GPL(hw_dvt_hypervisor_read_gpa);

/**
 * hw_dvt_hypervisor_write_gpa - copy data from host data buffer to GPA
 * @vdavinci: a vdavinci
 * @gpa: guest physical address
 * @buf: host data buffer
 * @len: data length
 *
 * Returns:
 * Zero on success, negative error code if failed.
 */
int hw_dvt_hypervisor_write_gpa(void *__vdavinci,
		unsigned long gpa, void *buf, unsigned long len)
{
    if (hw_dvt_check_is_vm_mode() && get_vdavinci_virtual_ops()->vdavinci_hypervisor_write_gpa != NULL) {
        return get_vdavinci_virtual_ops()->vdavinci_hypervisor_write_gpa(__vdavinci, gpa, buf, len);
    }

    return 0;
}
EXPORT_SYMBOL_GPL(hw_dvt_hypervisor_write_gpa);

/**
 * hw_dvt_hypervisor_gfn_to_mfn - translate a GFN to MFN
 * @vdavinci: a vdavinci
 * @gpfn: guest pfn
 *
 * Returns:
 * MFN on success, hw_dvt_INVALID_ADDR if failed.
 */
unsigned long hw_dvt_hypervisor_gfn_to_mfn(
		void *__vdavinci, unsigned long gfn)
{
    if (hw_dvt_check_is_vm_mode() && get_vdavinci_virtual_ops()->vdavinci_hypervisor_gfn_to_mfn != NULL) {
        return get_vdavinci_virtual_ops()->vdavinci_hypervisor_gfn_to_mfn(__vdavinci, gfn);
    }

    return 0;
}
EXPORT_SYMBOL_GPL(hw_dvt_hypervisor_gfn_to_mfn);

int hw_dvt_hypervisor_dma_pool_init(void *__vdavinci)
{
    if (hw_dvt_check_is_vm_mode() && get_vdavinci_virtual_ops()->vdavinci_hypervisor_dma_pool_init != NULL) {
        return get_vdavinci_virtual_ops()->vdavinci_hypervisor_dma_pool_init(__vdavinci);
    }

    return 0;
}
EXPORT_SYMBOL_GPL(hw_dvt_hypervisor_dma_pool_init);

void hw_dvt_hypervisor_dma_pool_uninit(void *__vdavinci)
{
    if (hw_dvt_check_is_vm_mode() && get_vdavinci_virtual_ops()->vdavinci_hypervisor_dma_pool_uninit != NULL) {
        return get_vdavinci_virtual_ops()->vdavinci_hypervisor_dma_pool_uninit(__vdavinci);
    }

    return;
}
EXPORT_SYMBOL_GPL(hw_dvt_hypervisor_dma_pool_uninit);


/**
 * hw_dvt_hypervisor_dma_map_guest_page - setup dma map for guest page
 * @vdavinci: a vdavinci
 * @gfn: guest pfn
 * @size: page size
 * @dma_sgt: retrieve allocated dma addr list(sg_table)
 *
 * Returns:
 * 0 on success, negative error code if failed.
 */
int hw_dvt_hypervisor_dma_map_guest_page(
		void *__vdavinci, unsigned long gfn, unsigned long size,
		ka_sg_table_t **dma_sgt)
{
    if (hw_dvt_check_is_vm_mode() && get_vdavinci_virtual_ops()->vdavinci_hypervisor_dma_map_guest_page != NULL) {
        return get_vdavinci_virtual_ops()->vdavinci_hypervisor_dma_map_guest_page(__vdavinci, gfn, size, dma_sgt);
    }

    return 0;
}
EXPORT_SYMBOL_GPL(hw_dvt_hypervisor_dma_map_guest_page);


/**
 * hw_dvt_hypervisor_dma_unmap_guest_page - cancel dma map for guest page
 * @vdavinci: a vdavinci
 * @dma_sgt: the dma addr list(sg_table)
 */
void hw_dvt_hypervisor_dma_unmap_guest_page(
		void *__vdavinci, ka_sg_table_t *dma_sgt)
{
    if (hw_dvt_check_is_vm_mode() && get_vdavinci_virtual_ops()->vdavinci_hypervisor_dma_unmap_guest_page != NULL) {
        return get_vdavinci_virtual_ops()->vdavinci_hypervisor_dma_unmap_guest_page(__vdavinci, dma_sgt);
    }

    return;
}
EXPORT_SYMBOL_GPL(hw_dvt_hypervisor_dma_unmap_guest_page);

bool hw_dvt_hypervisor_dma_pool_active(void *__vdavinci)
{
    if (hw_dvt_check_is_vm_mode() && get_vdavinci_virtual_ops()->vdavinci_hypervisor_dma_pool_active != NULL) {
        return get_vdavinci_virtual_ops()->vdavinci_hypervisor_dma_pool_active(__vdavinci);
    }

    return true;
}
EXPORT_SYMBOL_GPL(hw_dvt_hypervisor_dma_pool_active);

int hw_dvt_hypervisor_dma_map_guest_page_batch(void *__vdavinci,
    unsigned long *gfn, unsigned long *dma_addr, unsigned long count)
{
    if (hw_dvt_check_is_vm_mode() && get_vdavinci_virtual_ops()->vdavinci_hypervisor_dma_map_guest_page_batch != NULL) {
        return get_vdavinci_virtual_ops()->vdavinci_hypervisor_dma_map_guest_page_batch(__vdavinci, gfn, dma_addr,
            count);
    }

    return 0;
}
EXPORT_SYMBOL_GPL(hw_dvt_hypervisor_dma_map_guest_page_batch);

void hw_dvt_hypervisor_dma_unmap_guest_page_batch(void *__vdavinci,
    unsigned long *gfn, unsigned long *dma_addr, unsigned long count)
{
    if (hw_dvt_check_is_vm_mode() &&
        get_vdavinci_virtual_ops()->vdavinci_hypervisor_dma_unmap_guest_page_batch != NULL) {
        return get_vdavinci_virtual_ops()->vdavinci_hypervisor_dma_unmap_guest_page_batch(__vdavinci, gfn, dma_addr,
            count);
    }
}
EXPORT_SYMBOL_GPL(hw_dvt_hypervisor_dma_unmap_guest_page_batch);

bool hw_dvt_hypervisor_is_valid_gfn(
		void *__vdavinci, unsigned long gfn)
{
    if (hw_dvt_check_is_vm_mode() && get_vdavinci_virtual_ops()->vdavinci_hypervisor_is_valid_gfn != NULL) {
        return get_vdavinci_virtual_ops()->vdavinci_hypervisor_is_valid_gfn(__vdavinci, gfn);
    }

    return 0;
}
EXPORT_SYMBOL_GPL(hw_dvt_hypervisor_is_valid_gfn);

int hw_dvt_hypervisor_mmio_get(void **dst, int *size, void *__vdavinci, int bar)
{
    if (hw_dvt_check_is_vm_mode() && get_vdavinci_virtual_ops()->vdavinci_hypervisor_mmio_get != NULL) {
        return get_vdavinci_virtual_ops()->vdavinci_hypervisor_mmio_get(dst, size, __vdavinci, bar);
    }

    return 0;
}
KA_EXPORT_SYMBOL(hw_dvt_hypervisor_mmio_get);

void *hw_dvt_hypervisor_dma_alloc_coherent(ka_device_t *dev, size_t size, ka_dma_addr_t *dma_handle, ka_gfp_t gfp)
{
    if (hw_dvt_check_is_vm_mode() && get_vdavinci_virtual_ops()->vdavinci_hypervisor_dma_alloc_coherent != NULL) {
        return get_vdavinci_virtual_ops()->vdavinci_hypervisor_dma_alloc_coherent(dev, size, dma_handle, gfp);
    }

    return NULL;
}
EXPORT_SYMBOL_GPL(hw_dvt_hypervisor_dma_alloc_coherent);

void hw_dvt_hypervisor_dma_free_coherent(ka_device_t *dev, size_t size, void *cpu_addr, ka_dma_addr_t dma_handle)
{
    if (hw_dvt_check_is_vm_mode() && get_vdavinci_virtual_ops()->vdavinci_hypervisor_dma_free_coherent != NULL) {
        return get_vdavinci_virtual_ops()->vdavinci_hypervisor_dma_free_coherent(dev, size, cpu_addr, dma_handle);
    }
}
KA_EXPORT_SYMBOL(hw_dvt_hypervisor_dma_free_coherent);

ka_dma_addr_t hw_dvt_hypervisor_dma_map_single(ka_device_t *dev, void *ptr, size_t size, ka_dma_data_direction_t dir)
{
    if (hw_dvt_check_is_vm_mode() && get_vdavinci_virtual_ops()->vdavinci_hypervisor_dma_map_single != NULL) {
        return get_vdavinci_virtual_ops()->vdavinci_hypervisor_dma_map_single(dev, ptr, size, dir);
    }

    return (~(ka_dma_addr_t)0);
}
EXPORT_SYMBOL_GPL(hw_dvt_hypervisor_dma_map_single);

void hw_dvt_hypervisor_dma_unmap_single(ka_device_t *dev, ka_dma_addr_t addr, size_t size, ka_dma_data_direction_t dir)
{
    if (hw_dvt_check_is_vm_mode() && get_vdavinci_virtual_ops()->vdavinci_hypervisor_dma_unmap_single != NULL) {
        return get_vdavinci_virtual_ops()->vdavinci_hypervisor_dma_unmap_single(dev, addr, size, dir);
    }
}
KA_EXPORT_SYMBOL(hw_dvt_hypervisor_dma_unmap_single);

ka_dma_addr_t hw_dvt_hypervisor_dma_map_page(ka_device_t *dev, ka_page_t *page, size_t offset,
                                          size_t size, ka_dma_data_direction_t dir)
{
    if (hw_dvt_check_is_vm_mode() && get_vdavinci_virtual_ops()->vdavinci_hypervisor_dma_map_page != NULL) {
        return get_vdavinci_virtual_ops()->vdavinci_hypervisor_dma_map_page(dev, page, offset, size, dir);
    }
    return (~(ka_dma_addr_t)0);
}
KA_EXPORT_SYMBOL(hw_dvt_hypervisor_dma_map_page);
void hw_dvt_hypervisor_dma_unmap_page(ka_device_t *dev, ka_dma_addr_t addr, size_t size, ka_dma_data_direction_t dir)
{
    if (hw_dvt_check_is_vm_mode() && get_vdavinci_virtual_ops()->vdavinci_hypervisor_dma_unmap_page != NULL) {
        return get_vdavinci_virtual_ops()->vdavinci_hypervisor_dma_unmap_page(dev, addr, size, dir);
    }
}
KA_EXPORT_SYMBOL(hw_dvt_hypervisor_dma_unmap_page);

int hw_dvt_hypervisor_get_reserve_iova(ka_device_t *dev, ka_dma_addr_t *iova_addr, size_t *size)
{
    if (hw_dvt_check_is_vm_mode() && get_vdavinci_virtual_ops()->vdavinci_get_reserve_iova_for_check != NULL) {
        return get_vdavinci_virtual_ops()->vdavinci_get_reserve_iova_for_check(dev, iova_addr, size);
    }
    return 0;
}
KA_EXPORT_SYMBOL(hw_dvt_hypervisor_get_reserve_iova);
