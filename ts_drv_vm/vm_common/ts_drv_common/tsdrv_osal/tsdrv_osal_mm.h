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

#ifndef TSDRV_MM_H
#define TSDRV_MM_H
#include <linux/types.h>
#include "ka_memory_pub.h"
#include "ka_base_pub.h"
#include "ka_common_pub.h"

void *tsdrv_alloc_pages_exact_nid(int nid, size_t size, gfp_t gfp_mask);
void *tsdrv_alloc_pages_exact(size_t size, gfp_t gfp_mask);
void tsdrv_free_pages_exact(void *virt, size_t size);

#endif
