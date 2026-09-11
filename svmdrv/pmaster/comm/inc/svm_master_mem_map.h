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
 * Create: 2023-08-21
 */
#ifndef SVM_MASTER_MEM_MAP_H
#define SVM_MASTER_MEM_MAP_H

#include "svm_cmd.h"
#include "devmm_proc_info.h"
#include "svm_vmma_mng.h"

#ifdef CFG_SOC_PLATFORM_ESL_FPGA
#define DEVMM_MEM_MAP_MAX_PAGE_NUM_PER_MSG 512ULL
#else
#define DEVMM_MEM_MAP_MAX_PAGE_NUM_PER_MSG 3072ULL
#endif

int devmm_ioctl_mem_map(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
int devmm_ioctl_mem_unmap(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);

int devmm_ioctl_mem_query_owner(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
int devmm_ioctl_mem_set_access(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
int devmm_ioctl_mem_get_access(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);

int devmm_msg_to_agent_mem_map(struct devmm_svm_process *svm_proc, struct devmm_vmma_info *info);
int devmm_msg_to_agent_mem_unmap(struct devmm_svm_process *svm_proc, struct devmm_vmma_info *info);
int devmm_ioctl_resv_addr_info_query(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
void devmm_destroy_all_heap_vmmas_by_devid(struct devmm_svm_process *svm_proc, u32 devid);

#endif
