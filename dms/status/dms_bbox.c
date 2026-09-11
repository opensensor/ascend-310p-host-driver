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
#ifndef AOS_LLVM_BUILD
#ifdef CFG_SOC_PLATFORM_MDC_V51

#include "dms_bbox.h"
#include "dms_template.h"
#include "dms_cmd_def.h"
#include "devdrv_user_common.h"
#include "urd_acc_ctrl.h"
#include "bbox_pub.h"

#ifndef STATIC
#define STATIC static
#endif

STATIC int dms_get_last_boot_state_from_bbox(void *feature, char *in, unsigned int in_len,
                                             char *out, unsigned int out_len)
{
    int ret;
    unsigned int dev_id = 0;
    unsigned int boot_type = 0;
    unsigned int state;
    last_bootstate_info_t bootstate_info = {0};

    if ((in == NULL) || (out == NULL) || (in_len != sizeof(last_bootstate_info_t)) ||
        (out_len != sizeof(unsigned int))) {
        dms_err("Invalid parameter. (in_buff=%s; in_len=%u;out_buff=%s; out_len=%u)\n",
                (in == NULL) ? "NULL" : "OK", in_len, (out == NULL) ? "NULL" : "OK", out_len);
        return -EINVAL;
    }
    bootstate_info = *(last_bootstate_info_t *)in;
    dev_id = bootstate_info.dev_id;
    boot_type = bootstate_info.boot_type;
    ret = bbox_getLastBootState(dev_id, boot_type, &state);
    if (ret != 0) {
        dms_err("get last boot state failed, (dev_id=%u, boot_type=%u, ret=%d)\n", dev_id, boot_type, ret);
        return ret;
    }

    *(unsigned int *)out = state;
    dms_debug("get last boot state success.\n");
    return 0;
}

BEGIN_DMS_MODULE_DECLARATION(DMS_MODULE_BLACK_BOX)
BEGIN_FEATURE_COMMAND()
ADD_FEATURE_COMMAND(DMS_MODULE_BLACK_BOX, DMS_MAIN_CMD_BBOX, DMS_SUBCMD_GET_LAST_BOOT_STATE,
                    NULL, NULL, DMS_SUPPORT_ALL, dms_get_last_boot_state_from_bbox)
END_FEATURE_COMMAND()
END_MODULE_DECLARATION()

int dms_bbox_init(void)
{
    CALL_INIT_MODULE(DMS_MODULE_BLACK_BOX);
    return 0;
}

void dms_bbox_uninit(void)
{
    CALL_EXIT_MODULE(DMS_MODULE_BLACK_BOX);
}

#endif
#endif
