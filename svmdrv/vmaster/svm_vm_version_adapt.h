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
 * Create: 2022-09-13
 */

#ifndef SVM_VM_VERSION_ADAPT_H
#define SVM_VM_VERSION_ADAPT_H

#include "linux/types.h"

#include "svm_version_adapt.h"

void devmm_vm_sync_version(u32 devid);
bool devmm_vm_is_need_version_adapt(void);
bool devmm_vm_is_need_return_head_result(void);

int devmm_vm_mng_rx_version_adapt_pre_proc(void *msg);
int devmm_vm_mng_rx_version_adapt_post_proc(void *msg);
int devmm_vm_rx_version_adapt_pre_proc(void *msg);
int devmm_vm_rx_version_adapt_post_proc(void *msg);

int devmm_vm_mng_tx_version_adapt_pre_proc(void *msg);
int devmm_vm_mng_tx_version_adapt_post_proc(void *msg);
int devmm_vm_tx_version_adapt_pre_proc(void *msg, u64 msg_len,
    struct svm_ver_adapt_msg_info *new_msg_info);
int devmm_vm_tx_version_adapt_post_proc(void *msg, u64 msg_len,
    struct svm_ver_adapt_msg_info *new_msg_info);

#endif /* SVM_VM_VERSION_ADAPT_H */

