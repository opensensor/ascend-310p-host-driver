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

#ifndef __DMS_NODE_TYPE_H__
#define __DMS_NODE_TYPE_H__
#include "dms_device_node_type.h"

typedef enum {
    DMS_DEV_NODE0 = 0x0,
    DMS_DEV_NODE1 = 0x1,
    DMS_DEV_ID_MAX
} DMS_DEVICE_NODE_ID;

typedef enum {
    HAL_DMS_DEV_TYPE_BASE_SERVCIE = 0x600,
    HAL_DMS_DEV_TYPE_PROC_MGR = 0x601,
    HAL_DMS_DEV_TYPE_IAMMGR = 0x602,
    HAL_DMS_DEV_TYPE_PROC_LAUNCHER = 0x603,
    HAL_DMS_DEV_TYPE_ADDA = 0x604,
    HAL_DMS_DEV_TYPE_DMP_DAEMON = 0x605,
    HAL_DMS_DEV_TYPE_SKLOGD = 0x606,
    HAL_DMS_DEV_TYPE_SLOGD = 0x607,
    HAL_DMS_DEV_TYPE_LOG_DAEMON = 0x608,
    HAL_DMS_DEV_TYPE_HDCD = 0x609,
    HAL_DMS_DEV_TYPE_AICPU_SCH = 0x60B,
    HAL_DMS_DEV_TYPE_QUEUE_SCH = 0x60C,
    HAL_DMS_DEV_TYPE_AICPU_CUST_SCH = 0x60E,
    HAL_DMS_DEV_TYPE_HCCP = 0x60F,
    HAL_DMS_DEV_TYPE_TSD_DAEMON = 0x610,
    HAL_DMS_DEV_TYPE_TIMER_SERVER = 0x616,
    HAL_DMS_DEV_TYPE_OS_LINUX = 0x617,
    HAL_DMS_DEV_TYPE_DATA_MASTER = 0x619,
    HAL_DMS_DEV_TYPE_CFG_MGR = 0x61A,
    HAL_DMS_DEV_TYPE_DATA_GW = 0x61D,
    HAL_DMS_DEV_TYPE_RESMGR = 0x623,
    HAL_DMS_DEV_TYPE_DRV_KERNEL = 0x625,
    HAL_DMS_DEV_TYPE_MAX
} HAL_DMS_DEVICE_NODE_TYPE;

/* bit 15~8: 000-hardware/kernel 110-soft 111-product */
#define DMS_EVENT_OBJ_KERNEL (0x0)
#define DMS_EVENT_OBJ_USER (0x6)
#define DMS_EVENT_OBJ_PRODUCT (0x7)
#define DMS_EVENT_OBJ_TYPE(node_type) (((node_type) >> 8) & 0x7)

int dms_get_node_type_str(unsigned short node_type, char *node_str, unsigned int str_len);
int dms_check_node_type(int node_type);

#endif
