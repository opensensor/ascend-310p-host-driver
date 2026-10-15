/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef ASCEND_DEVICE_DMA_UAPI_H
#define ASCEND_DEVICE_DMA_UAPI_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define ASCEND_DEVICE_DMA_ABI_VERSION 1
#define ASCEND_DEVICE_DMA_CAP_HOST_ROUTING_ISOLATED (1ULL << 0)
#define ASCEND_DEVICE_DMA_CAP_SOURCE_REGISTRATION (1ULL << 1)
#define ASCEND_DEVICE_DMA_MAX_SOURCE_BYTES (128ULL << 30)
/* No device mapping or submission capabilities are implemented in ABI v1. */
struct ascend_device_dma_lease {
    __u32 version;
    __u32 dev_id;
    __u32 flags;
    __u32 channel;
    __aligned_u64 generation;
    __aligned_u64 capabilities;
    __u32 sq_depth;
    __u32 cq_depth;
    __aligned_u64 reserved[3];
};

#define ASCEND_DEVICE_DMA_ACQUIRE _IOWR('D', 0xd0, struct ascend_device_dma_lease)
#define ASCEND_DEVICE_DMA_QUERY _IOWR('D', 0xd1, struct ascend_device_dma_lease)
#define ASCEND_DEVICE_DMA_RELEASE _IOW('D', 0xd2, struct ascend_device_dma_lease)

/* Startup-only, read-only DMA source. No bus addresses are exported. The
 * application must finish populating this range before registration and keep
 * it immutable until unregister/close. One region per lease; repeated ranks
 * may pin the same shared physical pages without making table copies.
 */
struct ascend_device_dma_source {
    __u32 version;
    __u32 flags;
    __aligned_u64 generation;
    __aligned_u64 address;
    __aligned_u64 length;
    __aligned_u64 region_id;
    __u32 page_count;
    __u32 segment_count;
    __aligned_u64 reserved[2];
};

#define ASCEND_DEVICE_DMA_SOURCE_REGISTER _IOWR('D', 0xd3, struct ascend_device_dma_source)
#define ASCEND_DEVICE_DMA_SOURCE_QUERY _IOWR('D', 0xd4, struct ascend_device_dma_source)
#define ASCEND_DEVICE_DMA_SOURCE_RELEASE _IOW('D', 0xd5, struct ascend_device_dma_source)

#endif
