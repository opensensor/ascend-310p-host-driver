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

#include <linux/errno.h>
#include <linux/slab.h>
#include "dms_define.h"
#include "dms_event_adapt.h"
#include "smf_event_adapt.h"

void dms_event_adapt_init(void)
{
    struct smf_event_adapt apt = {0};

    apt.subscribe_from_device = dms_event_subscribe_from_device;
    apt.clean_to_device = dms_event_clean_to_device;
    apt.mask_event_code = dms_event_mask_event_code;
    apt.get_event_code_from_bar = dms_get_event_code_from_bar;
    apt.get_health_code_from_bar = dms_get_health_code_from_bar;
    apt.distribute_all_devices_event_to_bar = dms_distribute_all_devices_event_to_bar;
    apt.distribute_to_bar = dms_event_distribute_to_bar;
    apt.get_event_para = dms_get_event_para;
#ifndef AOS_LLVM_BUILD
    apt.logical_id_to_physical_id = devdrv_manager_container_logical_id_to_physical_id;
    apt.get_container_ns_id = devdrv_manager_container_get_docker_id;
    (void)dms_event_set_add_exception_handle(dms_event_box_add_exception);
#endif
    dms_event_host_init();
    (void)smf_event_adapt_init(&apt);
}
void dms_event_adapt_exit(void)
{
    (void)dms_event_set_add_exception_handle(NULL);
    dms_event_host_uninit();
    smf_event_adapt_uninit();
}

