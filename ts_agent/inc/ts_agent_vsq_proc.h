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

#ifndef TS_AGENT_VSQ_PROC_H
#define TS_AGENT_VSQ_PROC_H

#include "ts_agent_common.h"

void proc_vsq(vsq_base_info_t *vsq_base_info);
int vsq_top_proc(vsq_base_info_t *vsq_base_info, u32 cmd_num);
void init_task_convert_func(void);
#endif // TS_AGENT_VSQ_PROC_H
