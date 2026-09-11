/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
 * Create: 2025-07-14
 */

#include "ka_task_pub.h"

#include "udis_log.h"
#include "udis_management.h"

STATIC struct udis_ctrl_block *g_udis_cb[UDIS_DEVICE_UDEVID_MAX];
STATIC struct rw_semaphore g_udis_cb_lock[UDIS_DEVICE_UDEVID_MAX];

struct udis_ctrl_block* udis_get_ctrl_block(unsigned int udevid)
{
    if (udevid >= UDIS_DEVICE_UDEVID_MAX) {
        udis_err("Invalide udevid. (udevid=%u)\n", udevid);
        return NULL;
    }
    return g_udis_cb[udevid];
}

int udis_set_ctrl_block(unsigned int udevid, struct udis_ctrl_block *ctrl_block)
{
    if (udevid >= UDIS_DEVICE_UDEVID_MAX) {
        udis_err("Invalide udevid. (udevid=%u)\n", udevid);
        return -EINVAL;
    }
    g_udis_cb[udevid] = ctrl_block;
    return 0;
}

void udis_cb_rwlock_init(unsigned int udevid)
{
    ka_task_init_rwsem(&g_udis_cb_lock[udevid]);
}

void udis_cb_write_lock(unsigned int udevid)
{
    down_write(&g_udis_cb_lock[udevid]);
}

void udis_cb_write_unlock(unsigned int udevid)
{
    up_write(&g_udis_cb_lock[udevid]);
}

void udis_cb_read_lock(unsigned int udevid)
{
    down_read(&g_udis_cb_lock[udevid]);
}

void udis_cb_read_unlock(unsigned int udevid)
{
    up_read(&g_udis_cb_lock[udevid]);
}