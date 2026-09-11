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
*/

#ifndef __DMS_HOST_AICPU_INFO_H
#define __DMS_HOST_AICPU_INFO_H

#define DMS_MODULE_HOST_AICPU "dms_host_aicpu"

#define CPUINFO_FILE_LEN 256
#define BITMAP_MAX_LEN 8
struct dms_host_aicpu_info {
    unsigned int num;
    unsigned long long bitmap[BITMAP_MAX_LEN];
    unsigned int work_mode;
    unsigned int frequency;
};

int dms_host_aicpu_init(void);
void dms_host_aicpu_exit(void);
int dms_set_host_aicpu_info(void *feature, char *in, u32 in_len, char *out, u32 out_len);
int dms_get_host_aicpu_info(void *feature, char *in, u32 in_len, char *out, u32 out_len);

#endif