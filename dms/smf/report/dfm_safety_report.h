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

#ifndef DFM_SAFETY_REPORT
#define DFM_SAFETY_REPORT

#include <linux/list.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/gfp.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#ifdef AOS_LLVM_BUILD
#include <linux/atomic.h>
#include <linux/fs.h>
#endif

#include "drvfault_user_common.h"
#include "dms_sensor.h"
#include "dms_event.h"
#include "dms_node_type.h"

enum dfm_event_attr {
    DFM_EVENT_RESUME = 0,
    DFM_EVENT_OCCUR = 1,
    DFM_EVENT_ONETIME = 2
};

typedef int(*dfm_get_nodeid_t)(unsigned long long base_paddr, u32 *node_id);

struct dfm_safety_module {
    struct dfm_node *dev_node;
    DMS_DEVICE_NODE_TYPE node_type;
    u8 dev_id;
    u8 node_id;
    u32 section_type;
    u32 max_event;
};

/*
 * dfm_safety_handler - Reports safety fault events based on the safety status
 * @module_info: module information, see struct @dfm_safety_module
 * @fault_status: Fault status register values
 * @hwinfo_num: numbers of fault status values
 * @event_list: fault event result, can't be null, and size must be larger
 *      than @module_info->max_event
 *
 * This function is used to receive the value of an interrupt status register
 * sent by the fault management module, parse the fault status bit, converge
 * the fault according to the bit, and report the fault to the device specified
 * in @module_info.
 * During event reporting, the event queue is automatically selected based
 * on the sensor type configured in the converge table.
 */
int dfm_safety_handler(const struct dfm_safety_module *module_info,
    const struct safety_fault_status *fault_status,
    unsigned int *event_num, struct safety_event *event_list);

#endif
