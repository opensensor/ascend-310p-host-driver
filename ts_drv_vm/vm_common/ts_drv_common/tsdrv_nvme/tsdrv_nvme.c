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
#include <linux/types.h>
#include "tsdrv_nvme.h"

#ifdef CFG_FEATURE_RUNTIME_NO_THREAD
int tsdrv_nvme_ctx_init(struct tsdrv_ctx *ctx, u32 tsnum)
{
    int err;

    err = shm_sqcq_ctx_init(ctx, tsnum);
    if (err != 0) {
        return err;
    }
    err = logic_sqcq_ctx_init(ctx, tsnum);
    if (err != 0) {
        shm_sqcq_ctx_exit(ctx, tsnum);
        return err;
    }
    return 0;
}

void tsdrv_nvme_ctx_exit(struct tsdrv_ctx *ctx, u32 tsnum)
{
    shm_sqcq_ctx_exit(ctx, tsnum);
    logic_sqcq_ctx_exit(ctx, tsnum);
}

int tsdrv_nvme_init(u32 devid, u32 fid, u32 tsnum)
{
    int err;

    err = shm_sqcq_dev_init(devid, fid, tsnum);
    if (err != 0) {
        return err;
    }
    err = logic_sqcq_dev_init(devid, fid, tsnum);
    if (err != 0) {
        shm_sqcq_dev_exit(devid, fid, tsnum);
        return err;
    }

    return 0;
}

void tsdrv_nvme_exit(u32 devid, u32 fid, u32 tsnum)
{
    shm_sqcq_dev_exit(devid, fid, tsnum);
    logic_sqcq_dev_exit(devid, fid, tsnum);
}
#else
int tsdrv_nvme_ctx_init(struct tsdrv_ctx *ctx, u32 tsnum)
{
    return 0;
}

void tsdrv_nvme_ctx_exit(struct tsdrv_ctx *ctx, u32 tsnum)
{
}

int tsdrv_nvme_init(u32 devid, u32 fid, u32 tsnum)
{
    return 0;
}

void tsdrv_nvme_exit(u32 devid, u32 fid, u32 tsnum)
{
}
#endif
