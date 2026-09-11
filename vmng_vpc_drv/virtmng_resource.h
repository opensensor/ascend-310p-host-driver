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

#ifndef VIRTMNG_RESOURCE_H
#define VIRTMNG_RESOURCE_H

#include "ka_memory_pub.h"

/* load irq and amdin irq, based on protocol. beside msg base 1 */
#define VMNG_MSIX_MSG_ADMIN 0
#define VMNG_DB_MSG_ADMIN 0

#define VMNG_IRQ_NUM_FOR_MSG 95

/* msix table */
#define VMNG_MSIX_NUM_LODA 1
#define VMNG_MSIX_NUM_MSG VMNG_IRQ_NUM_FOR_MSG
#define VMNG_MSXI_NUM_MSG_BLOCK_RX_REPLY 10
#define VMNG_MSIX_NUM_EXTERNAL 32

#define VMNG_MSIX_BASE_LOAD 0
#define VMNG_MSIX_BASE_MSG (VMNG_MSIX_BASE_LOAD + VMNG_MSIX_NUM_LODA)
#define VMNG_MSIX_BASE_MSG_BLOCK_RX_REPLY (VMNG_MSIX_BASE_MSG + VMNG_MSIX_NUM_MSG)
#define VMNG_MSIX_BASE_EXTERNAL (VMNG_MSIX_BASE_MSG_BLOCK_RX_REPLY + VMNG_MSXI_NUM_MSG_BLOCK_RX_REPLY)
#define VMNG_MSIX_BASE_MAX (VMNG_MSIX_BASE_EXTERNAL + VMNG_MSIX_NUM_EXTERNAL)

/* external msix */
#define VMNG_MSIX_NUM_EXTERNAL_TSDRV 2
#define VMNG_MSIX_BASE_EXTERNAL_MIN (VMNG_MSIX_BASE_EXTERNAL)
#define VMNG_MSIX_BASE_EXTERNAL_TSDRV (VMNG_MSIX_BASE_EXTERNAL_MIN)
#define VMNG_MSIX_BASE_EXTERNAL_MAX (VMNG_MSIX_BASE_EXTERNAL_TSDRV + VMNG_MSIX_NUM_EXTERNAL_TSDRV)

/* doorbell */
#define VMNG_DB_NUM_LOAD 1
#define VMNG_DB_NUM_MSG VMNG_IRQ_NUM_FOR_MSG
#define VMNG_DB_NUM_MSG_BLOCK_TX_FINISH 0
#define VMNG_DB_NUM_EXTERNAL 32

#define VMNG_DB_BASE_LOAD 0
#define VMNG_DB_BASE_MSG (VMNG_DB_BASE_LOAD + VMNG_DB_NUM_LOAD)
#define VMNG_DB_BASE_MSG_BLOCK_TX_FINISH (VMNG_DB_BASE_MSG + VMNG_DB_NUM_MSG)
#define VMNG_DB_BASE_EXTERNAL (VMNG_DB_BASE_MSG_BLOCK_TX_FINISH + VMNG_DB_NUM_MSG_BLOCK_TX_FINISH)
#define VMNG_DB_BASE_MAX (VMNG_DB_BASE_EXTERNAL + VMNG_DB_NUM_EXTERNAL)

/* external db */
#define VMNG_DB_NUM_EXTERNAL_TSDRV 2
#define VMNG_DB_BASE_EXTERNAL_MIN (VMNG_DB_BASE_EXTERNAL)
#define VMNG_DB_BASE_EXTERNAL_TSDRV (VMNG_DB_BASE_EXTERNAL_MIN)
#define VMNG_DB_BASE_EXTERNAL_MAX (VMNG_DB_BASE_EXTERNAL_TSDRV + VMNG_DB_NUM_EXTERNAL_TSDRV)

bool vmng_is_external_db(u32 db_index);
bool vmng_is_external_msix(u32 msix_index);

#endif
