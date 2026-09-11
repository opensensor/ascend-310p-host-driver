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

#include "dms_template.h"
#include "dms_cmd_def.h"
#include "dms_acc_ctrl.h"
#include "hdcdrv_core.h"
#include "hdcdrv_status.h"

#ifdef STATIC_SKIP
    #define STATIC
#else
    #define STATIC static
#endif
#define P2P_DSMI_MAIN_CMD_LEN   20
static char g_p2p_dsmi_main_cmd[P2P_DSMI_MAIN_CMD_LEN] = {0};

// DMS_SUPPORT_ALL
BEGIN_DMS_MODULE_DECLARATION(DMS_INTER_CHIP_COMMUNICATION_CMD_NAME)
BEGIN_FEATURE_COMMAND()
ADD_FEATURE_COMMAND(DMS_INTER_CHIP_COMMUNICATION_CMD_NAME, DMS_MAIN_CMD_PCIE, DMS_SUBCMD_GET_PCIE_LINK_INFO, NULL, NULL,
                    DMS_SUPPORT_ALL, hdcdrv_dsmi_get_link_status)
ADD_FEATURE_COMMAND(DMS_INTER_CHIP_COMMUNICATION_CMD_NAME, DMS_GET_SET_DEVICE_INFO_CMD, ZERO_CMD, g_p2p_dsmi_main_cmd,
                    "dmp_daemon", DMS_SUPPORT_ALL, hdcdrv_dsmi_p2p_com_set)
END_FEATURE_COMMAND()
END_MODULE_DECLARATION()

STATIC int hdcdrv_link_para_check(char *in, u32 in_len, u32 correct_in_len,
                                  char *out, u32 out_len, u32 correct_out_len)
{
    if ((in == NULL) || (in_len != correct_in_len)) {
        hdcdrv_err("input char is null or in_len is wrong. (in_len=%u; correct_in_len=%u)\n",
            in_len, correct_in_len);
        return HDCDRV_ERR;
    }

    if ((out == NULL) || (out_len != correct_out_len)) {
        hdcdrv_err("output char is null or out_len is wrong. (out_len=%u; correct_out_len=%u)\n",
            out_len, correct_out_len);
        return HDCDRV_ERR;
    }

    return HDCDRV_OK;
}

int hdcdrv_dsmi_get_link_status(void *feature,
    char *in, unsigned int in_len, char *out, unsigned int out_len)
{
    int ret;
    struct hdcdrv_link_info_para_in *in_cfg = NULL;
    struct devdrv_pcie_link_info_para link_info = {0};

    (void)feature;

    ret = hdcdrv_link_para_check(in, in_len, (u32)sizeof(struct hdcdrv_link_info_para_in),
                                 out, out_len, (u32)sizeof(struct devdrv_pcie_link_info_para));
    if (ret != 0) {
        return -EINVAL;
    }

    in_cfg = (struct hdcdrv_link_info_para_in *)in;
    if (in_cfg->sub_cmd != PCIE_SUB_CMD_PCIE_INFO) {
        hdcdrv_err("input pararmeter is invalid.(sub_cmd=%d)\n", in_cfg->sub_cmd);
        return -EINVAL;
    }

    link_info.link_status = HDCDRV_HDC_DISCONNECT;
    ret = hdcdrv_get_link_status(&link_info);
    if (ret != 0) {
        return ret;
    }

    *(struct devdrv_pcie_link_info_para*)out = link_info;
    return HDCDRV_OK;
}

int hdcdrv_dsmi_p2p_com_set(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len)
{
    struct dms_set_device_info_in *in_cfg = NULL;
    u32 sub_cmd;
    int ret;

    if ((in == NULL) || (in_len != sizeof(struct dms_set_device_info_in))) {
        hdcdrv_err("Input char is NULL or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }

    in_cfg = (struct dms_set_device_info_in *)in;
    sub_cmd = in_cfg->sub_cmd;
    switch (sub_cmd) {
        case DMS_SUBCMD_P2P_COM_FORCE_LINKDOWN:
            ret = hdcdrv_force_link_down();
            break;
        default:
            hdcdrv_err("input subcmd is wrong. (subcmd=%u)\n", sub_cmd);
            ret = -EINVAL;
            break;
    }

    return ret;
}
void hdcdrv_dsmi_feature_init(void)
{
#ifdef CFG_FEATURE_HDC_REG_MEM
    int ret;
    hdcdrv_info("hdcdrv_dsmi_feature_init.\n");
    ret = sprintf_s(g_p2p_dsmi_main_cmd, sizeof(g_p2p_dsmi_main_cmd),
        "main_cmd=0x%x", DMS_MAIN_CMD_P2P_COM);
    if (ret <= 0) {
        hdcdrv_err("sprintf_s can main_cmd failed.(ret=%d, main_cmd=0x%x)\n", ret, DMS_MAIN_CMD_P2P_COM);
        return;
    }
    CALL_INIT_MODULE(DMS_INTER_CHIP_COMMUNICATION_CMD_NAME);
#endif
    return;
}

void hdcdrv_dsmi_feature_uninit(void)
{
#ifdef CFG_FEATURE_HDC_REG_MEM
    hdcdrv_info("hdcdrv_dsmi_feature_uninit.\n");
    CALL_EXIT_MODULE(DMS_INTER_CHIP_COMMUNICATION_CMD_NAME);
#endif
    return;
}
