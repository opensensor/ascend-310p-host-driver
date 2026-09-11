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

#ifndef SVM_HOST_MSG_CLIENT_H
#define SVM_HOST_MSG_CLIENT_H

#include <linux/types.h>
#include "devmm_channel.h"

int devmm_host_msg_chan_init(void);
void devmm_host_msg_chan_uninit(void);
int devmm_host_chan_msg_send(u32 agent_id, void *msg, u32 len, u32 out_len);
bool devmm_host_agent_is_ready(u32 agent_id);
int devmm_host_chan_msg_recv(void *msg, unsigned int len, unsigned int out_len);
void devmm_register_host_agent_msg_send_handle(svm_host_agent_msg_send_handle func);
#endif

