/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
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
 * Create: 2023-08-24
 */
#ifndef SVM_MASTER_ADDR_REF_OPS_H
#define SVM_MASTER_ADDR_REF_OPS_H

#include "svm_cmd.h"
#include "devmm_proc_info.h"

/* For ioctl_arg without size, should call ref_ops(ref inc or dec) in dispatch func. */
#define SVM_ADDR_REF_OPS_UNKNOWN_SIZE 0

int devmm_get_ioctl_addr_info(struct devmm_ioctl_arg *arg, u32 cmd_id, struct devmm_ioctl_addr_info *info);

#endif
