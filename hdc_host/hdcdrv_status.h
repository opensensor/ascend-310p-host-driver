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

#ifndef _HDCDRV_STATUS_H_
#define _HDCDRV_STATUS_H_

#include "drv_log.h"
#include "hdcdrv_core_com.h"

#define DMS_PCIE_CMD_NAME "DMS_PCIE"
#define DMS_INTER_CHIP_COMMUNICATION_CMD_NAME DMS_PCIE_CMD_NAME

int hdcdrv_dsmi_get_link_status(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len);
int hdcdrv_dsmi_p2p_com_set(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len);
void hdcdrv_dsmi_feature_init(void);
void hdcdrv_dsmi_feature_uninit(void);

struct hdcdrv_link_info_para_in {
    unsigned int dev_id;
    unsigned int sub_cmd;
};

typedef enum {
    PCIE_SUB_CMD_PCIE_INFO = 0,
    PCIE_SUB_CMD_MAX,
} HDCDRV_PCIE_SUB_CMD;
#endif
