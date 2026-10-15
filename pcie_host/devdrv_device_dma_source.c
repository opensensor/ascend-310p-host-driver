// SPDX-License-Identifier: GPL-2.0-only
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "ascend_device_dma.h"
#include "devdrv_device_dma_source.h"

/* Bound each GUP operation; registration is cold setup, never a token path. */
#define DEVICE_DMA_PIN_BATCH_PAGES 65536U

struct devdrv_device_dma_source {
    struct device *dev;
    struct mm_struct *mm;
    struct page **pages;
    struct sg_table table;
    struct ascend_device_dma_source info;
    unsigned long pinned;
    bool charged;
    bool table_allocated;
    bool mapped;
};

void devdrv_device_dma_source_destroy(struct devdrv_device_dma_source *source)
{
    if (source == NULL) {
        return;
    }
    if (source->mapped) {
        dma_unmap_sgtable(source->dev, &source->table, DMA_TO_DEVICE, 0);
    }
    if (source->table_allocated) {
        sg_free_table(&source->table);
    }
    if (source->pinned) {
        /* Device reads only: these pages must not be marked dirty. */
        unpin_user_pages(source->pages, source->pinned);
    }
    if (source->charged) {
        account_locked_vm(source->mm, source->info.page_count, false);
    }
    if (source->mm != NULL) {
        mmdrop(source->mm);
    }
    if (source->dev != NULL) {
        put_device(source->dev);
    }
    kvfree(source->pages);
    kfree(source);
}

int devdrv_device_dma_source_create(struct device *dev,
    const struct ascend_device_dma_source *request,
    struct devdrv_device_dma_source **result)
{
    struct devdrv_device_dma_source *source;
    unsigned long start, count, offset;
    int ret;

    *result = NULL;
    if (!request->address || !request->length ||
        request->length > ASCEND_DEVICE_DMA_MAX_SOURCE_BYTES ||
        request->address > ULONG_MAX || request->length > ULONG_MAX ||
        request->length > ULONG_MAX - request->address || current->mm == NULL) {
        return -EINVAL;
    }
    if (!access_ok((void __user *)(unsigned long)request->address, request->length)) {
        return -EFAULT;
    }
    start = (unsigned long)request->address & PAGE_MASK;
    offset = offset_in_page(request->address);
    count = DIV_ROUND_UP(offset + request->length, PAGE_SIZE);
    if (!count || count > U32_MAX) {
        return -EOVERFLOW;
    }
    source = kzalloc(sizeof(*source), GFP_KERNEL);
    if (source == NULL) {
        return -ENOMEM;
    }
    source->info = *request;
    source->info.page_count = count;
    source->dev = get_device(dev);
    source->mm = current->mm;
    mmgrab(source->mm);
    ret = account_locked_vm(source->mm, count, true);
    if (ret != 0) {
        goto fail;
    }
    source->charged = true;
    source->pages = kvmalloc_array(count, sizeof(*source->pages), GFP_KERNEL);
    if (source->pages == NULL) {
        ret = -ENOMEM;
        goto fail;
    }
    while (source->pinned < count) {
        unsigned long batch = min_t(unsigned long, count - source->pinned, DEVICE_DMA_PIN_BATCH_PAGES);
        long pinned = pin_user_pages_fast(start + source->pinned * PAGE_SIZE, batch,
            FOLL_LONGTERM, source->pages + source->pinned);

        if (pinned > 0) {
            source->pinned += pinned;
        }
        if (pinned != (long)batch) {
            ret = pinned < 0 ? (int)pinned : -EFAULT;
            goto fail;
        }
        cond_resched();
    }
    /* Preserve the exact byte range, including partial first/last pages. */
    ret = sg_alloc_table_from_pages(&source->table, source->pages, count, offset,
        request->length, GFP_KERNEL);
    if (ret != 0) {
        goto fail;
    }
    source->table_allocated = true;
    ret = dma_map_sgtable(source->dev, &source->table, DMA_TO_DEVICE, 0);
    if (ret != 0) {
        goto fail;
    }
    source->mapped = true;
    source->info.segment_count = source->table.nents;
    *result = source;
    return 0;
fail:
    devdrv_device_dma_source_destroy(source);
    return ret;
}

void devdrv_device_dma_source_describe(const struct devdrv_device_dma_source *source,
    struct ascend_device_dma_source *reply)
{
    *reply = source->info;
}
