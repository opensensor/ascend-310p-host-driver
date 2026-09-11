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
*/
#ifndef DMS_XPC_COMMON
#define DMS_XPC_COMMON

#ifdef CFG_FEATURE_PARTIAL_GOOD
#define XPC_REPEAT_NUM 2000U
#define XPC_WAITTIME_MIN 5000U  // 5000us
#define XPC_WAITTIME_MAX 5100U  // 5100us
#define READ_BUF_MAX 2048U
#define XPC_POLL_TIMEOUT 1000U
#define XPC_POLL_RESULT_ERROR -10

#include <linux/xpc_kernel_inter.h>
#include <linux/xpc_kernel_def.h>
#include "dms_define.h"

typedef int (*func)(char*, u32);

int dms_xpc_open_channel(int *usr_chl_id);
int dms_xpc_read_msg(int chl_id, struct chl_poll_ret *poll_ret, func callback_handler);
void dms_xpc_read(int chl_id, unsigned int next_type, func callback_handler);
int dms_xpc_poll_single(int chl_id, struct chl_poll_ret *poll_ret);
int dms_xpc_send(int chl_id, const char *buf, u32 len);

#endif
#endif
