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

#ifndef VTSDRV_LOGIC_CQ_H
#define VTSDRV_LOGIC_CQ_H

int logic_sqcq_mbox_send(u32 devid, u32 tsid, u8 *mbox_msg);
int logic_cq_phy_cq_clean(struct tsdrv_id_inst *id_inst, u32 phy_cq, u32 logic_cqid, u32 type);

int logic_cq_vcq_handle(u32 devid, u32 tsid, u32 vcq_id, u32 vcq_tail);

#endif
