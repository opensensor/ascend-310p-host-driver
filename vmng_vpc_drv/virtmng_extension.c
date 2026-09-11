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

#include "ka_base_pub.h"

#include "virtmng_public_def.h"
#include "virtmng_extension.h"

void vmng_msg_cmn_verify_send_prepare(struct vmng_msg_common_pcie_txd_verify *x1,
    struct vmng_msg_common_pcie_txd_verify *x2, struct vmng_tx_msg_proc_info *tx_info)
{
    u32 data_len = sizeof(struct vmng_msg_common_pcie_txd_verify);

    x1->cmd = VMNG_V2P_MSG_COMMON_PCIE_CMD_TEST;
    x1->d1 = ka_base_get_random_u32();
    x1->d2 = ka_base_get_random_u32();
    x2->cnt = x1->cnt;
    x2->d1 = x1->d1;
    x2->d2 = x1->d2;

    tx_info->data = x1;
    tx_info->in_data_len = data_len;
    tx_info->out_data_len = data_len;
    tx_info->real_out_len = 0;

    vmng_debug("Get process value. (d1=%x; d2=%x; len=%u)\n", x1->d1, x1->d2, tx_info->out_data_len);
}
