/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2021. All rights reserved.
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

#ifndef TSDRV_SET_RUNTIME_CONFLICT_CHECK
#define TSDRV_SET_RUNTIME_CONFLICT_CHECK

#include <linux/types.h>

#define DEVDRV_RUNTIME_AVAILABLE                    0x0
#define DEVDRV_RUNTIME_ON_HOST                      0xA5A5
#define DEVDRV_RUNTIME_ON_DEVICE                    0xDECE

void tsdrv_set_runtime_run_init(u32 devid);
int tsdrv_set_runtime_run_conflict_check(u32 devid);
int tsdrv_set_runtime_available_conflict_check(u32 devid);

#endif // TSDRV_SET_RUNTIME_CONFLICT_CHECK

