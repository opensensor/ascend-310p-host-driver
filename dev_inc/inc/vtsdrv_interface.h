/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
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
 * Create: 2022-04-05
 */

#ifndef DRV_VTSDRV_INTERFACE_H
#define DRV_VTSDRV_INTERFACE_H
#include <linux/types.h>

#ifdef CFG_FEATURE_VM_GRACEFUL_RELEASE
void vtsdrv_wait_pm_recycle_exit(u32 devid, pid_t tgid);
#else
static void vtsdrv_wait_pm_recycle_exit(u32 devid, pid_t tgid)
{
    return;
}
#endif

#endif

