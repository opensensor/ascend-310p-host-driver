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

#ifndef URD_INIT_H
#define URD_INIT_H

#include "ka_base_pub.h"

#include "dms_cmd_def.h"

struct urd_file_private_stru {
    pid_t owner_pid;
    ka_atomic_t work_count;
};

#define MSG_FROM_USER           0
#define MSG_FROM_KERNEL         1
#define MSG_FROM_USER_REST_ACC  2   /* restrict access */
#define MSG_FROM_TYPE_MAX       3

#define FILTER_LEN 128
#define PAYLOAD_LEN_MAX 340
struct urd_forward_msg {
    u32 main_cmd;
    u32 sub_cmd;
    char filter[FILTER_LEN];
    unsigned int filter_len;
    u32 output_len;
    u32 payload_len;
    char payload[PAYLOAD_LEN_MAX];
};

int dms_cmd_process_from_kernel(struct dms_ioctl_arg* ctl_arg);

#endif
