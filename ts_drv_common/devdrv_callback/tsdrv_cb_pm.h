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

#ifndef TSDRV_CB_PM_H
#define TSDRV_CB_PM_H

#include "devdrv_cb.h"

int callback_get_physic_sq(struct callback_phy_sqcq *cb_sqcq);
int callback_get_physic_cq(struct callback_phy_sqcq *cb_sqcq);
void callback_free_physic_sq(struct callback_phy_sqcq *cb_sqcq);
void callback_destory_cq(struct callback_phy_sqcq *cb_sqcq);
s32 callback_mbox_send(u32 type, struct callback_phy_sqcq *cb_sqcq);
s32 callback_mbox_logic_cq(u32 cmd, struct callback_phy_sqcq *phy_sqcq,
    struct callback_logic_cq_sub *sub_cq);
int callback_set_doorbell(struct callback_phy_sqcq *cb_sqcq, u32 idx, u32 val);
void callback_func_lock(struct callback_info *cb_info);
void callback_func_unlock(struct callback_info *cb_info);
void callback_irq_lock(struct callback_phy_sqcq_info *phy_sq);
void callback_irq_unlock(struct callback_phy_sqcq_info *phy_sq);

#endif

