/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef VDAVINCI_GUEST_MEMORY_H
#define VDAVINCI_GUEST_MEMORY_H

#include <linux/mm.h>
#include <linux/vfio.h>
#include <linux/iommu.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0)
/* A successful result owns exactly npage VFIO pins; errors own none. */
static inline int vdavinci_pin_guest_pages(struct vfio_device *device,
                                           unsigned long gfn, int npage,
                                           struct page **pages)
{
    dma_addr_t iova;
    int ret;

    if (!device || !pages || npage <= 0 || npage > VFIO_PIN_PAGES_MAX_ENTRIES)
        return -EINVAL;
    if (gfn > (ULONG_MAX >> PAGE_SHIFT) - (npage - 1))
        return -EOVERFLOW;

    iova = (dma_addr_t)gfn << PAGE_SHIFT;
    ret = vfio_pin_pages(device, iova, npage, IOMMU_READ | IOMMU_WRITE, pages);
    if (ret == npage)
        return ret;
    if (ret > 0)
        vfio_unpin_pages(device, iova, ret);
    return ret < 0 ? ret : -EFAULT;
}

static inline unsigned long vdavinci_lookup_guest_pfn(struct vfio_device *device,
                                                       unsigned long gfn)
{
    struct page *page = NULL;
    unsigned long pfn;

    if (vdavinci_pin_guest_pages(device, gfn, 1, &page) != 1)
        return ~0UL;

    pfn = page_to_pfn(page);
    vfio_unpin_pages(device, (dma_addr_t)gfn << PAGE_SHIFT, 1);
    /* This preserves the vendor's lookup-only ABI. The returned PFN does not
     * carry a pin: actual DMA must use the separately pinned DMA pool.
     */
    return pfn;
}
#endif

#endif /* VDAVINCI_GUEST_MEMORY_H */
