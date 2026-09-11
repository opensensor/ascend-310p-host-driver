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

#ifndef HVTSDRV_LOGIC_CQ_H
#define HVTSDRV_LOGIC_CQ_H
#include "logic_cq.h"

int hvtsdrv_logic_refresh_alloc_msg(u32 devid, u32 tsid, u32 fid, struct logic_cqsq_mbox_msg *mbox_msg);
void hvtsdrv_logic_refresh_free_msg(u32 devid, u32 tsid, u32 fid, struct logic_cqsq_mbox_msg *mbox_msg);
int hvtsdrv_logic_cq_alloc(u32 devid, u32 tsid, u32 fid, ka_pid_t tgid, u8 *msg);
int hvtsdrv_logic_cq_free(u32 devid, u32 tsid, u32 fid, ka_pid_t tgid, u8 *msg);
int hvtsdrv_logic_cq_head_update(u32 devid, u32 tsid, u32 fid, ka_pid_t tgid, u8 *msg);

#endif
