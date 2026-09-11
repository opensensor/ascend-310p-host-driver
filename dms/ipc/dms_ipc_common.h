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
#ifndef __DMS_IPC_COMMON_H
#define __DMS_IPC_COMMON_H

#ifdef CONFIG_IPC_COMM_FROM_IMU_V2
struct dms_ipc_lpm {
    unsigned char sub_cmd;
    unsigned char cmd;
    unsigned char cmd_dest;
    unsigned char cmd_src;
    unsigned short msg_seq;
    unsigned char msg_type : 1;
    unsigned char reserved : 3;
    unsigned char version  : 4;
    unsigned char len;
    unsigned char data[ICM_MSG_DATA_LENGTH];
    unsigned short crc16;
};
#else
struct dms_ipc_lpm {
    u8 cmd_type1;
    u8 cmd_type0;
    u8 target_id;
    u8 source_id;

    u8 cmd_para0;
    u8 cmd_para1;
    u8 cmd_para2;
    u8 cmd_para3;

    u8 cmd_data0;
    u8 cmd_data1;
    u8 cmd_data2;
    u8 cmd_data3;

    u32 data1;
    u32 data2;
    u32 data3;
    u32 data4;
    u32 data5;
};
#endif

typedef struct dms_lpm_ipc_ctrl {
    u32 info_type;
    u32 core_id;
    u8 send_len;        /* len: (send_len * 4) bytes */
    u8 recv_len;        /* len: (redv_len * 4) bytes */
    u8 ack_v_pos;       /* Ack value pos, when 'set_ret_value' is null */
    u8 ack_v_len;
    u32 retry_times;    /* send retry times */
    u32 ipc_obj;        /* OBJ_TS */
    u32 channel_fixed;  /* ipc channel fixed */

    /* Check is need send, flow control(time inval check). */
    bool (*is_need_send)(u32 dev_id, const struct dms_lpm_ipc_ctrl *lpm_ipc);
    /* Check ack msg format, or cache data/ try send again. */
    int (*check_ack_msg)(u32 dev_id, const struct dms_lpm_ipc_ctrl *lpm_ipc, struct dms_ipc_lpm *ack_msg);
    /* Copy ack msg data to user data, and set data len. */
    int (*set_ret_value)(u32 dev_id, const struct dms_lpm_ipc_ctrl *lpm_ipc, struct dms_ipc_lpm *ack_msg,
        u8 *buf, u32 *len);
    struct dms_ipc_lpm ipc_msg;
} lpm_ipc_ctrl_t;

#define NA             0
#ifdef CONFIG_IPC_COMM_FROM_IMU_V2
#define MSG_HEADS_WORD 8
#define MSG_TOTAL_WORD 24
#else
#define MSG_HEADS_WORD 2
#define MSG_TOTAL_WORD 8
#define SET_MSG_HEADS_WORD 4
#endif

#define NO_REPEATS     0
#define LP_MSG_NO_ERR  0

/* LP IPC CHAN SPECIAL */
#define DMS_LPM_TX_TS           0
#define DMS_LPM_TX_LPM3         1
#ifdef CFG_EDP_MSG
#define DMS_LP_Q_TX_RPID4_ACPU0 12
#else
#define DMS_LP_Q_TX_RPID4_ACPU0 11
#endif

#define DMS_IMU_TX_MBX25        5

int lp_ack_check_common(u32 dev_id, const lpm_ipc_ctrl_t *lpm_ipc, struct dms_ipc_lpm *ack_msg);
int lp_ack_set_value_common(u32 dev_id, const lpm_ipc_ctrl_t *lpm_ipc, struct dms_ipc_lpm *ack_msg,
    u8 *buf, u32 *len);

#endif
