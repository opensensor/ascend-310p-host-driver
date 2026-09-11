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
#include "ka_list_pub.h"
#include "ka_barrier_pub.h"
#include "securec.h"
#include "tsdrv_ioctl.h"
#include "tsdrv_common.h"
#include "shm_sqcq.h"
#include "tsdrv_device.h"
#include "vtsdrv_common.h"
#include "ascend_hal_define.h"
#include "hvtsdrv_shm_sqcq.h"

int hvtsdrv_shm_sqcq_alloc(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    /* cloud vm & container do not support runtime no thread!! */
#ifdef CFG_SOC_PLATFORM_CLOUD
    arg->shm_sqcq_alloc_para.sq_id = DEVDRV_MAX_SQ_NUM;
    arg->shm_sqcq_alloc_para.cq_id = DEVDRV_MAX_CQ_NUM;
    TSDRV_PRINT_WARN("Capacity splitting container scenario not support no-thread type,"
        " pls retry normal type.\n");
#else /* !CFG_SOC_PLATFORM_CLOUD */
    u32 devid, fid;
    int err, tgid;

    devid = tsdrv_get_devid_by_ctx(ctx);
    fid = tsdrv_get_fid_by_ctx(ctx);
    tgid = ctx->tgid;
    err = shm_ioctl_cqsq_alloc(ctx, arg);
    if (err != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Shm sqcq alloc fail. (devid=%u; fid=%u; tgid=%d; err=%d)\n", devid, fid, tgid, err);
#endif
        return err;
    }

    /* overwrite rdonly flag for container environment */
    arg->shm_sqcq_alloc_para.rdonly = TSRRV_FLAG_SQ_RDONLY;
#endif /* CFG_SOC_PLATFORM_CLOUD */
    return 0;
}

int hvtsdrv_shm_sqcq_free(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
#ifdef CFG_SOC_PLATFORM_CLOUD
    TSDRV_PRINT_WARN("Capacity splitting container scenario not support no-thread type,"
               " pls retry normal type.\n");
    return -ENODEV;
#else /* !CFG_SOC_PLATFORM_CLOUD */
    return shm_ioctl_cqsq_free(ctx, arg);
#endif /* CFG_SOC_PLATFORM_CLOUD */
}

int hvtsdrv_get_shm_phy_sq(u32 devid, u32 fid, u32 tsid, struct phy_sq_alloc_para *phy_sq)
{
    struct tsdrv_ts_resource *ts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
    struct devdrv_ts_sq_info *sq_info = NULL;
    struct devdrv_sq_sub_info *sq_sub = NULL;
    u32 sqid = ts_res->shm_phy_sq[0];
    if (sqid >= DEVDRV_MAX_SQ_NUM) {
        TSDRV_PRINT_ERR("The sqid is invalid. (sqid=%u)\n", sqid);
        return -EINVAL;
    }

    sq_info = tsdrv_get_sq_info(devid, fid, tsid, sqid);
    if (sq_info == NULL) {
        TSDRV_PRINT_ERR("Failed to get sq_info. (devid=%u; fid=%u; tsid=%u; sqid=%d)\n", devid, fid, tsid, sqid);
        return -EINVAL;
    }
    sq_sub = sq_info->sq_sub;

    phy_sq->sq_id = sqid;
    phy_sq->sq_paddr = sq_sub->phy_addr;
    phy_sq->sq_bar_addr = sq_sub->bar_addr;

    return 0;
}

int hvtsdrv_shm_offset_get(u32 devid, u32 tsid, u32 fid, ka_pid_t tgid, u8 *msg)
{
    struct shm_sq_offset *shm_offset = (struct shm_sq_offset *)msg;
    struct tsdrv_ts_resource *ts_res;
    enum tsdrv_dev_status status;

    status = tsdrv_get_dev_status(devid, fid);
    if (status != TSDRV_DEV_ACTIVE) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("hvtsdrv dev-%u fid-%u status-%u not active.\n", devid, fid, status);
        return -EINVAL;
#endif
    }

    ts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
    shm_offset->offset = ts_res->shm_offset;
    return 0;
}

