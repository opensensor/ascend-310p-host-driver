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
#ifndef __DEVDRV_DRIVER_PM_H
#define __DEVDRV_DRIVER_PM_H

#include "devdrv_common.h"

#if defined(CFG_SOC_PLATFORM_MINI) && !defined(DEVMNG_UT) && !defined(LOG_UT)
int devdrv_get_runtime_runningplat(u32 devid, u64 *running_plat);
int devdrv_set_runtime_runningplat(u32 devid, u64 running_plat);
#endif

#if defined(CFG_SOC_PLATFORM_MINI) && !defined(CFG_SOC_PLATFORM_MINIV2) && !defined(DEVMNG_UT) && !defined(LOG_UT)
int devdrv_set_power_mode(struct devdrv_info *info, enum devdrv_power_mode mode);
int devdrv_is_low_power(struct devdrv_info *info);
int devdrv_is_ts_work(struct devdrv_info *info);
void devdrv_set_ts_sleep(struct devdrv_info *info);
void devdrv_set_ts_work(struct devdrv_info *info);
void devdrv_set_ts_down(struct devdrv_info *info);
void devdrv_set_ts_booting(struct devdrv_info *info);
void devdrv_set_ts_initing(struct devdrv_info *info);
int devdrv_set_ts_status(struct devdrv_info *info, enum devdrv_ts_status status);
int devdrv_driver_try_suspend(struct devdrv_info *info);
int devdrv_driver_try_resume(struct devdrv_info *info);
void devdrv_driver_hardware_exception(struct devdrv_info *info);
#else
int devdrv_is_ts_work(struct devdrv_ts_mng *ts_mng);
void devdrv_set_ts_sleep(struct devdrv_info *dev_info, u32 tsid);
void devdrv_set_ts_work(struct devdrv_info *dev_info, u32 tsid);
void devdrv_set_ts_down(struct devdrv_info *dev_info, u32 tsid);
void devdrv_set_ts_initing(struct devdrv_info *dev_info, u32 tsid);
void devdrv_set_ts_booting(struct devdrv_info *dev_info, u32 tsid);
int devdrv_set_ts_status(struct devdrv_info *dev_info, u32 tsid, enum devdrv_ts_status status);
void devdrv_driver_hardware_exception(struct devdrv_info *info,
    uint32_t tsid);
int devdrv_is_ts_ready(int devid);
#endif

#endif
