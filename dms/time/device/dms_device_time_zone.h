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

#ifndef __DMS_DEVICE_TIME_ZONE_H
#define __DMS_DEVICE_TIME_ZONE_H

#include <linux/time.h>

#include "devdrv_manager_common.h"

/* process white list */
#define WHITE_LIST_PROCESS_NUM 1
#define PROCESS_NAME_DMP "dmp_daemon"

struct dms_time_sync {
    long interval_seconds;
};

int dms_process_time_sync(void *feature, char *in, u32 in_len, char *out, u32 out_len);
void dms_get_time_interval(struct timespec64 *time_sync);
#endif
