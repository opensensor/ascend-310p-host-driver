/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2023. All rights reserved.
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
#include "devdrv_common.h"
#include "tsdrv_pci_chan.h"
#include "tsdrv_h2d_chan.h"
#include "tsdrv_log.h"

int tsdrv_h2d_fast_chan_init(u32 devid)
{
    const struct tsdrv_h2d_chan_ops *g_h2d_chan_ops = tsdrv_get_h2d_chan_ops();

    if (g_h2d_chan_ops->h2d_fast_chan_init != NULL) {
        return g_h2d_chan_ops->h2d_fast_chan_init(devid);
    }
#ifndef TSDRV_UT
    TSDRV_PRINT_ERR("h2d_fast_chan_init is NULL, devid=%u\n", devid);
    return -EINVAL;
#endif
}

int tsdrv_h2d_fast_chan_exit(u32 devid)
{
    const struct tsdrv_h2d_chan_ops *g_h2d_chan_ops = tsdrv_get_h2d_chan_ops();

    if (g_h2d_chan_ops->h2d_fast_chan_exit != NULL) {
        return g_h2d_chan_ops->h2d_fast_chan_exit(devid);
    }
#ifndef TSDRV_UT
    TSDRV_PRINT_ERR("h2d_fast_chan_exit is NULL, devid=%u\n", devid);
    return -EINVAL;
#endif
}

int tsdrv_h2d_slow_chan_init(u32 devid)
{
    const struct tsdrv_h2d_chan_ops *g_h2d_chan_ops = tsdrv_get_h2d_chan_ops();

    if (g_h2d_chan_ops->h2d_slow_chan_init != NULL) {
        return g_h2d_chan_ops->h2d_slow_chan_init(devid);
    }
#ifndef TSDRV_UT
    TSDRV_PRINT_ERR("h2d_slow_chan_init is NULL, devid=%u\n", devid);
    return -EINVAL;
#endif
}

int tsdrv_h2d_slow_chan_exit(u32 devid)
{
    const struct tsdrv_h2d_chan_ops *g_h2d_chan_ops = tsdrv_get_h2d_chan_ops();

    if (g_h2d_chan_ops->h2d_slow_chan_exit != NULL) {
        return g_h2d_chan_ops->h2d_slow_chan_exit(devid);
    }
#ifndef TSDRV_UT
    TSDRV_PRINT_ERR("h2d_slow_chan_exit is NULL, devid=%u\n", devid);
    return -EINVAL;
#endif
}

int tsdrv_h2d_chan_setup(void)
{
    const struct tsdrv_h2d_chan_ops *g_h2d_chan_ops = tsdrv_get_h2d_chan_ops();

    if (g_h2d_chan_ops->h2d_chan_setup != NULL) {
        return g_h2d_chan_ops->h2d_chan_setup();
    }
#ifndef TSDRV_UT
    TSDRV_PRINT_ERR("h2d_chan_setup is NULL\n");
    return -EINVAL;
#endif
}

void tsdrv_h2d_chan_cleanup(void)
{
    const struct tsdrv_h2d_chan_ops *g_h2d_chan_ops = tsdrv_get_h2d_chan_ops();

    if (g_h2d_chan_ops->h2d_chan_cleanup == NULL) {
        TSDRV_PRINT_ERR("h2d_chan_cleanup is NULL\n");
        return;
    }
    g_h2d_chan_ops->h2d_chan_cleanup();
}

int tsdrv_h2d_fast_msg_send(u32 devid, void *tx, size_t tx_size)
{
#ifndef TSDRV_UT
    const struct tsdrv_h2d_chan_ops *g_h2d_chan_ops = tsdrv_get_h2d_chan_ops();

    if (g_h2d_chan_ops->h2d_fast_msg_send != NULL) {
        return g_h2d_chan_ops->h2d_fast_msg_send(devid, tx, tx_size);
    }

    TSDRV_PRINT_ERR("h2d_fast_msg_send is NULL, devid=%u\n", devid);
    return -EINVAL;
#endif
}

int tsdrv_h2d_slow_msg_send(u32 devid, void *tx, size_t tx_size)
{
    const struct tsdrv_h2d_chan_ops *g_h2d_chan_ops = tsdrv_get_h2d_chan_ops();

    if (g_h2d_chan_ops->h2d_slow_msg_send != NULL) {
        return g_h2d_chan_ops->h2d_slow_msg_send(devid, tx, tx_size);
    }
#ifndef TSDRV_UT
    TSDRV_PRINT_ERR("h2d_slow_msg_send is NULL, devid=%u\n", devid);
    return -EINVAL;
#endif
}

int tsdrv_h2d_fast_rx_handler_register(u32 devid, u32 cmd_type,
    int (*rx_handler)(u32 devid, struct tsdrv_msg_info *msg, u32 *ack_len))
{
#ifndef TSDRV_UT
    const struct tsdrv_h2d_chan_ops *g_h2d_chan_ops = tsdrv_get_h2d_chan_ops();

    if (g_h2d_chan_ops->fast_rx_handler_register != NULL) {
        return g_h2d_chan_ops->fast_rx_handler_register(devid, cmd_type, rx_handler);
    }
    TSDRV_PRINT_ERR("fast_rx_handler_register is NULL, devid=%u\n", devid);
    return -EINVAL;
#endif
}

int tsdrv_h2d_slow_rx_handler_register(u32 devid, u32 cmd_type,
    int (*rx_handler)(u32 devid, struct tsdrv_msg_info *msg, u32 *ack_len))
{
    const struct tsdrv_h2d_chan_ops *g_h2d_chan_ops = tsdrv_get_h2d_chan_ops();

    if (g_h2d_chan_ops->slow_rx_handler_register != NULL) {
        return g_h2d_chan_ops->slow_rx_handler_register(devid, cmd_type, rx_handler);
    }
#ifndef TSDRV_UT
    TSDRV_PRINT_ERR("slow_rx_handler_register is NULL, devid=%u\n", devid);
    return -EINVAL;
#endif
}
