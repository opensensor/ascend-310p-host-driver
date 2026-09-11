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
 * Create: 2025-08-29
 */

#ifndef _UDIS_CMD_H_
#define _UDIS_CMD_H_
#include "udis.h"

#define UDIS_FUNC_MAX_NUM 10

struct udis_func_info {
    UDIS_MODULE_TYPE module_type;
    char name[UDIS_MAX_NAME_LEN];
    udis_trigger func;
};

int udis_feature_get_device_info(void *feature, char *in, u32 in_len, char *out, u32 out_len);
int udis_feature_set_device_info(void *feature, char *in, u32 in_len, char *out, u32 out_len);

#endif