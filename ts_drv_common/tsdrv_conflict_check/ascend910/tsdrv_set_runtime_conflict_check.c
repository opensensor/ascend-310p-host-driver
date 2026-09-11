/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2020. All rights reserved.
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
#ifndef TSDRV_KERNEL_UT
#include "tsdrv_set_runtime_conflict_check.h"
#include "devdrv_common.h"
#include "tsdrv_device.h"

void tsdrv_set_runtime_run_init(u32 devid)
{
}

int tsdrv_set_runtime_run_conflict_check(u32 devid)
{
    return 0;
}

int tsdrv_set_runtime_available_conflict_check(u32 devid)
{
    return 0;
}
#else
int tsdrv_set_runtime_conflict_check_ascend910(void)
{
    return 0;
}
#endif
