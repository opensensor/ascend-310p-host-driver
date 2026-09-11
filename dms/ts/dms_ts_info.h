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
*/

#ifndef DMS_TS_STATUS_H
#define DMS_TS_STATUS_H

#define DMS_MODULE_TS_INFO "dms_ts_info"

#if defined(CFG_SOC_PLATFORM_CLOUD)
    #define DEVDRV_MANGER_MAX_DEVICE_NUM 64
#elif defined(CFG_SOC_PLATFORM_MINIV2)
#if defined(CFG_SOC_PLATFORM_MDC_V51)
    #define DEVDRV_MANGER_MAX_DEVICE_NUM 1
#else
    #define DEVDRV_MANGER_MAX_DEVICE_NUM 2
#endif
#elif defined(CFG_SOC_PLATFORM_MINI)
    #define DEVDRV_MANGER_MAX_DEVICE_NUM 64
#endif

#define DMS_AI_PROFILING_VALUE  0xEF
#define DMS_AI_PROFILING_CONFLIC_RET 2
#define DMS_AI_TIMEOUT_RET 7
#define START_MSG 0
#define END_MSG 1
#define NPU_CORE_NUM 5

typedef enum {
    DSMI_TS_SUB_AICORE_UTILIZATION_RATE = 0,
    DSMI_TS_SUB_VECTORCORE_UTILIZATION_RATE = 1,
    DSMI_TS_SUB_FFTS_TYPE, // Obtains the type of FFTS or FFTS+
    DSMI_TS_SUB_SET_FAULT_MASK,
    DSMI_TS_SUB_GET_FAULT_MASK,
    DSMI_TS_SUB_LAUNCH_AICORE_STL,
    DSMI_TS_SUB_AICORE_STL_STATUS,
    DSMI_TS_SUB_AIC_UTILIZATION_RATE_ASYN,
    DSMI_TS_SUB_AIV_UTILIZATION_RATE_ASYN,
    DSMI_TS_SUB_ACC_UTILIZATION_RATE,
    DSMI_TS_SUB_NPU_UTILIZATION_RATE,
    DSMI_TS_SUB_COMMON_MSG, // consistent to DSMI_TS_SUB_CMD_COMMON_MSG
    DSMI_TS_SUB_NPU_MULTI_UTILIZATION_RATE = 12,
    DSMI_TS_SUB_CMD_MAX_VALUE
} DSMI_SUB_TS_INFO;

typedef struct _core_utilization_rate {
    unsigned int dev_id;
    unsigned int cmd_type;
    unsigned char *core_utilization_rate;
    unsigned int core_num;
    unsigned int vfid;
} core_utilization_rate_t;

struct dms_ts_info_in {
    unsigned int dev_id;
    unsigned int vfid;
    unsigned int core_id;
};

struct dms_ts_hb_status_in {
    unsigned int dev_id;
    unsigned int vf_id;
    unsigned int ts_id;
};

int dms_ts_init(void);
void dms_ts_uninit(void);
int dms_get_ai_info_from_ts(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len);
int dms_get_ai_info_from_lp(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len);
int ts_get_ai_utilization(unsigned int dev_id, unsigned int vfid, unsigned int core_id, unsigned int *utilization);
int dms_calc_aicore_aivector_utilization(unsigned int dev_id, unsigned int cmd_type, struct dms_ts_info_in *ts_info,
    unsigned int *utilization);

#ifdef CFG_FEATURE_AI_UTIL_BY_CALCULATE
int ts_get_ai_utilization_by_calculate(unsigned int dev_id, unsigned int vfid, unsigned int core_id,
    unsigned int *utilization);
#define ts_get_ai_utilization ts_get_ai_utilization_by_calculate
#endif
#ifdef CFG_FEATURE_AI_UTIL_FROM_IPC
int ts_get_ai_utilization_from_ipc(unsigned int dev_id, unsigned int vfid, unsigned int core_id,
    unsigned int *utilization);
#define ts_get_ai_utilization ts_get_ai_utilization_from_ipc
#endif

#ifdef CFG_FEATURE_TS_FAULT_MASK
int dms_get_ts_fault_mask(void *feature, char *in, u32 in_len, char *out, u32 out_len);
int dms_set_ts_fault_mask(void *feature, char *in, u32 in_len, char *out, u32 out_len);
#endif
#endif
