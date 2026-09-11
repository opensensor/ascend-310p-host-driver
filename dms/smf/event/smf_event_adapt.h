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

#ifndef __SMF_EVENT_ADAPT_H__
#define __SMF_EVENT_ADAPT_H__
#include "dms_kernel_version_adapt.h"
#include "kernel_version_adapt.h"
#include "devmng_shm_info.h"
#include "dms_cmd_def.h"

struct smf_event_adapt {
    int (*subscribe_from_device)(u32 phyid);
    int (*clean_to_device)(u32 phyid);
    int (*mask_event_code)(u32 phyid, u32 event_code, u8 mask);
    int (*get_event_code_from_bar)(u32 devid, u32 *health_code, u32 health_len,
        struct shm_event_code *event_code, u32 event_len);
    int (*get_health_code_from_bar)(u32 devid, u32 *health_code, u32 health_len);
    int (*distribute_all_devices_event_to_bar)(void);
    int (*distribute_to_bar)(u32 phyid);
    int (*logical_id_to_physical_id)(u32 logical_dev_id, u32 *physical_dev_id, u32 *vfid);
    int (*get_event_para)(int phyid, struct dms_event_para *dms_event, u32 in_cnt, u32 *event_num);
    int (*get_container_ns_id)(u32 *ns_id);
};

int smf_event_subscribe_from_device(u32 phyid);
int smf_event_clean_to_device(u32 phyid);
int smf_event_mask_event_code(u32 phyid, u32 event_code, u8 mask);
int smf_get_event_code_from_bar(u32 devid, u32 *health_code, u32 health_len,
    struct shm_event_code *event_code, u32 event_len);
int smf_get_health_code_from_bar(u32 devid, u32 *health_code, u32 health_len);
int smf_event_distribute_to_bar(u32 phyid);
int smf_distribute_all_devices_event_to_bar(void);
int smf_event_adapt_init(struct smf_event_adapt *apt);
void smf_event_adapt_uninit(void);
int smf_logical_id_to_physical_id(u32 logical_dev_id, u32 *physical_dev_id, u32 *vfid);
int smf_get_remote_event_para(int phyid, struct dms_event_para *dms_event, u32 in_cnt, u32 *event_num);
int smf_get_container_ns_id(u32 *ns_id);
#endif
