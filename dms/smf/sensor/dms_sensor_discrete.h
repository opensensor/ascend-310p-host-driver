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

#ifndef __DMS_SENSOR_DISCRETE_H__
#define __DMS_SENSOR_DISCRETE_H__

unsigned int dms_sensor_class_init_discrete(struct dms_sensor_object_cb *psensor_obj);
unsigned int dms_process_discrete_result_report(struct dms_node_sensor_cb *node_sensor_cb,
    struct dms_sensor_object_cb *psensor_obj_cb, struct dms_sensor_event_data *pevent_data);
unsigned int dms_sensor_class_check_discrete(struct dms_sensor_object_cfg *psensor_obj_cfg);
#endif
