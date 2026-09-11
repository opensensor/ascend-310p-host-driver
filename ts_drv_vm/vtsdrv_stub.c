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

#ifndef TSDRV_KERNEL_UT
#include <linux/types.h>

#include "ka_memory_pub.h"

#define MAX_AICPU_CORE_NUM 32U

void hvtsdrv_cb_cq_handler(u32 devid, u32 fid, u32 tsid, u32 cq_id)
{
}

s32 hvtsdrv_cb_inform_tsagent_vsq_tail(u32 devid, u32 fid, u32 vsqid, u32 tail)
{
    return 0;
}

int hvdevmng_get_aicore_num(u32 devid, u32 fid, u32 *aicore_num)
{
    *aicore_num = MAX_AICPU_CORE_NUM;
    return 0;
}

u32 devdrv_get_dev_chip_type(u32 dev_id)
{
    return 1;
}

void hvdevmng_set_dev_ts_resource(u32 devid, u32 fid, u32 tsid, void *data)
{
    return;
}

void tsdrv_dma_sync_cpu(u32 devid, dma_addr_t dma_addr, size_t size, ka_dma_data_direction_t dir)
{
    return;
}
#else
void hvdevmng_set_dev_ts_resource(void)
{
    return;
}
#endif
