/*
* Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
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
* Create: 2023-5-29
*/

#ifndef DMS_CPU_FREQ_H
#define DMS_CPU_FREQ_H

#include "dms_template.h"

int osc_freq_init(void);
void osc_freq_exit(void);
int dms_get_device_osc_freq(u32 devid, u64 *freq);
int dms_get_host_osc_freq(u64 *freq);

#define DMS_MODULE_OSC_FREQ  "dms_osc_freq"
INIT_MODULE_FUNC(DMS_MODULE_OSC_FREQ);
EXIT_MODULE_FUNC(DMS_MODULE_OSC_FREQ);

#endif
