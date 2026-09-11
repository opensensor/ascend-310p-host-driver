/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
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
 * Create: 2023-3-3
 */
#ifndef AOS_LLVM_BUILD

#ifdef CFG_FEATURE_EMMC_INFO_FROM_DRIVER
#include <linux/mmc/mmc.h>
#endif
#include "dms_emmc_info.h"
#include "dms_template.h"
#include "dms_cmd_def.h"
#include "devdrv_user_common.h"
#include "dms_acc_ctrl.h"
#include "uda.h"

#ifndef STATIC
#define STATIC static
#endif

STATIC int dms_get_emmc_info_check_para(char *in, unsigned int in_len, char *out, unsigned int out_len)
{
    struct devdrv_emmc_info_para *ioctl_para = NULL;
    unsigned int check_size[MAX_EMMC_INFO_TYPE] = {0};
    unsigned int dev_num;

    if ((in == NULL) || (in_len != sizeof(struct devdrv_emmc_info_para)) || (out == NULL)) {
        dms_err("Invalid parameter. (in_buff=%s,in_len=%u; out_buff=%s)\n",
            (in == NULL) ? "NULL" : "OK", in_len, (out == NULL) ? "NULL" : "OK");
        return -EINVAL;
    }
    ioctl_para = (struct devdrv_emmc_info_para *)in;
    dev_num = uda_get_dev_num();
    if (ioctl_para->dev_id >= dev_num) {
        dms_err("Device id not valid. (dev_id=%u; dev_num=%u)\n", ioctl_para->dev_id, dev_num);
        return -ENODEV;
    }
    if (ioctl_para->info_type > DMS_EMMC_SUB_CMD_MANUF_STATUS) {
        return -EINVAL;
    }
    check_size[DMS_EMMC_SUB_CMD_STATUS] = sizeof(struct dms_emmc_status_stru);
    check_size[DMS_EMMC_SUB_CMD_MANUF_STATUS] = DMS_EMMC_MANUF_STATUS_LEN;
    if (out_len < check_size[ioctl_para->info_type]) {
        dms_err("para check failed, (out_len=%u)\n", out_len);
        return -EINVAL;
    }
    return 0;
}

STATIC int dms_get_emmc_info(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len)
{
    int ret;
#ifdef CFG_FEATURE_EMMC_INFO_FROM_DRIVER
    struct devdrv_emmc_info_para *ioctl_para = (struct devdrv_emmc_info_para *)in;
    unsigned int in_size;
    void *in_buf = NULL;
    unsigned int out_size = 0;
    struct dms_emmc_status_stru emmc_status = {0};
    unsigned char manu_status[DMS_EMMC_MANUF_STATUS_LEN] = {0};
#endif

    dms_info("dms_get_emmc_info\n");
    ret = dms_get_emmc_info_check_para(in, in_len, out, out_len);
    if (ret != 0) {
        dms_err("get emmc info check para fail, (ret=%d)\n", ret);
        return ret;
    }

#ifdef CFG_FEATURE_EMMC_INFO_FROM_DRIVER
    switch (ioctl_para->info_type) {
        case DMS_EMMC_SUB_CMD_STATUS:
            in_size = sizeof(struct dms_emmc_status_stru);
            in_buf = (void *)(&emmc_status);
            break;
        case DMS_EMMC_SUB_CMD_MANUF_STATUS:
            in_size = DMS_EMMC_MANUF_STATUS_LEN;
            in_buf = (void *)manu_status;
            break;
        default:
            return -EINVAL;
    }
    ret = get_emmc_info(ioctl_para->info_type, in_buf, in_size, &out_size);
    if (ret != 0) {
        dms_err("get emmc std status failed, (ret=%d)\n", ret);
        return ret;
    }
    ret = memcpy_s(out, out_len, in_buf, out_size);
    if (ret != 0) {
        dms_err("copy emmc status fail\n");
        return ret;
    }
#endif
    dms_info("get emmc status success.\n");
    return 0;
}

BEGIN_DMS_MODULE_DECLARATION(DMS_MODULE_EMMC)
BEGIN_FEATURE_COMMAND()
ADD_FEATURE_COMMAND(DMS_MODULE_EMMC, DMS_MAIN_CMD_EMMC, DMS_SUBCMD_GET_EMMC_INFO, NULL, NULL, DMS_SUPPORT_ALL,
    dms_get_emmc_info)
END_FEATURE_COMMAND()
END_MODULE_DECLARATION()

int dms_emmc_init(void)
{
    dms_info("dms_emmc_init\n");
    CALL_INIT_MODULE(DMS_MODULE_EMMC);
    return 0;
}

void dms_emmc_uninit(void)
{
    dms_info("dms_emmc_uninit\n");
    CALL_EXIT_MODULE(DMS_MODULE_EMMC);
}

#endif
