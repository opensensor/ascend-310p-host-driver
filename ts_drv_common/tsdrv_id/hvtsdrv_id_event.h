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
#ifndef TSDRV_ID_EVENT_H
#define TSDRV_ID_EVENT_H

#include "devdrv_common.h"
#include "tsdrv_sync.h"
int tsdrv_event_id_init(u32 devid, u32 fid);
void tsdrv_event_id_uninit(u32 devid, u32 fid);
int tsdrv_event_id_maping(u32 devid, struct tsdrv_sync_event *event);

#endif

