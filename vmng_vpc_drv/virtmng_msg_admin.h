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

#ifndef VIRTMNG_MSG_ADMIN_H
#define VIRTMNG_MSG_ADMIN_H
#include "virtmng_interface.h"
#include "virtmng_msg_pub.h"

#define VMNG_CREATE_CLUSTER_FINISH 0x15864
#if !defined(EMU_ST) && !defined(VIRTMNG_UT)
#define VMNG_WAIT_TIME_LEN 100000000 // 100s admin msg timeout
#else
#define VMNG_WAIT_TIME_LEN 1000 // 1000us admin msg timeout for ut
#endif
#define VMNG_WAIT_CYCLE 200

/* V2P cmd */
enum vmngh_admin_opcode {
    VMNGH_ADMIN_OPCODE_CREATE_CLUSTER = 0,
    VMNGH_ADMIN_OPCODE_RM_VDEV,
    VMNGH_ADMIN_OPCODE_MAX,
};

/* P2V cmd */
struct vmng_create_cluster_cmd {
    u32 opcode;
    enum vmng_msg_chan_type chan_type;
    struct vmng_msg_chan_res res;
    u32 mem_off_base;
    u32 int_irq[];
};

struct vmng_create_cluster_reply {
    u32 opcode;
    enum vmng_msg_chan_type chan_type;
    u32 remote_alloc_finish;
};

struct vmng_host_rm_vdev_cmd_reply {
    u32 opcode;
    u32 rm_mode;
    u32 finish;
};

/* V2P cmd */
enum vmnga_admin_opcode {
    VMNGA_ADMIN_OPCODE_RM_PDEV = 0,
    VMNGA_ADMIN_OPCODE_MAX,
};

struct vmng_agent_rm_pdev_cmd_reply {
    u32 opcode;
    u32 rm_mode;
    u32 finish;
};

int vmng_admin_msg_send(struct vmng_msg_chan_tx *msg_chan, struct vmng_tx_msg_proc_info *tx_info, u32 opcode_d1,
    u32 opcode_d2);
#endif
