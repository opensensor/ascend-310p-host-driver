/*
* Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
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
* Create: 2023-12-21
*/


#include "ascend_kernel_hal.h"
#include "devdrv_common.h"
#include "devdrv_manager_common.h"
#include "dbl/spod_info.h"
#include "dbl/uda.h"

#define SERVER_ID_MAX 47
#define SERVER_ID_INVALID 0x3FF

STATIC void devdrv_make_set_sdid(u32 host_udevid, struct devdrv_info *dev_info)
{
    int ret;
    u32 sdid = 0;
    struct sdid_parse_info parse_stru = {0};
    struct spod_info spod_stru = {0};
    if (dev_info->server_id > SERVER_ID_MAX) {
        dev_info->server_id = SERVER_ID_INVALID;
    }
    parse_stru.server_id = dev_info->server_id;
    parse_stru.chip_id = dev_info->chip_id;
    parse_stru.die_id = dev_info->die_id;
    parse_stru.udevid = host_udevid;
    ret = dbl_make_sdid(&parse_stru, &sdid);
    if (ret != 0) {
        devdrv_drv_err("make sdid fail. (dev_id=%u;server=%u;chip=%u;die=%u;udevid=%u)\n", dev_info->dev_id,
            parse_stru.server_id, parse_stru.chip_id, parse_stru.die_id, parse_stru.udevid);
        return;
    }
    devdrv_drv_info("make sdid success. (dev_id=%u;sdid=0x%x;server=%u;chip=%u;die=%u;udevid=%u)\n",
        dev_info->dev_id, sdid, parse_stru.server_id, parse_stru.chip_id, parse_stru.die_id, parse_stru.udevid);
    spod_stru.sdid = sdid;
    spod_stru.scale_type = dev_info->scale_type;
    spod_stru.super_pod_id = dev_info->super_pod_id;
    spod_stru.server_id = dev_info->server_id;
    ret = dbl_set_spod_info(dev_info->dev_id, &spod_stru);
    if (ret != 0) {
        devdrv_drv_err("set spod info fail.(ret=%d)\n", ret);
    }
}


#ifdef CFG_HOST_ENV
void devdrv_host_generate_sdid(struct devdrv_info *dev_info)
{
    if (!uda_is_phy_dev(dev_info->dev_id)) {
        return;
    }
    devdrv_make_set_sdid(dev_info->dev_id, dev_info);
}

#else
void devdrv_device_generate_sdid(u32 udevid)
{
    struct devdrv_info *dev_info = NULL;
    int ret;
    u32 host_udevid = 0;

    if (!uda_is_phy_dev(udevid)) {
        return;
    }
    dev_info = devdrv_manager_get_devdrv_info(udevid);
    if (dev_info == NULL) {
        devdrv_drv_err("get dev_info fail. (udevid=%u)\n", udevid);
        return;
    }
    ret = uda_dev_get_remote_udevid(dev_info->dev_id, &host_udevid);
    if (ret != 0) {
        devdrv_drv_err("get remote udevid fail. (dev_id=%u;ret=%d)\n", dev_info->dev_id, ret);
        return;
    }
    devdrv_make_set_sdid(host_udevid, dev_info);
}

#endif
