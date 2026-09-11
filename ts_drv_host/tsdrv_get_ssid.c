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

#include "tsdrv_get_ssid.h"
#include "tsdrv_sync.h"
#include "tsdrv_common.h"
#include "tsdrv_log.h"

int tsdrv_msg_sync_ssid(u32 dev_id, u32 tsid, u32 hpid, u32 vfid)
{
    struct tsdrv_msg_sync_ssid *payload = NULL;
    struct tsdrv_msg_info msg_info = {{0}};
    int ret;

    msg_info.header.msg_id = TSDRV_CHAN_SYNC_SSID;
    msg_info.header.valid = TSDRV_MSG_SYNC_MAGIC;
    msg_info.header.result = TSDRV_MSG_INVALID_RESULT;
    msg_info.header.tsid = tsid;

    payload = (struct tsdrv_msg_sync_ssid *)msg_info.payload;
    payload->hpid = hpid;
    payload->vfid = vfid;
    payload->ssid = -1;
    ret = tsdrv_sync_msg_send(dev_id, &msg_info);
    if ((ret != 0) || (msg_info.header.result != 0) || (msg_info.header.valid != TSDRV_MSG_SYNC_RSP_MAGIC)) {
        TSDRV_PRINT_ERR("device(%u) msg chan sync ssid failed, ret = %d, result = %u, valid = 0x%x.\n",
            dev_id, ret, (u32)msg_info.header.result, (u32)msg_info.header.valid);
        return TSDRV_INVALID_SSID;
    }

    return payload->ssid;
}

s32 tsdrv_sync_ssid_proc(u32 devid, struct tsdrv_msg_info *msg, u32 *ack_len)
{
    return 0;
}

