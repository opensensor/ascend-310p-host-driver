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
#include "kvmdt.h"
#include "vfio_ops.h"

/**
 * hw_dvt_hypervisor_inject_msix - inject a MSIX interrupt into vdavinci
 *
 * Returns:
 * Zero on success, negative error code if failed.
 */
int hw_dvt_hypervisor_inject_msix(void *__vdavinci, u32 vector)
{
    struct hw_vdavinci *vdavinci = (struct hw_vdavinci *)__vdavinci;
    u16 command, control;
    int ret;

    if (vdavinci == NULL) {
        return -EINVAL;
    }

    command = *(u16 *)(&vdavinci_cfg_space(vdavinci)[PCI_COMMAND]);
	control = *(u16 *)(&vdavinci_cfg_space(vdavinci)[DAVINCI_PCI_MSIX_FLAGS]);

    if (!(command & PCI_COMMAND_INTX_DISABLE) ||
        !(control & PCI_MSIX_FLAGS_ENABLE) ||
        (control & PCI_MSIX_FLAGS_MASKALL)) {
        vascend_info(vdavinci->dvt->vdavinci_priv->dev,
                     "hw_dvt_hypervisor_inject_msix failed, msix cap flag did't support\n");
        return -EPERM;
    }

    ret = g_hw_kvmdt_ops.inject_msix(vdavinci->handle, vector);
    if (ret)
        return ret;

    return 0;
}

/**
 * hw_dvt_hypervisor_read_gpa - copy data from GPA to host data buffer
 * @vdavinci: a vdavinci
 * @gpa: guest physical addr
 * @buf: host data buffer
 * @len: data length
 *
 * Returns:
 * Zero on success, negative error code if failed.
 */
int hw_dvt_hypervisor_read_gpa(void *__vdavinci,
                               unsigned long gpa, void *buf, unsigned long len)
{
    struct hw_vdavinci *vdavinci = (struct hw_vdavinci *)__vdavinci;

    if (vdavinci == NULL || buf == NULL) {
        return -EINVAL;
    }
    return g_hw_kvmdt_ops.read_gpa(vdavinci->handle, gpa, buf, len);
}

/**
 * hw_dvt_hypervisor_write_gpa - copy data from host data buffer to GPA
 * @vdavinci: a vdavinci
 * @gpa: guest physical addr
 * @buf: host data buffer
 * @len: data length
 *
 * Returns:
 * Zero on success, negative error code if failed.
 */
int hw_dvt_hypervisor_write_gpa(void *__vdavinci,
                                unsigned long gpa, void *buf, unsigned long len)
{
    struct hw_vdavinci *vdavinci = (struct hw_vdavinci *)__vdavinci;

    if (vdavinci == NULL || buf == NULL) {
        return -EINVAL;
    }
    return g_hw_kvmdt_ops.write_gpa(vdavinci->handle, gpa, buf, len);
}

/**
 * hw_dvt_hypervisor_gfn_to_mfn - translate a GFN to MFN
 * @vdavinci: a vdavinci
 * @gpfn: guest pfn
 *
 * Returns:
 * MFN on success, hw_dvt_INVALID_ADDR if failed.
 */
unsigned long hw_dvt_hypervisor_gfn_to_mfn(void *__vdavinci, unsigned long gfn)
{
    struct hw_vdavinci *vdavinci = (struct hw_vdavinci *)__vdavinci;

    if (vdavinci == NULL) {
        return ~0;
    }
    return g_hw_kvmdt_ops.gfn_to_mfn(vdavinci->handle, gfn);
}

/**
 * hw_dvt_hypervisor_dma_pool_init - dma pool init
 * @vdavinci: a vdavinci
 *
 * Returns:
 * 0 on success, negative error code if failed.
 */
int hw_dvt_hypervisor_dma_pool_init(void *__vdavinci)
{
    struct hw_vdavinci *vdavinci = (struct hw_vdavinci *)__vdavinci;

    if (vdavinci == NULL) {
        return -EINVAL;
    }

    return g_hw_kvmdt_ops.dma_pool_init(vdavinci);
}

/**
 * hw_dvt_hypervisor_dma_pool_uninit - dma pool uninit
 * @vdavinci: a vdavinci
 */
void hw_dvt_hypervisor_dma_pool_uninit(void *__vdavinci)
{
    struct hw_vdavinci *vdavinci = (struct hw_vdavinci *)__vdavinci;

    if (vdavinci == NULL) {
        return;
    }
    g_hw_kvmdt_ops.dma_pool_uninit(vdavinci);
}

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
int hw_dvt_hypervisor_dma_map_guest_page(void *__vdavinci,
                                         unsigned long gfn, unsigned long size,
                                         struct sg_table **dma_sgt)
{
    struct hw_vdavinci *vdavinci = (struct hw_vdavinci *)__vdavinci;

    if (vdavinci == NULL || dma_sgt == NULL) {
        return -EINVAL;
    }

    return g_hw_kvmdt_ops.dma_get_iova(vdavinci, gfn, size, dma_sgt);
}

/**
 * hw_dvt_hypervisor_dma_unmap_guest_page - cancel dma map for guest page
 * @vdavinci: a vdavinci
 * @dma_sgt: the dma addr list(sg_table)
 */
void hw_dvt_hypervisor_dma_unmap_guest_page(void *__vdavinci,
                                            struct sg_table *dma_sgt)
{
    if (__vdavinci == NULL || dma_sgt == NULL) {
        pr_err("vdavinci or dma_sgt is null\n");
        return;
    }

    return g_hw_kvmdt_ops.dma_put_iova(dma_sgt);
}

/**
  * hw_dvt_hypervisor_dma_pool_active - dma pool active or not
  * @__vdavinci: a vdavinci
  *
  * Returns:
  * true on active, false on inactive.
  */
bool hw_dvt_hypervisor_dma_pool_active(void *__vdavinci)
{
    if (__vdavinci == NULL) {
        return false;
    }

    return true;
}

/**
  * hw_dvt_hypervisor_dma_map_guest_page_batch
  * - setup dma map for guest page when dma pool is active
  * @__vdavinci: a vdavinci
  * @gfn: guest pfn array
  * @dma_addr: retrieve dma addr array
  * @count: array size
  *
  * Returns:
  * 0 on success, negative error code if failed.
  */
int hw_dvt_hypervisor_dma_map_guest_page_batch(void *__vdavinci,
    unsigned long *gfn, unsigned long *dma_addr, unsigned long count)
{
    struct hw_vdavinci *vdavinci = (struct hw_vdavinci *)__vdavinci;

    if (vdavinci == NULL || gfn == NULL || dma_addr == NULL
            || count == 0) {
        return -EINVAL;
    }

    return g_hw_kvmdt_ops.dma_get_iova_batch(vdavinci, gfn, dma_addr, count);
}

/**
  * hw_dvt_hypervisor_dma_unmap_guest_page_batch
  * - cancel dma map for guest page when dma pool is active
  * @__vdavinci: a vdavinci
  * @gfn: guest pfn array
  * @dma_addr: retrieve dma addr array
  * @count: array size
  */
void hw_dvt_hypervisor_dma_unmap_guest_page_batch(void *__vdavinci, unsigned long *gfn,
    unsigned long *dma_addr, unsigned long count)
{
}

/**
 * hw_dvt_hypervisor_is_valid_gfn - check if a visible gfn
 * @vdavinci: a vdavinci
 * @gfn: guest PFN
 *
 * Returns:
 * true on valid gfn, false on not.
 */
bool hw_dvt_hypervisor_is_valid_gfn(void *__vdavinci, unsigned long gfn)
{
    struct hw_vdavinci *vdavinci = (struct hw_vdavinci *)__vdavinci;

    if (vdavinci == NULL) {
        return false;
    }
    if (g_hw_kvmdt_ops.is_valid_gfn == NULL) {
        return true;
    }

    return g_hw_kvmdt_ops.is_valid_gfn(vdavinci->handle, gfn);
}

int hw_dvt_hypervisor_mmio_get(void **dst, int *size, void *__vdavinci, int bar)
{
    if (g_hw_kvmdt_ops.mmio_get == NULL || __vdavinci == NULL ||
        dst == NULL || size == NULL) {
        return -EINVAL;
    }

    return g_hw_kvmdt_ops.mmio_get(dst, size, __vdavinci, bar);
}
