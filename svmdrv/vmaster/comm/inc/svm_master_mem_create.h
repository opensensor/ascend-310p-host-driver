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
 * Create: 2023-08-11
 */
#ifndef SVM_MASTER_MEM_CREATE_H
#define SVM_MASTER_MEM_CREATE_H

#include "svm_cmd.h"
#include "devmm_channel.h"
#include "devmm_proc_info.h"
#include "devmm_common.h"

int devmm_ioctl_mem_create(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
int devmm_ioctl_mem_release(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
int devmm_agent_mem_release(struct devmm_svm_process *svm_proc, struct devmm_devid *devids,
    u64 pg_num, int id, u32 free_type);
int devmm_agent_mem_release_public(struct devmm_chan_mem_release *msg);
int devmm_master_mem_release(struct devmm_svm_process *svm_proc, u64 pg_num, int id, u32 free_type);

#endif
