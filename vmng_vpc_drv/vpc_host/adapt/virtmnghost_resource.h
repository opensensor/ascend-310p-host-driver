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
#ifndef _VIRTMNGHOST_RESOURCE_H
#define _VIRTMNGHOST_RESOURCE_H

#include "virtmng_msg_pub.h"

#define VMNG_MSG_NUM_TX_ADMIN 1
#define VMNG_MSG_NUM_TX_COMMON 8
#define VMNG_MSG_NUM_TX_VPC (VMNG_MSG_VPC_NUM_TX_TEST + \
                             VMNG_MSG_VPC_NUM_TX_HDC + \
                             VMNG_MSG_VPC_NUM_TX_DEVMM + \
                             VMNG_MSG_VPC_NUM_TX_DEVMM_MNG + \
                             VMNG_MSG_VPC_NUM_TX_TSDRV + \
                             VMNG_MSG_VPC_NUM_TX_DEVMNG + \
                             VMNG_MSG_VPC_NUM_TX_HDC_CTRL + \
                             VMNG_MSG_VPC_NUM_TX_PCIE + \
                             VMNG_MSG_VPC_NUM_TX_DVPP)
#define VMNG_MSG_NUM_TX_BLOCK (VMNG_MSG_BLOCK_NUM_TX_HDC)

/* tx msg */
#define VMNG_MSG_BASE_TX_ADMIN 0
#define VMNG_MSG_BASE_TX_COMMON (VMNG_MSG_BASE_TX_ADMIN + VMNG_MSG_NUM_TX_ADMIN)
#define VMNG_MSG_BASE_TX_VPC (VMNG_MSG_BASE_TX_COMMON + VMNG_MSG_NUM_TX_COMMON)
#define VMNG_MSG_BASE_TX_BLOCK (VMNG_MSG_CHAN_NUM_MAX - VMNG_MSG_BLOCK_NUM_TX_HDC - 1)

/* tx vpc */
#define VMNG_MSG_VPC_BASE_TX_TEST   VMNG_MSG_BASE_TX_VPC
#define VMNG_MSG_VPC_BASE_TX_HDC    (VMNG_MSG_VPC_BASE_TX_TEST + VMNG_MSG_VPC_NUM_TX_TEST)
#define VMNG_MSG_VPC_BASE_TX_DEVMM  (VMNG_MSG_VPC_BASE_TX_HDC + VMNG_MSG_VPC_NUM_TX_HDC)
#define VMNG_MSG_VPC_BASE_TX_DEVMM_MNG (VMNG_MSG_VPC_BASE_TX_DEVMM + VMNG_MSG_VPC_NUM_TX_DEVMM)
#define VMNG_MSG_VPC_BASE_TX_TSDRV  (VMNG_MSG_VPC_BASE_TX_DEVMM_MNG + VMNG_MSG_VPC_NUM_TX_DEVMM_MNG)
#define VMNG_MSG_VPC_BASE_TX_DEVMNG (VMNG_MSG_VPC_BASE_TX_TSDRV + VMNG_MSG_VPC_NUM_TX_TSDRV)
#define VMNG_MSG_VPC_BASE_TX_PCIE (VMNG_MSG_VPC_BASE_TX_DEVMNG + VMNG_MSG_VPC_NUM_TX_DEVMNG)
#define VMNG_MSG_VPC_BASE_TX_HDC_CTL (VMNG_MSG_VPC_BASE_TX_PCIE + VMNG_MSG_VPC_NUM_TX_PCIE)
#define VMNG_MSG_VPC_BASE_TX_DVPP (VMNG_MSG_VPC_BASE_TX_HDC_CTL + VMNG_MSG_VPC_NUM_TX_HDC_CTRL)
#define VMNG_MSG_VPC_BASE_TX_ESCHED (VMNG_MSG_VPC_BASE_TX_DVPP + VMNG_MSG_VPC_NUM_TX_DVPP)
#define VMNG_MSG_VPC_BASE_TX_QUEUE (VMNG_MSG_VPC_BASE_TX_ESCHED + VMNG_MSG_VPC_NUM_TX_ESCHED)
#define VMNG_MSG_VPC_BASE_TX_RESERVE1 (VMNG_MSG_VPC_BASE_TX_QUEUE + VMNG_MSG_VPC_NUM_TX_QUEUE)
#define VMNG_MSG_VPC_BASE_TX_RESERVE2 (VMNG_MSG_VPC_BASE_TX_RESERVE1 + 0)

/* tx block */
#define VMNG_MSG_BLOCK_BASE_TX_HDC VMNG_MSG_BASE_TX_BLOCK

#define VMNG_MSG_NUM_RX_ADMIN 1
#define VMNG_MSG_NUM_RX_COMMON 8
#define VMNG_MSG_NUM_RX_VPC (VMNG_MSG_VPC_NUM_RX_TEST + \
                             VMNG_MSG_VPC_NUM_RX_HDC + \
                             VMNG_MSG_VPC_NUM_RX_DEVMM + \
                             VMNG_MSG_VPC_NUM_RX_DEVMM_MNG + \
                             VMNG_MSG_VPC_NUM_RX_TSDRV + \
                             VMNG_MSG_VPC_NUM_RX_DEVMNG + \
                             VMNG_MSG_VPC_NUM_RX_HDC_CTRL + \
                             VMNG_MSG_VPC_NUM_RX_PCIE)
#define VMNG_MSG_NUM_RX_BLOCK (VMNG_MSG_BLOCK_NUM_RX_HDC)

/* rx msg */
#define VMNG_MSG_BASE_RX_ADMIN 0
#define VMNG_MSG_BASE_RX_COMMON (VMNG_MSG_BASE_RX_ADMIN + VMNG_MSG_NUM_RX_ADMIN)
#define VMNG_MSG_BASE_RX_VPC    (VMNG_MSG_BASE_RX_COMMON + VMNG_MSG_NUM_RX_COMMON)
#define VMNG_MSG_BASE_RX_BLOCK (VMNG_MSG_CHAN_NUM_MAX - VMNG_MSG_BLOCK_NUM_RX_HDC - 1)

/* rx vpc */
#define VMNG_MSG_VPC_BASE_RX_TEST   VMNG_MSG_BASE_RX_VPC
#define VMNG_MSG_VPC_BASE_RX_HDC    (VMNG_MSG_VPC_BASE_RX_TEST + VMNG_MSG_VPC_NUM_RX_TEST)
#define VMNG_MSG_VPC_BASE_RX_DEVMM  (VMNG_MSG_VPC_BASE_RX_HDC + VMNG_MSG_VPC_NUM_RX_HDC)
#define VMNG_MSG_VPC_BASE_RX_DEVMM_MNG (VMNG_MSG_VPC_BASE_RX_DEVMM + VMNG_MSG_VPC_NUM_RX_DEVMM)
#define VMNG_MSG_VPC_BASE_RX_TSDRV  (VMNG_MSG_VPC_BASE_RX_DEVMM_MNG + VMNG_MSG_VPC_NUM_RX_DEVMM_MNG)
#define VMNG_MSG_VPC_BASE_RX_DEVMNG  (VMNG_MSG_VPC_BASE_RX_TSDRV + VMNG_MSG_VPC_NUM_RX_TSDRV)
#define VMNG_MSG_VPC_BASE_RX_PCIE  (VMNG_MSG_VPC_BASE_RX_DEVMNG + VMNG_MSG_VPC_NUM_RX_DEVMNG)
#define VMNG_MSG_VPC_BASE_RX_HDC_CTRL  (VMNG_MSG_VPC_BASE_RX_PCIE + VMNG_MSG_VPC_NUM_RX_PCIE)
#define VMNG_MSG_VPC_BASE_RX_DVPP  (VMNG_MSG_VPC_BASE_RX_HDC_CTRL + VMNG_MSG_VPC_NUM_RX_HDC_CTRL)
#define VMNG_MSG_VPC_BASE_RX_ESCHED  (VMNG_MSG_VPC_BASE_RX_DVPP + VMNG_MSG_VPC_NUM_RX_DVPP)
#define VMNG_MSG_VPC_BASE_RX_QUEUE  (VMNG_MSG_VPC_BASE_RX_ESCHED + VMNG_MSG_VPC_NUM_RX_ESCHED)
#define VMNG_MSG_VPC_BASE_RX_RESERVE1  (VMNG_MSG_VPC_BASE_RX_QUEUE + VMNG_MSG_VPC_NUM_RX_QUEUE)
#define VMNG_MSG_VPC_BASE_RX_RESERVE2  (VMNG_MSG_VPC_BASE_RX_RESERVE1 + 0)
/* rx block */
#define VMNG_MSG_BLOCK_BASE_RX_HDC VMNG_MSG_BASE_RX_BLOCK

struct vmng_msg_chan_res *vmngh_get_msg_cluster_res_default(enum vmng_msg_chan_type type);
void vmngh_set_blk_irq_array_default(struct vmng_msg_dev *msg_dev, enum vmng_msg_chan_type chan_type,
    const struct vmng_msg_chan_res *res, struct vmng_msg_chan_irqs *irq_array);

struct vmng_msg_chan_res *vmngh_get_msg_cluster_res_sriov(enum vmng_msg_chan_type type);
void vmngh_set_blk_irq_array_sriov(struct vmng_msg_dev *msg_dev, enum vmng_msg_chan_type chan_type,
    const struct vmng_msg_chan_res *res, struct vmng_msg_chan_irqs *irq_array);

#endif