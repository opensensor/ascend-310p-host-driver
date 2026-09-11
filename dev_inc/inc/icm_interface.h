/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2023. All rights reserved.
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
 * Create: 2021-10-15
 */

#ifndef ICM_INTERFACE_H
#define ICM_INTERFACE_H
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/notifier.h>

#ifdef AOS_LLVM_BUILD
#include <linux/atomic.h>
#endif
#include "drv_ipc.h"

#define ICM_MSG_MAX_LENGTH 32
#define RESERVE_LEN (ICM_MSG_DATA_LENGTH - sizeof(u32) * 4)

#define ICM_IPC_MSG_REQ 0
#define ICM_IPC_MSG_RESQ 1

#define ICM_IPC_MSG_OLD_VER 0
#define ICM_IPC_MSG_NEW_VER 1
/* ipcdrv_message data, crc is in the last two bytes */
#define ICM_IPC_MSG_IPCDRV_VER 2
#define ICM_IPC_MSG_UNKNOW_VER 0xFF

#define CRC_POLYNOMIAL      0x1021
#define NULL_USHORT         0xFFFF
#ifndef AOS_LLVM_BUILD
#define BITS_PER_BYTE       8
#endif
#define BIT15               0x8000

#define ICM_IPC_MSG_HEAD_LEN 8
#define CMD_PARA_LEN 4

#define ICM_DEV_ID_BEG 16
#define ICM_MAILBOX_ID_MASK 0x0FFFFu
#ifndef AOS_LLVM_BUILD
#define ICM_INVAILED_FD U32_MAX
#else
#define ICM_INVAILED_FD ((u32)~0U)
#endif
#define ICM_FD_BUILD(dev_id, chan_id) (((dev_id) << ICM_DEV_ID_BEG) | ((chan_id) & ICM_MAILBOX_ID_MASK))

#define ICM_MAGIC_WORD 0x586663EE

struct devdrv_ipc_cmd {
    u8 cmd_sub;
    u8 cmd;
    u8 target_id;
    u8 source_id;
    u8 cmdPara0[CMD_PARA_LEN];
    u8 cmdPara1[CMD_PARA_LEN];
    u8 cmdPara2[CMD_PARA_LEN];
    u8 cmdPara3[CMD_PARA_LEN];
    u8 cmdPara4[CMD_PARA_LEN];
    u8 cmdPara5[CMD_PARA_LEN];
    u8 cmdPara6[CMD_PARA_LEN];
};

struct icmdrv_heartbeat_data {
    u32 count;
    u32 time_sec_low;
    u32 time_sec_hight;
    u32 time_usec;
    u8 data[RESERVE_LEN];
};

struct icmdrv_ipc_msg {
    u8 sub_cmd;
    u8 cmd;
    u8 cmd_dest;
    u8 cmd_src;
    u8 msg_type;
    u8 len;
    u8 data[ICM_MSG_DATA_LENGTH];
};

struct icmdrv_ipc_msg_info {
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

struct icmdrv_msg_chan {
    u32 dev_id;
    u32 chan_id;
    u32 peer;
    u32 magic;
    wait_queue_head_t wait_queue;
    struct mutex psci_mutex;
    struct notifier_block ipc_monitor;
    struct notifier_block icm_monitor;
    u32 wait_time;
    u32 wait_timeout_count;
    atomic_t wait_flag;
    u16 msg_seq;
    u32 msg_version;
    struct atomic_notifier_head notifier;
    u32 send_data[IPCDRV_RPROC_MSG_LENGTH];
    u32 ack_data[IPCDRV_RPROC_MSG_LENGTH];
};

enum icm_ipc_cmd_obj {
    OBJ_CMD_CCPU = 0,
    OBJ_CMD_ATF,
    OBJ_CMD_LP = 0x10,
    OBJ_CMD_TS = 0x20,
    OBJ_CMD_TSC,
    OBJ_CMD_TSV,
    OBJ_CMD_SI = 0x30,
    OBJ_CMD_ISP = 0x40,
    OBJ_CMD_ACPU = 0x50,
    OBJ_CMD_MAX
};

enum icm_ipc_main_cmd {
    ICM_MAIN_CMD_CCPU_HB = 0,
    ICM_MAIN_CMD_CCPU_REQ,
    ICM_MAIN_CMD_CCPU_RESP,
    ICM_MAIN_CMD_CCPU_NOTIFY,
    ICM_MAIN_CMD_CCPU_SET,
    ICM_MAIN_CMD_CCPU_DMP,
    ICM_MAIN_CMD_LP_HB = 0x10,
    ICM_MAIN_CMD_LP_REQ,
    ICM_MAIN_CMD_LP_RESP,
    ICM_MAIN_CMD_LP_NOTIFY,
    ICM_MAIN_CMD_LP_SET,
    ICM_MAIN_CMD_LP_DMP_MCU,
    ICM_MAIN_CMD_TS_HB = 0x20,
    ICM_MAIN_CMD_TS_REQ,
    ICM_MAIN_CMD_TS_RESP,
    ICM_MAIN_CMD_TS_NOTIFY,
    ICM_MAIN_CMD_TS_SET,
    ICM_MAIN_CMD_SI_HB = 0x30,
    ICM_MAIN_CMD_SI_REQ,
    ICM_MAIN_CMD_SI_RESP,
    ICM_MAIN_CMD_SI_NOTIFY,
    ICM_MAIN_CMD_SI_SET,
    ICM_MAIN_CMD_ISP_HB = 0x40,
    ICM_MAIN_CMD_ISP_REQ,
    ICM_MAIN_CMD_ISP_RESP,
    ICM_MAIN_CMD_ISP_NOTIFY,
    ICM_MAIN_CMD_ISP_SET,
    ICM_MAIN_CMD_ACPU_HB = 0x50,
    ICM_MAIN_CMD_ACPU_REQ,
    ICM_MAIN_CMD_ACPU_RESP,
    ICM_MAIN_CMD_ACPU_NOTIFY,
    ICM_MAIN_CMD_ACPU_SET,
    ICM_MAIN_CMD_MEMORY_REQ = 0x60,
    ICM_MAIN_CMD_MEMORY_RESP,
    ICM_MAIN_CMD_MEMORY_NOTIFY,
    ICM_MAIN_CMD_MEMORY_SET,
    ICM_MAIN_CMD_DVPP_HB = 0x70,
    ICM_MAIN_CMD_DVPP_REQ,
    ICM_MAIN_CMD_DVPP_RESP,
    ICM_MAIN_CMD_DVPP_NOTIFY,
    ICM_MAIN_CMD_DVPP_SET,
    ICM_MAIN_CMD_MAX
};

enum icm_ipc_sub_cmd {
    ICM_SUB_CMD_NO_TYPE = 0,
    ICM_SUB_CMD_TEMP,
    ICM_SUB_CMD_DDR_TEMP,
    ICM_SUB_CMD_HBM_TEMP,
    ICM_SUB_CMD_VRD_TEMP,
    ICM_SUB_CMD_SOC_TEMP,
    ICM_SUB_CMD_RESERVED1_TEMP,
    ICM_SUB_CMD_RESERVED2_TEMP,
    ICM_SUB_CMD_FREQ,
    ICM_SUB_CMD_DDR_FREQ,
    ICM_SUB_CMD_HBM_FREQ, // 10
    ICM_SUB_CMD_AIC_FREQ,
    ICM_SUB_CMD_AIV_FREQ,
    ICM_SUB_CMD_CCPU_FREQ,
    ICM_SUB_CMD_UPPER_DDR_FREQ,
    ICM_SUB_CMD_LOWER_DDR_FREQ,
    ICM_SUB_CMD_RESERVED1_FREQ,
    ICM_SUB_CMD_RESERVED2_FREQ,
    ICM_SUB_CMD_ECC,
    ICM_SUB_CMD_DDR_ECC,
    ICM_SUB_CMD_HBM_ECC, // 20
    ICM_SUB_CMD_POWER,
    ICM_SUB_CMD_SOC_POWER,
    ICM_SUB_CMD_AI_COMPUTING_POWER,
    ICM_SUB_CMD_AIV_COMPUTING_POWER,
    ICM_SUB_CMD_POWEROFF,
    ICM_SUB_CMD_SLEEP,
    ICM_SUB_CMD_SUSPEND,
    ICM_SUB_CMD_SUSPEND_READY,
    ICM_SUB_CMD_RESUME,
    ICM_SUB_CMD_IDLE, // 30
    ICM_SUB_CMD_VOLTAGE,
    ICM_SUB_CMD_AICORE_VOL_CURRENT,
    ICM_SUB_CMD_HYBIRD_VOL_CURRENT,
    ICM_SUB_CMD_TAISHAN_VOL_CURRENT,
    ICM_SUB_CMD_DDR_VOL_CURRENT,
    ICM_SUB_CMD_RESERVED1_VOL_CURRENT,
    ICM_SUB_CMD_RESERVED2_VOL_CURRENT,
    ICM_SUB_CMD_INFO,
    ICM_SUB_CMD_DDR_BASE_INFO,
    ICM_SUB_CMD_DDR_MANUFACTURES_INFO, // 40
    ICM_SUB_CMD_CORE_INFO,
    ICM_SUB_CMD_RESERVED1_INFO,
    ICM_SUB_CMD_RESERVED2_INFO,
    ICM_SUB_CMD_HEALTH,
    ICM_SUB_CMD_HEALTH_STATE,
    ICM_SUB_CMD_HEALTH_EVENT,
    ICM_SUB_CMD_EXCEPTION,
    ICM_SUB_CMD_EXCEPTION_INFO,
    ICM_SUB_CMD_THRESHOLD,
    ICM_SUB_CMD_DDR_THRESHOLD, // 50
    ICM_SUB_CMD_SOC_THRESHOLD,
    ICM_SUB_CMD_RESERVED1_THRESHOLD,
    ICM_SUB_CMD_RESERVED2_THRESHOLD,
    ICM_SUB_CMD_LIMIT,
    ICM_SUB_CMD_SUGGEST_LIMIT,
    ICM_SUB_CMD_CANCEL_LIMIT,
    ICM_SUB_CMD_AICORE_LIMIT,
    ICM_SUB_CMD_PROFILE_LIMIT,
    ICM_SUB_CMD_RESERVED2_LIMIT,
    ICM_SUB_CMD_ALARM, // 60
    ICM_SUB_CMD_TSENSOR_RESET_ALARM,
    ICM_SUB_CMD_CANCEL_RESET_ALARM,
    ICM_SUB_CMD_RESERVED1_ALARM,
    ICM_SUB_CMD_RESERVED2_ALARM,
    ICM_SUB_CMD_AI_RATE,
    ICM_SUB_CMD_AICORE_UTILIZATION_RATE,
    ICM_SUB_CMD_VECTORCORE_UTILIZATION_RATE,
    ICM_SUB_CMDC_GROUP,
    ICM_SUB_CMDC_OPERATE_GROUP,
    ICM_SUB_CMDC_GET_GROUP_INFO, // 70
    ICM_SUB_CMDV_OPERATE_GROUP,
    ICM_SUB_CMDV_GET_GROUP_INFO,
    ICM_SUB_CMD_HEART,
    ICM_SUB_CMD_SMOKE,
    ICM_SUB_CMD_AICORE_TASK,
    ICM_SUB_CMD_CORE_RAS_ERR,
    ICM_SUB_CMD_VERSION,
    ICM_SUB_CMD_DEBUG,
    ICM_SUB_CMD_LOG,
    ICM_SUB_CMD_MEM_OP, // 80
    ICM_SUB_CMD_CONFIG_AICPU_GICR,
    ICM_SUB_CMD_CREAT_VF,
    ICM_SUB_CMD_DESTORY_VF,
    ICM_SUB_CMD_RESET_VF,
    ICM_SUB_CMD_SIZE,
    ICM_SUB_CMD_DDR_SIZE,
    ICM_SUB_CMD_HBM_SIZE,
    ICM_SUB_CMD_AI_COMPUTING_POWER_END, // 88
    ICM_SUB_CMD_AIV_COMPUTING_POWER_END, // 89
    ICM_SUB_CMD_GET_QOS, // 90
    ICM_SUB_CMD_SET_QOS, // 91
    ICM_SUB_CMD_GET_VRD_INFO, // 92 temp
    ICM_SUB_CMD_SET_VRD_INFO, // 93 upgrade, config
    ICM_SUB_CMD_POWEROFF_ACK,
    ICM_SUB_CMD_QUERY_LP_ACG = 102,
    ICM_SUB_CMD_STRESS_DOG = 103,
    ICM_SUB_CMD_NOCMT_INFO = 104,
    ICM_SUB_CMD_TURBO_ONOFF = 150,
    ICM_SUB_CMD_MEM_ALLOC,
    ICM_SUB_CMD_MEM_FREE,
    ICM_SUB_CMD_CFG_QOS, // 153, also see 90, 91
    ICM_SUB_CMD_STL_LAUNCH, // 154
    ICM_SUB_CMD_STL_QUERY, // 155
    ICM_SUB_CMD_STL_BIND, // 156
    ICM_SUB_CMD_SET_BIST_INFO, // 157
    ICM_SUB_CMD_TS_RAS_MSG, // 158
    ICM_SUB_CMD_ACC_COMPUTING_POWER, // 159
    ICM_SUB_CMD_ACC_COMPUTING_POWER_END, // 160
    ICM_SUB_CMD_TS_UPDATE_PATCH_START,   // 161
    ICM_SUB_CMD_TS_UPDATE_PATCH_END, // 162
    ICM_SUB_CMD_EXCHANGE_TS_CAPACITY, // 163
    ICM_SUB_CMD_LPM_IDLE_ADJUST, // 164
    ICM_SUB_CMD_NPU_MULTI_UTILIZATION_RATE,  // 165
    ICM_SUB_CMD_NPU_MULTI_UTILIZATION_RATE_END,  // 166
    ICM_SUB_CMD_ALARM_SIMULATE = 255, // fault injection
    ICM_SUB_CMD_MAX
};

enum icm_ipc_peer_obj {
    IPC_OBJ_LP,
    IPC_OBJ_TS,
    IPC_OBJ_TSC,
    IPC_OBJ_TSV,
    IPC_OBJ_SI,
    IPC_OBJ_ISP,
    IPC_OBJ_RTC,
    IPC_OBJ_MAX
};

typedef int (*notifier_call)(struct notifier_block *nb, unsigned long len, void *data);
typedef u32 ICM_HANDLE;

ICM_HANDLE icm_msg_chan_alloc(u32 dev_id, u32 peer);
u16 icm_crc16(const u8 *data, u16 len);
void icm_msg_chan_free(ICM_HANDLE fd);
int icm_msg_send_async(ICM_HANDLE fd, u32 *msg, u32 send_len);
int icm_msg_fast_send_async(ICM_HANDLE fd, u32 *msg, u32 send_len);
int icm_msg_send_sync(ICM_HANDLE fd, u32 *msg, u32 send_len,
    u32 *ack_buf, u32 ack_len);

int icm_rx_register(u32 dev_id, u32 chan_id, u32 peer, struct notifier_block *nb);
int icm_rx_unregister(u32 dev_id, u32 chan_id, u32 peer, struct notifier_block *nb);
u32 icm_get_msg_version(u32 dev_id, u32 chan_id);

#endif
