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
#ifndef TSDRV_H2D_CHAN_OPS_H
#define TSDRV_H2D_CHAN_OPS_H

#include "devdrv_interface.h"
#include "devdrv_common.h"

#define MAX_D2H_RX_CMD_NUM  30U

struct tsdrv_h2d_rx_handler {
    int (*rx_handler[MAX_D2H_RX_CMD_NUM])(u32 devid, struct tsdrv_msg_info *msg, u32 *ack_len);
};

struct tsdrv_h2d_chan_ops {
    int (*h2d_chan_setup)(void);
    int (*h2d_chan_cleanup)(void);

    int (*h2d_fast_msg_send)(u32 devid, void *tx, size_t tx_size);
    int (*h2d_slow_msg_send)(u32 devid, void *tx, size_t tx_size);

    int (*h2d_fast_chan_init)(u32 devid);
    int (*h2d_fast_chan_exit)(u32 devid);

    bool (*h2d_slow_chan_ready)(u32 devid);

    int (*h2d_slow_chan_init)(u32 devid);
    int (*h2d_slow_chan_exit)(u32 devid);

    int (*fast_rx_handler_register)(u32 devid, u32 cmd_type,
        int (*rx_handler)(u32 devid, struct tsdrv_msg_info *msg, u32 *ack_len));
    int (*slow_rx_handler_register)(u32 devid, u32 cmd_type,
        int (*rx_handler)(u32 devid, struct tsdrv_msg_info *msg, u32 *ack_len));
};

#endif /* __TSDRV_H2D_CHAN_OPS_H */
