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

#ifndef HCCS_CREDIT_H
#define HCCS_CREDIT_H

#include <linux/time.h>

#define MACRO_PORT_NUM 8
typedef struct hccs_credit_num {
    unsigned int pa_credit_num[MACRO_PORT_NUM];
    unsigned long long query_cnt;
} hccs_credit_num;

#define DMS_HCCS_MAX_PCS_NUM        (16)
typedef struct hccs_credit_info {
    unsigned int credit_num[DMS_HCCS_MAX_PCS_NUM];
    unsigned int reserve[DMS_HCCS_MAX_PCS_NUM * 4];
} hccs_credit_info_t;

typedef struct hccs_credit_update_info {
    unsigned long long old_cnt;
    unsigned long long log_cnt;
    unsigned long long old_update_timestamp;
} hccs_credit_update_info;

#endif