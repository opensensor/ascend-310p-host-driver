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

#ifndef SVM_HOT_RESET_H
#define SVM_HOT_RESET_H

#include <linux/types.h>
#include "ka_net_pub.h"
#include "ka_task_pub.h"
#include "ka_list_pub.h"
#include "ka_base_pub.h"
#include "svm_cmd.h"

struct svm_business_pid_info {
    ka_list_head_t list;
    ka_pid_t pid;
};

struct svm_business_info {
    ka_atomic_t devmm_is_ready[DEVMM_MAX_DEVICE_NUM];
    u64 stop_business_flag;
    ka_rw_semaphore_t stop_business_rw_sema;
    u32 business_ref_cnt[DEVMM_MAX_DEVICE_NUM];
    struct svm_business_pid_info pid_info[DEVMM_MAX_DEVICE_NUM];
    ka_rw_semaphore_t business_ref_cnt_rw_sema[DEVMM_MAX_DEVICE_NUM];
};

int devmm_hotreset_pre_handle(u32 dev_id);
int devmm_hotreset_cancel_handle(u32 dev_id);

int devmm_register_reboot_notifier(void);
void devmm_unregister_reboot_notifier(void);

void devmm_svm_business_info_init(u32 devid);
void devmm_svm_business_info_uninit(u32 devid);

int devmm_add_pid_into_business(u32 devid, ka_pid_t pid);
void devmm_remove_pid_from_business(u32 devid, ka_pid_t pid);
void devmm_remove_pid_from_all_business(ka_pid_t pid);
int devmm_alloc_business_info(void);
void devmm_free_business_info(void);

bool devmm_get_stop_business_flag(u32 devid);
bool devmm_wait_business_finish(u32 devid);

bool devmm_is_active_reboot_status(void);
#endif