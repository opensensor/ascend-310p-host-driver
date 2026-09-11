/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
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
* Create: 2024-06-03
*/
#include "dbl/feature_loader.h"
#include "dms_template.h"
#include "dms_cmd_def.h"
#include "devdrv_user_common.h"
#include "urd_acc_ctrl.h"
#include "log_drv_agent.h"
#include "dms_log_info.h"

#ifdef DMS_UT
#define STATIC
#else
#define STATIC static
#endif

#define DMS_LOG_BUF_SIZE 0x1400000
struct LogInfo {
    void *buffer;
    u32 length;  // size of buffer
    u32 buf_len;  // actual len of host log ringbuffer
};

STATIC int dms_get_kernel_log_from_logdrv(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len)
{
    struct LogInfo *arg = NULL;
    int ret;

    if ((in == NULL) || (out == NULL)) {
        dms_err("Invalid parameter. (arg=%s; out=%s)\n", (in == NULL) ? "NULL" : "OK", (out == NULL) ? "NULL" : "OK");
        return -EINVAL;
    }

    if ((in_len != sizeof(struct LogInfo)) || (out_len != sizeof(u32))) {
        dms_err("Invalid len. (in_len=%u; out_len=%u)\n", in_len, out_len);
        return -EINVAL;
    }

    arg = (struct LogInfo *)in;
    if (arg->length != DMS_LOG_BUF_SIZE) {
        dms_err("Invalid length. (length=%u)\n", arg->length);
        return -EINVAL;
    }

    ret = log_get_ringbuffer(arg->buffer, arg->length, (u32 *)out);
    if (ret != 0) {
        return ret;
    }

    return 0;
}

BEGIN_DMS_MODULE_DECLARATION(DMS_MODULE_LOG)
BEGIN_FEATURE_COMMAND()
ADD_FEATURE_COMMAND(DMS_MODULE_LOG, DMS_MAIN_CMD_LOG, DMS_SUBCMD_GET_LOG_INFO,
                    NULL, NULL, DMS_SUPPORT_ROOT_ONLY, dms_get_kernel_log_from_logdrv)
END_FEATURE_COMMAND()
END_MODULE_DECLARATION()

int dms_log_init(void)
{
    CALL_INIT_MODULE(DMS_MODULE_LOG);
    dms_info("Dms log init success.\n");
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(dms_log_init, FEATURE_LOADER_STAGE_5);

void dms_log_uninit(void)
{
    CALL_EXIT_MODULE(DMS_MODULE_LOG);
    dms_info("Dms log uninit success.\n");
}
DECLAER_FEATURE_AUTO_UNINIT(dms_log_uninit, FEATURE_LOADER_STAGE_5);
