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

#ifndef QUEUE_MSG_H
#define QUEUE_MSG_H

#include "queue_channel.h"
#include "queue_status_record.h"

#include "devdrv_interface.h"

#define REPLY_MSG_QUEUE_STATUS_TYPE_NUM (DEV_END_REPLY - DEV_QUEUE_STATUS_RECORE_START)
#define REPLY_MSG_TIME_RECORD_SIZE (sizeof(long long int) * REPLY_MSG_QUEUE_STATUS_TYPE_NUM)
struct queue_reply_complete_msg {
    struct queue_chan_dma_info dma_info;
    long long int time_record[REPLY_MSG_QUEUE_STATUS_TYPE_NUM];
    unsigned int qid;
    unsigned int dma_node_num;
};

int queue_drv_msg_chan_init(void);
void queue_drv_msg_chan_uninit(void);
void queue_register_hdc_cb_func(void);
void queue_unregister_hdc_cb_func(void);

ka_device_t *queue_get_device(u32 dev_id);
void queue_put_device(u32 dev_id);
void queue_recv_work(ka_work_struct_t *work);

#endif
