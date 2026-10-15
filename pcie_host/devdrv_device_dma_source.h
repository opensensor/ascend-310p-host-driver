/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef DEVDRV_DEVICE_DMA_SOURCE_H
#define DEVDRV_DEVICE_DMA_SOURCE_H

#include <linux/types.h>
struct device;
struct devdrv_device_dma_source;
struct ascend_device_dma_source;

int devdrv_device_dma_source_create(struct device *dev,
    const struct ascend_device_dma_source *request,
    struct devdrv_device_dma_source **result);
void devdrv_device_dma_source_destroy(struct devdrv_device_dma_source *source);
void devdrv_device_dma_source_describe(const struct devdrv_device_dma_source *source,
    struct ascend_device_dma_source *reply);

#endif
