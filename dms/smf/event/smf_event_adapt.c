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
#include <linux/time.h>
#include <linux/slab.h>

#include "dms_define.h"
#include "dms_event.h"
#include "smf_event_adapt.h"
#include "ka_kernel_def_pub.h"

static struct smf_event_adapt g_event_adapt = {0};

int smf_distribute_all_devices_event_to_bar(void)
{
    if (g_event_adapt.distribute_all_devices_event_to_bar == NULL) {
        return DRV_ERROR_NONE;
    }

    return g_event_adapt.distribute_all_devices_event_to_bar();
}

int smf_event_subscribe_from_device(u32 phyid)
{
    if (g_event_adapt.subscribe_from_device == NULL) {
        return DRV_ERROR_NONE;
    }

    return g_event_adapt.subscribe_from_device(phyid);
}

int smf_event_clean_to_device(u32 phyid)
{
    if (g_event_adapt.clean_to_device == NULL) {
        return DRV_ERROR_NONE;
    }

    return g_event_adapt.clean_to_device(phyid);
}

int smf_logical_id_to_physical_id(u32 logical_dev_id, u32 *physical_dev_id, u32 *vfid)
{
    if (g_event_adapt.logical_id_to_physical_id == NULL) {
        return DRV_ERROR_NONE;
    }

    return g_event_adapt.logical_id_to_physical_id(logical_dev_id, physical_dev_id, vfid);
}

int smf_event_mask_event_code(u32 phyid, u32 event_code, u8 mask)
{
    if (g_event_adapt.mask_event_code == NULL) {
        return DRV_ERROR_NONE;
    }
    return g_event_adapt.mask_event_code(phyid, event_code, mask);
}
int smf_get_event_code_from_bar(u32 devid, u32 *health_code, u32 health_len,
    struct shm_event_code *event_code, u32 event_len)
{
    if (g_event_adapt.get_event_code_from_bar == NULL) {
        return DRV_ERROR_NONE;
    }

    return g_event_adapt.get_event_code_from_bar(devid, health_code, health_len,
        event_code, event_len);
}

int smf_get_health_code_from_bar(u32 devid, u32 *health_code, u32 health_len)
{
    if (g_event_adapt.get_health_code_from_bar == NULL) {
        return DRV_ERROR_NONE;
    }

    return g_event_adapt.get_health_code_from_bar(devid, health_code, health_len);
}

int smf_event_distribute_to_bar(u32 phyid)
{
    if (g_event_adapt.distribute_to_bar == NULL) {
        return DRV_ERROR_NONE;
    }

    return g_event_adapt.distribute_to_bar(phyid);
}

int smf_get_remote_event_para(int phyid, struct dms_event_para *dms_event, u32 in_cnt, u32 *event_num)
{
    if (g_event_adapt.get_event_para == NULL) {
        return DRV_ERROR_NONE;
    }

    return g_event_adapt.get_event_para(phyid, dms_event, in_cnt, event_num);
}

int smf_get_container_ns_id(u32 *ns_id)
{
    if (g_event_adapt.get_container_ns_id == NULL) {
        return DRV_ERROR_NONE;
    }

    return g_event_adapt.get_container_ns_id(ns_id);
}

int smf_event_adapt_init(struct smf_event_adapt *apt)
{
    if (apt == NULL) {
        return DRV_ERROR_PARA_ERROR;
    }

    (void)memcpy_s(&g_event_adapt, sizeof(struct smf_event_adapt), apt, sizeof(struct smf_event_adapt));
    return DRV_ERROR_NONE;
}
KA_EXPORT_SYMBOL(smf_event_adapt_init);

void smf_event_adapt_uninit(void)
{
    (void)memset_s(&g_event_adapt, sizeof(struct smf_event_adapt), 0, sizeof(struct smf_event_adapt));
}
KA_EXPORT_SYMBOL(smf_event_adapt_uninit);

