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
 * Create: 2022-11-29
 */
#ifndef SVM_SRCU_WORK_H
#define SVM_SRCU_WORK_H

#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_net_pub.h"
#include "ka_list_pub.h"

#define DEVMM_SRCU_SUBWORK_DEFAULT_TYPE 0
#define DEVMM_SRCU_SUBWORK_ENSURE_EXEC_TYPE 1

struct devmm_srcu_work {
    ka_delayed_work_t dwork;

    u64 subwork_num;
    u64 subwork_peak_num;

    ka_list_head_t head;
    ka_spinlock_t lock;
};

typedef void (*srcu_subwork_func)(u64 *arg, u64 arg_size);

void devmm_default_srcu_work_init(void);
void devmm_srcu_work_init(struct devmm_srcu_work *srcu_work);
void devmm_srcu_work_uninit(struct devmm_srcu_work *srcu_work);
int devmm_srcu_subwork_add(struct devmm_srcu_work *srcu_work, u32 type, srcu_subwork_func func, u64 *arg, u64 arg_size);
void devmm_srcu_work_stats_print(struct devmm_srcu_work *srcu_work);

#endif /* SVM_SRCU_WORK_H */
