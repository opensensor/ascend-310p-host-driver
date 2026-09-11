/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
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
* Create: 2024-6-29
*/

#ifndef _HDCDRV_PFSTAT_H_
#define _HDCDRV_PFSTAT_H_

#include "pfstat_interface.h"
#include "hdcdrv_core.h"
#include "hdcdrv_core_com.h"

#define HDCDRV_TX_FINISH_TH          0ULL

enum latency_dot_idx {
	TX_FINISH_LATENCY = 0,
	TX_SEND_LATENCY,
	TX_WAIT_SQ_LATENCY,
	TX_FINISH_SCHE_LATENCY,
	RX_RECV_SCHE_LATENCY,
	DMA_COPY_LATENCY,
	RX_TASK_SCHE_LATENCY,
    TX_SQ_DMA_LATENCY,
    TX_CQ_DMA_LATENCY,
	SYNC_MSG_LATENCY,

	LATENCY_DOT_IDX_MAX
};

#define PF_MSG_CHAN_LATENCY_STATS_ITEM LATENCY_DOT_IDX_MAX

typedef int (*FP_PFSTAT_CREATE_LATENCY)(const struct pfstat_latency_attr *attr, struct pfstat_stat_handle **handle);
typedef int (*FP_PFSTAT_DELETE_LATENCY)(struct pfstat_stat_handle *handle);
typedef int (*FP_PFSTAT_GET_TIMESTAMP)(unsigned long long *timestamp);
typedef unsigned long long (*FP_PFSTAT_GET_PERIOD)(unsigned long long before, unsigned long long after);
typedef int (*FP_PFSTAT_UPDATE_LATENCY_ITEM)(
	struct pfstat_stat_handle *handle, unsigned int item_index, unsigned long long latency);

struct pfstat_opt_hook {
	bool opt_avail;
	FP_PFSTAT_CREATE_LATENCY pfstat_create_latency;
	FP_PFSTAT_DELETE_LATENCY pfstat_delete_latency;
	FP_PFSTAT_GET_TIMESTAMP pfstat_get_timestamp;
	FP_PFSTAT_GET_PERIOD pfstat_get_period;
	FP_PFSTAT_UPDATE_LATENCY_ITEM pfstat_update_latency_item;
};

struct pfstat_priv_info {
	struct pfstat_stat_handle *update_handle;
	struct mutex mutex;
};

struct pfstat_latency_info {
	struct pfstat_latency_attr *pf_latency_attr;
};

struct hdcdrv_pfstat_priv {
    struct pfstat_opt_hook pfstat_opts;
	struct pfstat_latency_info latency_info;
    struct pfstat_priv_info priv_info[HDCDRV_PFSTATE_MAX_CHAN];
    bool pfstat_init_finish;
};

int hdcdrv_pfstat_init(void);
void hdcdrv_pfstat_exit(void);
int hdcdrv_pfstat_create_stats_handle(u32 chan_id);
int hdcdrv_pfstat_delete_stats_handle(u32 chan_id);
void hdcdrv_pfstat_get_timestamp(u64 *timestamp);
void hdcdrv_pfstat_update_latency(u32 chan_id, enum latency_dot_idx latency_dot, u64 before_time);

#endif
