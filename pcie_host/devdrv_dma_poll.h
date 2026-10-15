/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef DEVDRV_DMA_POLL_H
#define DEVDRV_DMA_POLL_H

#define DEVDRV_DMA_QUERY_SPIN_MAX_US 20U
#define DEVDRV_DMA_QUERY_SPIN_MAX_BYTES 8192U

/* Runtime setting is sampled once per request. No change to IRQ/async work. */
static inline unsigned int devdrv_dma_query_spin_budget(unsigned int configured,
    int copy_type, int wait_type, unsigned long long bytes)
{
    if (copy_type != DEVDRV_DMA_SYNC || wait_type != DEVDRV_DMA_WAIT_QUREY ||
        bytes == 0 || bytes > DEVDRV_DMA_QUERY_SPIN_MAX_BYTES)
        return 0;
    return configured;
}

#endif
