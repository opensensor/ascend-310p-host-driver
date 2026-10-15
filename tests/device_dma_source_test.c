// SPDX-License-Identifier: GPL-2.0-only
/* Compile the actual registration implementation against deterministic kernel
 * service mocks. Verify resource accounting at every failure boundary.
 */
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "ascend_device_dma.h"

#define __user
#define GFP_KERNEL 0
#define PAGE_SIZE 4096UL
#define PAGE_MASK (~(PAGE_SIZE - 1))
#define offset_in_page(x) ((x) & (PAGE_SIZE - 1))
#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))
#define U32_MAX UINT32_MAX
#define FOLL_LONGTERM 4
#define DMA_TO_DEVICE 1
#define min_t(type, a, b) ((type)(a) < (type)(b) ? (type)(a) : (type)(b))
struct device { int references; };
struct mm_struct { int references; unsigned long locked; };
struct page { int unused; };
struct sg_table { unsigned int nents; };
struct task_struct { struct mm_struct *mm; };
static struct task_struct task;
#define current (&task)
static int fail_allocation, allocations, allocation_calls, fail_charge, fail_sg, fail_map;
static int tables, mappings, pin_calls, pin_fail_call, short_pin, fail_access;
static unsigned long pinned_pages, sg_offset, sg_length, next_pin_address;
static void *kzalloc(size_t bytes, int flags)
{
    (void)flags;
    if (++allocation_calls == fail_allocation) return NULL;
    void *p = calloc(1, bytes); assert(p); allocations++; return p;
}
static void *kvmalloc_array(size_t count, size_t size, int flags)
{ return kzalloc(count * size, flags); }
static void kfree(void *p) { if (p) { allocations--; free(p); } }
#define kvfree kfree
static struct device *get_device(struct device *dev) { dev->references++; return dev; }
static void put_device(struct device *dev) { assert(dev->references > 0); dev->references--; }
static void mmgrab(struct mm_struct *mm) { mm->references++; }
static void mmdrop(struct mm_struct *mm) { assert(mm->references > 0); mm->references--; }
static int account_locked_vm(struct mm_struct *mm, unsigned long pages, bool inc)
{
    if (inc && fail_charge) return -ENOMEM;
    if (inc) mm->locked += pages;
    else { assert(mm->locked >= pages); mm->locked -= pages; }
    return 0;
}
static int access_ok(const void *address, unsigned long bytes)
{ (void)address; (void)bytes; return !fail_access; }
static long pin_user_pages_fast(unsigned long address, int count, int flags, struct page **pages)
{
    assert(flags == FOLL_LONGTERM && count > 0 && count <= 65536);
    (void)pages;
    if (pin_calls) assert(address == next_pin_address);
    next_pin_address = address + count * PAGE_SIZE;
    pin_calls++;
    if (pin_calls == pin_fail_call) {
        if (!short_pin) return -EFAULT;
        pinned_pages += count - 1;
        return count - 1;
    }
    pinned_pages += count;
    return count;
}
static void unpin_user_pages(struct page **pages, unsigned long count)
{
    assert(pages && !mappings && !tables && pinned_pages >= count);
    pinned_pages -= count;
}
static void cond_resched(void) {}
static int sg_alloc_table_from_pages(struct sg_table *table, struct page **pages,
    unsigned int count, unsigned long offset, unsigned long bytes, int flags)
{
    (void)flags;
    assert(pages && count == pinned_pages && offset < PAGE_SIZE);
    assert(count == DIV_ROUND_UP(offset + bytes, PAGE_SIZE));
    sg_offset = offset; sg_length = bytes;
    if (fail_sg) return -ENOMEM;
    table->nents = count; tables++; return 0;
}
static void sg_free_table(struct sg_table *table)
{ (void)table; assert(!mappings && tables == 1); tables--; }
static int dma_map_sgtable(struct device *dev, struct sg_table *table, int direction, int attrs)
{
    assert(dev->references && table->nents && direction == DMA_TO_DEVICE && !attrs);
    if (fail_map) return -EIO;
    mappings++; table->nents = 1; return 0;
}
static void dma_unmap_sgtable(struct device *dev, struct sg_table *table, int direction, int attrs)
{
    assert(dev->references && table->nents && direction == DMA_TO_DEVICE && !attrs);
    assert(mappings == 1 && tables == 1 && pinned_pages); mappings--;
}
#include "device_dma_source_test.inc"

static void clean(struct device *dev, struct mm_struct *mm)
{
    assert(!dev->references && !mm->references && !mm->locked);
    assert(!pinned_pages && !mappings && !tables && !allocations);
}
static void reset(void)
{
    fail_allocation = allocation_calls = fail_charge = fail_sg = fail_map = 0;
    pin_calls = pin_fail_call = short_pin = fail_access = 0;
}
int main(void)
{
    struct device dev = {0};
    struct mm_struct mm = {0};
    struct ascend_device_dma_source request = {.version = 1, .generation = 7,
        .region_id = 3, .address = 4090, .length = 320}, reply;
    struct devdrv_device_dma_source *source;
    task.mm = &mm;
    assert(devdrv_device_dma_source_create(&dev, &request, &source) == 0);
    assert(source && mm.locked == 2 && pinned_pages == 2 && mappings == 1);
    assert(sg_offset == 4090 && sg_length == 320);
    devdrv_device_dma_source_describe(source, &reply);
    assert(reply.address == 4090 && reply.length == 320 && reply.page_count == 2);
    assert(reply.segment_count == 1 && reply.region_id == 3 && reply.generation == 7);
    devdrv_device_dma_source_destroy(source); clean(&dev, &mm);
    /* Every allocation/charge/pin/map failure must unwind exactly once. */
    for (int failure = 0; failure < 8; ++failure) {
        reset();
        if (failure < 2) fail_allocation = failure + 1;
        if (failure == 2) fail_charge = 1;
        if (failure == 3) pin_fail_call = 1;
        if (failure == 4) { pin_fail_call = 1; short_pin = 1; }
        if (failure == 5) fail_sg = 1;
        if (failure == 6) fail_map = 1;
        if (failure == 7) fail_access = 1;
        source = (void *)1;
        assert(devdrv_device_dma_source_create(&dev, &request, &source) < 0);
        assert(source == NULL); clean(&dev, &mm);
    }
    reset();
    request.address = ULONG_MAX - 1; request.length = 4;
    assert(devdrv_device_dma_source_create(&dev, &request, &source) == -EINVAL);
    request.address = 4096; request.length = 0;
    assert(devdrv_device_dma_source_create(&dev, &request, &source) == -EINVAL);
    request.length = ASCEND_DEVICE_DMA_MAX_SOURCE_BYTES + 1;
    assert(devdrv_device_dma_source_create(&dev, &request, &source) == -EINVAL);
    request.length = (65536UL + 2) * PAGE_SIZE;
    for (int partial = 0; partial < 2; ++partial) {
        reset(); pin_fail_call = 2; short_pin = partial;
        assert(devdrv_device_dma_source_create(&dev, &request, &source) == -EFAULT);
        assert(pin_calls == 2); clean(&dev, &mm);
    }
    reset();
    assert(devdrv_device_dma_source_create(&dev, &request, &source) == 0);
    assert(pin_calls == 2 && pinned_pages == 65538);
    devdrv_device_dma_source_destroy(source); clean(&dev, &mm);
    task.mm = NULL;
    assert(devdrv_device_dma_source_create(&dev, &request, &source) == -EINVAL);
    devdrv_device_dma_source_destroy(NULL);
    clean(&dev, &mm);
    return 0;
}
