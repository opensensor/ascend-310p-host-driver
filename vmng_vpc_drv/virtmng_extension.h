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

#ifndef VIRTMNG_EXTENSION_H
#define VIRTMNG_EXTENSION_H
#include "virtmng_msg_common.h"
#include "virtmng_interface.h"
#include "devdrv_interface.h"

struct vmng_msg_tx_data_dma_copy_sync_link {
    enum vmng_v2p_msg_common_pcie_cmd cmd;
    enum devdrv_dma_data_type type;
    u32 node_cnt;
    struct devdrv_dma_node dma_node[];
};

void vmng_msg_cmn_verify_send_prepare(struct vmng_msg_common_pcie_txd_verify *x1,
    struct vmng_msg_common_pcie_txd_verify *x2, struct vmng_tx_msg_proc_info *tx_info);
#endif
