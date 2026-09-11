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
#ifndef __DMS_SENSOR_GENERAL_H__
#define __DMS_SENSOR_GENERAL_H__

/* NOTE: Must be consistent with DMS_DECLARE_SEN_TYPE_START(0x01) of dms_sensor_type.c */
#define DMS_SENSOR_FAULT_LEVEL_LOW_MINOR    0x0
#define DMS_SENSOR_FAULT_LEVEL_LOW_MAJOR    0x2
#define DMS_SENSOR_FAULT_LEVEL_LOW_CRITICAL 0x4
#define DMS_SENSOR_FAULT_LEVEL_UP_MINOR     0x7
#define DMS_SENSOR_FAULT_LEVEL_UP_MAJOR     0x9
#define DMS_SENSOR_FAULT_LEVEL_UP_CRITICAL  0xb

#define DMS_GEN_SEN_STATUS_GOOD               0U
#define DMS_GEN_SEN_STATUS_LOW_MINOR_FAULT    1U
#define DMS_GEN_SEN_STATUS_LOW_MAJOR_FAULT    2U
#define DMS_GEN_SEN_STATUS_LOW_CRITICAL_FAULT 3U
#define DMS_GEN_SEN_STATUS_UP_MINOR_FAULT     4U
#define DMS_GEN_SEN_STATUS_UP_MAJOR_FAULT     5U
#define DMS_GEN_SEN_STATUS_UP_CRITICAL_FAULT  6U

#define DMS_SENSOR_THRES_LOW_MINOR_SERIES 0x01U    /* low minor threshold support */
#define DMS_SENSOR_THRES_LOW_MAJOR_SERIES 0x02U    /* low major threshold support */
#define DMS_SENSOR_THRES_LOW_CRITICAL_SERIES 0x04U /* low critical threshold support */
#define DMS_SENSOR_THRES_UP_MINOR_SERIES 0x08U     /* up minor threshold support */
#define DMS_SENSOR_THRES_UP_MAJOR_SERIES 0x10U     /* up major threshold support */
#define DMS_SENSOR_THRES_UP_CRITICAL_SERIES 0x20U  /* up critical threshold support */

#define DMS_GENERAL_SENSOR_THRESSERIES_MASK                                                                         \
    (DMS_SENSOR_THRES_LOW_MINOR_SERIES | DMS_SENSOR_THRES_LOW_MAJOR_SERIES | DMS_SENSOR_THRES_LOW_CRITICAL_SERIES | \
        DMS_SENSOR_THRES_UP_MINOR_SERIES | DMS_SENSOR_THRES_UP_MAJOR_SERIES | DMS_SENSOR_THRES_UP_CRITICAL_SERIES)

#define DMS_GENERAL_SENSOR_MAX_THRES 0x7FFFFFFFU /* Maximum integer of type int */
#define DMS_GENERAL_SENSOR_MIN_THRES 0x80000000U /* Minimum integer of type int */

int dms_sensor_thres_set(unsigned int sensor_type, const struct dms_general_sensor *thres_info);
int dms_sensor_thres_set(unsigned int sensor_type, const struct dms_general_sensor *thres_info);
int dms_sensor_class_check_general(struct dms_sensor_object_cfg *psensor_obj_cfg);
unsigned int dms_sensor_class_init_general(struct dms_sensor_object_cb *psensor_obj);
unsigned int dms_process_general_result_report(struct dms_node_sensor_cb *node_sensor_cb,
    struct dms_sensor_object_cb *psensor_obj_cb, struct dms_sensor_event_data *pevent_data);
unsigned int dms_sensor_class_check_statis(struct dms_sensor_object_cfg *psensor_obj_cfg);
#endif
