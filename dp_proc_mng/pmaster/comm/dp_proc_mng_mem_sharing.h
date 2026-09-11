/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
 * Create: 2025-9-9
 */
#ifndef DP_PROC_MNG_MEM_SHARING_H
#define DP_PROC_MNG_MEM_SHARING_H
#include <linux/types.h>
#include "ascend_hal_define.h"
#include "dp_proc_mng_cmd.h"
#include "dp_proc_mng_proc_info.h"

int dp_proc_mng_ioctl_mem_sharing(struct file *file, struct dp_proc_mng_ioctl_arg *arg);
int dp_proc_mng_exit_mem_sharing(struct dp_proc_mng_ctx *proc_ctx);

int dp_mem_mng_init(void);
void dp_mem_mng_uninit(void);

#endif
