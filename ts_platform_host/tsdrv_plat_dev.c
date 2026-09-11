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
#include <linux/types.h>
#include <linux/delay.h>

#include "ka_base_pub.h"
#include "ka_system_pub.h"
#include "devdrv_common.h"
#include "tsdrv_log.h"
#include "tsdrv_plat_dev.h"

enum plat_dev_status {
    PLAT_DEV_UNINIT = 0,
    PLAT_DEV_INIT,
    PLAT_DEV_HANDSHAKING,
    PLAT_DEV_UNINITING,
};

struct plat_dev {
    ka_atomic_t status;
    void *priv_data;
} g_plat_dev[TSDRV_MAX_DAVINCI_NUM];

static inline struct plat_dev *get_plat_dev(u32 devid)
{
    return &g_plat_dev[devid];
}

void *plat_get_priv(u32 devid)
{
    struct plat_dev *plat_dev = get_plat_dev(devid);
    return plat_dev->priv_data;
}

void *plat_dev_set_handshake(u32 devid)
{
    struct plat_dev *plat_dev = get_plat_dev(devid);
    enum plat_dev_status status;

    status = ka_base_atomic_cmpxchg(&plat_dev->status, PLAT_DEV_INIT, PLAT_DEV_HANDSHAKING);
    if (status != PLAT_DEV_INIT) {
        TSDRV_PRINT_ERR("invalid status=%d, devid=%u\n", status, devid);
        return NULL;
    }
    return plat_dev->priv_data;
}

void plat_dev_reset_handshake(u32 devid)
{
    struct plat_dev *plat_dev = get_plat_dev(devid);

    ka_base_atomic_set(&plat_dev->status, PLAT_DEV_INIT);
}

int plat_dev_init(u32 devid, void *priv_data)
{
    struct plat_dev *plat_dev = get_plat_dev(devid);
    enum plat_dev_status status;

    if (priv_data == NULL) {
        TSDRV_PRINT_ERR("priv is NULL, devid=%u\n", devid);
        return -EFAULT;
    }
    status = ka_base_atomic_cmpxchg(&plat_dev->status, PLAT_DEV_UNINIT, PLAT_DEV_INIT);
    if (status != PLAT_DEV_UNINIT) {
        TSDRV_PRINT_ERR("invalid status=%d, devid=%u\n", status, devid);
        return -EINVAL;
    }
    plat_dev->priv_data = priv_data;
    return 0;
}

void *plat_dev_uniniting(u32 devid)
{
#define CUR_LOOP_NUM 10
#define CUR_MSLEEP   1000

    struct plat_dev *plat_dev = get_plat_dev(devid);
    enum plat_dev_status status;
    u32 loop = CUR_LOOP_NUM;

    while (loop != 0) {
        status = ka_base_atomic_cmpxchg(&plat_dev->status, PLAT_DEV_INIT, PLAT_DEV_UNINITING);
        if (status == PLAT_DEV_INIT) {
            return plat_dev->priv_data;
        }
        TSDRV_PRINT_WARN("invalid status=%d devid=%u loop=%u\n", (int)status, devid, loop);
        ka_system_msleep(CUR_MSLEEP);
        loop--;
    }
    return NULL;
}

int plat_dev_uninit(u32 devid)
{
    struct plat_dev *plat_dev = get_plat_dev(devid);
    enum plat_dev_status status;
    enum plat_dev_status old;

    old = ka_base_atomic_read(&plat_dev->status);
    if (old != PLAT_DEV_INIT && old != PLAT_DEV_UNINITING) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid old=%d, devid=%u\n", (int)old, devid);
#endif
        return -ENODEV;
    }
    status = ka_base_atomic_cmpxchg(&plat_dev->status, old, PLAT_DEV_UNINIT);
    if (status != old) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid status=%d old=%d, devid=%u\n", (int)status, (int)old, devid);
#endif
        return -ENODEV;
    }
    plat_dev->priv_data = NULL;
    return 0;
}

void plat_dev_setup(void)
{
    u32 devid;

    for (devid = 0; devid < TSDRV_MAX_DAVINCI_NUM; devid++) {
        struct plat_dev *plat_dev = get_plat_dev(devid);
        ka_base_atomic_set(&plat_dev->status, PLAT_DEV_UNINIT);
        plat_dev->priv_data = NULL;
    }
}

void plat_dev_cleanup(void)
{
    u32 devid;

    for (devid = 0; devid < TSDRV_MAX_DAVINCI_NUM; devid++) {
        struct plat_dev *plat_dev = get_plat_dev(devid);
        ka_base_atomic_set(&plat_dev->status, PLAT_DEV_UNINIT);
        plat_dev->priv_data = NULL;
    }
}

