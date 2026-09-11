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
#ifndef QUEUE_CTX_PRIVATE_H
#define QUEUE_CTX_PRIVATE_H

#include "ka_task_pub.h"
#include "ka_list_pub.h"

#include "queue_module.h"
#include "queue_fops.h"

struct context_private_data {
    ka_list_head_t node_list_head;
    ka_spinlock_t lock;
    int hdc_session[MAX_DEVICE];
};

#endif

