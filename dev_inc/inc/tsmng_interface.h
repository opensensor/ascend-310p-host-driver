/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
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
 * Create: 2022-08-13
 */
#ifndef __TSMNG_INTERFACE_H__
#define __TSMNG_INTERFACE_H__

#include <linux/types.h>
/* depends on module-icm */
#include "icm_interface.h"

/* All chips are defined the same */
enum ts_msg_cmd_type {
    IPCDRV_TS_SUSPEND = 0,                          /* outer in drv_platform */
    IPCDRV_TS_SUSPEND_READY = 1,                    /* outer in drv_platform */
    IPCDRV_TS_CONFIG_AICPU_GICR = 2,                /* not used */
    IPCDRV_TS_INFORM_CORE_INFO = 3,                 /* outer in drv_platform */
    IPCDRV_TS_INQUIRY_AI_RATE = 4,                  /* not used */
    IPCDRV_TS_INFORM_TS_IDLE = 5,                   /* only inner */
    IPCDRV_TS_INFORM_TS_LIMIT_AICORE = 6,           /* only inner */
    IPCDRV_TS_INFORM_TS_CANCEL_LIMIT = 7,           /* only inner */
    IPCDRV_TS_UPPER_DDR_FREQ = 8,                   /* outer in drv_devmng */
    IPCDRV_TS_LOWER_DDR_FREQ = 9,                   /* outer in drv_devmng */
    IPCDRV_TS_HEARTBEAT_TOAICPU = 10,               /* outer in drv_devmng, not called */
    IPCDRV_TS_AI_COMPUTING_POWER = 11,              /* only inner */
    IPCDRV_TS_INQUIRY_AICORE_TASK = 12,             /* outer in drv_devmng, not called */
    IPCDRV_TS_CORE_RAS_ERR = 13,                    /* only inner */
    IPCDRV_TS_AIV_COMPUTING_POWER = 16,             /* only inner */
    IPCDRV_TSC_OPERATE_GROUP = 17,                  /* outer in drv_devmng */
    IPCDRV_TSC_GET_GROUP_INFO = 18,                 /* outer in drv_devmng */
    IPCDRV_TSV_OPERATE_GROUP = 19,                  /* outer in drv_devmng */
    IPCDRV_TSV_GET_GROUP_INFO = 20,                 /* outer in drv_devmng */
    IPCDRV_TS_POWEROFF = 21,                        /* outer in drv_platform */
    IPCDRV_TS_MEM_OP = 22,                          /* not used */
    IPCDRV_TS_AICORE_UTILIZATION_RATE = 23,         /* outer in dms_status of drv_devmng */
    IPCDRV_TS_VECTORCORE_UTILIZATION_RATE = 24,     /* outer in dms_status of drv_devmng */
    IPCDRV_TS_ADV_CMDTYPE_ALARM_MSG_REPORT = 25,    /* outer in drv_platform */
    IPCDRV_TS_QOS_SET = 28,                         /* outer in drv_platform */
    IPCDRV_TS_QOS_GET = 29,                         /* outer in drv_platform */
    IPCDRV_TS_AICORE_UTILIZATION_RATE_END = 30,     /* outer in dms_status of drv_devmng */
    IPCDRV_TS_VECTORCORE_UTILIZATION_RATE_END = 31,
    IPCDRV_TS_AIC_AIV_FAULT_INFO = 32,
    IPCDRV_TS_MASK_ALARM = 33,
    IPCDRV_TS_AIC_UTILIZATION_RATE_ASYN = 34,       /* outer in ts_mng */
    IPCDRV_TS_AIV_UTILIZATION_RATE_ASYN = 35,       /* outer in ts_mng */
    IPCDRV_TS_AIC_UTILIZATION_RATE_ASYN_END = 36,       /* outer in ts_mng */
    IPCDRV_TS_AIV_UTILIZATION_RATE_ASYN_END = 37,       /* outer in ts_mng */
    IPCDRV_TS_CLOCK_OPERATE = 38,                   /* outer in drv_ts */
    IPCDRV_TS_FREQSCALING = 39,                     /* outer in drv_ts */
    IPCDRV_TS_DEVICE_OPERATE = 40,                  /* outer in drv_ts */
    IPCDRV_TS_QOS_CFG = 41,                         /* outer in drv_platform */
    IPCDRV_TS_STL_LAUNCH = 42,                      /* outer in drv_devdrv */
    IPCDRV_TS_STL_QUERY = 43,                       /* outer in drv_devdrv */
    IPCDRV_TS_ACC_UTILIZATION_RATE = 44,         /* outer in dms_status of asdrv_dms */
    IPCDRV_TS_ACC_UTILIZATION_RATE_END = 45,     /* outer in dms_status of asdrv_dms */    
    IPCDRV_TS_NPU_MULTI_UTILIZATION_RATE = 46,         /* outer in dms_status of asdrv_dms */
    IPCDRV_TS_NPU_MULTI_UTILIZATION_RATE_END = 47,     /* outer in dms_status of asdrv_dms */
    IPCDRV_TS_MAX,
};

typedef int (*tsmng_handler)(void *data, u32 dev_id);

/* EXPORT_SYMBOL APIs, called by other ko; all chips' APIs are the same */
#if defined(CFG_SOC_PLATFORM_MINI) && !defined(DCFG_SOC_PLATFORM_MINIV2)    /* only ascend310 ascend310rc */
int tsmng_inform_ts_limit_aicore(u32 dev_id);
int tsmng_inform_ts_cancel_limit_aicore(u32 dev_id);
#endif
int tsmng_get_ai_computing_power(unsigned long arg);
int tsmng_mailbox_rx_register(u32 dev_id);
void tsmng_mailbox_rx_unregister(u32 dev_id);
int tsmng_handler_register_upper_ddrfreq(u32 dev_id, tsmng_handler handler);
int tsmng_handler_register_lower_ddrfreq(u32 dev_id, tsmng_handler handler);
void tsmng_handler_unregister_ddrfreq(u32 dev_id);
int tsmng_handler_register_ts_heartbeat_toaicpu(u32 dev_id, tsmng_handler handler);
int tsmng_chk_ipc_crc16(struct ipcdrv_message *ipc_msg);

#ifdef CFG_UTIL_BY_CALCULATE
int tsmng_get_aic_data(u32 dev_id, u32 vfid);
int tsmng_get_aiv_data(u32 dev_id, u32 vfid);
int tsmng_get_powerinfo(u32 dev_id, u64 *aicore_cycle, u64 *timestamp1, u64 *timestamp2, u32 *system_flag);
#endif

int tsmng_inform_ts_mask_alarm(u8 type, u8 mask_switch, u32 dev_id, u32 tsid);
int tsmng_query_ts_mask_alarm(u8 type, u8 *mask_switch, u32 dev_id, u32 tsid);

#ifdef CFG_SOC_PLATFORM_CLOUD
void tsmng_ghes_ts_err_register(void);
void tsmng_ghes_ts_err_unregister(void);
#endif

#ifdef CFG_FEATURE_LP_ENABLE
/* feature group */
#ifndef MAX_IPCDRV_MSG_LENGTH
#define MAX_IPCDRV_MSG_LENGTH ICM_MSG_MAX_LENGTH
#endif

#define TS_IPC_MSG_HEAD_LEN 4
#define TS_IPC_MSG_CRC_LEN 2
#define TS_IPC_MSG_SERIAL_BYTE_OCCUPY 4
#define TS_IPC_MSG_PAYLOAD_LEN  (MAX_IPCDRV_MSG_LENGTH - TS_IPC_MSG_HEAD_LEN - TS_IPC_MSG_SERIAL_BYTE_OCCUPY)

struct ts_ipcdrv_message {
    struct ipcdrv_msg_header ipc_msg_header;
    unsigned int msg_serial_num;
    u8 ipcdrv_payload[TS_IPC_MSG_PAYLOAD_LEN];
};

#define COMPUTE_GROUP_INFO_RES_NUM 8
struct ipc_operate_group_req_payload {
    unsigned char  group_id;
    unsigned char  operate_flag;
    unsigned char  extend_attribute; // bit 0=1表示是默认group
    unsigned char  aicore_number; // 0~9
    unsigned char  aivector_number; // 0~7
    unsigned char  sdma_number; // 0~15
    unsigned char  aicpu_number; // 0~15
    unsigned char  active_sq_number; // 0~31
    unsigned char  res[COMPUTE_GROUP_INFO_RES_NUM];
};

struct ipc_operate_group_rsp_payload {
    unsigned char result;
    unsigned char operate_flag;
};

int tsmng_group_getinfo_from_ts_sync(unsigned int device_id, int ts_id,
    struct ts_ipcdrv_message *req_msg, struct ts_ipcdrv_message *rsp_msg);
int tsmng_group_operate_to_ts_sync(unsigned int device_id, int ts_id,
    struct ts_ipcdrv_message *req_msg, struct ts_ipcdrv_message *rsp_msg);
#endif

void tsmng_ts_plugin_load(u32 dev_id);

#endif  /* __TSMNG_INTERFACE_H__ */
