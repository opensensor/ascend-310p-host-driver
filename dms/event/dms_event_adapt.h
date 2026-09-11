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

#ifndef __DMS_EVENT_ADAPT_H__
#define __DMS_EVENT_ADAPT_H__
#include "dms_kernel_version_adapt.h"
#include "kernel_version_adapt.h"
#include "devmng_shm_info.h"
#include "dms_cmd_def.h"
int devdrv_manager_container_logical_id_to_physical_id(u32 logical_dev_id, u32 *physical_dev_id, u32 *vfid);

int devdrv_host_black_box_add_exception(u32 devid, u32 code,
    struct timespec stamp, const void *data);
int dms_event_subscribe_from_device(u32 phyid);
int dms_event_clean_to_device(u32 phyid);
int dms_event_mask_event_code(u32 phyid, u32 event_code, u8 mask);
void devdrv_device_black_box_add_exception(u32 devid, u32 code);
int dms_get_event_code_from_bar(u32 devid, u32 *health_code, u32 health_len,
    struct shm_event_code *event_code, u32 event_len);
int dms_get_health_code_from_bar(u32 devid, u32 *health_code, u32 health_len);
int dms_event_distribute_to_bar(u32 phyid);
int dms_event_disable_event_code(u32 phyid, u32 event_code);
int dms_event_box_add_exception(u32 devid, u32 code, struct timespec stamp);
int dms_get_event_para(int dev_id, struct dms_event_para *dms_event, u32 in_cnt, u32 *event_num);
int devdrv_manager_container_get_docker_id(u32 *docker_id);

void dms_event_host_init(void);
void dms_event_host_uninit(void);
int dms_distribute_all_devices_event_to_bar(void);
int dms_save_exception_in_local(void);
int dms_remote_event_save_in_local_init(void);
void dms_remote_event_save_in_local_exit(void);
void dms_event_adapt_init(void);
void dms_event_adapt_exit(void);

#endif
