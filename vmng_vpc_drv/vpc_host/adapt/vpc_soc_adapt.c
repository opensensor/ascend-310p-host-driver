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

#include "virtmnghost_resource.h"
#include "virtmnghost_vpc_unit.h"
#include "vpc_soc_adapt.h"

static inline bool vmngh_is_support_sriov(struct vmng_msg_dev *msg_dev)
{
    if (msg_dev->msg_dev_type != VMNG_MSG_DEV_UNDEFINE) {
        return msg_dev->msg_dev_type == VMNG_MSG_DEV_SRIOV;
    }
    if (ka_pci_sriov_get_totalvfs(((struct vmngh_vpc_unit *)(msg_dev->unit))->pdev) == 0) {
        msg_dev->msg_dev_type = VMNG_MSG_DEV_NORMAL;
    } else {
        msg_dev->msg_dev_type = VMNG_MSG_DEV_SRIOV;
    }
    return msg_dev->msg_dev_type == VMNG_MSG_DEV_SRIOV;
}

struct vmng_msg_chan_res *vmngh_get_msg_cluster_res(struct vmng_msg_dev *msg_dev, enum vmng_msg_chan_type type)
{
    if (vmngh_is_support_sriov(msg_dev)) {
        return vmngh_get_msg_cluster_res_sriov(type);
    }
    return vmngh_get_msg_cluster_res_default(type);
}

void vmngh_set_blk_irq_array_adapt(struct vmng_msg_dev *msg_dev, enum vmng_msg_chan_type chan_type,
    const struct vmng_msg_chan_res *res, struct vmng_msg_chan_irqs *irq_array)
{
    if (vmngh_is_support_sriov(msg_dev)) {
        vmngh_set_blk_irq_array_sriov(msg_dev, chan_type, res, irq_array);
        return;
    }
    vmngh_set_blk_irq_array_default(msg_dev, chan_type, res, irq_array);
    return;
}
