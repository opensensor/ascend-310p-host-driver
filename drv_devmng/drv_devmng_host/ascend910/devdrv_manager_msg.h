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

#ifndef DEVDRV_MANAGER_MSG_H
#define DEVDRV_MANAGER_MSG_H

#include "devdrv_common.h"
#include "devdrv_manager_common.h"
#include "devdrv_pm.h"
#include "devdrv_manager.h"
#include "devdrv_interface.h"

typedef struct devdrv_core_utilization {
    u32 dev_id;
    u32 vfid;
    u32 core_type; /* 0: aicore  1: aivector 2:aicpu */
    u32 utilization;
} devdrv_core_utilization_t;

typedef enum devdrv_core_type {
    DEV_DRV_TYPE_AICORE = 0,
    DEV_DRV_TYPE_AIVECTOR,
    DEV_DRV_TYPE_AICPU,
    DEV_DRV_TYPE_MAX,
} devdrv_core_type_t;

#define FILTER_LEN 128
#define PAYLOAD_LEN_MAX 340
struct urd_forward_msg {
    u32 main_cmd;
    u32 sub_cmd;
    char filter[FILTER_LEN];
    unsigned int filter_len;
    u32 output_len;
    u32 payload_len;
    char payload[PAYLOAD_LEN_MAX];
};


int devdrv_manager_h2d_sync_get_devinfo(struct devdrv_info *dev_info);
int devdrv_manager_h2d_query_resource_info(u32 devid, struct devdrv_manager_msg_resource_info *dinfo);
u32 devdrv_manager_h2d_query_dmp_started(u32 devid);
int devdrv_manager_h2d_sync_get_core_utilization(struct devdrv_core_utilization *core_util);
int devdrv_manager_h2d_sync_urd_forward(uint32_t dev_id, uint32_t vfid, struct urd_forward_msg *urd_msg);

#endif /* __DEVDRV_MANAGER_MSG_H */
