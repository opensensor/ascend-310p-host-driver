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

#ifndef VIRTMNGAGENT_MSG_COMMON_H
#define VIRTMNGAGENT_MSG_COMMON_H

#include "virtmng_interface.h"
#include "virtmng_msg_pub.h"

int vmnga_common_msg_send(u32 dev_id, enum vmng_msg_common_type cmn_type, struct vmng_tx_msg_proc_info *tx_info);
int vmnga_msg_cluster_recv_common(void *msg_chan_in, struct vmng_msg_chan_rx_proc_info *proc_info);
int vmnga_msg_common_recv_pcie(u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info);

void vmnga_register_extended_common_msg_client(struct vmng_msg_dev *msg_dev);
void vmnga_unregister_extended_common_msg_client(struct vmng_msg_dev *msg_dev);

#endif
