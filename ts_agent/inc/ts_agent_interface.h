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

#ifndef TS_AGENT_INTERFACE_H
#define TS_AGENT_INTERFACE_H

#include <linux/types.h>
#include "hvtsdrv_tsagent.h"

int ts_agent_vf_create(u32 dev_id, u32 vf_id);

void ts_agent_vf_destroy(u32 dev_id, u32 vf_id);

int ts_agent_vsq_proc(struct tsdrv_id_inst *id_inst, u32 vsq_id, enum vsqcq_type vsq_type, u32 sqe_num);

int ts_agent_trans_mailbox_msg(void *mailbox_msg, u32 msg_len, struct hvtsdrv_trans_mailbox_ctx *trans_ctx);

struct ts_agent_update_sqe_ops {
    /* add this function for query rdma db pa */
    u64 (*rdma_query_db_pa)(u32 devid, u32 sqid, u32 qpn);
    /* when update ub db pa, add a new update func below */
};
void ts_agent_update_sqe_register(struct ts_agent_update_sqe_ops *ops);
void ts_agent_update_sqe_unregister(void);
void ts_agent_vf_rollback_proc(u32 dev_id, u32 vf_id, bool record[]);
#endif // TS_AGENT_INTERFACE_H
