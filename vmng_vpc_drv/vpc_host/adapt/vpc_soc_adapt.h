/*
* Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
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
* Create: 2022-12-13
*/
#ifndef __VPC_SOC_ADAPT_H
#define __VPC_SOC_ADAPT_H
#include "virtmng_msg_pub.h"

struct vmng_msg_chan_res *vmngh_get_msg_cluster_res(struct vmng_msg_dev *msg_dev, enum vmng_msg_chan_type type);

void vmngh_set_blk_irq_array_adapt(struct vmng_msg_dev *msg_dev, enum vmng_msg_chan_type chan_type,
    const struct vmng_msg_chan_res *res, struct vmng_msg_chan_irqs *irq_array);

#endif