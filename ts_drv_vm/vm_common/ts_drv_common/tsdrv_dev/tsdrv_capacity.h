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
#ifndef TSDRV_CAPACITY_H
#define TSDRV_CAPACITY_H

#include "tsdrv_kernel_common.h"
#include "tsdrv_device.h"

#define TSDRV_INVALIC_CAPACITY  0xFFFFFFFFU
#ifdef CFG_SOC_PLATFORM_CLOUD_V2
#define RESERVED_NUM 10
#else
#define RESERVED_NUM 0
#endif

int tsdrv_set_ts_capacity(struct tsdrv_ts_resource *ts_res, enum TSDRV_CAPACITY capacity);
int tsdrv_get_ts_capacity(struct tsdrv_ts_resource *ts_res, struct tsdrv_id_capacity *capacity);

#endif /* TSDRV_CAPACITY_H */

