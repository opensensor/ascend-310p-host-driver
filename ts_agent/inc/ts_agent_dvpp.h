/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
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
 * Create: 2022-08-10
 */

#ifndef TS_AGENT_DVPP_H
#define TS_AGENT_DVPP_H

typedef struct ts_agent_dvpp_ops {
    int (*dvpp_sqe_update)(u32 devid, u32 tsid, int pid, void *vsqe);
} ts_agent_dvpp_ops_t;

void tsagent_dvpp_register(ts_agent_dvpp_ops_t *ops);

void tsagent_dvpp_unregister(void);

#endif // TS_AGENT_DVPP_H
