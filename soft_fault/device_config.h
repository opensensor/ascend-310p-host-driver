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

#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include "dms_sensor.h"
#include "dms_sensor_type.h"
#include "dms_define.h"

#define SF_SENSOR_SCAN_TIME 120 /* 120ms */

#define AST_MASK 0x01 /* DMS_SEN_TYPE_HB assert_event_mask */
#define DST_MASK 0x01 /* DMS_SEN_TYPE_HB deassert_event_mask */

enum soft_sensor_sub_id {
    SF_SUB_ID0 = 0,
#ifdef CFG_FEATURE_SRIOV
    SF_SUB_ID_MAX = 32
#else
    SF_SUB_ID_MAX = 8
#endif
};

enum soft_user_id {
    SF_SENSOR_DAVINCI = 0,
    SF_SENSOR_OS = 1,
    SF_SENSOR_DRV = 2,
    SF_SENSOR_USER = 3,
    SF_USER_MAX = 65
};

enum soft_dev_node {
#ifdef CFG_FEATURE_SRIOV
    DEV_ID_MAX = 64
#else
    DEV_ID_MAX = 4
#endif
};

#endif /* DEVICE_CONFIG_H */
