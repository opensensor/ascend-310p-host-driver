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

#ifndef VIRTMNGHOST_VPC_H
#define VIRTMNGHOST_VPC_H
#include "virtmng_msg_pub.h"

int vmngh_alloc_all_vpc_msg_cluster(u32 dev_id, u32 fid);
void vmngh_free_all_vpc_msg_cluster(u32 dev_id, u32 fid);

int vmngh_alloc_all_block_msg_cluster(struct vmng_msg_dev *msg_dev);
void vmngh_free_all_block_msg_cluster(struct vmng_msg_dev *msg_dev);
int vmngh_prepare_msg_chan(struct vmng_msg_dev *msg_dev);
void vmngh_unprepare_msg_chan(struct vmng_msg_dev *msg_dev);

#endif
