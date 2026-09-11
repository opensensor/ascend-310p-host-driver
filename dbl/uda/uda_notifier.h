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
 * Create: 2023-2-20
 */

#ifndef UDA_NOTIFIER_H
#define UDA_NOTIFIER_H

#include "ka_list_pub.h"
#include "ka_memory_pub.h"

#include "uda.h"
#include "uda_pub_def.h"

struct uda_notifier_node {
    ka_list_head_t node;
    u32 status[UDA_UDEV_MAX_NUM];
    const char *notifier;
    uda_notify func;
    u32 call_count;
    u32 call_finish;
};

struct uda_notifiers {
    ka_list_head_t pri_head[UDA_PRI_MAX];
    ka_rw_semaphore_t sem;
};

void uda_for_each_notifiers(void *priv,
    void (*func)(struct uda_dev_type *type, struct uda_notifiers *notifiers, void *priv));

int uda_notifier_call(u32 udevid, struct uda_dev_type *type, enum uda_notified_action action);
int uda_notifier_init(void);
void uda_notifier_uninit(void);

#endif

