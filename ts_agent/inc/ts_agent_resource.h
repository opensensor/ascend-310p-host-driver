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

#ifndef TS_AGENT_RESOURCE_H
#define TS_AGENT_RESOURCE_H

#include <linux/types.h>
#include "ts_agent_common.h"

int get_vf_info(u32 dev_id, u32 vf_id, struct vmng_soc_resource_enquire *vf_res);

int get_vf_vsq_num(u32 dev_id, u32 vf_id, u32 ts_id, u32 *vsq_num);

int convert_sq_id(const vsq_base_info_t *vsq_base_info, u16 v_sq_id, u16 *sq_id);

int convert_stream_id(const vsq_base_info_t *vsq_base_info, u16 v_stream_id, u16 *stream_id);

int convert_event_id(const vsq_base_info_t *vsq_base_info, u16 v_event_id, u16 *event_id);

int convert_notify_id(const vsq_base_info_t *vsq_base_info, u16 v_notify_id, u16 *notify_id);

int convert_model_id(const vsq_base_info_t *vsq_base_info, u16 v_model_id, u16 *model_id);

int fill_vsq_info(vsq_base_info_t *vsq_base_info);

int get_vsq_head_and_tail(const vsq_base_info_t *vsq_base_info, u32 *head, u32 *tail);

#endif // TS_AGENT_RESOURCE_H
