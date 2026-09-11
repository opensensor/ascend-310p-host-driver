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
* Create: 2024-03-29
*/
#ifndef DMS_DEV_INFO_CHECK_H
#define DMS_DEV_INFO_CHECK_H

#include "urd_define.h"
#include "dms_cmd_def.h"

int dms_dev_info_args_check(const struct dms_get_device_info_in *input, const u32 in_len,
    const struct dms_get_device_info_out *output, const u32 out_len, const u32 min_buff);

#endif