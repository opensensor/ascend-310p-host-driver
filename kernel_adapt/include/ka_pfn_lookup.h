/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef KA_PFN_LOOKUP_H
#define KA_PFN_LOOKUP_H

#include <linux/mm.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 13, 0)
static inline int ka_mm_follow_pfn(struct vm_area_struct *vma, unsigned long address,
                                  unsigned long *pfn)
{
    struct follow_pfnmap_args args = { .vma = vma, .address = address };
    int ret;

    /* Match follow_pfn(): the caller holds mmap_lock and owns the mapping.
     * This is an address lookup, not a page pin for DMA.
     */
    if (!(vma->vm_flags & (VM_IO | VM_PFNMAP)))
        return -EINVAL;
    ret = follow_pfnmap_start(&args);
    if (ret)
        return ret;
    *pfn = args.pfn;
    follow_pfnmap_end(&args);
    return 0;
}
#else
#define ka_mm_follow_pfn(ka_vma, address, pfn)  follow_pfn(ka_vma, address, pfn)
#endif

#endif /* KA_PFN_LOOKUP_H */
