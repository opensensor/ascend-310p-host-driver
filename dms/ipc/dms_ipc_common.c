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

#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/kfifo.h>
#include <linux/delay.h>
#include <linux/time.h>

#include "devdrv_user_common.h"
#include "devdrv_ipc.h"
#include "devdrv_ipc_msg.h"
#include "dms_kernel_version_adapt.h"
#include "dms_define.h"
#include "dms_cmd_def.h"
#include "icm_interface.h"
#include "dms_ipc_common.h"
#include "dms_ipc_interface.h"
#include "kernel_version_adapt.h"
#include "devdrv_user_common.h"

int lp_ack_set_value_common(u32 dev_id, const lpm_ipc_ctrl_t *lpm_ipc, struct dms_ipc_lpm *ack_msg,
    u8 *buf, u32 *len)
{
    int i;
    u8 *data = (u8 *)ack_msg;

    if (lpm_ipc == NULL || ack_msg == NULL) {
        dms_err("Ipc ctrl or ack msg is null. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    if (buf == NULL || len == NULL) {
        dms_err("Buf or len is null. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    if ((lpm_ipc->ack_v_pos + lpm_ipc->ack_v_len >= sizeof(struct dms_ipc_lpm)) ||
        (*len < lpm_ipc->ack_v_len)) {
        dms_err("Lpm ipc format invalid. (dev_id=%u; pos=%u; ack_len=%u; in_len=%u)\n",
            dev_id, lpm_ipc->ack_v_pos, lpm_ipc->ack_v_len, *len);
        return -EINVAL;
    }

    for (i = 0; i < lpm_ipc->ack_v_len; i++) {
        buf[i] = data[lpm_ipc->ack_v_pos + i];
    }
    *len = lpm_ipc->ack_v_len;

    return 0;
}

#ifdef CONFIG_IPC_COMM_FROM_LPM
STATIC const lpm_ipc_ctrl_t g_ipc_format_list[] = {
    /* use rate */
    {LPM_USE_RATE,     LPM_AICORE0_ID,  MSG_HEADS_WORD, MSG_HEADS_WORD,
        5, 1, NO_REPEATS, IPC_OBJ_TS, DMS_LPM_TX_TS,
        NULL, NULL, lp_ack_set_value_common,
        {8,       4,  0, 0}},
};
#elif defined(CONFIG_IPC_COMM_FROM_IMU_V1) && ! defined(DMS_DEVICE_ST)
int lp_ack_check_common(u32 dev_id, const lpm_ipc_ctrl_t *lpm_ipc, struct dms_ipc_lpm *ack_msg)
{
    if (lpm_ipc == NULL || ack_msg == NULL) {
        dms_err("Ipc ctrl or ack msg is null. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    /* Check base msg */
    if ((ack_msg->cmd_type0 != lpm_ipc->ipc_msg.cmd_type0) ||
        (ack_msg->cmd_type1 != lpm_ipc->ipc_msg.cmd_type1)) {
        dms_err("Get invalid ipc message. (dev_id=%u; atype0=%u; atype1=%u; stype0=%u; stype1=%u)\n", dev_id,
            ack_msg->cmd_type0, ack_msg->cmd_type1, lpm_ipc->ipc_msg.cmd_type0, lpm_ipc->ipc_msg.cmd_type1);
        return -EINVAL;
    }
    return 0;
}

STATIC const lpm_ipc_ctrl_t g_ipc_format_list[] = {
    {LPM_FREQUENCY,     LPM_AICORE0_ID, MSG_HEADS_WORD, MSG_TOTAL_WORD,
        MSG_TOTAL_WORD, sizeof(u32), NO_REPEATS, IPC_OBJ_LP, DMS_IMU_TX_MBX25,
        NULL, lp_ack_check_common, lp_ack_set_value_common,
        {CMD_INQUIRY,         IPC_IMU_AICORE, OBJ_IMU_DMP, OBJ_AP_DMP}},
};
#else
STATIC const lpm_ipc_ctrl_t g_ipc_format_list[] = {{}};
#endif

#ifndef DMS_DEVICE_ST
STATIC const lpm_ipc_ctrl_t *get_lpm_ipc_ctrl(u32 info_type, u32 core_id)
{
#ifdef CONFIG_IPC_COMM_FROM_IMU_STUB
    dms_debug("Ipc ctrl stub. (info_type=%u; core_id=%u)\n", info_type, core_id);
    return NULL;
#else
    int i;
    int len;

    /* Find ipc msg table */
    len = sizeof(g_ipc_format_list) / sizeof(lpm_ipc_ctrl_t);
    for (i = 0; i < len; i++) {
        if ((core_id == g_ipc_format_list[i].core_id) &&
            (info_type == g_ipc_format_list[i].info_type)) {
            break;
        }
    }
    if (i == len) {
        return NULL;
    }

    return &g_ipc_format_list[i];
#endif
}
#endif

static int lpm_base_ipc_sync_send(unsigned int dev_id, const lpm_ipc_ctrl_t *lpm_ipc, struct dms_ipc_lpm *ack_msg)
{
    int ret;
    unsigned int repeats = 0;
    struct dms_ipc_lpm ipc_send_msg = {0};

    if (lpm_ipc->is_need_send != NULL) {
        if (lpm_ipc->is_need_send(dev_id, lpm_ipc) == false) {
            /* Flow control, skip send. */
            return 0;
        }
    }

    /* Send msg */
    ret = memcpy_s(&ipc_send_msg, sizeof(struct dms_ipc_lpm),
        &lpm_ipc->ipc_msg, lpm_ipc->send_len * sizeof(u32));
    if (ret != 0) {
        dms_err("Memcpy ipc msg failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

SEND_RETRY:
    ret = icm_msg_send_sync(ICM_FD_BUILD(dev_id, lpm_ipc->channel_fixed),
        (rproc_msg_t *)&ipc_send_msg, lpm_ipc->send_len,
        (rproc_msg_t *)ack_msg, lpm_ipc->recv_len);
    if (ret != 0) {
        /* ret is checked by the upper layer. */
        return ret;
    }

    /* Check ack msg format, or cache data/ try send again. */
    if (lpm_ipc->check_ack_msg != NULL) {
        ret = lpm_ipc->check_ack_msg(dev_id, lpm_ipc, ack_msg);
        if (ret && (repeats < lpm_ipc->retry_times)) {
            repeats++;
            goto SEND_RETRY;
        }
        if (ret != 0) {
            dms_err("Icm base msg send failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        }
    }

    return ret;
}

int lpm_get_info_from_ipc(u32 dev_id, u32 info_type, u32 core_id, u8 *data, u32 *len)
{
    int ret;
    const lpm_ipc_ctrl_t *lpm_ipc = NULL;
    struct dms_ipc_lpm ack_msg = {0};
#ifndef DMS_DEVICE_ST
    lpm_ipc = get_lpm_ipc_ctrl(info_type, core_id);
    if (lpm_ipc == NULL) {
        dms_err("Not support ipc msg type. (dev_id=%u; info_type=%u; core_id=%u)\n",
            dev_id, info_type, core_id);
        return -EINVAL;
    }

    /* Send msg */
    ret = lpm_base_ipc_sync_send(dev_id, lpm_ipc, &ack_msg);
    if (ret != 0) {
        if (ret == ETIMEDOUT) {
            dms_warn("Icm base msg send timeout. (dev_id=%u; ret=%d)\n", dev_id, ret);
        } else {
            dms_err("Icm base msg send failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        }
        return ret;
    }

    /* Set value */
    if (lpm_ipc->set_ret_value != NULL) {
        return lpm_ipc->set_ret_value(dev_id, lpm_ipc, &ack_msg, data, len);
    }

#endif
    return 0;
}
