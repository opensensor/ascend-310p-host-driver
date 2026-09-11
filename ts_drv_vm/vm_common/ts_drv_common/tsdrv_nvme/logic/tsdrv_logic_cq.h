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

#ifndef TSDRV_LOGIC_CQ_H
#define TSDRV_LOGIC_CQ_H

#include "ka_task_pub.h"

void logic_sqcq_phy_cq_handler(u32 devid, u32 tsid, struct devdrv_ts_cq_info *cq_info);
int logic_sqcq_mbox_send(u32 devid, u32 tsid, u8 *mbox_msg);
int logic_cq_phy_cq_clean(struct tsdrv_id_inst *id_inst, u32 phy_cq, u32 logic_cqid, u32 type);
bool tsdrv_is_logic_cq_belong_to_proc(struct tsdrv_ctx *ctx, u32 tsid, u32 logic_cqid);
void tsdrv_parse_cqe_to_logic_cq(struct tsdrv_ts_resource *ts_res, struct devdrv_ts_cq_info *cq_info);
struct devdrv_ctrl_report *tsdrv_get_ctrl_report(struct devdrv_ts_cq_info *cq_info);
#endif
