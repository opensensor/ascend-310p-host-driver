/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef DEVDRV_DEVICE_DMA_H
#define DEVDRV_DEVICE_DMA_H

struct devdrv_dma_dev;
int devdrv_device_dma_init(void);
void devdrv_device_dma_uninit(void);
void devdrv_device_dma_attach(struct devdrv_dma_dev *dma_dev);
void devdrv_device_dma_detach(struct devdrv_dma_dev *dma_dev);
int devdrv_device_dma_suspend(struct devdrv_dma_dev *dma_dev);

#endif
