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

#include <linux/sizes.h>
#include <linux/slab.h>

#include "ka_kernel_def_pub.h"

#include "dfm_safety_report.h"
#include "dfm_dev_register.h"
#include "dfm_report.h"
#include "dms_sensor_type.h"
#include "dms_sensor.h"

int dfm_add_sensor_event(struct dfm_sensor *sensor, const dfm_event *event);

STATIC int dfm_chk_safety_param(const struct dfm_safety_module *module_info,
    const struct safety_fault_status *fault_status,
    unsigned int *event_num, struct safety_event *event_list)
{
    if (module_info == NULL) {
        dfm_err("module info is null\n");
        return -EINVAL;
    } else if (fault_status == NULL) {
        dfm_err("safety fault is null\n");
        return -EINVAL;
    } else if (event_num == NULL) {
        dfm_err("event_num is null\n");
        return -EINVAL;
    } else if (event_list == NULL) {
        dfm_err("event_list is null\n");
        return -EINVAL;
    }

    if ((*event_num) >= module_info->max_event) {
        dfm_err("current event_num(%u) is great than max event(%u)\n", *event_num, module_info->max_event);
        return -EINVAL;
    }
    return 0;
}

STATIC void dfm_set_safety_event(const struct dfm_safety_module *module_info,
    struct safety_event *fault_event, u8 sensor_type, u8 err_type)
{
    static u8 g_event_serial = 0;
    u32 event_severity = 0;

    fault_event->node_type = module_info->node_type;
    fault_event->sensor_type = sensor_type;
    fault_event->event_type = (unsigned short)err_type;
    fault_event->node_id = (u8)module_info->node_id;  /* the index of devices */
    fault_event->sub_node_type = (u8)0;
    fault_event->sub_node_id = (u8)0;
    fault_event->event_assertion = (u8)DFM_EVENT_OCCUR;  /* 0:RESUME 1:OCCUR 2:ONE_TIME */
    fault_event->event_serial_num = g_event_serial++;
    if (dms_get_event_severity(module_info->node_type, sensor_type, err_type, &event_severity) != 0) {
        dfm_warn("get event severity warn.(sensor_type=%u, err_type=%u)\n",
            sensor_type, err_type);
    }
    fault_event->event_severity = (u8)event_severity;
}

const struct ras_fault_converge_item *dfm_get_coverage_node(const struct dfm_safety_module *module_info,
    const struct safety_fault_status *fault_status, u32 safety_bit)
{
    const struct ras_fault_converge_item *coverage_node = NULL;
    if (module_info == NULL) {
        dfm_err("module_info is null\n");
        return NULL;
    }
    if (module_info->dev_node == NULL) {
        dfm_err("module_info->dev_node is null\n");
        return NULL;
    }
    if (module_info->dev_node->coverage_version == EVENT_COVERAGE_V2) {
        coverage_node = module_info->dev_node->get_converage_node2(
            fault_status, module_info->section_type, safety_bit);
    } else {
        coverage_node = module_info->dev_node->get_converage_node(module_info->section_type, safety_bit);
    }

    return coverage_node;
}

int dfm_report_safety_fault(const struct dfm_safety_module *module_info,
    const struct safety_fault_status *fault_status,
    u32 safety_bit, struct safety_event *fault_event)
{
    const struct ras_fault_converge_item *coverage_node = NULL;
    struct dfm_sensor *sensor = NULL;
    int ret;

    coverage_node = dfm_get_coverage_node(module_info, fault_status, safety_bit);
    if (coverage_node == NULL) {
        dfm_err("get safety code failed.(bit id=%u)\n", safety_bit);
        return -EINVAL;
    }

    sensor = dfm_get_sensor(module_info->dev_node, coverage_node->sensor_type);
    if (sensor == NULL) {
        dfm_err("can't find device's sensor by sensor type.(dev name=%.*s, sensor type=0x%x)\n",
            DMS_MAX_DEV_NAME_LEN, module_info->dev_node->node->node_name, coverage_node->sensor_type);
        return -EINVAL;
    }

    ret = dfm_add_sensor_event(sensor, coverage_node);
    if ((ret != 0) && (ret != -EEXIST)) {
        dfm_err("enqueue fault event failed.(ret=%d)\n", ret);
        return ret;
    }
    dfm_set_safety_event(module_info, fault_event,
        sensor->dms_sensor.sensor_type, coverage_node->error_type);

    /* If add event to sensor success. notify the dms to scan event */
    if (ret == 0) {
        if (dms_sensor_event_notify(module_info->dev_id, &sensor->dms_sensor) != 0) {
            dfm_warn("send notify to dms sensor warn.\n");
        }
    }

    dfm_debug("(node:0x%X,%u)(subnode:0x%X,%u) sensor_type=0x%X, event_type=0x%X, severity=%d, assertion=%d, sn:%u\n",
        fault_event->node_type, fault_event->node_id, fault_event->sub_node_type, fault_event->sub_node_id,
        fault_event->sensor_type, fault_event->event_type, fault_event->event_severity, fault_event->event_assertion,
        fault_event->event_serial_num);

    return 0;
}

int dfm_safety_handler(const struct dfm_safety_module *module_info,
    const struct safety_fault_status *fault_status,
    unsigned int *event_num, struct safety_event *event_list)
{
    struct safety_event *fault_event = NULL;
    u32 status_val, safety_bit;

    if (dfm_chk_safety_param(module_info, fault_status, event_num, event_list) != 0) {
        return -EINVAL;
    }

    status_val = fault_status->fault_status;
    while ((status_val > 0) && ((*event_num) < module_info->max_event)) {
        fault_event = &event_list[*event_num];
        safety_bit = ffs(status_val) - 1;
        if (dfm_report_safety_fault(module_info, fault_status, safety_bit, fault_event) == 0) {
            fault_event->emu_id = fault_status->emu_id;
            fault_event->src_id = fault_status->src_id;
            fault_event->bit_id = fault_status->bit_id;
            *event_num = (*event_num) + 1;
        } else {
            dfm_err("report safety code failed.(emu id=%u,src id=%u,bit id=%u,status id=%u)\n",
                fault_status->emu_id, fault_status->src_id, fault_status->bit_id, safety_bit);
        }
        status_val &= ~(1U << safety_bit);
    }

    if (((*event_num) >= module_info->max_event) && (status_val != 0)) {
        dfm_warn("the max event size is not enough. (node type=0x%x, max_event=%u, curr status=0x%x)\n",
            module_info->node_type, module_info->max_event, status_val);
    }

    return 0;
}
KA_EXPORT_SYMBOL_GPL(dfm_safety_handler);
