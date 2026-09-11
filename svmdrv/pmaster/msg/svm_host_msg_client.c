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

#include <linux/types.h>

#include "devmm_channel.h"
#include "devmm_proc_info.h"
#include "svm_host_msg_client.h"

svm_host_agent_msg_send_handle host_send_handle = NULL;

void devmm_register_host_agent_msg_send_handle(svm_host_agent_msg_send_handle func)
{
    host_send_handle = func;
}
KA_EXPORT_SYMBOL_GPL(devmm_register_host_agent_msg_send_handle);

bool devmm_host_agent_is_ready(u32 agent_id)
{
    (void)agent_id;
    return (host_send_handle != NULL);
}

int devmm_host_chan_msg_send(u32 agent_id, void *msg, u32 len, u32 out_len)
{
    if (!devmm_host_agent_is_ready(agent_id)) {
        devmm_drv_warn("Host agent not ready. (agent_id=%u)\n", agent_id);
        return -EINVAL;
    }

    return host_send_handle(agent_id, msg, len, out_len);
}

int devmm_host_chan_msg_recv(void *msg, unsigned int len, unsigned int out_len)
{
    u32 real_out_len;

    return devmm_chan_msg_dispatch(msg, len, out_len, &real_out_len, &devmm_channel_msg_processes[0]);
}
KA_EXPORT_SYMBOL_GPL(devmm_host_chan_msg_recv);

int devmm_host_msg_chan_init(void)
{
    return 0;
}

void devmm_host_msg_chan_uninit(void)
{
}

