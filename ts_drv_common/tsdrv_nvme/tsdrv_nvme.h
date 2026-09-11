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
#ifndef TSDRV_NVME_H
#define TSDRV_NVME_H

#include "logic_cq.h"
#include "shm_sqcq.h"
#include "devdrv_cbctrl.h"
#include "devdrv_id.h"

int tsdrv_nvme_ctx_init(struct tsdrv_ctx *ctx, u32 tsnum);
void tsdrv_nvme_ctx_exit(struct tsdrv_ctx *ctx, u32 tsnum);

int tsdrv_nvme_init(u32 devid, u32 fid, u32 tsnum);
void tsdrv_nvme_exit(u32 devid, u32 fid, u32 tsnum);

#endif /* __TSDRV_NVME_H */

