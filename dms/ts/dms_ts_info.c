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
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/notifier.h>
#include <linux/atomic.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/gfp.h>

#include "ka_task_pub.h"
#include "ka_base_pub.h"

#include "icm_interface.h"
#include "devdrv_user_common.h"
#include "drv_ipc.h"
#include "devdrv_ipc.h"
#include "tsmng_interface.h"
#if (defined(CFG_FEATURE_AI_UTIL_BY_CALCULATE) || defined(CFG_FEATURE_AI_UTIL_FROM_IPC))
#include "dms_ipc_interface.h"
#endif
#include "dms_define.h"
#include "dms_template.h"
#include "dms_cmd_def.h"
#include "dms_ts_info.h"
#include "ascend_platform.h"
#include "dms_basic_info.h"
#include "urd_acc_ctrl.h"
#include "dms_ts_scan.h"
#ifdef CFG_FEATURE_SUPPORT_NPU_UTIL
#include "dms_ts_npu_utilization.h"
#endif
#include "devdrv_manager_common.h"
#include "dbl_mem_alloc_interface.h"
#include "devdrv_manager_comm.h"
#ifndef AOS_LLVM_BUILD
#include "ascend_kernel_hal.h"
#endif

#include "ka_memory_pub.h"

BEGIN_DMS_MODULE_DECLARATION(DMS_MODULE_TS_INFO)
BEGIN_FEATURE_COMMAND()
#ifdef CFG_FEATURE_AIC_AIV_UTIL_FROM_TS
ADD_FEATURE_COMMAND(DMS_MODULE_TS_INFO, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_AI_INFO_FROM_TS, NULL, NULL,
    DMS_SUPPORT_ALL_USER, dms_get_ai_info_from_ts)
#elif (defined(CFG_FEATURE_AI_UTIL_BY_CALCULATE) || defined(CFG_FEATURE_AI_UTIL_FROM_IPC))
ADD_FEATURE_COMMAND(DMS_MODULE_TS_INFO, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_AI_INFO_FROM_TS, NULL, NULL,
    DMS_SUPPORT_ALL_USER, dms_get_ai_info_from_lp)
#endif

#ifdef CFG_FEATURE_TS_FAULT_MASK
ADD_FEATURE_COMMAND(DMS_MODULE_TS_INFO, DMS_GET_GET_DEVICE_INFO_CMD, ZERO_CMD, DMS_FILTER_TS_INFO, NULL,
    DMS_SUPPORT_ALL, dms_get_ts_fault_mask)
ADD_FEATURE_COMMAND(DMS_MODULE_TS_INFO, DMS_GET_SET_DEVICE_INFO_CMD, ZERO_CMD, DMS_FILTER_TS_INFO, "dmp_daemon",
    DMS_SUPPORT_ALL, dms_set_ts_fault_mask)
#endif

END_FEATURE_COMMAND()
END_MODULE_DECLARATION()

int copy_from_user_safe(void *to, const void __user *from, unsigned long n);
int copy_to_user_safe(void __user *to, const void *from, unsigned long n);

#ifdef CFG_FEATURE_TS_FAULT_MASK
#define MASK_TYPE_TS_AIC_ERROR 1
#define MASK_SWITCH_ON 1
#define MASK_SWITCH_OFF 0
int dms_get_ts_fault_mask(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    unsigned int dev_id;
    struct dms_get_device_info_in *mask_in = (struct dms_get_device_info_in *)in;
    struct dms_get_device_info_out *mask_out = (struct dms_get_device_info_out *)out;
    struct dms_ts_fault_mask mask = {0};

    if (in == NULL || in_len != sizeof(struct dms_get_device_info_in)) {
        dms_err("Input arg is NULL, or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }

    if (out == NULL || out_len != sizeof(struct dms_get_device_info_out)) {
        dms_err("Output arg is NULL, or out_len is wrong. (out_len=%u)\n", out_len);
        return -EINVAL;
    }
    if (mask_in->buff_size != sizeof(struct dms_ts_fault_mask)) {
        dms_err("Buff_size is wrong. (buff_size=%u; sizeof(buff)=%lu)\n", mask_in->buff_size,
            sizeof(struct dms_ts_fault_mask));
        return -EINVAL;
    }
    if (mask_in->sub_cmd != DSMI_TS_SUB_GET_FAULT_MASK) {
        return -EOPNOTSUPP;
    }

    ret = copy_from_user_safe(&mask, (void *)(uintptr_t)mask_in->buff, sizeof(struct dms_ts_fault_mask));
    if (ret != 0) {
        dms_err("copy_from_user_safe failed. (ret=%d)\n", ret);
        return ret;
    }

    dev_id = mask_in->dev_id;
    if (dev_id >= DEVDRV_MANGER_MAX_DEVICE_NUM || mask.type != MASK_TYPE_TS_AIC_ERROR) {
        dms_err("Parameters in mask are invalid. (dev_id=%u; type=%u)\n", dev_id, mask.type);
        return -EINVAL;
    }

    ret = tsmng_query_ts_mask_alarm((u8)mask.type, (u8 *)&mask.mask_switch, dev_id, 0);
    if (ret != 0) {
        dms_err("tsmng_query_ts_mask_alarm failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    ret = copy_to_user_safe((void *)(uintptr_t)mask_in->buff, &mask, sizeof(struct dms_ts_fault_mask));
    if (ret != 0) {
        dms_err("copy_to_user_safe failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    mask_out->out_size = sizeof(struct dms_ts_fault_mask);
    return 0;
}

int dms_set_ts_fault_mask(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    unsigned int dev_id;
    struct dms_set_device_info_in *mask_in = (struct dms_set_device_info_in *)in;
    struct dms_ts_fault_mask mask = {0};

    if (in == NULL || in_len != sizeof(struct dms_set_device_info_in)) {
        dms_err("Input arg is NULL, or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }
    if (mask_in->buff_size != sizeof(struct dms_ts_fault_mask)) {
        dms_err("Buff_size is wrong. (buff_size=%u; sizeof(buff)=%lu)\n", mask_in->buff_size,
            sizeof(struct dms_ts_fault_mask));
        return -EINVAL;
    }
    if (mask_in->sub_cmd != DSMI_TS_SUB_SET_FAULT_MASK) {
        return -EOPNOTSUPP;
    }

    ret = copy_from_user_safe(&mask, (void *)(uintptr_t)mask_in->buff, sizeof(struct dms_ts_fault_mask));
    if (ret != 0) {
        dms_err("copy_from_user_safe failed. (dev_id=%u; ret=%d)\n", mask_in->dev_id, ret);
        return ret;
    }

    dev_id  = mask_in->dev_id;

    if (dev_id >= DEVDRV_MANGER_MAX_DEVICE_NUM ||
        mask.type != MASK_TYPE_TS_AIC_ERROR || mask.mask_switch > MASK_SWITCH_ON) {
        dms_err("Parameters in mask are invalid. (dev_id=%u; type=%u; mask_switch=%u)\n",
                dev_id, mask.type, mask.mask_switch);
        return -EINVAL;
    }
    dms_event("Set ts fault mask. (dev_id=%u; mask_type=%u; mask_switch=%u).\n", dev_id, mask.type, mask.mask_switch);
    ret = tsmng_inform_ts_mask_alarm((u8)mask.type, (u8)mask.mask_switch, dev_id, 0);
    if (ret != 0) {
        dms_err("tsmng_inform_ts_mask_alarm failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    return 0;
}
#endif

int dms_ts_init(void)
{
#ifdef CFG_FEATURE_SUPPORT_NPU_UTIL
    int ret = dms_ts_npu_utilization_init();
    if (ret != 0) {
        dms_err("Dms_ts_npu_utilization_init failed. (ret=%d)\n", ret);
        return ret;
    }
#endif
    CALL_INIT_MODULE(DMS_MODULE_TS_INFO);
#if (defined(CFG_FEATURE_AI_UTIL_BY_CALCULATE) || defined(CFG_FEATURE_AI_UTIL_FROM_IPC))
    (void)ts_scan_init();
#endif
    return 0;
}

void dms_ts_uninit(void)
{
    CALL_EXIT_MODULE(DMS_MODULE_TS_INFO);
#if (defined(CFG_FEATURE_AI_UTIL_BY_CALCULATE) || defined(CFG_FEATURE_AI_UTIL_FROM_IPC))
    (void)ts_scan_uninit();
#endif

#ifdef CFG_FEATURE_SUPPORT_NPU_UTIL
    (void)dms_ts_npu_utilization_exit();
#endif
    return;
}

#ifdef CFG_FEATURE_AIC_AIV_UTIL_FROM_TS

#define COPY_TO_USER 1
#define NOT_COPY_TO_USER 0

#ifdef CFG_FEATURE_IPC_FORMAT_V2
    #define FEATURE_IPC_VARIABLE_DEFINE \
        struct icmdrv_ipc_msg ipc_msg = {0}; \
        struct icmdrv_ipc_msg_info ack_msg = {0}; \
        unsigned char *pdata = &ack_msg.data[1]; \
        u32 max = ICM_MSG_DATA_LENGTH - 1
#else
    #define FEATURE_IPC_VARIABLE_DEFINE \
        struct ipcdrv_message ipc_msg = {0}; \
        struct ipcdrv_message ack_msg = {0}; \
        unsigned char *pdata = &ack_msg.ipcdrv_payload[1]; \
        u32 max = IPCDRV_MSG_LENGTH - 1
#endif

STATIC KA_TASK_DEFINE_MUTEX(g_aicore_mutex);

STATIC int mutex_lock_interruptible_retry(struct mutex *lock)
{
    int ret, i;
    for (i = 0; i < 3; i++) { /* retry 3 times */
        ret = ka_task_mutex_lock_interruptible(lock);
        if (ret == 0) {
            return 0;
        }
        dms_warn("A signal arrived process was interrupted. (ret=%d)\n", ret);
    }
    return ret;
}

STATIC unsigned int dms_get_aic_aiv_num(unsigned int cmd_type)
{
    switch (cmd_type) {
        case DSMI_TS_SUB_AICORE_UTILIZATION_RATE:
            return DMS_AI_CORE_NUM;
        case DSMI_TS_SUB_VECTORCORE_UTILIZATION_RATE:
            return DMS_AI_VECTOR_NUM;
        case DSMI_TS_SUB_ACC_UTILIZATION_RATE:
            return 1;
        case DSMI_TS_SUB_NPU_MULTI_UTILIZATION_RATE:
            return NPU_CORE_NUM;
        default:
            return 0;
    }
}

#ifdef CFG_FEATURE_IPC_FORMAT_V2
STATIC int dms_get_msg_from_ts(u32 dev_id, struct icmdrv_ipc_msg *ipc, struct icmdrv_ipc_msg_info *msg_ret,
    u8 *ts_ret, u32 *data_len)
{
    int ret;
    u32 chan_id;

    if ((ipc == NULL) || (msg_ret == NULL)) {
        dms_err("ipc or msg_ret pointer is null.\n");
        return -EINVAL;
    }

    if ((ipc->sub_cmd == ICM_SUB_CMD_AI_COMPUTING_POWER) ||
        (ipc->sub_cmd == ICM_SUB_CMD_AI_COMPUTING_POWER_END) ||
        (ipc->sub_cmd == ICM_SUB_CMD_AIV_COMPUTING_POWER) ||
        (ipc->sub_cmd == ICM_SUB_CMD_AIV_COMPUTING_POWER_END) ||
        (ipc->sub_cmd == ICM_SUB_CMD_ACC_COMPUTING_POWER) ||
        (ipc->sub_cmd == ICM_SUB_CMD_ACC_COMPUTING_POWER_END) ||
        (ipc->sub_cmd == ICM_SUB_CMD_NPU_MULTI_UTILIZATION_RATE) ||
        (ipc->sub_cmd == ICM_SUB_CMD_NPU_MULTI_UTILIZATION_RATE_END)) {
        chan_id = DMS_TS_IPC_CHAN_ID;
    } else {
        return -EOPNOTSUPP;
    }
#ifdef CFG_FEATURE_VF_USE_DEVID
    if (dev_id >= DEVDRV_PF_DEV_MAX_NUM) {
        /*
        * VF device id is start from DEVDRV_PF_DEV_MAX_NUM, each PF has VDAVINCI_MAX_VFID_NUM of VFs
        */
        dev_id = (dev_id - DEVDRV_PF_DEV_MAX_NUM) / VDAVINCI_MAX_VFID_NUM;
    }
#endif
    ret = icm_msg_send_sync(ICM_FD_BUILD(dev_id, chan_id), (rproc_msg_t *)ipc,
        IPCDRV_RPROC_MSG_LENGTH, (rproc_msg_t *)msg_ret, IPCDRV_RPROC_MSG_LENGTH);
    if (ret != 0) {
        dms_err("icm send msg failed. (cmd_type=%u; sub_cmd=%u, ret=%d)\n", ipc->cmd, ipc->sub_cmd, ret);
        return ret;
    }

    *ts_ret = msg_ret->data[0];
    *data_len = msg_ret->len;

    if ((msg_ret->sub_cmd != ipc->sub_cmd) || (msg_ret->cmd != ipc->cmd) ||
        (msg_ret->cmd_dest != ipc->cmd_src) || (msg_ret->cmd_src != ipc->cmd_dest)) {
        dms_err("Ipc message validity check failed."
            "(ts_reply_ipc.cmd_dest=%u; ts_reply_ipc.cmd_src=%u;"
            "send_ipc.cmd_dest=%u; sen_ipc.cmd_src=%u).\n",
            msg_ret->cmd_dest, msg_ret->cmd_src, ipc->cmd_dest, ipc->cmd_src);
        return -EIO;
    }
    return ret;
}

STATIC int dms_fill_ipcmsg_to_ts(core_utilization_rate_t core_status, struct icmdrv_ipc_msg *ipc_msg, int flag)
{
    ipc_msg->data[0] = (u8)core_status.vfid;
    ipc_msg->cmd = ICM_MAIN_CMD_TS_REQ;
    ipc_msg->cmd_dest = OBJ_CMD_TS;
    ipc_msg->cmd_src = OBJ_CMD_CCPU;
    ipc_msg->len = sizeof(u8);

    switch (core_status.cmd_type) {
        case DSMI_TS_SUB_AICORE_UTILIZATION_RATE:
            ipc_msg->sub_cmd = (flag == START_MSG) ? (ICM_SUB_CMD_AI_COMPUTING_POWER) :
                (ICM_SUB_CMD_AI_COMPUTING_POWER_END);
            break;
        case DSMI_TS_SUB_VECTORCORE_UTILIZATION_RATE:
            ipc_msg->sub_cmd = (flag == START_MSG) ? (ICM_SUB_CMD_AIV_COMPUTING_POWER) :
                (ICM_SUB_CMD_AIV_COMPUTING_POWER_END);
            break;
        case DSMI_TS_SUB_ACC_UTILIZATION_RATE:
            ipc_msg->sub_cmd = (flag == START_MSG) ? (ICM_SUB_CMD_ACC_COMPUTING_POWER) :
                (ICM_SUB_CMD_ACC_COMPUTING_POWER_END);
            break;
        case DSMI_TS_SUB_NPU_MULTI_UTILIZATION_RATE:
            ipc_msg->sub_cmd = (flag == START_MSG) ? (ICM_SUB_CMD_NPU_MULTI_UTILIZATION_RATE) :
                (ICM_SUB_CMD_NPU_MULTI_UTILIZATION_RATE_END);
            break;
        default:
            return -EOPNOTSUPP;
    }

    return 0;
}
#else
STATIC int dms_get_msg_from_ts(u32 dev_id, struct ipcdrv_message *ipc, struct ipcdrv_message *msg_ret,
    u8 *ts_ret, u32 *data_len)
{
    int ret;
    u32 chan_id;

    if ((ipc == NULL) || (msg_ret == NULL)) {
        dms_err("ipc or msg_ret pointer is null.\n");
        return -EINVAL;
    }

    if ((ipc->ipc_msg_header.cmd_type == IPCDRV_TS_AICORE_UTILIZATION_RATE) ||
        (ipc->ipc_msg_header.cmd_type == IPCDRV_TS_AICORE_UTILIZATION_RATE_END) ||
        (ipc->ipc_msg_header.cmd_type == IPCDRV_TS_ACC_UTILIZATION_RATE) ||
        (ipc->ipc_msg_header.cmd_type == IPCDRV_TS_ACC_UTILIZATION_RATE_END) ||
        (ipc->ipc_msg_header.cmd_type == IPCDRV_TS_NPU_MULTI_UTILIZATION_RATE) ||
        (ipc->ipc_msg_header.cmd_type == IPCDRV_TS_NPU_MULTI_UTILIZATION_RATE_END)) {
        /* only support dc */
        chan_id = HISI_RPROC_TSC_TX_RPID0;
    } else if ((ipc->ipc_msg_header.cmd_type == IPCDRV_TS_VECTORCORE_UTILIZATION_RATE) ||
        (ipc->ipc_msg_header.cmd_type == IPCDRV_TS_VECTORCORE_UTILIZATION_RATE_END)) {
#ifdef CFG_SOC_PLATFORM_MDC_V51
        chan_id = HISI_RPROC_TSV_TX_RPID0;
#else
        chan_id = HISI_RPROC_TSC_TX_RPID0;
#endif
    } else {
        return -EOPNOTSUPP;
    }

    ret = icm_msg_send_sync(ICM_FD_BUILD(dev_id, chan_id), (rproc_msg_t *)ipc,
        sizeof(struct ipcdrv_message) / sizeof(rproc_msg_t), (rproc_msg_t *)msg_ret, IPCDRV_RPROC_MSG_LENGTH);
    if (ret != 0) {
        dms_err("icm send msg failed. (cmd_type=%u; ret=%d)\n", ipc->ipc_msg_header.cmd_type, ret);
        return ret;
    }

    *ts_ret = msg_ret->ipcdrv_payload[0];
    *data_len = (u32)(msg_ret->ipc_msg_header.msg_length) - sizeof(struct ipcdrv_msg_header);

    if ((msg_ret->ipc_msg_header.cmd_type != ipc->ipc_msg_header.cmd_type)) {
        dms_err("ipc message validity check failed, drv_devmng_msg.head.cmd_type(%u), "
            "ts_reply_msg.head.cmd_type(%u).\n", ipc->ipc_msg_header.cmd_type, msg_ret->ipc_msg_header.cmd_type);
        return -EIO;
    }
    return ret;
}

STATIC int dms_fill_ipcmsg_to_ts(core_utilization_rate_t core_status, struct ipcdrv_message *ipc_msg, int flag)
{
    ipc_msg->ipcdrv_payload[0] = (u8)core_status.vfid;
    ipc_msg->ipc_msg_header.msg_type = MSGTYPE_DRIVER_SEND;
    ipc_msg->ipc_msg_header.sync_type = IPCDRV_MSG_ASYNC;
    ipc_msg->ipc_msg_header.msg_length = sizeof(struct ipcdrv_msg_header) + sizeof(u8);
    ipc_msg->ipc_msg_header.msg_index = 0;

    switch (core_status.cmd_type) {
        case DSMI_TS_SUB_AICORE_UTILIZATION_RATE:
            ipc_msg->ipc_msg_header.cmd_type = (flag == START_MSG) ? (IPCDRV_TS_AICORE_UTILIZATION_RATE) :
                (IPCDRV_TS_AICORE_UTILIZATION_RATE_END);
            break;
        case DSMI_TS_SUB_VECTORCORE_UTILIZATION_RATE:
            ipc_msg->ipc_msg_header.cmd_type = (flag == START_MSG) ? (IPCDRV_TS_VECTORCORE_UTILIZATION_RATE) :
                (IPCDRV_TS_VECTORCORE_UTILIZATION_RATE_END);
            break;
        case DSMI_TS_SUB_ACC_UTILIZATION_RATE:
            ipc_msg->ipc_msg_header.cmd_type = (flag == START_MSG) ? (IPCDRV_TS_ACC_UTILIZATION_RATE) :
                (IPCDRV_TS_ACC_UTILIZATION_RATE_END);
            break;
        case DSMI_TS_SUB_NPU_MULTI_UTILIZATION_RATE:
            ipc_msg->ipc_msg_header.cmd_type = (flag == START_MSG) ? (IPCDRV_TS_NPU_MULTI_UTILIZATION_RATE) :
                (IPCDRV_TS_NPU_MULTI_UTILIZATION_RATE_END);
            break;
        default:
            return -EOPNOTSUPP;
    }

    return 0;
}
#endif

STATIC int dms_get_core_info(core_utilization_rate_t *core_status, u8 *core, unsigned int core_len, int flag)
{
    /* if in 'profiling' mode, there will be only one log record. */
    static bool first_times = true;
    u8 ts_ret;
    u32 i, data_len, core_num;
    int ret;
    FEATURE_IPC_VARIABLE_DEFINE;

    core_num = dms_get_aic_aiv_num(core_status->cmd_type);
    if ((core_num == 0) || (core_num > max)) {
        dms_err("core_num exceed max value. (core_num=%u)\n", core_num);
        return -EINVAL;
    }

    ret = dms_fill_ipcmsg_to_ts(*core_status, &ipc_msg, flag);
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "dms_fill_ipcmsg_to_ts failed, ret = %d.\n", ret);
        return ret;
    }

    ret = dms_get_msg_from_ts(core_status->dev_id, &ipc_msg, &ack_msg, &ts_ret, &data_len);
    if (ret != 0) {
        dms_err("dms_get_msg_from_ts failed, ret = %d.\n", ret);
        return ret;
    }

    if (ts_ret != 0) {
        if (ts_ret == DMS_AI_PROFILING_CONFLIC_RET) {
            if (first_times) {
                dms_info("ts ack_msg profiling. (ts_ret=%u, flag=%d)\n", ts_ret, flag);
                first_times = false;
            }

            core_status->core_num = core_num;
            for (i = 0; i < core_len; i++) {
                core[i] = DMS_AI_PROFILING_VALUE; /* 0xEF: means conflict with profiling */
            }
            return 0;
        } else if (ts_ret == DMS_AI_TIMEOUT_RET) {
            dms_warn("ts ack_msg timeout. (ts_ret=%u, flag=%d)\n", ts_ret, flag);
            goto out;
        }
        dms_err("get ts msg failed, (ts_ret=%u, flag=%d)\n", ts_ret, flag);
        return ts_ret;
    }
    /* not 'profiling' mode and make it true, there will be a log record next times in 'profiling' mode. */
    first_times = true;

out:
    if (data_len == 0) {
        dms_err("Ack_msg.data_length is zero. (data_len=%u)\n", data_len);
        return -EIO;
    } else if (data_len == 1) { /* receive START_MSG ack, return */
        return 0;
    }

    core_status->core_num = data_len - sizeof(ts_ret);
    if (core_status->core_num > core_len) {
        dms_err("core_num(%d) is larger than max_data_length(%d).\n", core_status->core_num, core_len);
        return -EIO;
    }

    ret = memcpy_s(core, core_len, pdata, core_status->core_num);
    if (ret != 0) {
        dms_err("Memcpy_s failed. (ret=%d).\n", ret);
        return ret;
    }

    return ts_ret == DMS_AI_TIMEOUT_RET ? DMS_AI_TIMEOUT_RET : ret;
}

static void check_core_data_from_ts(const unsigned char *core_util, unsigned int core_num, unsigned int cmd_type)
{
    unsigned int i;

    for (i = 0; i < core_num; i++) {
        /* 0xEE means damaged core; 0xEF means in profiling mode; utilization would never over 100 */
        if ((core_util[i] > 100) && (core_util[i] != 0xEF && core_util[i] != 0xEE)) {
            dms_err("Invalid untilization. (utilization=%u%%, core_index=%u, cmd_type=%u)",
                core_util[i], i, cmd_type);
        }
    }
}

#define MAX_CORE_NUM 64
STATIC int dms_get_utilization_data(core_utilization_rate_t *core_status, int flag, int copy_flag)
{
    unsigned int i, j = 0;
    int ret, ts_ret = 0;
    unsigned int max_core_num = 0;
    u8 *core = NULL;
    struct devdrv_info *dev_info = NULL;

    max_core_num = dms_get_aic_aiv_num(core_status->cmd_type);
    if ((max_core_num == 0) || (max_core_num > MAX_CORE_NUM)) {
        dms_err("Core_num exceed max value. (core_num=%u)\n", max_core_num);
        return -EINVAL;
    }

    core = (u8*)dbl_kzalloc(max_core_num, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (core == NULL) {
        dms_err("kzalloc failed.\n");
        return -ENOMEM;
    }

    ret = dms_get_core_info(core_status, core, max_core_num, flag);
    if (ret != 0) {
        if (ret == DMS_AI_TIMEOUT_RET) {
            ts_ret = DMS_AI_TIMEOUT_RET;
            dms_warn("get core info timeout, (flag=%d)\n", flag);
            goto out;
        }
        dms_err("Dms get aicore info failed. (ret=%d, flag=%d)\n", ret, flag);
        dbl_kfree(core);
        core = NULL;
        return ret;
    }

    /* profiling */
    if (core[0] == DMS_AI_PROFILING_VALUE) {
        ts_ret = DMS_AI_PROFILING_CONFLIC_RET;
        dms_warn("profiling mode, (flag=%d)\n", flag);
        goto out;
    }

    /* start message return */
    if (flag == START_MSG) {
        dbl_kfree(core);
        core = NULL;
        return 0;
    }

out:
    if (!devdrv_manager_is_pf_device(core_status->dev_id)) {
        dev_info = devdrv_manager_get_devdrv_info(core_status->dev_id);
        if (dev_info == NULL) {
            dms_err("Get dev_info failed, dev_info is NULL.\n");
            dbl_kfree(core);
            core = NULL;
            return -EINVAL;
        }

        for (i = 0; i < max_core_num; i++) {
            if (core_status->cmd_type == DSMI_TS_SUB_AICORE_UTILIZATION_RATE &&
                ((dev_info->aicore_bitmap >> i) & 1) == 1) {
#ifdef CFG_FEATURE_VFIO_SOC
                core[j] = 0;
#else
                core[j] = core[i];
#endif
                ++j;
            } else if (core_status->cmd_type == DSMI_TS_SUB_VECTORCORE_UTILIZATION_RATE &&
                ((dev_info->vector_core_bitmap >> i) & 1) == 1) {
#ifdef CFG_FEATURE_VFIO_SOC
                core[j] = 0;
#else
                core[j] = core[i];
#endif
                ++j;
            }
        }

        if (core_status->cmd_type == DSMI_TS_SUB_AICORE_UTILIZATION_RATE) {
            core_status->core_num = dev_info->ai_core_num;
        } else if (core_status->cmd_type == DSMI_TS_SUB_ACC_UTILIZATION_RATE) {
            core_status->core_num = 1;
        } else {
            core_status->core_num = dev_info->vector_core_num;
        }
    }

    check_core_data_from_ts(core, core_status->core_num, core_status->cmd_type);
    if (copy_flag == COPY_TO_USER) {
        ret = copy_to_user_safe(core_status->core_utilization_rate, core, core_status->core_num);
        if (ret != 0) {
            dms_err("[dev_id = %d]:core_status.coreinfo copy_to_user_safe failed.\n", core_status->dev_id);
            dbl_kfree(core);
            core = NULL;
            return ret;
        }
    } else {
        ret = memcpy_s(core_status->core_utilization_rate, core_status->core_num, core, core_status->core_num);
        if (ret != 0) {
            dms_err("memcpy failed. (ret=%d)\n", ret);
            dbl_kfree(core);
            core = NULL;
            return ret;
        }
    }

    dbl_kfree(core);
    core = NULL;
    return (ts_ret != 0) ? ts_ret : ret;
}

STATIC int dms_get_ai_info_from_ts_para_check(char *in, u32 in_len, char *out, u32 out_len)
{
    core_utilization_rate_t *core_status = (core_utilization_rate_t *)in;
    unsigned int soc_type = CHIP_END;
    int ret;
#ifdef CFG_FEATURE_DEVICE_DEVID_CONVERT
    u32 phys_id = DEVDRV_MAX_DAVINCI_NUM + 1, vfid = 0;
#endif

    if ((in == NULL) || (in_len != sizeof(core_utilization_rate_t))) {
        dms_err("input arg is NULL, or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }

    if ((out == NULL) || (out_len != sizeof(core_status->core_num))) {
        dms_err("output arg is NULL, or out_len is wrong. (out_len=%u)\n", out_len);
        return -EINVAL;
    }

    ret = hal_kernel_get_soc_type(core_status->dev_id, &soc_type);
    if (ret != 0) {
        devdrv_drv_err("Failed to get soc type. (dev_id=%u; ret=%d)\n", core_status->dev_id, ret);
        return ret;
    }

    if (soc_type == CHIP_610LITE) {
        if (core_status->cmd_type == DSMI_TS_SUB_VECTORCORE_UTILIZATION_RATE) {
            return -EOPNOTSUPP;
        }
    }

#ifdef CFG_FEATURE_DEVICE_DEVID_CONVERT
    ret = devdrv_manager_container_logical_id_to_physical_id(core_status->dev_id, &phys_id, &vfid);
    if (ret != 0) {
        devdrv_drv_err("Failed to transfer logical ID to physical ID. (dev_id=%u; ret=%d)\n",
            core_status->dev_id, ret);
        return ret;
    }
    core_status->dev_id = phys_id;
#else
    if ((core_status->dev_id >= DEVDRV_MANGER_MAX_DEVICE_NUM) || (core_status->vfid >= VDAVINCI_MAX_VFID_NUM)) {
        dms_err("Invalid para. (dev_id=%u; vfid=%u)\n", core_status->dev_id, core_status->vfid);
        return -EINVAL;
    }
#endif

    if (((core_status->cmd_type == DSMI_TS_SUB_NPU_UTILIZATION_RATE) ||
        (core_status->cmd_type == DSMI_TS_SUB_NPU_MULTI_UTILIZATION_RATE)) &&
        (!devdrv_manager_is_pf_device(core_status->dev_id) || (core_status->vfid > 0))) {
        return -EOPNOTSUPP;
    }

    return 0;
}

STATIC int dms_get_aicore_aivector_utilization(core_utilization_rate_t *core_status, int copy_flag)
{
    int ret, retry_num = 0;

    if (core_status == NULL) {
        dms_info("core_utilization_rate_t is NULL. \n");
        return -EINVAL;
    }

    if (!devdrv_manager_ts_is_enable()) {
        return -EOPNOTSUPP;
    }

    ret = mutex_lock_interruptible_retry(&g_aicore_mutex);
    if (ret != 0) {
        dms_err("g_aicore_mutex interrupted by a signal. (ret=%d)\n", ret);
        return ret;
    }
    do {
        retry_num++;
        ret = dms_get_utilization_data(core_status, START_MSG, copy_flag);
        if (ret != 0) {
            if (ret == DMS_AI_PROFILING_CONFLIC_RET) {
                goto out;
            }
            dms_err("Starting calculate utilization failed. (ret=%d)\n", ret);
            mutex_unlock(&g_aicore_mutex);
            return -ENODATA;
        }

        msleep(50); /* interval 50 ms */

        ret = dms_get_utilization_data(core_status, END_MSG, copy_flag);
        if (ret != 0) {
            if (ret == DMS_AI_PROFILING_CONFLIC_RET) {
                goto out;
            }
        }
    } while ((ret == DMS_AI_TIMEOUT_RET) && (retry_num < 2)); /* 2 times */

    if (ret != 0) {
        if (ret == DMS_AI_TIMEOUT_RET) {
            dms_warn("Calculate utilization timeout twice. (ret=%d)\n", ret);
            goto out;
        }
        dms_err("Ending calculate utilization failed. (ret=%d)\n", ret);
        mutex_unlock(&g_aicore_mutex);
        return -ENODATA;
    }

out:
    mutex_unlock(&g_aicore_mutex);
    return 0;
}

int dms_calc_aicore_aivector_utilization(unsigned int dev_id, unsigned int cmd_type, struct dms_ts_info_in *ts_info,
    unsigned int *utilization)
{
    core_utilization_rate_t core_status = {0};
    int ret = 0;
    int i, total_rate = 0;
    int count = 0;

    if (ts_info == NULL || utilization == NULL) {
        dms_err("Invalid Parameter. (ts_info_is_null=%d; utilization_is_null=%d)\n",
            (ts_info == NULL), (utilization == NULL));
        return -EINVAL;
    }

    core_status.core_utilization_rate = (unsigned char*)dbl_kzalloc(MAX_CORE_NUM, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (core_status.core_utilization_rate == NULL) {
        dms_err("Kzalloc failed. (dev_id=%u)\n", dev_id);
        return -ENOMEM;
    }

    core_status.dev_id = dev_id;
    core_status.cmd_type = cmd_type;
    ret = dms_get_aicore_aivector_utilization(&core_status, NOT_COPY_TO_USER);
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "Failed to get utilization. (dev_id=%u, ret=%d)\n", dev_id, ret);
        goto free_and_exit;
    }

    if (core_status.core_num == 0) {
        *utilization = 0;
        goto free_and_exit;
    }

    /* 0xEE: damaged core, 0xEF: conflict with profiling */
    for (i = 0; i < core_status.core_num; i++) {
        if (core_status.core_utilization_rate[i] == 0xEE) {
            count++;
        } else if (core_status.core_utilization_rate[i] == 0xEF) {
            *utilization = 0xEF;
            goto free_and_exit;
        } else {
            total_rate += core_status.core_utilization_rate[i];
        }
    }

    if (count == core_status.core_num) {
        *utilization = 0;
    } else {
        *utilization = total_rate / (core_status.core_num - count);
    }

free_and_exit:
    dbl_kfree(core_status.core_utilization_rate);
    core_status.core_utilization_rate = NULL;
    return ret;
}

#define ACC_UTILIZATION_OFFSET 2
int dms_get_npu_mutil_utilization(core_utilization_rate_t *core_status, int copy_flag)
{
    int ret;
    unsigned char utilization = 0;
 
    ret = dms_get_aicore_aivector_utilization(core_status, COPY_TO_USER);
    if (ret != 0) {
        dms_err("Get aicore and aivector utilization failed. (ret=%d)\n", ret);
        return ret;
    }
 
#ifdef CFG_FEATURE_SUPPORT_NPU_UTIL
    u32 occupy_count = 0;
    ret = rb_read_all(core_status->dev_id, &occupy_count);
    if (ret != 0) {
        dms_err("Get npu occupy_count failed. (devid=%u; ret=%d)\n", core_status->dev_id, ret);
        return ret;
    }
 
    occupy_count = occupy_count * NPU_UTILIZATION_RATE_PERCENT / NPU_UTILIZATION_SAMPLE_NUM;
    utilization = (occupy_count < NPU_UTILIZATION_RATE_PERCENT)? occupy_count : NPU_UTILIZATION_RATE_PERCENT;
#else
    /* ts return data: aic_avg aiv_avg acc_utillization aic_max aiv_max */
    ret = copy_from_user_safe(&utilization, core_status->core_utilization_rate + ACC_UTILIZATION_OFFSET, 1);
    if (ret != 0) {
        dms_err("Get npu utilization failed. (devid=%u; ret=%d)\n", core_status->dev_id, ret);
        return ret;
    }
#endif
    ret = copy_to_user_safe(core_status->core_utilization_rate + core_status->core_num, &utilization, 1);
    if (ret != 0) {
        dms_err("Copy utilization to user space failed. (devid=%u; core_num = %u; ret=%d)\n",
            core_status->dev_id, core_status->core_num, ret);
        return ret;
    }
 
    core_status->core_num++;
    return 0;
}

int dms_get_ai_info_from_ts(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret = 0;
    core_utilization_rate_t *core_status = (core_utilization_rate_t *)in;

    ret = dms_get_ai_info_from_ts_para_check(in, in_len, out, out_len);
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "Check parameters failed. (ret=%d)\n", ret);
        return ret;
    }

    switch(core_status->cmd_type) {
        case DSMI_TS_SUB_NPU_UTILIZATION_RATE:
#ifdef CFG_FEATURE_SUPPORT_NPU_UTIL
            ret = dms_get_npu_utilization(core_status, COPY_TO_USER);
            break;
#endif
        case DSMI_TS_SUB_NPU_MULTI_UTILIZATION_RATE:
            ret = dms_get_npu_mutil_utilization(core_status, COPY_TO_USER);
            break;
        default:
            ret = dms_get_aicore_aivector_utilization(core_status, COPY_TO_USER);
    }
    if (ret != 0) {
        dms_err("Failed to get utilization. (dev_id=%u, ret=%d)\n", core_status->dev_id, ret);
        return ret;
    }

    *(unsigned int *)out = core_status->core_num;
    return 0;
}

#elif (defined(CFG_FEATURE_AI_UTIL_BY_CALCULATE) || defined(CFG_FEATURE_AI_UTIL_FROM_IPC))

STATIC int dms_ts_id_check(u32 dev_id, u32 core_id)
{
    int ret;
    u32 dev_num;

    ret = devdrv_get_devnum(&dev_num);
    if (ret != 0) {
        dms_err("Get device num failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    if (dev_id >= dev_num) {
        dms_err("Device id not valid. (dev_id=%u; dev_num=%u)\n", dev_id, dev_num);
        return -ENODEV;
    }

    if (core_id >= LPM_INVALID_ID) {
        dms_err("Core id not valid. (dev_id=%u; core_id=%u)\n", dev_id, core_id);
        return -EINVAL;
    }

    return 0;
}

STATIC int dms_ts_para_check(char *in, u32 in_len, u32 correct_in_len,
    char *out, u32 out_len, u32 correct_out_len)
{
    if ((in == NULL) || (in_len != correct_in_len)) {
        dms_err("Input char is NULL or in_len is wrong. (in_len=%u; correct_in_len=%u)\n",
            in_len, correct_in_len);
        return -EINVAL;
    }

    if ((out == NULL) || (out_len != correct_out_len)) {
        dms_err("Output char is NULL or out_len is wrong. (out_len=%u; correct_out_len=%u)\n",
            out_len, correct_out_len);
        return -EINVAL;
    }

    return 0;
}

#ifdef CFG_FEATURE_AI_UTIL_FROM_IPC
int ts_get_ai_utilization_from_ipc(u32 dev_id, u32 vfid, u32 core_id, u32 *utilization)
{
    int ret;
    u32 len = sizeof(u8);
    u8 value = 0;

    ret = dms_ts_id_check(dev_id, core_id);
    if (ret != 0) {
        dms_err("Parameter error. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    ret = lpm_get_info_from_ipc(dev_id, LPM_USE_RATE, LPM_AICORE0_ID, &value, &len);
    if (ret == ETIMEDOUT) {
        dms_warn("ts get utilization timeout from ipc. (dev_id=%u; core_id=%d; ret=%d)\n", dev_id, core_id, ret);
        return ret;
    } else if (ret != 0) {
        dms_err("ts get info from ipc failed. (dev_id=%u; core_id=%d; ret=%d)\n", dev_id, core_id, ret);
        return ret;
    }
    *utilization = (u32)value;

    return 0;
}
#endif

#ifdef CFG_FEATURE_AI_UTIL_BY_CALCULATE
STATIC void ts_get_aicore_mutex_init(struct mutex *mutex)
{
    static atomic_t aicore_mutex_init_test = KA_BASE_ATOMIC_INIT(0);
    if (ka_base_atomic_cmpxchg(&aicore_mutex_init_test, 0, 1) == 0) {
        mutex_init(mutex);
    }
    return;
}

STATIC int mutex_lock_interruptible_retry(struct mutex *lock)
{
    int ret, i;
    for (i = 0; i < 3; i++) { /* retry 3 times */
        ret = ka_task_mutex_lock_interruptible(lock);
        if (ret == 0) {
            return 0;
        }
        dms_warn("A signal arrived process was interrupted. (ret=%d\n", ret);
    }
    return ret;
}

#define TS_MAX_CYCLE    10

int ts_get_aicore_current_frequency(u32 dev_id, u32 vfid, u32 core_id, u32 *first_aicore_freq, u32 *second_aicore_freq)
{
    int ret;
    unsigned long interval_time;
    unsigned int delay_time;
    static unsigned long last_time = 0;
    u32 len = sizeof(u32);

    interval_time = abs(jiffies - last_time);
    if (interval_time < msecs_to_jiffies(TS_MAX_CYCLE)) {
        delay_time = TS_MAX_CYCLE - jiffies_to_msecs(interval_time);
        delay_time = (delay_time > TS_MAX_CYCLE) ? TS_MAX_CYCLE : delay_time;
        msleep(delay_time);
    }

    ret = lpm_get_info_from_ipc(dev_id, LPM_FREQUENCY, LPM_AICORE0_ID, (u8 *)first_aicore_freq, &len);
    if (ret != 0) {
        dms_err("Failed to obtain the frequency through the ipc."
            "(dev_id=%u; core_id=%u; ret=%d)\n", dev_id, core_id, ret);
        return ret;
    }

    ret = tsmng_get_aic_data(dev_id, vfid);
    if (ret != 0) {
        dms_err("Get aicore data failed. (dev_id=%u, core_id=%u, vfid=%u)\n", dev_id, core_id, vfid);
        return ret;
    }

    msleep(50); /* interval 50 ms */

    ret = lpm_get_info_from_ipc(dev_id, LPM_FREQUENCY, LPM_AICORE0_ID, (u8 *)second_aicore_freq, &len);
    if (ret != 0) {
        dms_err("Failed to obtain the frequency through the ipc."
            "(dev_id=%u; core_id=%u; ret=%d)\n", dev_id, core_id, ret);
        return ret;
    }

    last_time = jiffies;

    ret = tsmng_get_aic_data(dev_id, vfid);
    if (ret != 0) {
        dms_err("Get aicore data failed. (dev_id=%u)\n", dev_id);
        return ret;
    }

    return 0;
}

#define SYSTEM_OSAL_HIGH_FREQ 100    /* 100M */
#define SYSTEM_OSAL_LOW_FREQ 100    /* 100M */

STATIC int ts_calculate_aicore_util(u32 dev_id, u32 core_id, u32 aicore_freq, u32 *ai_core_util)
{
    int ts_ret;
    u64 aicore_cycle;
    u64 timestamp1;
    u64 timestamp2;
    u64 timestamp_diff;
    unsigned int system_flag;
    u64 system_fre;

    ts_ret = tsmng_get_powerinfo(dev_id, &aicore_cycle, &timestamp1, &timestamp2, &system_flag);
    system_fre = ((system_flag == 1) ? SYSTEM_OSAL_HIGH_FREQ : SYSTEM_OSAL_LOW_FREQ);

    if (ts_ret != 0) {
        dms_info("Get aicore not success. (ts_ret=%d)\n", ts_ret);
        if (ts_ret == DMS_AI_PROFILING_CONFLIC_RET) {
            *ai_core_util = 0xEF;
            return 0;
        }
        return EFAULT;
    }

    if ((timestamp2 > timestamp1) && (aicore_freq != 0)) {
        timestamp_diff = timestamp2 - timestamp1;
    } else {
        dms_err("Calculate error. (timestamp2=%llu; timestamp1=%llu; aicore_freq=%u)\n",
            timestamp2, timestamp1, aicore_freq);
        return -EINVAL;
    }

    *ai_core_util = (unsigned int)((aicore_cycle * system_fre * DATA_TO_PERCENTAGE * DATA_TO_PERCENTAGE_TEN
        + timestamp_diff * (u64)aicore_freq * DATA_TO_PERCENTAGE_FIVE)
        / (timestamp_diff * (u64)aicore_freq * DATA_TO_PERCENTAGE_TEN));

    *ai_core_util = (*ai_core_util > DATA_TO_PERCENTAGE ? DATA_TO_PERCENTAGE : *ai_core_util);

    return 0;
}

int ts_get_ai_utilization_by_calculate(u32 dev_id, u32 vfid, u32 core_id, u32 *utilization)
{
    int ret;
    static struct mutex aicore_mutex;
    u32 first_aicore_freq = 0;
    u32 second_aicore_freq;
    u32 max_aicore_freq = 0;

    ret = dms_ts_id_check(dev_id, core_id);
    if (ret != 0) {
        dms_err("Parameter error. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    ts_get_aicore_mutex_init(&aicore_mutex);
    ret = mutex_lock_interruptible_retry(&aicore_mutex);
    if (ret != 0) {
        dms_err("A signal arrived process was interrupted. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    ret = ts_get_aicore_current_frequency(dev_id, vfid, core_id, &first_aicore_freq, &second_aicore_freq);
    if (ret != 0) {
        dms_err("Ts get aicore current frequency failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        mutex_unlock(&aicore_mutex);
        return ret;
    }

    max_aicore_freq = ((first_aicore_freq > second_aicore_freq) ? first_aicore_freq : second_aicore_freq);
    ret = ts_calculate_aicore_util(dev_id, core_id, max_aicore_freq, utilization);
    if (ret != 0) {
        dms_err("Ts calculate aicore utilization failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        mutex_unlock(&aicore_mutex);
        return ret;
    }

    mutex_unlock(&aicore_mutex);
    return 0;
}
#endif

int dms_calc_aicore_aivector_utilization(unsigned int dev_id, unsigned int cmd_type, struct dms_ts_info_in *ts_info,
    unsigned int *utilization)
{
    int ret;
    struct ts_scan_list *item = NULL;

    if (ts_info == NULL || utilization == NULL) {
        dms_err("Invalid Parameter. (ts_info_is_null=%d; utilization_is_null=%d)\n",
            (ts_info == NULL), (utilization == NULL));
        return -EINVAL;
    }

    if (ts_info->vfid > 0) {
        return -EOPNOTSUPP;
    }

    item = find_operate_ts_info_str(DMS_SUBCMD_GET_AI_INFO_FROM_TS, ts_info->core_id);
    if (item == NULL) {
        return -EOPNOTSUPP;
    }

    if (item->scan_flag == 0) {
        ret = item->get_value_handle(ts_info->dev_id, ts_info->vfid, ts_info->core_id, utilization);
        if (ret != 0) {
            dms_err("Dms get ai utilization failed. (dev_id=%u; core_id=%u; ret=%d)\n",
                ts_info->dev_id, ts_info->core_id, ret);
            return ret;
        }
    } else {
        if (ts_info->dev_id >= TS_DEVICE_NUM_MAX) {
            dms_err("Invalid device id. (dev_id=%u)\n", ts_info->dev_id);
            return -EINVAL;
        }
        *utilization = item->pf_value[ts_info->dev_id];
    }

    return 0;
}

int dms_get_ai_info_from_lp(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    u32 utilization = 0;
    struct dms_ts_info_in *ts_info = NULL;

    ret = dms_ts_para_check(in, in_len, sizeof(struct dms_ts_info_in), out, out_len, sizeof(u32));
    if (ret != 0) {
        dms_err("Para check failed.\n");
        return ret;
    }

    ts_info = (struct dms_ts_info_in *)in;

    ret = dms_calc_aicore_aivector_utilization(ts_info->dev_id, 0, ts_info, &utilization);
    if (ret == -EOPNOTSUPP) {
        return ret;
    } else if (ret != 0) {
        dms_err("Failed to get ai utilization. (dev_id=%u, ret=%d)\n", ts_info->dev_id, ret);
        return ret;
    }

    *(u32 *)out = utilization;

    return 0;
}

#endif
