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

#ifndef DFM_REPORT_H
#define DFM_REPORT_H

#include <linux/types.h>

#include "dfm_dev_register.h"
#include "dms_sensor.h"

/**
* dfm_gen_event_id - generate fault event id
* @dev_node_type: refer to enum @DMS_DEVICE_NODE_TYPE
* @sensor_type: refer to enum @DMS_SENSOR_TYPE_T
* @error_type: the error type
*
* Event IDs are generated based on the following rules. The fault location
* is fixed at 10-Device-Linux and the node type is fixed at 000-hardware.
* The following table describes the generation rules.
* +-----------------------+------------------+------------------------------+------------------------------+
* | fault position        | Reserve          | NodeType                     |           EventType          |
* |-----------------------+------------------+------------------------------+------------------------------|
* | 2(31~30)              | 2(29~28)         | 3(27-25)         | 8(24~17)  | 8(16~9)         | 9(8~0)     |
* |-----------------------|------------------|------------------|-----------|-----------------|------------|
* |01: Host               | 00: default      | 000~010:hardware | node type | sensor type     | error type |
* |10: Device-Linux       |                  | 011~110:software |           |                 |            |
* |11: Device-AOSCore     |                  |                  |           |                 |            |
* +-----------------------+------------------+------------------+-----------+-----------------+------------+
*/
u32 dfm_gen_event_id(u8 dev_node_type, u8 sensor_type, u8 error_type);

/*
 * dfm_add_event - Add an event to the sensor fault event list.
 * @sensor: event sensor, can't be null
 * @node_id: the index of the device nodes
 * @event: event will reported. can't be null
 *
 * When a module detects a new event, you can call this function to add
 * the event to the event list. The function checks whether the newly added
 * event already exists in the event list. If the event exists, the
 * function will not be added again.
 *
 * The function will return -EEXIST if the event has exists.
 * will return -ENOMEM if kmalloc event node failed.
 * On success, dfm_add_event() will return 0.
 */
int dfm_add_event(struct dfm_struct *dfm, u32 node_id, const dfm_event *event);

/*
* dfm_remove_event - Remove an event from the sensor fault event list.
* @dfm: dfm struct, can't be null
* @node_id: the index of the device nodes
* @event: event will remove. can't be null
*
* This interface is used to delete a fault event from the fault list when
* the current fault is rectified. The sensor_type and error_type parameters
* of @event are the basis for deleting data. Other parameters of @event are
* not be checked.
*
* The function will return -EEXIST if the event has exists.
* will return -EINVAL if input parameters are invalid.
* On success, will return 0.
*/
int dfm_remove_event(struct dfm_struct *dfm, u32 node_id, const dfm_event *event);

/*
 * dfm_clear_event_list - clear all event in the sensor fault event list.
 * @sensor: event sensor, can't be null
 *
 * This function is used to clear all events of a specified sensor. This function
 * is used for maintenance and test. After the events in the event queue are cleared,
 * the fault management framework automatically reports the fault rectification event.
 * You can add the event repeatedly and report the event again.
 */
void dfm_clear_event_list(struct dfm_sensor *sensor);

/*
 * dfm_clear_sensor_events - clear all event in the sensor fault event list.
 * @dfm: device fault manager struct
 * @node_id: device node id
 * @sensor_type: sensor type of registed in @dfm
 *
 * This function is used to clear the event queue of the sensor. When the
 * system suspend and resume, the fault event queue of the sensor is cleared.
 * and the sensor event queue in fault management framework is also cleared.
 */
int dfm_clear_sensor_events(struct dfm_struct *dfm, u32 node_id, u8 sensor_type);

/*
 * dfm_scan_sensor_events - scan all event in the sensor's fault event list.
 * @sensor: event sensor, can't be null
 * @data: scan result will store to data
 *
 * This function is used to obtain the event queue of the sensor. When the
 * fault management framework sensor scans device events, it obtains the fault
 * event list of the current sensor. If the event is a one time event, the
 * event is automatically deleted from the fault event list of the current
 * sensor after it is saved to @data. Not a single event, it stays in the queue.
 */
void dfm_scan_sensor_events(struct dfm_sensor *sensor, struct dms_sensor_event_data *events);

/*
* dfm_scan_events - scan all event in the dfm's sensor fault event list.
* @dfm: device fault manager struct
* @node_id: device node id
* @sensor_type: sensor type of registed in @dfm
* @data: scan result will store to data
*
* If @node_id exceeds the range of the device, -EINVAL is returned.
* If @sensor_type does not exist in the sensor of the device node.
* -EFAULT is returned. The data of the current fault event is saved
* in data.
*/
int dfm_scan_events(struct dfm_struct *dfm, u32 node_id, u8 sensor_type, struct dms_sensor_event_data *data);

/*
 * dfm_set_event_resume_attr - Set event resume attr.
 * @dfm: device fault manager struct
 * @node_id: the index of the device nodes
 * @event: event will reported. can't be null
 *
 * When a module recieves occur event and add to event list,
 * a resume event coming as soon as poosible, you can call this func
 * to modify event attr. When scan event list, make sure the event can be removed.
 *
 * return void.
 */
void dfm_set_event_resume_attr(struct dfm_struct *dfm, u32 node_id, const dfm_event *event);

/*
 * dfm_event_is_exist - check the event whether exist in event list.
 * @node_id: the index of the device nodes
 * @event: event be checked. can't be null
 *
 * If input param is invalid or not exist, return false.
 * If event exist return true;
 */
bool dfm_event_is_exist(struct dfm_struct *dfm, u32 node_id, const dfm_event *event);
int dfm_add_rascode_to_describe(unsigned int devid, dfm_event *event, unsigned int rascode);
void dfm_print_event_by_module(const char *module_name, const dfm_event *event, u32 initial_rascode);

#endif
