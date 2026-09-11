/*
* Copyright (c) Huawei Technologies Co., Ltd. 2019-2022. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* Description:
* Author: huawei
* Create: 2019-10-15
*/

#ifndef VTSDRV_SHM_SQ_H
#define VTSDRV_SHM_SQ_H

#include <linux/types.h>

#include "tsdrv_device.h"
#include "tsdrv_log.h"
#include "vtsdrv_common.h"
#include "devdrv_cqsq.h"

#ifdef CFG_FEATURE_RUNTIME_NO_THREAD

int vtsdrv_shm_sq_ctx_init(struct tsdrv_ctx *ctx, u32 tsnum);
void vtsdrv_shm_sq_ctx_exit(struct tsdrv_ctx *ctx, u32 tsnum);
int vtsdrv_get_shm_phy_addr(u32 devid, u32 tsid, struct tsdrv_phy_addr_get *info);
int vtsdrv_shm_ioctl_sq_alloc(struct tsdrv_ctx *ctx, struct vtsdrv_msg *msg);
int vtsdrv_shm_ioctl_sq_free(struct tsdrv_ctx *ctx, struct vtsdrv_msg *msg);

#else /* !CFG_FEATURE_RUNTIME_NO_THREAD */

static inline int vtsdrv_shm_sq_ctx_init(struct tsdrv_ctx *ctx, u32 tsnum)
{
    return 0;
}

static inline void vtsdrv_shm_sq_ctx_exit(struct tsdrv_ctx *ctx, u32 tsnum)
{
}

static inline int vtsdrv_get_shm_phy_addr(u32 devid, u32 tsid, struct tsdrv_phy_addr_get *info)
{
    return 0;
}

static inline int vtsdrv_shm_ioctl_sq_alloc(struct tsdrv_ctx *ctx, struct vtsdrv_msg *msg)
{
    TSDRV_PRINT_WARN("Shm sq alloc is not supported\n");
    return -ENODEV;
}

static inline int vtsdrv_shm_ioctl_sq_free(struct tsdrv_ctx *ctx, struct vtsdrv_msg *msg)
{
    TSDRV_PRINT_WARN("Shm sq alloc is not supported\n");
    return -ENODEV;
}

#endif /* CFG_FEATURE_RUNTIME_NO_THREAD */

#endif /* _VTSDRV_SHM_SQ_H_ */
