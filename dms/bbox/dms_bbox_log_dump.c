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
#include "devdrv_manager.h"
#include "urd_acc_ctrl.h"
#ifdef CFG_FEATURE_LOG_DUMP_FROM_PCIE
#include "devdrv_pcie.h"
#endif
#include "dms_bbox_log_dump.h"

#ifdef DMS_UT
#define STATIC
#else
#define STATIC static
#endif

STATIC int dms_devlog_dump_from_logdrv(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len)
{
    int ret;

    if (in == NULL) {
        dms_err("Invalid parameter, in is NULL.\n");
        return -EINVAL;
    }

    if (in_len != sizeof(struct devdrv_bbox_logdump)) {
        dms_err("Error parameter. (in_len=%u; correct in_len=%lu)\n", in_len, sizeof(struct devdrv_bbox_logdump));
        return -EINVAL;
    }

    ret = devdrv_manager_devlog_dump((struct devdrv_bbox_logdump *)in);
    if (ret != 0) {
        return ret;
    }

    return 0;
}

#ifdef CFG_FEATURE_LOG_DUMP_FROM_PCIE
STATIC int dms_devlog_dump_from_pcie(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len)
{
    int ret;

    if (in == NULL) {
        dms_err("Invalid parameter. (in=%s)\n", (in == NULL) ? "NULL" : "OK");
        return -EINVAL;
    }

    if (in_len != sizeof(struct devdrv_bbox_pcie_logdump)) {
        dms_err("Error parameter. (in_len=%u; correct in_len=%lu)\n", in_len, sizeof(struct devdrv_bbox_pcie_logdump));
        return -EINVAL;
    }

    ret = devdrv_pcie_devlog_dump((struct devdrv_bbox_pcie_logdump *)in);
    if (ret != 0) {
        return ret;
    }

    return 0;
}
#endif

BEGIN_DMS_MODULE_DECLARATION(DMS_MODULE_BBOX)
BEGIN_FEATURE_COMMAND()
ADD_FEATURE_COMMAND(DMS_MODULE_BBOX, DMS_MAIN_CMD_BBOX, DMS_SUBCMD_GET_LOG_DUMP_INFO, NULL, NULL,
    DMS_ACC_NOT_LIMIT_USER | DMS_ENV_NOT_NORMAL_DOCKER | DMS_VDEV_NOTSUPPORT, dms_devlog_dump_from_logdrv)

#ifdef CFG_FEATURE_LOG_DUMP_FROM_PCIE
ADD_FEATURE_COMMAND(DMS_MODULE_BBOX, DMS_MAIN_CMD_BBOX, DMS_SUBCMD_GET_PCIE_LOG_DUMP_INFO, NULL, NULL,
    DMS_ACC_NOT_LIMIT_USER | DMS_ENV_NOT_NORMAL_DOCKER | DMS_VDEV_NOTSUPPORT, dms_devlog_dump_from_pcie)
#endif
END_FEATURE_COMMAND()
END_MODULE_DECLARATION()

int dms_bbox_init(void)
{
    CALL_INIT_MODULE(DMS_MODULE_BBOX);
    dms_info("Dms bbox init success.\n");
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(dms_bbox_init, FEATURE_LOADER_STAGE_5);

void dms_bbox_uninit(void)
{
    CALL_EXIT_MODULE(DMS_MODULE_BBOX);
    dms_info("Dms bbox uninit success.\n");
}
DECLAER_FEATURE_AUTO_UNINIT(dms_bbox_uninit, FEATURE_LOADER_STAGE_5);
