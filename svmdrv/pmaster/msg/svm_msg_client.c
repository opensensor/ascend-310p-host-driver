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

#include "svm_cmd.h"
#include "devmm_channel.h"
#include "devmm_common.h"
#include "devmm_proc_info.h"
#include "svm_device_msg_client.h"
#include "svm_host_msg_client.h"
#include "svm_dev_res_mng.h"
#include "svm_msg_client.h"

typedef int (*svm_agent_msg_send_handle)(u32 agent_id, void *msg, u32 len, u32 out_len);

static svm_agent_msg_send_handle msg_send_handle[SVM_MAX_AGENT_NUM] = {NULL, };

ka_device_t *devmm_device_get_by_devid(u32 dev_id)
{
    struct devmm_dev_res_mng *dev_res_mng = NULL;
    struct svm_id_inst id_inst;

    if (dev_id < SVM_DEVICE_SIDE_AGENT_NUM) {
        svm_id_inst_pack(&id_inst, dev_id, 0);
        dev_res_mng = devmm_dev_res_mng_get(&id_inst);
        return (dev_res_mng != NULL) ? dev_res_mng->dev_msg_client.dev : NULL;
    }

    return NULL;
}

void devmm_device_put_by_devid(u32 dev_id)
{
    struct devmm_dev_res_mng *dev_res_mng = NULL;
    struct svm_id_inst id_inst;

    if (dev_id < SVM_DEVICE_SIDE_AGENT_NUM) {
        svm_id_inst_pack(&id_inst, dev_id, 0);
        dev_res_mng = devmm_dev_res_mng_get(&id_inst);
        if (dev_res_mng != NULL) {
            devmm_dev_res_mng_put(dev_res_mng);
            devmm_dev_res_mng_put(dev_res_mng); /* devmm_dev_res_mng_get */
        }
    }
}

bool devmm_device_is_ready(u32 dev_id)
{
    if (devmm_is_host_agent(dev_id)) {
#ifndef EMU_ST
        return devmm_host_agent_is_ready(dev_id);
#endif
    }

    if (dev_id < SVM_DEVICE_SIDE_AGENT_NUM) {
        return devmm_device_agent_is_ready(dev_id);
    }
#ifndef EMU_ST
    return false;
#endif
}

STATIC int devmm_send_msg_to_agent(void *msg, unsigned int len, unsigned int out_len)
{
    struct devmm_chan_msg_head *head = (struct devmm_chan_msg_head *)msg;
    u32 head_len = sizeof(struct devmm_chan_msg_head);
    u32 real_out_len = (out_len >= head_len) ? out_len : head_len;
    u32 agent_id = head->dev_id;
    int ret;

    if ((agent_id >= SVM_MAX_AGENT_NUM) || (msg_send_handle[agent_id] == NULL)) {
        devmm_drv_err("Agent_id is error. (agent_id=%d)\n", agent_id);
        return -EINVAL;
    }

    head->result = 0;
    /* devmm_chan_msg_head->devid; master to agent: dst agent id */
    ret = msg_send_handle[agent_id](agent_id, msg, len, real_out_len);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_err("Message send failed. (agent_id=%d; ret=%d) \n", agent_id, ret);
        return ret;
#endif
    }

    devmm_svm_stat_send_inc();

    return head->result;
}

int devmm_chan_msg_send_inner(void *msg, unsigned int len, unsigned int out_len)
{
    return devmm_send_msg_to_agent(msg, len, out_len);
}

int devmm_chan_msg_send(void *msg, unsigned int len, unsigned int out_len)
{
    return devmm_send_msg_to_agent(msg, len, out_len);
}
EXPORT_SYMBOL_UNRELEASE(devmm_chan_msg_send);

void devmm_init_msg(void)
{
    int i;

    for (i = 0; i < DEVMM_MAX_DEVICE_NUM; i++) {
        msg_send_handle[i] = devmm_device_chan_msg_send;
    }
    msg_send_handle[SVM_HOST_AGENT_ID] = NULL;
}
