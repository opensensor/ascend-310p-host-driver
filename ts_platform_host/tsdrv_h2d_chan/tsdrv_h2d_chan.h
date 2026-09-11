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
#ifndef HVTSDRV_H2D_CHAN_H
#define HVTSDRV_H2D_CHAN_H

#include <linux/types.h>
#include "devdrv_common.h"
#include "tsdrv_h2d_chan_ops.h"

int tsdrv_h2d_fast_chan_init(u32 devid);
int tsdrv_h2d_fast_chan_exit(u32 devid);

int tsdrv_h2d_slow_chan_init(u32 devid);
int tsdrv_h2d_slow_chan_exit(u32 devid);

int tsdrv_h2d_chan_setup(void);
void tsdrv_h2d_chan_cleanup(void);

int tsdrv_h2d_fast_msg_send(u32 devid, void *tx, size_t tx_size);

int tsdrv_h2d_slow_msg_send(u32 devid, void *tx, size_t tx_size);

int tsdrv_h2d_fast_rx_handler_register(u32 devid, u32 cmd_type,
    int (*rx_handler)(u32 devid, struct tsdrv_msg_info *msg, u32 *ack_len));

int tsdrv_h2d_slow_rx_handler_register(u32 devid, u32 cmd_type,
    int (*rx_handler)(u32 devid, struct tsdrv_msg_info *msg, u32 *ack_len));

#endif /* _HVTSDRV_H2D_CHAN_H_ */
