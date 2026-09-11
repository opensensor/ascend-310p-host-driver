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
#include <linux/workqueue.h>
#include <linux/rwsem.h>

#include "devdrv_interface.h"
#include "dp_proc_mng_channel.h"
#include "dp_proc_mng_cmd.h"
#include "dp_proc_mng_log.h"
#include "dp_proc_mng_msg_client.h"

int dp_proc_mng_common_msg_send(void *msg, unsigned int len, unsigned int out_len)
{
    struct dp_proc_mng_chan_msg_head *tmp_msg = NULL;
    u32 head_len = sizeof(struct dp_proc_mng_chan_msg_head);
    u32 device_id, out_len_tmp;
    int ret;

    if (msg == NULL) {
        dp_proc_mng_drv_err("Message is NULL.\n");
        return -EINVAL;
    }

    tmp_msg = (struct dp_proc_mng_chan_msg_head *)msg;
    tmp_msg->result = 0;
    device_id = tmp_msg->process_id.devid;

    if (device_id >= DP_PROC_MNG_MAX_DEVICE_NUM) {
        dp_proc_mng_drv_err("Host send device error. (device_id=%d)\n", device_id);
        return -ENODEV;
    }

    out_len_tmp = (out_len >= head_len) ? out_len : head_len;
    ret = devdrv_common_msg_send(device_id, msg, len, out_len_tmp, &out_len_tmp, DEVDRV_COMMON_MSG_DP_PROC_MNG);
    if (ret != 0) {
        dp_proc_mng_drv_warn("Device is not up yet, wait for a while and try again check. (ret=%d)\n", ret);
        return -ECOMM;
    }

    return tmp_msg->result;
}

STATIC int dp_proc_mng_host_msg_recv(u32 devid, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len)
{
    int ret;

    if ((data == NULL) || (real_out_len == NULL)) {
#ifndef EMU_ST
        dp_proc_mng_drv_err("Data or out_len is NULL. (data=%pK; out_len=%pK)\n", data, real_out_len);
        return -EINVAL;
#endif
    }

    if (in_data_len < sizeof(struct dp_proc_mng_chan_msg_head)) {
#ifndef EMU_ST
        dp_proc_mng_drv_err("In_data_len is invalid. (in_data_len=%u)\n", in_data_len);
        return -EMSGSIZE;
#endif
    }

    if (devid >= DP_PROC_MNG_MAX_DEVICE_NUM) {
#ifndef EMU_ST
        dp_proc_mng_drv_err("Device_id must less than DP_PROC_MNG_MAX_DEVICE_NUM. "
            "(devid=%u; DP_PROC_MNG_MAX_AGENT_DEVICE_NUM=%d)\n",
            devid, DP_PROC_MNG_MAX_DEVICE_NUM);
        return -ENODEV;
#endif
    }

    ret = dp_proc_mng_chan_msg_dispatch(data, in_data_len, out_data_len, real_out_len,
        &dp_proc_mng_master_msg_processes[0]);
    if (ret != 0) {
        dp_proc_mng_drv_err("Host msg recv process failed. (devid=%u)\n", devid);
    }

    return ret;
}

struct devdrv_common_msg_client dp_proc_mng_host_comm_msg_client = {
    .type = DEVDRV_COMMON_MSG_DP_PROC_MNG,
    .common_msg_recv = dp_proc_mng_host_msg_recv,
};
