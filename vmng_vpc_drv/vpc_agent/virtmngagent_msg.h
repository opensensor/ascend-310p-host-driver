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

#ifndef VIRTMNGAGENT_MSG_H
#define VIRTMNGAGENT_MSG_H

#include "virtmng_msg_pub.h"
#include "ka_base_pub.h"

struct vmng_msg_dev *vmnga_get_msg_dev_by_id(u32 dev_id);
int vmnga_rx_irq_init(struct vmng_msg_chan_rx *msg_chan);
int vmnga_rx_irq_uninit(struct vmng_msg_chan_rx *msg_chan);

int vmnga_vpc_msg_init(void *unit_in);
void vmnga_uninit_vpc_msg(struct vmng_msg_dev *msg_dev);

int vmnga_get_remote_db(u32 dev_id, enum vmng_get_irq_type type, u32 *db_base, u32 *db_num);
int vmnga_trigger_remote_db(u32 dev_id, u32 db_index);
int vmnga_get_local_msix(u32 dev_id, enum vmng_get_irq_type type, u32 *msix_base, u32 *msix_num);
int vmnga_register_local_msix(u32 dev_id, u32 msix_index, ka_irq_handler_t handler, void *data, const char *name);
int vmnga_unregister_local_msix(u32 dev_id, u32 msix_index, void *data);

void vmnga_tx_finish_task(unsigned long data);

#endif
