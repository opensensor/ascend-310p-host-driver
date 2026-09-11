/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef TEST_LINUX_MM_H
#define TEST_LINUX_MM_H
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>

#define PAGE_SHIFT 12
#define PAGE_SIZE (1UL << PAGE_SHIFT)
struct page { unsigned long pfn; };
static inline unsigned long page_to_pfn(const struct page *page) { return page->pfn; }

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
#endif
