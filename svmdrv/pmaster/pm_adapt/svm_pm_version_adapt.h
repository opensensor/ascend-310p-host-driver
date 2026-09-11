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

#ifndef SVM_PM_VERSION_ADAPT_H
#define SVM_PM_VERSION_ADAPT_H

#include "virtmng_interface.h"

#include "devmm_proc_info.h"
#include "svm_version_adapt.h"

int devmm_pm_sync_version(u32 vm_id, struct vmng_rx_msg_proc_info *proc_info);
void devmm_pm_clear_vm_version(u32 vm_id);

bool devmm_pm_is_need_version_adapt(u32 vm_id);
bool devmm_pm_is_need_return_head_result(u32 vm_id);

int devmm_pm_mng_rx_version_adapt_pre_proc(u32 vm_id, void *msg);
int devmm_pm_mng_rx_version_adapt_post_proc(u32 vm_id, void *msg);
int devmm_pm_rx_version_adapt_pre_proc(u32 vm_id, void *msg, u64 msg_len,
    struct svm_ver_adapt_msg_info *new_msg_info);
int devmm_pm_rx_version_adapt_post_proc(u32 vm_id, void *msg, u64 msg_len,
    struct svm_ver_adapt_msg_info *new_msg_info);

int devmm_pm_mng_tx_version_adapt_pre_proc(u32 vm_id, void *msg);
int devmm_pm_mng_tx_version_adapt_post_proc(u32 vm_id, void *msg);
int devmm_pm_tx_version_adapt_pre_proc(u32 vm_id, void *msg);
int devmm_pm_tx_version_adapt_post_proc(u32 vm_id, void *msg);

#endif /* SVM_PM_VERSION_ADAPT_H */

