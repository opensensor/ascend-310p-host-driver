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
* Create: 2024-12-19
*/

#ifndef HCCS_INIT_H
#define HCCS_INIT_H

#include "dms_template.h"

#ifdef CFG_HOST_ENV
int dms_get_hccs_credit_info(void *feature, char *in, u32 in_len, char *out, u32 out_len);
#else
int dms_feature_get_hccs_info(void *feature, char *in, u32 in_len, char *out, u32 out_len);
int dms_hccs_credit_info_task_register(u32 dev_id);
int dms_hccs_credit_info_task_unregister(u32 dev_id);
int dms_hccs_statistic_task_register(u32 dev_id);
int dms_hccs_statistic_task_unregister(u32 dev_id);
int dms_hccs_feature_init(void);
void dms_hccs_feature_exit(void);
#endif

#define DMS_MODULE_HCCS "hccs"
INIT_MODULE_FUNC(DMS_MODULE_HCCS);
EXIT_MODULE_FUNC(DMS_MODULE_HCCS);

#endif