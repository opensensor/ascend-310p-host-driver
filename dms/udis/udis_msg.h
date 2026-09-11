/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
 * Create: 2025-07-23
 */

#ifndef _UDIS_MSG_H_
#define _UDIS_MSG_H_

#include "udis_define.h"

#define UDIS_MSG_VALID 0x5A5A
#define UDIS_MSG_INVALID_RESULT 0x1A
#define UDIS_MSG_RETRY_TIMES 3
#define UDIS_MSG_RETRY_INTERVAL_S 2
#define UDIS_NEED_SEND_ALL_NODE_FLAG 2

struct udis_dma_addr {
    struct list_head node;
    dma_addr_t host_dma_addr;
    dma_addr_t dev_dma_addr;
    u32 data_len; // 源数据的长度
    UDIS_UPDATE_TYPE update_type;
    UDIS_MODULE_TYPE module_id;
    char name[UDIS_MAX_NAME_LEN];
};

struct udis_msg_head {
    unsigned int dev_id;
    unsigned int msg_id;
    unsigned short valid;  /* validity judgement, 0x5A5A is valide */
    unsigned short result; /* process result from rp, zero for succ, non zero for fail */
    unsigned int tsid;
    unsigned int vfid;
};
#define UDIS_MSG_INFO_LEN 128UL
#define UDIS_MSG_PAYLOAD_LEN (UDIS_MSG_INFO_LEN - sizeof(struct udis_msg_head))
struct udis_msg_info {
    struct udis_msg_head head;
    unsigned char payload[UDIS_MSG_PAYLOAD_LEN];
};

enum udis_d2h_chan_type {
    UDIS_CHAN_D2H_REGIST = 0,
    UDIS_CHAN_D2H_UNREGIST,
    UDIS_CHAN_D2H_MAX_ID
};

enum udis_h2d_chan_type {
    UDIS_CHAN_H2D_READY = 0,
    UDIS_CHAN_H2D_UNINIT,
    UDIS_CHAN_H2D_MAX_ID
};

struct udis_msg_notify_work {
    struct work_struct work;
    unsigned int udevid;
    unsigned long privilege_data;
};

int udis_common_chan_init(void);
void udis_common_chan_uninit(void);
int udis_send_dma_node_to_host(unsigned int udevid, struct udis_dma_node *dma_node, enum udis_d2h_chan_type udis_chan);
void udis_msg_send_work_init(unsigned int udevid);
void udis_msg_send_work_uninit(unsigned int udevid);
void udis_all_dma_node_op_to_host(unsigned int udevid, struct udis_ctrl_block *ucb, enum udis_d2h_chan_type chan_type);
int udis_send_host_ready_msg_to_device(unsigned int udevid);
int udis_send_host_vf_uninit_notify(unsigned int udevid);

#endif