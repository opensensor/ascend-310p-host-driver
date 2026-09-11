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

#ifndef DMS_CHIP_INFO_H
#define DMS_CHIP_INFO_H

#include "drv_type.h"

int dms_feature_get_chip_info(void *feature, char *in, u32 in_len, char *out, u32 out_len);
int dms_trans_and_check_id(u32 logical_dev_id, u32 *physical_dev_id, u32 *vfid);
#ifdef CFG_FEATURE_REBOOT_REASON
int dms_get_reboot_reason(void *feature, char *in, u32 in_len, char *out, u32 out_len);
#endif
#endif
