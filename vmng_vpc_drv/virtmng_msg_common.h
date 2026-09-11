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

#ifndef VIRTMNG_MSG_COMMON_H
#define VIRTMNG_MSG_COMMON_H
#include "virtmng_interface.h"

#define VMNG_MSG_CLUSTER_STATUS_DISABLE 0x0
#define VMNG_MSG_CLUSTER_STATUS_INIT 0x1
#define VMNG_MSG_CLUSTER_STATUS_ENABLE 0x2

enum vmng_v2p_msg_common_pcie_cmd {
    VMNG_V2P_MSG_COMMON_PCIE_CMD_TEST = 0,
    VMNG_V2P_MSG_COMMON_PCIE_CMD_MAX
};

enum vmng_p2v_msg_common_pcie_cmd {
    VMNG_P2V_MSG_COMMON_PCIE_CMD_TEST = 0,
    VMNG_P2V_MSG_COMMON_PCIE_CMD_MAX
};

struct vmng_msg_common {
    struct vmng_msg_cluster *msg_chan;
    int (*common_fun[VMNG_MSG_COMMON_TYPE_MAX])(u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info);
};

struct vmng_msg_common_pcie_txd_verify {
    enum vmng_v2p_msg_common_pcie_cmd cmd;
    u32 cnt;
    u32 d1;
    u32 d2;
};

int vmng_msg_recv_common_verfiy_info(u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info);
#endif
