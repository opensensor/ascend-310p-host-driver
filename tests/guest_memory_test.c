/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include "guest_memory.h"
#include "guest_dma_range.h"

static struct vfio_device device = { .id = 1 };
static struct page host_page = { .pfn = 1234 };
static int pin_result, pin_calls, unpin_calls, owned;
static dma_addr_t expected_iova;

int vfio_pin_pages(struct vfio_device *dev, dma_addr_t iova, int count,
                  int prot, struct page **pages)
{
    assert(dev == &device);
    assert(iova == expected_iova);
    assert(prot == (IOMMU_READ | IOMMU_WRITE));
    assert(count > 0 && count <= VFIO_PIN_PAGES_MAX_ENTRIES);
    pin_calls++;
    owned = pin_result > 0 ? pin_result : 0;
    if (owned)
        pages[0] = &host_page;
    return pin_result;
}

void vfio_unpin_pages(struct vfio_device *dev, dma_addr_t iova, int count)
{
    assert(dev == &device);
    assert(iova == expected_iova);
    assert(count == owned);
    unpin_calls++;
    owned = 0;
    host_page.pfn = 0; /* Access after unpin would observe a different PFN. */
}

int main(void)
{
    struct page *pages[VFIO_PIN_PAGES_MAX_ENTRIES];
    const unsigned long max_gfn = ULONG_MAX >> PAGE_SHIFT;

    assert(vdavinci_pin_guest_pages(NULL, 0, 1, pages) == -EINVAL);
    assert(vdavinci_pin_guest_pages(&device, 0, 1, NULL) == -EINVAL);
    assert(vdavinci_pin_guest_pages(&device, 0, 0, pages) == -EINVAL);
    assert(vdavinci_pin_guest_pages(&device, 0, -1, pages) == -EINVAL);
    assert(vdavinci_pin_guest_pages(&device, 0, 513, pages) == -EINVAL);
    assert(vdavinci_pin_guest_pages(&device, max_gfn + 1, 1, pages) == -EOVERFLOW);
    assert(vdavinci_pin_guest_pages(&device, max_gfn, 2, pages) == -EOVERFLOW);
    assert(pin_calls == 0 && unpin_calls == 0);

    expected_iova = 10 * PAGE_SIZE;
    pin_result = 3;
    assert(vdavinci_pin_guest_pages(&device, 10, 3, pages) == 3);
    assert(owned == 3 && unpin_calls == 0);
    vfio_unpin_pages(&device, expected_iova, 3);

    pin_result = 2; /* Partial pin must roll back exactly those two pages. */
    assert(vdavinci_pin_guest_pages(&device, 10, 3, pages) == -EFAULT);
    assert(owned == 0 && unpin_calls == 2);
    pin_result = 0;
    assert(vdavinci_pin_guest_pages(&device, 10, 3, pages) == -EFAULT);
    pin_result = -EACCES;
    assert(vdavinci_pin_guest_pages(&device, 10, 3, pages) == -EACCES);
    assert(unpin_calls == 2);
    assert(vdavinci_lookup_guest_pfn(&device, 10) == ~0UL);
    assert(unpin_calls == 2);

    pin_result = 1;
    host_page.pfn = 1234;
    expected_iova = (dma_addr_t)max_gfn << PAGE_SHIFT;
    assert(vdavinci_lookup_guest_pfn(&device, max_gfn) == 1234);
    assert(owned == 0 && unpin_calls == 3);

    assert(!vdavinci_ram_overlaps_iova(10, 2, 10 * PAGE_SIZE, 0));
    assert(!vdavinci_ram_overlaps_iova(10, 0, 10 * PAGE_SIZE, PAGE_SIZE));
    assert(!vdavinci_ram_overlaps_iova(10, 2, 9 * PAGE_SIZE, PAGE_SIZE));
    assert(!vdavinci_ram_overlaps_iova(10, 2, 12 * PAGE_SIZE, PAGE_SIZE));
    assert(vdavinci_ram_overlaps_iova(10, 2, 10 * PAGE_SIZE - 1, 2));
    assert(vdavinci_ram_overlaps_iova(10, 2, 11 * PAGE_SIZE, 1));
    assert(vdavinci_ram_overlaps_iova(10, 2, 0, 20 * PAGE_SIZE));
    assert(vdavinci_ram_overlaps_iova(max_gfn, 1, 0, ~(u64)0));
    assert(vdavinci_ram_overlaps_iova(max_gfn, 1, ~(u64)0 - 1, 100));
    assert(!vdavinci_ram_overlaps_iova(0, 1, ~(u64)0 - 1, 100));
    return 0;
}
