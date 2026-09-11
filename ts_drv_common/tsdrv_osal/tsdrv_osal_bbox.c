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

#ifndef CFG_MANAGER_HOST_ENV
#include <linux/types.h>

#include "devdrv_dfm.h"
#include "tsdrv_osal_bbox.h"
#include "ka_kernel_def_pub.h"

#ifdef AOS_LLVM_BUILD
#include <linux/export.h>
#include <linux/time64.h>
#endif

int tsdrv_bbox_write(u32 devid, const char *msg)
{
#ifndef AOS_LLVM_BUILD
#if defined(CFG_SOC_PLATFORM_MINI) && !defined(CFG_SOC_PLATFORM_MINIV2) && !defined(CFG_SOC_PLATFORM_MINIV3)
    return dfm_write_black_box(DFM_MODULE_ID_DRIVER, DFM_SUBMODULE_ID_DEVMNG, msg);
#else
    return dfm_write_black_box(devid, DFM_MODULE_ID_DRIVER, DFM_SUBMODULE_ID_DEVMNG, msg);
#endif
#else
    return 0;
#endif
}
KA_EXPORT_SYMBOL_GPL(tsdrv_bbox_write);

void tsdrv_bbox_system_err_time(u32 devid, u32 excep_id, struct timespec64 *ts, u32 arg)
{
    excep_time timestamp;

    timestamp.tv_sec = ts->tv_sec;
    timestamp.tv_usec = ts->tv_nsec / NSEC_PER_USEC;

#if defined(CFG_SOC_PLATFORM_MINI) && !defined(CFG_SOC_PLATFORM_MINIV2) && !defined(CFG_SOC_PLATFORM_MINIV3)
    /* miniv1 */
    mntn_system_error(excep_id, timestamp, 0);
#else
    bbox_system_error(devid, excep_id, &timestamp, arg);
#endif
}
KA_EXPORT_SYMBOL_GPL(tsdrv_bbox_system_err_time);

#ifdef AOS_LLVM_BUILD
/* system error report */
static void tsdrv_system_error_report(u32 dev_id, u32 excep_id, u32 arg)
{
    struct timespec64 sys_time = {0};
    excep_time timestamp = {0};

    ka_system_ktime_get_coarse_ts64(&sys_time);
    timestamp.tv_sec = sys_time.tv_sec;
    timestamp.tv_usec = sys_time.tv_nsec / NSEC_PER_USEC;

    bbox_system_error(dev_id, excep_id, &timestamp, arg);
}
#endif

void tsdrv_bbox_system_err(u32 devid, u32 excep_id, u32 arg)
{
#ifndef AOS_LLVM_BUILD
    dfm_system_error_report_ex(devid, excep_id, arg);
#else
    tsdrv_system_error_report(devid, excep_id, arg);
#endif
}
KA_EXPORT_SYMBOL_GPL(tsdrv_bbox_system_err);

#endif /* CFG_MANAGER_HOST_ENV */
