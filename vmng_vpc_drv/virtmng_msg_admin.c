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

#include "virtmng_msg_admin.h"
#include "virtmng_public_def.h"

#include "ka_barrier_pub.h"
#include "ka_kernel_def_pub.h"

int vmng_admin_msg_send(struct vmng_msg_chan_tx *msg_chan, struct vmng_tx_msg_proc_info *tx_info, u32 opcode_d1,
    u32 opcode_d2)
{
    const u32 WAIT_TIME_LEN = VMNG_WAIT_TIME_LEN;
    const u32 WAIT_CYCLE = VMNG_WAIT_CYCLE;
    u32 *p_sq_status = NULL; /* srv and agent shr status. */
    int ret;
    struct vmng_msg_dev *msg_dev = NULL;

    if (msg_chan == NULL) {
        vmng_err("Input parameter is error.\n");
        return -EINVAL;
    }

    if (msg_chan->send_irq_to_remote == NULL) {
        vmng_err("Input parameter is error. (chan_id=%u)\n", msg_chan->chan_id);
        return -EINVAL;
    }

    ka_task_mutex_lock(&msg_chan->mutex);
    ret = vmng_msg_fill_desc(tx_info, opcode_d1, opcode_d2, msg_chan, &p_sq_status);
    if (ret != 0) {
        ka_task_mutex_unlock(&msg_chan->mutex);
        vmng_err("Fill descriptor failed. (ret=%d)\n", ret);
        return ret;
    }

    *p_sq_status = VMNG_MSG_SQ_STATUS_PREPARE;
    msg_dev = (struct vmng_msg_dev *)msg_chan->msg_dev;

    ka_wmb();
    msg_chan->send_irq_to_remote(msg_chan->msg_dev, msg_chan->tx_send_irq);

    ret = vmng_sync_msg_wait_undesire(p_sq_status, VMNG_MSG_SQ_STATUS_PREPARE, VMNG_MSG_SQ_STATUS_ENTER_PROC,
        WAIT_CYCLE, WAIT_TIME_LEN);
    if (ret < 0) {
        ka_task_mutex_unlock(&msg_chan->mutex);
        vmng_err("Wait rx time out. (status=0x%x; dev_id=%d; fid=%d)\n", *p_sq_status, msg_dev->dev_id, msg_dev->fid);
        return ret;
    }

    ret = vmng_msg_reply_data(msg_chan, tx_info->data, tx_info->out_data_len, &tx_info->real_out_len);
    if (ret < 0) {
        ka_task_mutex_unlock(&msg_chan->mutex);
        vmng_err("Call vmng_msg_reply_data failed.\n");
        return ret;
    }
    ka_task_mutex_unlock(&msg_chan->mutex);

    return ret;
}
KA_EXPORT_SYMBOL(vmng_admin_msg_send);
