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

#ifndef TSDRV_OSAL_BBOX_H
#define TSDRV_OSAL_BBOX_H

#include <linux/types.h>
#include <linux/time.h>

#ifdef AOS_LLVM_BUILD
#include <linux/time64.h>
#endif

#ifdef CFG_MANAGER_HOST_ENV
static inline int tsdrv_bbox_write(u32 devid, const char *msg)
{
    return 0;
}

static inline void tsdrv_bbox_system_err_time(u32 devid, u32 excep_id, struct timespec64 *ts, u32 arg)
{
}

static inline void tsdrv_bbox_system_err(u32 devid, u32 excep_id, u32 arg)
{
}

#else /* CFG_MANAGER_HOST_ENV */

int tsdrv_bbox_write(u32 devid, const char *msg);
void tsdrv_bbox_system_err_time(u32 devid, u32 excep_id, struct timespec64 *ts, u32 arg);
void tsdrv_bbox_system_err(u32 devid, u32 excep_id, u32 arg);

#endif /* CFG_MANAGER_HOST_ENV */

#endif /* TSDRV_OSAL_BBOX_H_ */
