/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef TEST_LINUX_VFIO_H
#define TEST_LINUX_VFIO_H
#include <linux/mm.h>
#include <linux/types.h>
#define VFIO_PIN_PAGES_MAX_ENTRIES 512
struct vfio_device { int id; };
int vfio_pin_pages(struct vfio_device *, dma_addr_t, int, int, struct page **);
void vfio_unpin_pages(struct vfio_device *, dma_addr_t, int);
#endif
