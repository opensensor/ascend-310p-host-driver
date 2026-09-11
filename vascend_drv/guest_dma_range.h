/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef VDAVINCI_GUEST_DMA_RANGE_H
#define VDAVINCI_GUEST_DMA_RANGE_H

#include <linux/mm.h>
#include <linux/types.h>

/* Use an inclusive last byte so a full-address-space invalidation cannot wrap.
 * A sub-page invalidation retires every page it intersects.
 */
static inline bool vdavinci_ram_overlaps_iova(unsigned long base_gfn,
                                              unsigned long npages,
                                              u64 iova, u64 length)
{
    u64 first_gfn, last_gfn, last_byte;

    if (!npages || !length)
        return false;
    last_byte = length - 1 > ~(u64)0 - iova ? ~(u64)0 : iova + length - 1;
    first_gfn = iova >> PAGE_SHIFT;
    last_gfn = last_byte >> PAGE_SHIFT;
    if (base_gfn > last_gfn)
        return false;
    return first_gfn <= base_gfn || first_gfn - base_gfn < npages;
}

#endif /* VDAVINCI_GUEST_DMA_RANGE_H */
