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

#ifndef VIRTMNGAGENT_MSG_ADMIN_H
#define VIRTMNGAGENT_MSG_ADMIN_H

#include "virtmng_msg_pub.h"

typedef int (*vmnga_admin_func)(struct vmng_msg_dev *msg_dev,
    struct vmng_msg_chan_rx_proc_info *proc_info);

int vmnga_register_admin_rx_func(int opcode, vmnga_admin_func admin_func);
void vmnga_unregister_admin_rx_func(int opcode);

int vmnga_init_msg_admin(struct vmng_msg_dev *msg_dev);
void vmnga_uninit_vpc_msg_admin(struct vmng_msg_dev *msg_dev);

#endif
