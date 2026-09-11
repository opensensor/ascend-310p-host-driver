/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
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
 * Create: 2022-08-13
 */
#include "devdrv_manager_comm.h"
#include "dp_proc_mng_channel.h"

struct dp_proc_mng_chan_handlers_st dp_proc_mng_master_msg_processes[DP_PROC_MNG_CHAN_D2H_MAX_ID] = {
};

int dp_proc_mng_chan_msg_dispatch(void *msg, u32 in_data_len, u32 out_data_len, u32 *ack_len,
    const struct dp_proc_mng_chan_handlers_st *msg_process)
{
    struct dp_proc_mng_chan_msg_head *head_msg = (struct dp_proc_mng_chan_msg_head *)msg;
    u32 data_len = in_data_len > out_data_len ? in_data_len : out_data_len;
    u32 head_len = sizeof(struct dp_proc_mng_chan_msg_head);
    u32 msg_id;
    int ret;

    head_msg->result = 0;
    *ack_len = 0;
    msg_id = head_msg->msg_id;
    if ((msg_id >= DP_PROC_MNG_CHAN_D2H_MAX_ID) || (msg_process[msg_id].chan_msg_processes == NULL)) {
        dp_proc_mng_drv_err("Invalid message_id or none process func. (msg_id=%u)\n", msg_id);
        ret = -ENOMSG;
        goto save_msg_ret;
    }

#ifdef CFG_FEATURE_VFIO
    // later add vfio scene
#endif

    if (data_len < msg_process[msg_id].msg_size) {
#ifndef EMU_ST
        dp_proc_mng_drv_err("Invalid process_len. (data_len=%u; in_len=%u; out_len=%u)\n", data_len, in_data_len, out_data_len);
        ret = -EMSGSIZE;
#endif
        goto save_msg_ret;
    }

    if (head_msg->process_id.vfid >= DP_PROC_MNG_MAX_VF_NUM) {
        dp_proc_mng_drv_err("Message_id has invalid. (msg_id=%u; vfid=%d)\n", msg_id, head_msg->process_id.vfid);
        ret = -EINVAL;
        goto save_msg_ret;
    }
    ret = msg_process[msg_id].chan_msg_processes(msg, ack_len);
save_msg_ret:
    if (ret != 0) {
        head_msg->result = (short)ret;
        *ack_len = (*ack_len > head_len) ? *ack_len : head_len;
        dp_proc_mng_drv_err("Host msg recv process failed. (devid=%u; ret=%d)\n", head_msg->process_id.devid, ret);
    }

    return 0;
}
