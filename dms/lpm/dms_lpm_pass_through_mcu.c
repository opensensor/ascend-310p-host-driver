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
* Create: 2022-12-14
*/

#include <linux/types.h>
#include <linux/uaccess.h>
#include "devdrv_user_common.h"
#include "drv_ipc.h"
#include "dms_ipc_common.h"
#include "dms_define.h"
#include "dms_template.h"
#include "dms_cmd_def.h"
#include "devdrv_ipc.h"
#include "urd_acc_ctrl.h"
#include "dms_ipc_common.h"
#include "icm_interface.h"
#include "ascend_platform.h"

#define CMD_IMU_MCU_MSG 5
#define MCU_SEND_DATA_LEN 24
#define SUB_CMD_IMU_MCU_MSG 0
#ifdef CONFIG_IPC_COMM_FROM_IMU_V1
#define MAX_IPCDRV_MSG_LENGTH_MCU 32
#define MAX_IPCDRV_MSG_HEAD_MCU 4
#else
#define MAX_IPCDRV_MSG_LENGTH_MCU 96
#define MAX_IPCDRV_MSG_HEAD_MCU 10
#endif
#define CMD_DMP_MSG_HEAD_LEN_MCU 4
#define MAX_CHIP_NUM 4

/* 24 bytes: ipc mail size 32 bytes, 4 bytes ipc cmd head, 4 bytes dmp ipc msg head */
#define CMD_DMP_MSG_MAX_LEN_MCU (MAX_IPCDRV_MSG_LENGTH_MCU - MAX_IPCDRV_MSG_HEAD_MCU - CMD_DMP_MSG_HEAD_LEN_MCU)

typedef struct mcu_msg {
    unsigned char offset;
    unsigned char is_finish;
    unsigned char send_len;
    unsigned char rw_flag;
    unsigned char send_data[MCU_SEND_DATA_LEN];
} mcu_msg_t;

STATIC int dms_lpm_pass_through_mcu_fill_ipc_msg(struct dms_pass_through_mcu_in *msg_from_user,
    mcu_msg_t *mcu_msg_0, mcu_msg_t *mcu_msg_1, unsigned int *ipc_msg_len)
{
    int ret;
    unsigned int msg_len;
    unsigned int msg_next_len;

    if (msg_from_user->buf_len > CMD_DMP_MSG_MAX_LEN_MCU) {
        msg_len = CMD_DMP_MSG_MAX_LEN_MCU;
        msg_next_len = msg_from_user->buf_len - CMD_DMP_MSG_MAX_LEN_MCU;
        *ipc_msg_len = IPCDRV_RPROC_MSG_LENGTH << 1u;
    } else {
        mcu_msg_0->offset = 0;
        mcu_msg_0->is_finish = 1;
        mcu_msg_0->send_len = msg_from_user->buf_len;
        msg_len = msg_from_user->buf_len;
        msg_next_len = 0;
        *ipc_msg_len = IPCDRV_RPROC_MSG_LENGTH;
    }

    ret = (int)copy_from_user(mcu_msg_0->send_data, (void *)((uintptr_t)msg_from_user->buf), msg_len);
    if (ret != 0) {
        dms_err("Copy mcu message0 from user space failed. (ret=%d)\n", ret);
        return ret;
    }

    if (msg_next_len > 0) {
        ret = (int)copy_from_user(mcu_msg_1->send_data,
            (void *)((uintptr_t)msg_from_user->buf + msg_len), msg_next_len);
        if (ret != 0) {
            dms_err("Copy mcu message1 from user space failed. (ret=%d)\n", ret);
            return ret;
        }
    }

    return 0;
}

#ifdef CONFIG_IPC_COMM_FROM_IMU_V1
STATIC void dms_lpm_pass_through_mcu_fill_cmd(struct dms_pass_through_mcu_in *msg_from_user,
    struct dms_ipc_lpm *ipc_cmd_0, struct dms_ipc_lpm *ipc_cmd_1)
{
    ipc_cmd_0->cmd_type1 = SUB_CMD_IMU_MCU_MSG;
    ipc_cmd_0->cmd_type0 = CMD_IMU_MCU_MSG;
    ipc_cmd_0->target_id = OBJ_IMU;
    ipc_cmd_0->source_id = OBJ_AP;
    ipc_cmd_0->cmd_para3 = msg_from_user->rw_flag;

    ipc_cmd_1->cmd_type1 = SUB_CMD_IMU_MCU_MSG;
    ipc_cmd_1->cmd_type0 = CMD_IMU_MCU_MSG;
    ipc_cmd_1->target_id = OBJ_IMU;
    ipc_cmd_1->source_id = OBJ_AP;
    ipc_cmd_1->cmd_para3 = msg_from_user->rw_flag;
}

STATIC int dms_dms_lpm_pass_through_mcu_result_process(struct dms_ipc_lpm *ack,
    struct dms_pass_through_mcu_out *mcu_msg_out)
{
    int ret;

    if ((ack->source_id != OBJ_IMU) || (ack->target_id != OBJ_AP) ||
        (ack->cmd_type0 != CMD_IMU_MCU_MSG) || (ack->cmd_type1 != SUB_CMD_IMU_MCU_MSG)) {
        dms_err("Get invalid ipc message. (ack_source_id=%u; ack_target_id=%u; ack_cmd_type0=%u; ack_cmd_type1=%u.\n",
                ack->source_id, ack->target_id, ack->cmd_type0, ack->cmd_type1);
        return -EINVAL;
    }

    ret = memcpy_s(mcu_msg_out->response_data, MCU_RESP_LEN, &ack->cmd_para0, MCU_RESP_LEN);
    if (ret) {
        dms_err("Copy response data failed. (ret=%d)\n", ret);
        return ret;
    }

    mcu_msg_out->response_len = MCU_RESP_LEN;
    return 0;
}

STATIC int dms_lpm_pass_through_mcu_by_imu(struct dms_pass_through_mcu_in *msg_from_user,
    struct dms_pass_through_mcu_out *mcu_msg_out)
{
    int ret;
    unsigned int ipc_msg_len = 0;
    mcu_msg_t *mcu_msg[2] = {NULL}; /* 2 msg to mcu */
    struct dms_ipc_lpm ipc[2] = {0}; /* 2 ipc msg */
    struct dms_ipc_lpm ack = {0};

    dms_lpm_pass_through_mcu_fill_cmd(msg_from_user, &ipc[0], &ipc[1]);
    mcu_msg[0] = (mcu_msg_t *)&ipc[0].cmd_para0;
    mcu_msg[1] = (mcu_msg_t *)&ipc[1].cmd_para0;

    ret = dms_lpm_pass_through_mcu_fill_ipc_msg(msg_from_user, mcu_msg[0], mcu_msg[1], &ipc_msg_len);
    if (ret != 0) {
        dms_err("Fill ipc msg failed. (device_id=%u; ret=%d)\n", msg_from_user->dev_id, ret);
        return ret;
    }

    ret = icm_msg_send_sync(ICM_FD_BUILD(msg_from_user->dev_id, HISI_RPROC_TX_IMU_MBX24),
        (rproc_msg_t *)ipc, ipc_msg_len, (rproc_msg_t *)&ack, IPCDRV_RPROC_MSG_LENGTH);
    if (ret != 0) {
        dms_warn("Can't send sync msg to imu. (device_id=%u; ret=%d)\n", msg_from_user->dev_id, ret);
        return ret;
    }

    return dms_dms_lpm_pass_through_mcu_result_process(&ack, mcu_msg_out);
}
#else
STATIC void dms_lpm_pass_through_mcu_fill_cmd(struct dms_pass_through_mcu_in *msg_from_user,
    struct icmdrv_ipc_msg *ipc_cmd_0, struct icmdrv_ipc_msg *ipc_cmd_1)
{
    mcu_msg_t *mcu_msg[2] = {NULL}; /* 2 msg to mcu */

    ipc_cmd_0->sub_cmd = ICM_SUB_CMD_NO_TYPE;
    ipc_cmd_0->cmd = ICM_MAIN_CMD_LP_DMP_MCU;
    ipc_cmd_0->cmd_dest = OBJ_CMD_LP;
    ipc_cmd_0->cmd_src = OBJ_CMD_CCPU;
    ipc_cmd_0->len = ICM_MSG_DATA_LENGTH;
    mcu_msg[0] = (mcu_msg_t *)ipc_cmd_0->data;
    mcu_msg[0]->rw_flag = msg_from_user->rw_flag;

    ipc_cmd_1->sub_cmd = ICM_SUB_CMD_NO_TYPE;
    ipc_cmd_1->cmd = ICM_MAIN_CMD_LP_DMP_MCU;
    ipc_cmd_1->cmd_dest = OBJ_CMD_LP;
    ipc_cmd_1->cmd_src = OBJ_CMD_CCPU;
    ipc_cmd_1->len = ICM_MSG_DATA_LENGTH;
    mcu_msg[1] = (mcu_msg_t *)ipc_cmd_1->data;
    mcu_msg[1]->rw_flag = msg_from_user->rw_flag;
}

STATIC int dms_dms_lpm_pass_through_mcu_result_process(struct dms_ipc_lpm *ack,
    struct dms_pass_through_mcu_out *mcu_msg_out)
{
    int ret;

    if ((ack->cmd_src != OBJ_CMD_LP) || (ack->cmd_dest != OBJ_CMD_CCPU) ||
        (ack->cmd != ICM_MAIN_CMD_LP_DMP_MCU) || (ack->sub_cmd != ICM_SUB_CMD_NO_TYPE)) {
        dms_err("Get invalid ipc message. (ack_cmd_src=%u; ack_cmd_dest=%u; ack_cmd=%d, ack_sub_cmd=%d)\n",
                ack->cmd_src, ack->cmd_dest, ack->cmd, ack->sub_cmd);
        return -EINVAL;
    }

    ret = memcpy_s(mcu_msg_out->response_data, MCU_RESP_LEN, &ack->data, MCU_RESP_LEN);
    if (ret != 0) {
        dms_err("Copy response data to output buffer failed. (ret=%d)\n", ret);
        return ret;
    }

    mcu_msg_out->response_len = MCU_RESP_LEN;
    return 0;
}

STATIC int dms_lpm_pass_through_mcu_by_imu(struct dms_pass_through_mcu_in *msg_from_user,
    struct dms_pass_through_mcu_out *mcu_msg_out)
{
    int ret;
    struct icmdrv_ipc_msg ipc[2] = {0}; /* 2 ipc msg */
    struct dms_ipc_lpm ack = {0};
    mcu_msg_t *mcu_msg[2] = {NULL}; /* 2 mcu msg */
    unsigned int ipc_msg_len = 0;

    dms_lpm_pass_through_mcu_fill_cmd(msg_from_user, &ipc[0], &ipc[1]);
    mcu_msg[0] = (mcu_msg_t *)ipc[0].data;
    mcu_msg[1] = (mcu_msg_t *)ipc[1].data;

    ret = dms_lpm_pass_through_mcu_fill_ipc_msg(msg_from_user, mcu_msg[0], mcu_msg[1], &ipc_msg_len);
    if (ret != 0) {
        dms_err("Fill ipc msg failed. (device_id=%u; ret=%d)\n", msg_from_user->dev_id, ret);
        return ret;
    }

    ret = icm_msg_send_sync(ICM_FD_BUILD(msg_from_user->dev_id, DMS_LP_IPC_CHAN_ID),
        (rproc_msg_t *)ipc, ipc_msg_len, (rproc_msg_t *)&ack, IPCDRV_RPROC_MSG_LENGTH);
    if (ret != 0) {
        dms_warn("Can't send sync msg to imu. (device_id=%u; ret=%d)\n", msg_from_user->dev_id, ret);
        return ret;
    }

    return dms_dms_lpm_pass_through_mcu_result_process(&ack, mcu_msg_out);
}
#endif

STATIC int dms_lpm_pass_through_mcu(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    struct dms_pass_through_mcu_in *msg_from_user = NULL;
    struct dms_pass_through_mcu_out *mcu_msg_out = NULL;

    if ((in == NULL) || (in_len != sizeof(struct dms_pass_through_mcu_in))) {
        dms_err("Input char is null or in_len is wrong. (in_is_null=%d; in_len=%u)\n", (in == NULL), in_len);
        return -EINVAL;
    }

    if ((out == NULL) || (out_len != sizeof(struct dms_pass_through_mcu_out))) {
        dms_err("Output char is null or out_len is wrong. (out_is_null=%d; out_len=%u)\n", (out == NULL), out_len);
        return -EINVAL;
    }

    msg_from_user = (struct dms_pass_through_mcu_in *)in;
    mcu_msg_out = (struct dms_pass_through_mcu_out *)out;
    if ((msg_from_user->dev_id >= MAX_CHIP_NUM) || (msg_from_user->buf_len > (CMD_DMP_MSG_MAX_LEN_MCU << 1u))) {
        dms_err("Device id invalid or input buffer length is too long. (device_id=%u; send_len=%u)\n",
            msg_from_user->dev_id, msg_from_user->buf_len);
        return -EINVAL;
    }

    return dms_lpm_pass_through_mcu_by_imu(msg_from_user, mcu_msg_out);
}

#define DMS_LPM_CMD_NAME "DMS_LPM_CMD_NAME"
BEGIN_DMS_MODULE_DECLARATION(DMS_LPM_CMD_NAME)
BEGIN_FEATURE_COMMAND()
ADD_FEATURE_COMMAND(DMS_LPM_CMD_NAME, DMS_MAIN_CMD_LPM, DMS_SUBCMD_PASS_THROUGTH_MCU, NULL, "dmp_daemon",
    DMS_SUPPORT_ALL, dms_lpm_pass_through_mcu)
END_FEATURE_COMMAND()
END_MODULE_DECLARATION()

int dms_lpm_init(void)
{
    dms_info("dms lpm init exec\n");
    CALL_INIT_MODULE(DMS_LPM_CMD_NAME);
    return 0;
}

void dms_lpm_uninit(void)
{
    CALL_EXIT_MODULE(DMS_LPM_CMD_NAME);
}

