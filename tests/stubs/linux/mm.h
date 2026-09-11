/* SPDX-License-Identifier: GPL-2.0-only */
#include <errno.h>

#define VM_IO 1UL
#define VM_PFNMAP 2UL
struct vm_area_struct { unsigned long vm_flags; };
struct follow_pfnmap_args {
    struct vm_area_struct *vma;
    unsigned long address;
    unsigned long pfn;
};
int follow_pfnmap_start(struct follow_pfnmap_args *args);
void follow_pfnmap_end(struct follow_pfnmap_args *args);
