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
#include "virtmng_msg_pub.h"
#include "hw_vdavinci.h"

#include "virtmnghost_resource_default.h"

struct vmng_msg_chan_res g_vmngh_type_res_default[VMNG_MSG_CHAN_TYPE_MAX] = {
    /* admin */
    {VMNG_MSG_BASE_TX_ADMIN, VMNG_MSG_NUM_TX_ADMIN,
     VMNG_MSG_BASE_RX_ADMIN, VMNG_MSG_NUM_RX_ADMIN},
    /* common */
    {VMNG_MSG_BASE_TX_COMMON, VMNG_MSG_NUM_TX_COMMON,
     VMNG_MSG_BASE_RX_COMMON, VMNG_MSG_NUM_RX_COMMON},
    /* vpc */
    {VMNG_MSG_VPC_BASE_TX_TEST, VMNG_MSG_VPC_NUM_TX_TEST,
     VMNG_MSG_VPC_BASE_RX_TEST, VMNG_MSG_VPC_NUM_RX_TEST},
    {VMNG_MSG_VPC_BASE_TX_HDC, VMNG_MSG_VPC_NUM_TX_HDC,
     VMNG_MSG_VPC_BASE_RX_HDC, VMNG_MSG_VPC_NUM_RX_HDC},
    {VMNG_MSG_VPC_BASE_TX_DEVMM, VMNG_MSG_VPC_NUM_TX_DEVMM,
     VMNG_MSG_VPC_BASE_RX_DEVMM, VMNG_MSG_VPC_NUM_RX_DEVMM},
    {VMNG_MSG_VPC_BASE_TX_DEVMM_MNG, VMNG_MSG_VPC_NUM_TX_DEVMM_MNG,
     VMNG_MSG_VPC_BASE_RX_DEVMM_MNG, VMNG_MSG_VPC_NUM_RX_DEVMM_MNG},
    {VMNG_MSG_VPC_BASE_TX_TSDRV, VMNG_MSG_VPC_NUM_TX_TSDRV,
     VMNG_MSG_VPC_BASE_RX_TSDRV, VMNG_MSG_VPC_NUM_RX_TSDRV},
    {VMNG_MSG_VPC_BASE_TX_DEVMNG, VMNG_MSG_VPC_NUM_TX_DEVMNG,
     VMNG_MSG_VPC_BASE_RX_DEVMNG, VMNG_MSG_VPC_NUM_RX_DEVMNG},
    {VMNG_MSG_VPC_BASE_TX_HDC_CTL, VMNG_MSG_VPC_NUM_TX_HDC_CTRL,
     VMNG_MSG_VPC_BASE_RX_HDC_CTRL, VMNG_MSG_VPC_NUM_RX_HDC_CTRL},
    {VMNG_MSG_VPC_BASE_TX_ESCHED, VMNG_MSG_VPC_NUM_TX_ESCHED,
     VMNG_MSG_VPC_BASE_RX_ESCHED, VMNG_MSG_VPC_NUM_RX_ESCHED},
    {VMNG_MSG_VPC_BASE_TX_QUEUE, VMNG_MSG_VPC_NUM_TX_QUEUE,
     VMNG_MSG_VPC_BASE_RX_QUEUE, VMNG_MSG_VPC_NUM_RX_QUEUE},
    {VMNG_MSG_VPC_BASE_TX_RESERVE1, 0,
     VMNG_MSG_VPC_BASE_RX_RESERVE1, 0},
    {VMNG_MSG_VPC_BASE_TX_RESERVE2, 0,
     VMNG_MSG_VPC_BASE_RX_RESERVE2, 0},
    /* block */
    {VMNG_MSG_BLOCK_BASE_TX_HDC, VMNG_MSG_BLOCK_NUM_TX_HDC,
     VMNG_MSG_BASE_RX_BLOCK, VMNG_MSG_BLOCK_NUM_RX_HDC},
};

struct vmng_msg_chan_res *vmngh_get_msg_cluster_res_default(enum vmng_msg_chan_type type)
{
    if (type >= VMNG_MSG_CHAN_TYPE_MAX) {
        return NULL;
    }
    return &g_vmngh_type_res_default[type];
}

void vmngh_set_blk_irq_array_default(struct vmng_msg_dev *msg_dev, enum vmng_msg_chan_type chan_type,
    const struct vmng_msg_chan_res *res, struct vmng_msg_chan_irqs *irq_array)
{
    int i;
    if (vmng_is_blk_chan(chan_type) == true) {
        for (i = 0; i < res->tx_num; i++) {
            irq_array->tx_finish_irq[i] =
                VMNG_DB_BASE_MSG_BLOCK_TX_FINISH + (res->tx_base - VMNG_MSG_BASE_TX_BLOCK) + i;
        }
        for (i = 0; i < res->rx_num; i++) { /* alloc for block, beyond msg irq */
            irq_array->rx_resp_irq[i] = VMNG_MSIX_BASE_MSG_BLOCK_RX_REPLY + (res->rx_base - VMNG_MSG_BASE_RX_BLOCK) + i;
        }
    }
}

