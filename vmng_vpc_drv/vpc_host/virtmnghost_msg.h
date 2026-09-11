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

#ifndef VIRTMNGHOST_MSG_H
#define VIRTMNGHOST_MSG_H
#include "virtmng_msg_pub.h"

int vmngh_tx_finish_db_hander(struct vmng_msg_dev *msg_dev, u32 db_vector);
int vmngh_msg_db_hanlder(struct vmng_msg_dev *msg_dev, u32 db_vector);

int vmngh_alloc_msg_cluster(struct vmng_msg_dev *msg_dev, enum vmng_msg_chan_type chan_type,
    struct vmng_msg_proc *msg_proc);

int vmngh_rx_irq_init(struct vmng_msg_chan_rx *msg_chan);
int vmngh_rx_irq_uninit(struct vmng_msg_chan_rx *msg_chan);
int vmngh_init_vpc_msg(void *unit_in);
void vmngh_uninit_vpc_msg(void *unit_in);

struct vmng_msg_dev *vmngh_get_msg_dev_by_id(u32 dev_id, u32 fid);

struct vmng_msg_chan_res *vmngh_get_msg_cluster_res_default(enum vmng_msg_chan_type type);
struct vmng_msg_chan_res *vmngh_get_msg_cluster_res_sriov(enum vmng_msg_chan_type type);

#endif
