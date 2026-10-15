/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef DEVDRV_DMA_PROFILE_H
#define DEVDRV_DMA_PROFILE_H

#include <linux/ktime.h>
#include <linux/string.h>
#include <linux/types.h>

/* Host software timestamps, not device-engine timestamps. No DMA addresses. */
struct devdrv_dma_profile {
    u64 start_ns;
    u64 submit_ns;
    u64 wait_ns;
    u64 complete_ns;
    u64 end_ns;
    u64 sleep_ns;
    u64 bytes;
    u32 sleeps;
    u32 queries;
    u32 spin_budget_us;
    u32 spin_queries;
    u32 spin_completed;
    u64 spin_ns;
};

static inline void devdrv_dma_profile_begin(struct devdrv_dma_profile *profile)
{
    memset(profile, 0, sizeof(*profile));
    profile->start_ns = ktime_get_ns();
}

static inline void devdrv_dma_profile_complete(struct devdrv_dma_profile *profile)
{
    if (profile != NULL)
        profile->complete_ns = ktime_get_ns();
}

static inline u64 devdrv_dma_profile_sleep_begin(struct devdrv_dma_profile *profile)
{
    if (profile == NULL)
        return 0;
    profile->sleeps++;
    return ktime_get_ns();
}

static inline void devdrv_dma_profile_sleep_end(struct devdrv_dma_profile *profile, u64 start)
{
    if (profile != NULL)
        profile->sleep_ns += ktime_get_ns() - start;
}

#endif
