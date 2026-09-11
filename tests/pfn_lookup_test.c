/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include "ka_pfn_lookup.h"

static int start_result, starts, ends;

int follow_pfnmap_start(struct follow_pfnmap_args *args)
{
    ++starts;
    assert(args->address == 0x123000);
    args->pfn = 0x456;
    return start_result;
}

void follow_pfnmap_end(struct follow_pfnmap_args *args)
{
    ++ends;
    args->pfn = 0; /* Outputs must have been copied before releasing the lock. */
}

int main(void)
{
    struct vm_area_struct vma = { 0 };
    unsigned long pfn = 0x789;

    assert(ka_mm_follow_pfn(&vma, 0x123000, &pfn) == -EINVAL);
    assert(starts == 0 && ends == 0 && pfn == 0x789);
    vma.vm_flags = VM_PFNMAP;
    start_result = -EFAULT;
    assert(ka_mm_follow_pfn(&vma, 0x123000, &pfn) == -EFAULT);
    assert(starts == 1 && ends == 0 && pfn == 0x789);
    start_result = 0;
    assert(ka_mm_follow_pfn(&vma, 0x123000, &pfn) == 0);
    assert(starts == 2 && ends == 1 && pfn == 0x456);
    vma.vm_flags = VM_IO;
    assert(ka_mm_follow_pfn(&vma, 0x123000, &pfn) == 0);
    assert(starts == 3 && ends == 2 && pfn == 0x456);
    return 0;
}
