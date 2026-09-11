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
#ifndef __DMS_SENSOR_NOTIFY_H__
#define __DMS_SENSOR_NOTIFY_H__

#include <linux/spinlock.h>
#include "dms_sensor.h"

/* Maximum notify sensor object number */
#define DMS_MAX_NOTIFY_SENSOR_OBJ_NUM 32

struct dms_sensor_notify_item {
    unsigned int dev_id;
    struct dms_sensor_object_cfg notify_sensor_obj;
};

struct dms_sensor_notify_queue {
    unsigned int head;
    unsigned int tail;
    unsigned int size;
    unsigned int count;
    struct dms_sensor_notify_item sensor_obj_queue[DMS_MAX_NOTIFY_SENSOR_OBJ_NUM];
    spinlock_t notify_lock;
};

int dms_sensor_event_notify(unsigned int dev_id, struct dms_sensor_object_cfg *psensor_obj_cfg);

void dms_sensor_notify_init(void);
void dms_sensor_notify_exit(void);
void dms_sensor_notify_count_clear(void);
void dms_sensor_notify_event_proc(unsigned int scan_mode);

#endif /* __DMS_SENSOR_NOTIFY_H__ */