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
#ifndef QUEUE_UT
#include "queue_module.h"
#include "queue_context.h"
#include "queue_ctx_private.h"

void *queue_context_private_data_create(void)
{
    struct context_private_data *ctx_private = NULL;
    int devid;

    ctx_private = (struct context_private_data *)queue_drv_kmalloc(
        sizeof(struct context_private_data), KA_GFP_ATOMIC | __KA_GFP_ACCOUNT);
    if (ctx_private == NULL) {
        return NULL;
    }

    for (devid = 0; devid < MAX_DEVICE; devid++) {
        ctx_private->hdc_session[devid] = -1;
    }
    KA_INIT_LIST_HEAD(&ctx_private->node_list_head);
    ka_task_spin_lock_init(&ctx_private->lock);

    return ctx_private;
}

void queue_context_private_data_destroy(void *private_data)
{
    struct context_private_data *ctx_private = (struct context_private_data *)private_data;

    queue_drv_kfree(ctx_private);
    return;
}

#else
void queue_context_host()
{
    return;
}
#endif

