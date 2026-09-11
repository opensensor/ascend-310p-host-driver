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

#ifndef TSDRV_DELAY_RECYCLE_H
#define TSDRV_DELAY_RECYCLE_H

#include <linux/types.h>

void tsdrv_add_mem_delay_free(u32 devid, void *vaddr, u32 size);
void tsdrv_mem_delay_free_init(u32 devid);
void tsdrv_delay_free_mem(u32 devid);

#endif
