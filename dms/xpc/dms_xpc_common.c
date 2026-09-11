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
* Create: 2022-10-26
*/

#ifdef CFG_FEATURE_PARTIAL_GOOD
#include <linux/delay.h>
#include <linux/kthread.h>

#include "ka_fs_pub.h"

#include "dms_xpc_common.h"

#define CMDLINE_BUFFER_SIZE     1024
#define CMDLINE_FILE_PATH       "/proc/cmdline"
#define XPC_ENABLE              1
#define XPC_DISABLE           0

char g_xpc_array[READ_BUF_MAX];

STATIC int dms_get_xpc_enable_flag(int *xpc_enable_flag)
{
    char buffer[CMDLINE_BUFFER_SIZE + 1] = {0};
    struct file *fp = NULL;
    loff_t pos = 0;
    int ret = 0;
    int read_len;

    fp = filp_open(CMDLINE_FILE_PATH, KA_O_RDONLY, 0);
    if (IS_ERR_OR_NULL(fp)) {
        dms_err("Open file failed. (file=%s; errno=%ld)\n", CMDLINE_FILE_PATH, PTR_ERR(fp));
        return -EIO;
    }

    read_len = kernel_read(fp, &buffer, CMDLINE_BUFFER_SIZE, &pos);
    if ((read_len <= 0) || (read_len >= CMDLINE_BUFFER_SIZE)) {
        dms_err("read len error. (read_len=%d; valid range:(%d, %d)))\n", read_len, 0, CMDLINE_BUFFER_SIZE);
        ret = -EIO;
        goto close_fp;
    }

    buffer[read_len] = '\0';

    if (strstr(buffer, "aosmode=kernel_utils") == NULL) {
        dms_warn("aosmode=kernel_utils is not found in cmdline.\n");
        *xpc_enable_flag = XPC_DISABLE;
        goto close_fp;
    }
    *xpc_enable_flag = XPC_ENABLE;

close_fp:
    filp_close(fp, NULL);
    fp = NULL;
    return ret;
}

int dms_xpc_open_channel(int *usr_chl_id)
{
    int chl_id, ret;
    int repeat = XPC_REPEAT_NUM;
    int xpc_enable_flag;

    struct chl_info ch1 = {
        .name = "dms_xpc_channel",
        .key = 0,
        .fifo_size = 0,
        .virt_domain = 0,
    };

    ret = dms_get_xpc_enable_flag(&xpc_enable_flag);
    if (ret != 0) {
        dms_err("Dms get xpc enable flag failed.\n");
        return -1;
    }

    if (xpc_enable_flag == XPC_DISABLE) {
        return -EOPNOTSUPP;
    }

    chl_id = xpcshm_open_channel(&ch1, CHL_O_CREATE);
    if (chl_id < 0) {
        dms_warn("Dms xpc channel is not created. (chl_id=%d)\n", chl_id);
        return -1;
    }

    // write only when both sides open, maximum wait time is 10s
    while (xpcshm_get_chl_state(chl_id) != XPC_CHL_STATE_OPEN) {
        usleep_range(XPC_WAITTIME_MIN, XPC_WAITTIME_MAX);
        if (repeat == 0) {
            (void)xpcshm_close_channel(chl_id);
            dms_warn("Dms wait xpc open channel timeout. (repeat_time=%u)\n", XPC_REPEAT_NUM);
            return -1;
        } else {
            repeat--;
        }
    }

    *usr_chl_id = chl_id;
    dms_info("Dms open xpc channel success. (channel_id=%d; repeat_time=%u)\n", chl_id, XPC_REPEAT_NUM - repeat);
    return 0;
}

int dms_xpc_read_msg(int chl_id, struct chl_poll_ret *poll_ret, func callback_handler)
{
    int ret;
    ret = dms_xpc_poll_single(chl_id, poll_ret);
    if (ret > 0) {
        dms_xpc_read(chl_id, poll_ret[0].type, callback_handler);
        return 0;
    }

    return ret;
}

void dms_xpc_read(int chl_id, unsigned int next_type, func callback_handler)
{
    int ret;
    unsigned int len = READ_BUF_MAX;

    while (true) {
        if (next_type == (PKT_TYPE_NORMAL | PKT_TYPE_INNER)) {
            (void)memset_s(g_xpc_array, READ_BUF_MAX, 0, READ_BUF_MAX);
            ret = xpcshm_read(chl_id, g_xpc_array, &len, &next_type);
            if (ret >= 0) {
                callback_handler(g_xpc_array, len);
            } else if (ret == XPC_ERR_CHL_EMPTY) {
                dms_info("Dms xpc channel is empty.\n");
                break;
            } else {
                dms_err("Dms xpc read failed. (ret=%d)\n", ret);
                break;
            }
        } else {
            dms_err("Dms xpc packet type is invalid. (next_type=%u)\n", next_type);
            break;
        }
    }
}

int dms_xpc_poll_single(int chl_id, struct chl_poll_ret *poll_ret)
{
    int ret;
    int chl_num = 1;
    int res_num = 0;
    int chl_id_array[1] = {chl_id};

    ret = xpcshm_poll(chl_id_array, chl_num, poll_ret, &res_num, XPC_POLL_TIMEOUT);
    if (ret == 0) {
        if (res_num == 1 && poll_ret[0].id == chl_id) {
            return poll_ret[0].len;
        } else {
            dms_err("Xpcshm_poll error. (ret=%d; res_num=%d; poll_ret[0].id=%u)\n", ret, res_num, poll_ret[0].id);
            return XPC_POLL_RESULT_ERROR;
        }
    }
    return ret;
}

int dms_xpc_send(int chl_id, const char *buf, u32 len)
{
    if (len > READ_BUF_MAX) {
        return -EINVAL;
    }

    return xpcshm_write(chl_id, buf, len);
}

#endif
