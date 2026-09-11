/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
* Create: 2025-07-11
*/

#ifndef __DMS_SDK_EX_VERSION_H
#define __DMS_SDK_EX_VERSION_H

#ifdef CFG_FEATURE_SDK_EX_VERSION
int dms_get_sdk_ex_version(void *feature, char *in, u32 in_len, char *out, u32 out_len);
int dms_set_sdk_ex_version(void *feature, char *in, u32 in_len, char *out, u32 out_len);
#else
static inline int dms_get_sdk_ex_version(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    return -EOPNOTSUPP;
}
static inline int dms_set_sdk_ex_version(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    return -EOPNOTSUPP;
}
#endif

#endif