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

#ifndef DEVDRV_HVDEVMNG_INIT__HOST_H
#define DEVDRV_HVDEVMNG_INIT__HOST_H

#include "hvdevmng_common.h"

int hvdevmng_init(void);
void hvdevmng_uninit(void);
int hvdevmng_set_core_num(u32 devid, u32 fid, u32 dtype);
int hvdevmng_get_aicore_num(u32 devid, u32 fid, u32 *aicore_num);
int hvdevmng_get_aicpu_num(u32 devid, u32 fid, u32 *aicpu_num, u32 *aicpu_bitmap);
int hvdevmng_get_vector_core_num(u32 devid, u32 fid, u32 *vectore_core_num);
int hvdevmng_get_template_name(u32 dev_id, u32 fid, u8 *name, u32 name_len);

#endif
