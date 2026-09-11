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
#include "dms_flash.h"
#include "securec.h"

#include "ka_memory_pub.h"

#ifdef DMS_UT
#define STATIC
#else
#define STATIC static
#endif

#define DSMI_MAIN_CMD_FLASH_LEN   20
STATIC char g_dsmi_main_cmd_flash[DSMI_MAIN_CMD_FLASH_LEN] = {0};
STATIC uint8_t *g_flash_erase_count = NULL;

STATIC int feature_para_check(char *in, u32 in_len, char *out, u32 out_len)
{
    if ((in == NULL) || (in_len != sizeof(struct dms_get_device_info_in))) {
        dms_err("input char is NULL or in_len is wrong. (in_len=%u; correct_in_len=%u)\n",
            in_len, sizeof(struct dms_get_device_info_in));
        return -EINVAL;
    }

    if ((out == NULL) || (out_len != sizeof(struct dms_get_device_info_out))) {
        dms_err("output char is NULL or out_len is wrong. (out_len=%u; correct_out_len=%u)\n",
            out_len, sizeof(struct dms_get_device_info_out));
        return -EINVAL;
    }

    return 0;
}

STATIC int dms_flash_get_erase_count(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len)
{
    int32_t ret;
    struct dms_get_device_info_in *in_cfg = (struct dms_get_device_info_in *)in;
    struct dms_get_device_info_out *out_cfg = (struct dms_get_device_info_out *)out;

    ret = feature_para_check(in, in_len, out, out_len);
    if (ret != 0) {
        dms_err("para check fail.\n");
        return -EINVAL;
    }
    if ((in_cfg->buff == NULL) ||
        (in_cfg->buff_size != DSMI_OUT_LEN_EACH_READ) ||
        (in_cfg->sub_cmd >= DSMI_READ_TIMES)) {
            dms_err("invalid param(buff=%u, buff_size=%u, sub_cmd=%u).\n",
                (in_cfg->buff != NULL), in_cfg->buff_size, in_cfg->sub_cmd);
            return -EINVAL;
    }

    if (in_cfg->sub_cmd == 0) {
        ret = sec_read_and_clear_erase_count(0, (uint16_t *)g_flash_erase_count, FLASH_ERASE_COUNT_LEN);
        if (ret != 0) {
            dms_err("sec_read_and_clear_erase_count fail(ret=%d).\n", ret);
            return -EFAULT;
        }
    }

    if (copy_to_user(in_cfg->buff, g_flash_erase_count + in_cfg->sub_cmd * DSMI_OUT_LEN_EACH_READ,
            DSMI_OUT_LEN_EACH_READ)) {
        dms_err("copy_to_user fail.\n");
        return -EFAULT;
    }
    out_cfg->out_size = DSMI_OUT_LEN_EACH_READ;

    return 0;
}

BEGIN_DMS_MODULE_DECLARATION(DMS_MODULE_FLASH)
BEGIN_FEATURE_COMMAND()
ADD_FEATURE_COMMAND(DMS_MODULE_FLASH, DMS_GET_GET_DEVICE_INFO_CMD, ZERO_CMD, g_dsmi_main_cmd_flash,
                    NULL, DMS_SUPPORT_ALL, dms_flash_get_erase_count)
END_FEATURE_COMMAND()
END_MODULE_DECLARATION()

int dms_flash_init(void)
{
    int32_t ret;

    ret = sprintf_s(g_dsmi_main_cmd_flash, sizeof(g_dsmi_main_cmd_flash), "main_cmd=0x%x", DMS_MAIN_CMD_FLASH);
    if (ret <= 0) {
        dms_info("sprintf_s main_cmd failed.(ret=%d, main_cmd=0x%x)\n", ret, DMS_MAIN_CMD_FLASH);
        return ret;
    }

    g_flash_erase_count = (uint8_t *)kzalloc(FLASH_ERASE_COUNT_LEN, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (IS_ERR_OR_NULL(g_flash_erase_count)) {
        dms_err("kzalloc fail.\n");
        return -ENOMEM;
    }

    CALL_INIT_MODULE(DMS_MODULE_FLASH);
    dms_info("Dms flash module init success.\n");
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(dms_flash_init, FEATURE_LOADER_STAGE_5);

void dms_flash_uninit(void)
{
    kfree(g_flash_erase_count);
    g_flash_erase_count = NULL;
    CALL_EXIT_MODULE(DMS_MODULE_FLASH);
    dms_info("Dms flash module uninit success.\n");
}
DECLAER_FEATURE_AUTO_UNINIT(dms_flash_uninit, FEATURE_LOADER_STAGE_5);
