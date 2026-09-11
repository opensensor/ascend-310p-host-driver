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

#ifndef TS_AGENT_VSQ_WORKER_H
#define TS_AGENT_VSQ_WORKER_H

#include <linux/types.h>
#include "hvtsdrv_tsagent.h"

int init_all_vf_work_ctx(void);

void destroy_all_vf_work_ctx(void);

int create_vf_worker(u32 dev_id, u32 vf_id, u32 ts_id, u32 vsq_num);

void destroy_vf_worker(u32 dev_id, u32 vf_id, u32 ts_id);

int schedule_vsq_work(const struct tsdrv_id_inst * const id_inst, u32 vsq_id,
    enum vsqcq_type vsq_type, u32 cmd_num);

#endif // TS_AGENT_VSQ_WORKER_H
