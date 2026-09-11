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

#include <linux/iommu.h>
#include "dvt.h"
#include "domain_manage.h"
#include "dma_pool.h"
#include "vfio_ops.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,0)) && (LINUX_VERSION_CODE < KERNEL_VERSION(6,1,0))
#include <linux/dma-iommu.h>
#endif

void hw_vdavinci_iommu_detach_group(struct hw_vdavinci *vdavinci)
{
    struct hw_vf_info *vf = &vdavinci->vf;
    struct iommu_group *group;
    struct device *dev = vdavinci_resource_dev(vdavinci);

    if (!vf->domain) {
        return;
    }
    group = iommu_group_get(dev);
    if (!group) {
        return;
    }
    iommu_detach_group(vf->domain, group);
    iommu_group_put(group);
    iommu_domain_free(vf->domain);
    vf->domain = NULL;
    put_iova_domain(&vf->iovad);
}

STATIC bool vfio_iommu_has_sw_msi(struct iommu_group *group, phys_addr_t *base)
{
    bool ret = false;
#if IS_VDAVINCI_KERNEL_VERSION_SUPPORT
    struct list_head group_resv_regions;
    struct iommu_resv_region *region, *next;

    INIT_LIST_HEAD(&group_resv_regions);
    iommu_get_group_resv_regions(group, &group_resv_regions);
    list_for_each_entry(region, &group_resv_regions, list) {
        /*
         * The presence of any 'real' MSI regions should take
         * precedence over the software-managed one if the
         * IOMMU driver happens to advertise both types.
         */
        if (region->type == IOMMU_RESV_MSI) {
            ret = false;
            break;
        }

        if (region->type == IOMMU_RESV_SW_MSI) {
            *base = region->start;
            ret = true;
        }
    }

    list_for_each_entry_safe(region, next, &group_resv_regions, list) {
        kfree(region);
    }
#endif
    return ret;
}

STATIC void init_vf_iovad(struct hw_vdavinci *vdavinci)
{
    struct vm_dom_info *vm_dom = vdavinci->vdev.domain;
    struct hw_vf_info *vf = &vdavinci->vf;

    vdavinci_init_iova_domain(&vf->iovad);
    vdavinci_copy_reserved_iova(&vm_dom->iovad, &vf->iovad);
}

int hw_vdavinci_iommu_attach_group(struct hw_vdavinci *vdavinci)
{
    struct hw_vf_info *vf = &vdavinci->vf;
    struct iommu_group *group;
    bool resv_msi = false;
    phys_addr_t resv_msi_base = 0;
    struct device *dev = vdavinci_resource_dev(vdavinci);
    int ret;

    if (vf->domain) {
        vascend_err(dev, "domain exist.");
        return -EEXIST;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(7, 0, 0)
    vf->domain = iommu_paging_domain_alloc(dev);
    if (IS_ERR(vf->domain)) {
        ret = PTR_ERR(vf->domain);
        vf->domain = NULL;
        vascend_err(dev, "Failed to allocate iommu domain: %d\n", ret);
        return ret;
    }
#else
    vf->domain = iommu_domain_alloc(&pci_bus_type);
    if (!vf->domain) {
        vascend_err(dev, "Failed to allocate iommu domain.");
        return -EIO;
    }
#endif

    group = iommu_group_get(dev);
    if (!group) {
        vascend_err(dev, "Failed to get iommu group.");
        ret = -ENODEV;
        goto out_domain;
    }

    ret = iommu_attach_group(vf->domain, group);
    if (ret) {
        vascend_err(dev, "Failed to attach group.");
        goto out_put_group;
    }

    resv_msi = vfio_iommu_has_sw_msi(group, &resv_msi_base);
    if (resv_msi) {
#if IS_VDAVINCI_KERNEL_VERSION_SUPPORT
        ret = iommu_get_msi_cookie(vf->domain, resv_msi_base);
#endif
        if (ret) {
            vascend_err(dev, "Failed to allocate msi cookie.");
            goto out_group;
        }
    }

    init_vf_iovad(vdavinci);
    iommu_group_put(group);
    return 0;

out_group:
    iommu_detach_group(vf->domain, group);
out_put_group:
    iommu_group_put(group);
out_domain:
    iommu_domain_free(vf->domain);
    vf->domain = NULL;
    return ret;
}

STATIC unsigned long aligned_nrpages(unsigned long addr, size_t size)
{
    unsigned long page_addr = addr;

    page_addr &= ~PAGE_MASK;
    return PAGE_ALIGN(page_addr + size) >> PAGE_SHIFT;
}

STATIC dma_addr_t vdavinci_do_map(struct hw_vdavinci *vdavinci, phys_addr_t paddr,
                                  size_t size, int dir, u64 dma_mask)
{
    dma_addr_t iova_addr = 0, start_paddr = 0;
    int ret = 0, prot = 0;
    unsigned long iova_pfn = 0, paddr_pfn = paddr >> PAGE_SHIFT;
    struct iova *new_iova = NULL;
    struct hw_vf_info *vf = &vdavinci->vf;
    unsigned long nr_pages = aligned_nrpages(paddr, size);
    struct device *dev = vdavinci_resource_dev(vdavinci);

    new_iova = alloc_iova(&vf->iovad, nr_pages,
                          dma_mask >> PAGE_SHIFT, true);
    if (new_iova == NULL) {
        vascend_err(vdavinci_to_dev(vdavinci), "alloc iova failed");
        goto error;
    }

    iova_pfn = new_iova->pfn_lo;
    iova_addr = iova_pfn << iova_shift(&vf->iovad);
    if (dir == DMA_TO_DEVICE || dir == DMA_BIDIRECTIONAL) {
        prot |= IOMMU_READ;
    }

    if (dir == DMA_FROM_DEVICE || dir == DMA_BIDIRECTIONAL) {
        prot |= IOMMU_WRITE;
    }

    ret = vdavinci_iommu_map(dev, iova_pfn << PAGE_SHIFT,
                             paddr_pfn << PAGE_SHIFT,
                             PAGE_ALIGN(size), prot);
    if (ret != 0) {
        vascend_err(vdavinci_to_dev(vdavinci), "iommu map failed %d", ret);
        goto error;
    }

    start_paddr = (phys_addr_t)iova_pfn << PAGE_SHIFT;
    start_paddr += paddr & ~PAGE_MASK;

    return start_paddr;
error:
    if (iova_pfn != 0) {
        free_iova(&vf->iovad, iova_pfn);
    }

    return DMA_MAPPING_ERROR;
}

STATIC dma_addr_t vdavinci_do_map_single(struct device *dev, phys_addr_t paddr,
                                         size_t size, int dir, u64 dma_mask)
{
    struct hw_vdavinci *vdavinci = find_vdavinci(dev);
    struct vm_dom_info *vm_dom = NULL;
    struct dev_dom_info *dev_dom = NULL;
    phys_addr_t start_paddr;

    if (!vdavinci || !vdavinci->is_passthrough) {
        vascend_err(dev, "not passthrough, domain not switch\n");
        return DMA_MAPPING_ERROR;
    }

    mutex_lock(&vdavinci->vdev.cache_lock);
    vm_dom = vdavinci->vdev.domain;
    if (!vm_dom || vm_dom->status != DOMAIN_PIN_STATUS_READY) {
        mutex_unlock(&vdavinci->vdev.cache_lock);
        vascend_err(dev, "dma pool status invalid\n");
        return DMA_MAPPING_ERROR;
    }

    down_read(&vm_dom->sem);
    mutex_unlock(&vdavinci->vdev.cache_lock);

    dev_dom = dev_dom_info_find(vm_dom, vdavinci);
    if (!dev_dom || dev_dom->status != DOMAIN_MAP_STATUS_READY) {
        vascend_err(dev, "dma pool not ready, not map\n");
        goto error;
    }

    start_paddr = vdavinci_do_map(vdavinci, paddr, size, dir, dma_mask);
    if (start_paddr == DMA_MAPPING_ERROR) {
        goto error;
    }

    up_read(&vm_dom->sem);
    return start_paddr;

error:
    up_read(&vm_dom->sem);

    vascend_err(dev, "Device request: %zx@%llx dir %d --- failed\n",
                size, (unsigned long long)paddr, dir);
    return DMA_MAPPING_ERROR;
}

dma_addr_t vdavinci_dma_map_page(struct device *dev, struct page *page, size_t offset,
                                 size_t size, enum dma_data_direction dir)
{
    if (!valid_dma_direction(dir)) {
        vascend_err(dev, "invalid dma direction %d\n", dir);
        return DMA_MAPPING_ERROR;
    }

    if (offset != 0) {
        vascend_err(dev, "offset should be 0 %zx\n", offset);
        return DMA_MAPPING_ERROR;
    }

    return vdavinci_do_map_single(dev, page_to_phys(page) + offset, size, dir, *dev->dma_mask);
}

dma_addr_t vdavinci_dma_map_single(struct device *dev, void *ptr, size_t size,
                                   enum dma_data_direction dir)
{
    struct page *page;
    size_t offset;

    if (is_vmalloc_addr(ptr)) {
        vascend_err(dev, "rejecting DMA map of vmalloc memory\n");
        return DMA_MAPPING_ERROR;
    }

    if (!valid_dma_direction(dir)) {
        vascend_err(dev, "invalid dma direction %d\n", dir);
        return DMA_MAPPING_ERROR;
    }

    page = virt_to_page(ptr);
    offset = offset_in_page(ptr);
    if (offset != 0) {
        vascend_err(dev, "address should PAGE align, %zx\n", offset);
    }

    return vdavinci_do_map_single(dev, page_to_phys(page) + offset, size, dir, *dev->dma_mask);
}

STATIC void vdavinci_do_unmap(struct device *dev, dma_addr_t dev_addr, size_t size)
{
    struct hw_vdavinci *vdavinci = find_vdavinci(dev);
    struct hw_vf_info *vf = NULL;
    unsigned long iova_pfn = dev_addr >> PAGE_SHIFT;

    if (!vdavinci || !vdavinci->is_passthrough) {
        vascend_err(dev, "unmap not passthrough\n");
        return;
    }

    vf = &vdavinci->vf;
    vdavinci_iommu_unmap(dev, iova_pfn << PAGE_SHIFT, PAGE_ALIGN(size));
    free_iova(&vf->iovad, iova_pfn);
}

void vdavinci_dma_unmap_single(struct device *dev, dma_addr_t addr, size_t size,
                               enum dma_data_direction dir)
{
    vdavinci_do_unmap(dev, addr, size);
}

void vdavinci_dma_unmap_page(struct device *dev, dma_addr_t addr, size_t size,
                             enum dma_data_direction dir)
{
    vdavinci_do_unmap(dev, addr, size);
}

void *vdavinci_dma_alloc_coherent(struct device *dev, size_t size,
                                  dma_addr_t *dma_handle, gfp_t flags)
{
    struct page *page = NULL;
    size_t align_size = PAGE_ALIGN(size);
    int order = get_order(align_size);

    page = alloc_pages(flags, order);
    if (!page) {
        vascend_debug("dev alloc page failed\n");
        return NULL;
    }

    *dma_handle = vdavinci_do_map_single(dev, page_to_phys(page),
                                         align_size, DMA_BIDIRECTIONAL,
                                         dev->coherent_dma_mask);
    if (*dma_handle != DMA_MAPPING_ERROR) {
        return page_address(page);
    }

    __free_pages(page, order);
    return NULL;
}

void vdavinci_dma_free_coherent(struct device *dev, size_t size,
                                void *vaddr, dma_addr_t dma_handle)
{
    size_t align_size = PAGE_ALIGN(size);
    int order = get_order(align_size);
    struct page *page = virt_to_page(vaddr);

    vdavinci_do_unmap(dev, dma_handle, align_size);
    __free_pages(page, order);
}
