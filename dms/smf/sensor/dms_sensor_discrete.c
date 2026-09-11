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
#include "dms_sensor.h"
#include "dms_define.h"
#include "dms_sensor_discrete.h"

unsigned int dms_sensor_class_init_discrete(struct dms_sensor_object_cb *psensor_obj)
{
    psensor_obj->class_cb.discrete_cb.pre_status = 0;
    return DRV_ERROR_NONE;
}

unsigned int dms_process_discrete_result_report(struct dms_node_sensor_cb *node_sensor_cb,
    struct dms_sensor_object_cb *psensor_obj_cb, struct dms_sensor_event_data *pevent_data)
{
    /* Whether the sensor has a status change flag */
    int i;
    DMS_EVENT_LIST_ITEM *p_new_event_list = NULL;

    for (i = 0; i < pevent_data->event_count; i++) {
        if (dms_check_sensor_data(psensor_obj_cb, pevent_data->sensor_data[i].current_value) != DRV_ERROR_NONE) {
            dms_err("check sensor data fail. (sensor_name=%.*s; current_value=%d)\n",
                DMS_SENSOR_DESCRIPT_LENGTH, psensor_obj_cb->sensor_object_cfg.sensor_name,
                pevent_data->sensor_data[i].current_value);
            return DRV_ERROR_PARA_ERROR;
        }
        /* the event is mask */
        if (!dms_sensor_check_mask_enable(psensor_obj_cb->sensor_object_cfg.assert_event_mask,
            pevent_data->sensor_data[i].current_value)) {
            continue;
        }
        (void)sensor_add_event_to_list(&p_new_event_list, &pevent_data->sensor_data[i], psensor_obj_cb);
    }
    (void)dms_update_sensor_list(psensor_obj_cb, p_new_event_list);

    return DRV_ERROR_NONE;
}

/* *************************************************************************
Function:        unsigned int dms_sensor_class_check_discrete(struct dms_sensor_type *psensor_type)

************************************************************************ */
unsigned int dms_sensor_class_check_discrete(struct dms_sensor_object_cfg *psensor_obj_cfg)
{
    return DRV_ERROR_NONE;
}