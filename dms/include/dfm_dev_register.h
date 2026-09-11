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

#ifndef DFM_DEV_REGISTER_H
#define DFM_DEV_REGISTER_H

#include <linux/types.h>
#include <linux/printk.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>

#include "dms_node_type.h"
#include "dms_define.h"
#include "dms_sensor.h"
#include "fpdc.h"
#include "drv_log.h"
#include "drvfault_user_common.h"

#ifdef STATIC_SKIP
#  define STATIC
#else
#  define STATIC static
#endif

#define MODULE_DFM "dfm_module"
#define dfm_err(fmt, ...) drv_err(MODULE_DFM, \
    "<%s:%d:%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__)
#define dfm_warn(fmt, ...) drv_warn(MODULE_DFM, \
    "<%s:%d:%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__)
#define dfm_event_log(fmt, ...) drv_event(MODULE_DFM, \
    "<%s:%d:%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__)
#define dfm_info(fmt, ...) drv_info(MODULE_DFM, \
    "<%s:%d:%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__)
#define dfm_debug(fmt, ...) drv_pr_debug(MODULE_DFM, \
    "<%s:%d:%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__)
#define dfm_custom_event(module, fmt, ...) drv_event(module, \
    "<%s:%d:%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__)

typedef struct ras_fault_converge_item dfm_event;
typedef struct dms_node  dms_node_t;

typedef struct dfm_event_list_node {
    dfm_event event;
    struct list_head node;
    unsigned char assertion;
} dfm_event_node;

enum dfm_event_coverage_version {
    EVENT_COVERAGE_V1 = 0,
    EVENT_COVERAGE_V2 = 1
};

typedef const dfm_event* (*dfm_get_event_coverage)(u32 section_type, u64 ras_code);
typedef const dfm_event* (*dfm_get_event_coverage_v2)(
    const struct safety_fault_status *fault_status, u32 section_type, u64 ras_code);

struct dfm_sensor {
    struct dms_sensor_object_cfg dms_sensor;
    unsigned int error_num;

    /* dfm event node list */
    struct list_head event_list;
    spinlock_t sensor_lock;
};

typedef int (*dms_post_proc)(const dfm_event *err);

struct dfm_node {
    u32 devid;
    u32 node_id;
    int node_type;
    /* DMS node object, which cannot be released before the module exits. */
    dms_node_t *node;

    FAULT_NOTIFY_FUNC fpdc_notify;
    dms_post_proc post_proc;

    enum dfm_event_coverage_version coverage_version;
    union {
        dfm_get_event_coverage get_converage_node;
        dfm_get_event_coverage_v2 get_converage_node2;
    };
    struct delayed_work unmask_irq_work;
    /* Device sensors, one device node may be has multiple sensors */
    u32 sensor_num;
    struct dfm_sensor *sensors;
};

struct dfm_struct {
    u32 dev_id;
    u32 node_num;
    struct dfm_node *dev_nodes;
};

/*
 * dfm_struct_init - init dfm struct
 * @dfm: dfm struct
 * @node_num: node numbers of device nodes
 * @sensor_num: sensor numbers of each device node
 *
 * Initializes the @dfm_struct data structure, including kmalloc the @dev_nodes
 * memory and @dev_nodes.sensors's memory.
 * No mater dfm_struct_init() return success or fail, must call
 * @dfm_struct_final() to free memory.
 */
int dfm_struct_init(struct dfm_struct *dfm, u32 node_num, u32 sensor_num);

/*
 * dfm_struct_final - free dfm struct memory alloced by @dfm_struct_init
 * @dfm: dfm struct
 * @node_num: node numbers of device nodes
 * @sensor_num: sensor numbers of each device node
 *
 * free the memory which malloc by @dfm_struct_init
 */
void dfm_struct_final(struct dfm_struct *dfm, u32 node_num, u32 sensor_num);

/*
* dfm_get_node- get dfm node by by @node_id from dfm node list of @dfm->dev_nodes.
* @dev_node: dfm struct
* @node_id: the index of node
*
* Obtains dfm_ndoe based on the sequence number specified by @node_id.
* If node_id is greater than the number of nodes in the dfm, NULL is returned.
*/
struct dfm_node *dfm_get_node(struct dfm_struct *dfm, u32 node_id);

/*
* dfm_duplicate_dms_nodes - construct all DMS node objects based
*     on the @node and set the node_id in sequence.
* @dfm: dfm struct, must been initialized by @dfm_init_struct
* @node: the duplicated DMS nodes will copy from it.
*
* All device node objects of the current module can be automatically generated.
* This function applies for the memory of each node object. Therefore, when the
* module exits, the @dfm_free_dms_nodes function needs to be called to release
* all memory.
* This function cannot be called repeatedly for the same dfm_struct.
*
* If success, 0 will be returned. Other return values indicate an error occur.
*/
int dfm_duplicate_dms_nodes(struct dfm_struct *dfm, dms_node_t *node);

/*
* dfm_free_dms_nodes - free the dms node objects alloc by @dfm_duplicate_dms_nodes.
* @dfm: dfm struct
*
* After the @dfm_duplicate_dms_nodes function is successfully invoked, the function
* needs to be called to release memory when the module exits.
* If the dfm_duplicate_dms_nodes function fails to be called, the function does not
* need to be called.
*/
void dfm_free_dms_nodes(struct dfm_struct *dfm);

/*
 * dfm_get_sensor - get sensor by @sensor_type from sensor list of @dev_node.
 * @dev_node: device node
 * @sensor_type: sensor type. refer to @DMS_SENSOR_TYPE_T
 *
 * Obtain the sensor corresponding to @sensor_type from the sensor of
 * @dev_node. If the sensor is found, the function returns the pointer
 * of the sensor. Otherwise, NULL is returned.
 */
struct dfm_sensor *dfm_get_sensor(struct dfm_node *dev_node, u8 sensor_type);

/*
 * dfm_register_nodes - register dfm node to dms framework.
 * @dfm: dfm struct, include dms node and dms sensors
 *
 * register dms nodes in @dfm_struct, include register dms node and dms sensors.
 */
int dfm_register_nodes(struct dfm_struct *dfm);

/*
 * dfm_register_nodes - register dfm node to dms framework.
 * @dfm: dfm struct, include dms node and dms sensors
 *
 * unregister dms nodes in @dfm_struct, include unregister dms node and dms sensors.
 */
void dfm_unregister_nodes(const struct dfm_struct *dmf);

/*
 * dfm_get_struct_array - get all dfm struct which has registerd
 * @dfm_num[out]: the number of the resule
 *
 * This interface is used for DFX, This API is used to obtain all registered
 * DFM struct objects. If the interface supports this function, the address of
 * the array is returned. Otherwise, NULL is returned.
 */
struct dfm_struct **dfm_get_struct_array(u32 *dfm_num);

#endif
