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
#ifndef _VIRTMNGHOST_RESOURCE_SRIOV_H
#define _VIRTMNGHOST_RESOURCE_SRIOV_H

/* tx msg alloc */
#define VMNG_MSG_VPC_NUM_TX_TEST 1
#define VMNG_MSG_VPC_NUM_TX_HDC 0
#define VMNG_MSG_VPC_NUM_TX_DEVMM 0
#define VMNG_MSG_VPC_NUM_TX_DEVMM_MNG 0
#define VMNG_MSG_VPC_NUM_TX_TSDRV 0
#define VMNG_MSG_VPC_NUM_TX_DEVMNG 0
#define VMNG_MSG_VPC_NUM_TX_HDC_CTRL 0
#define VMNG_MSG_VPC_NUM_TX_PCIE 0
#define VMNG_MSG_VPC_NUM_TX_DVPP 0
#define VMNG_MSG_VPC_NUM_TX_ESCHED 0
#define VMNG_MSG_VPC_NUM_TX_QUEUE 0
#define VMNG_MSG_BLOCK_NUM_TX_HDC 0
/* rx msg alloc */
#define VMNG_MSG_VPC_NUM_RX_TEST 1
#define VMNG_MSG_VPC_NUM_RX_HDC 0
#define VMNG_MSG_VPC_NUM_RX_DEVMM 0
#define VMNG_MSG_VPC_NUM_RX_DEVMM_MNG 0
#define VMNG_MSG_VPC_NUM_RX_TSDRV 32
#define VMNG_MSG_VPC_NUM_RX_DEVMNG 0
#define VMNG_MSG_VPC_NUM_RX_HDC_CTRL 0
#define VMNG_MSG_VPC_NUM_RX_PCIE 24
#define VMNG_MSG_VPC_NUM_RX_DVPP 8
#define VMNG_MSG_VPC_NUM_RX_ESCHED 0
#define VMNG_MSG_VPC_NUM_RX_QUEUE 0

#define VMNG_MSG_BLOCK_NUM_RX_HDC 0

#endif