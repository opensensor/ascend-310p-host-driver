/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2022. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Description:
 * Author: huawei
 * Create: 2019-10-15
 */
#ifndef DEVDRV_CB_STUB_H
#define DEVDRV_CB_STUB_H

#include <linux/types.h>

#include "devdrv_cbsqcq_common.h"
#include "devdrv_cb.h"

s32 callback_mbox_send(u32 type, struct callback_phy_sqcq *cb_sqcq);
int callback_set_doorbell(struct callback_phy_sqcq *cb_sqcq, u32 index, u32 val);
void callback_recycle_logic_cq(u32 devid, u32 fid, u32 tsid, struct tsdrv_ctx *ctx);
struct callback_phy_sqcq *callback_physic_sqcq_get(u32 devid, u32 fid, u32 tsid);
void callback_cq_proc(u32 devid, u32 fid, u32 tsid, u32 cqid);
s32 callback_cq_match(u32 fid, u32 cqid);
void callback_physic_sqcq_exit(struct tsdrv_device *tsdrv_dev, u32 fid, u32 tsid);

#endif /* __DEVDRV_CB_STUB_H */
