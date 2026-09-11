/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: tsch_defines_profling.h
 * Create: 2025-05-09
 */
#ifndef TS_TSCH_DEFINES_PROFILING_H
#define TS_TSCH_DEFINES_PROFILING_H

#ifndef DAVINCI_MDC_VOS
#if defined(__COMPILER_HUAWEILITEOS__)
#include <los_typedef.h>
#elif defined(__KERNEL__)
#else
#include <stdint.h>
#endif
#else
#include "ee_platform_types.h"
#endif

#ifdef STARS_CTRL_CPU
#include <linux/types.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @ingroup tsch
 * @brief max event num of dha_dfx
 */
#define DHA_DFX_EVENT_MAX_NUM (8U)

#define DHA_DFX_EVENT_COMMAND_BIT_LEN (8U)

#define MATA_MATA_EVENT_TYPE_NUM (4U)

#define SMMU_DFX_EVENT_MAX_NUM (8U)
 
#define NOC_DFX_EVENT_MAX_NUM (4U)

enum tag_ts_prof_type_e {
    TS_PROF_TYPE_TASK_BASE = 0,
    TS_PROF_TYPE_SAMPLE_BASE,
};

enum tag_ts_profil_command_type {
    TS_PROFILE_COMMAND_TYPE_ACK = 0,
    TS_PROFILE_COMMAND_TYPE_PROFILING_ENABLE = 1,
    TS_PROFILE_COMMAND_TYPE_PROFILING_DISABLE = 2,
    TS_PROFILE_COMMAND_TYPE_BUFFERFULL = 3,
    TS_PROFILE_COMMAND_TYPE_PRO_RPTR_UPDATE = 4,
    TS_PROFILE_COMMAND_TYPE_PRO_WPTR_UPDATE = 5,
};

typedef struct tag_ts_ts_cpu_profile_config {
    uint32_t period;
    uint32_t event_num;
    uint32_t event[0];
} ts_ts_cpu_profile_config_t;

typedef struct tag_ts_ai_cpu_profile_config {
    uint32_t type;  // 0-task base, 1-sample base
    uint32_t period;
    uint32_t event_num;
    uint32_t event[0];
} ts_ai_cpu_profile_config_t;

typedef struct tag_ts_ai_core_profile_config {
    uint32_t type;                   // 0-task base, 1-sample base
    uint32_t almost_full_threshold;  // sample base
    uint32_t period;                 // sample base
    uint32_t core_mask;              // sample base
    uint32_t event_num;              // public
    uint32_t event[8];               // 8:max event num
    uint32_t tag;                    // bit0==0-enable immediately, bit0==1-enable delay
} ts_ai_core_profile_config_t;

typedef struct tag_ts_ai_vector_profile_config {
    uint32_t type;                   // 0-task base, 1-sample base
    uint32_t almost_full_threshold;  // sample base
    uint32_t period;                 // sample base
    uint32_t core_mask;              // sample base
    uint32_t event_num;              // public
    uint32_t event[0];               // public
    uint32_t tag;                    // bit0==0-enable immediately, bit0==1-enable delay
} ts_ai_vector_profile_config_t;

typedef struct tag_ts_ts_fw_profile_config {
    uint32_t period;
    uint32_t ts_task_track;     // 1-enable,2-disable
    uint32_t ts_cpu_usage;      // 1-enable,2-disable
    uint32_t ai_core_status;    // 1-enable,2-disable
    uint32_t ts_timeline;       // 1-enable,2-disable
    uint32_t ai_vector_status;  // 1-enable,2-disable
    uint32_t ts_keypoint;       // 1-enable,2-disable
    uint32_t ts_taskstep;       // 1-enable,2-disable
    uint32_t ts_blockdim;       // 1-enable,2-disable
} ts_ts_fw_profile_config_t;

typedef struct tag_ts_l2_profile_config_t {
    uint32_t event_num;
    uint32_t event[DHA_DFX_EVENT_MAX_NUM];
} ts_l2_profile_config_t;

typedef struct tag_ts_stars_soc_log_config {
    uint32_t acsq_task;         // 1-enable,2-disable
    uint32_t acc_pmu;           // 1-enable,2-disable
    uint32_t cdqm_req;          // 1-enable,2-disable
    uint32_t dvpp_vpc_block;    // 1-enable,2-disable
    uint32_t dvpp_jpegd_block;  // 1-enable,2-disable
    uint32_t dvpp_jpege_block;  // 1-enable,2-disable
    uint32_t ffts_thread_task;  // 1-enable,2-disable
    uint32_t ffts_block;        // 1-enable,2-disable
    uint32_t sdma_dmu;          // 1-enable,2-disable
    uint32_t tag;               // bit0==0-enable immediately, bit0==1-enable delay
} ts_stars_soc_log_config_t;

typedef struct tag_ts_ffts_profile_config {
    uint32_t type;              // bit0-task base, bit1-sample base, bit2 blk task, bit3 sub task
    uint32_t period;            // sample base
    uint32_t core_mask;         // sample base
    uint32_t event_num;         // public
    uint16_t event[DHA_DFX_EVENT_MAX_NUM];    // public
} ts_ffts_profile_config_t;

typedef struct tag_ts_stars_ffts_profile_config {
    uint32_t cfg_mode;   // 0-none,1-aic,2-aiv,3-aic&aiv
    ts_ffts_profile_config_t aic_cfg;
    ts_ffts_profile_config_t aiv_cfg;
    uint32_t tag;        // bit0==0-enable immediately, bit0==1-enable delay, high 16bit user_profile_mode
} ts_stars_ffts_profile_config_t;

typedef struct  tag_stars_profile_config {
    uint32_t inner_switch;   // 1-enable,2-disable
    uint32_t period;        // ms
}ts_stars_profile_config_t;

typedef struct tag_stars_soc_profile_config {
    ts_stars_profile_config_t acc_pmu;
    ts_stars_profile_config_t on_chip;
    ts_stars_profile_config_t inter_die;
    ts_stars_profile_config_t inter_chip;
    ts_stars_profile_config_t low_power;
    ts_stars_profile_config_t stars_info;
}ts_stars_soc_profile_config_t;

typedef struct tag_ts_stars_biu_perf_config {
    uint32_t cycles;
} ts_stars_biu_perf_config_t;

typedef struct tag_ts_hwts_log_config {
    uint32_t tag;          // bit0==0-enable immediately, bit0==1-enable delay
} ts_hwts_log_config_t;

typedef enum tag_profile_base_tlv_type {
    TS_STARS_PROFILE_PARAM_DESC = 0,
    TS_STARS_PROFILE_HA_CFG = 1,
    TS_STARS_PROFILE_SMMU_CFG = 2,
    TS_STARS_PROFILE_NOC_CFG = 3,
    TS_STARS_PROFILE_CFG_RESERVE = 4
} ts_stars_profile_base_tlv_type;
 
typedef struct tag_profile_base_tlv {
   uint16_t type;  //Param Desc, HA cfg, SMMU cfg, NOC cfg;
   uint16_t len;
} ts_profile_base_tlv_t;
 
typedef struct tag_ha_cfg {
    uint32_t event_num;
    uint16_t ha_event[DHA_DFX_EVENT_MAX_NUM];
} ts_ha_cfg_t;
 
typedef struct tag_smmu_cfg {
    uint32_t event_num;
    uint16_t smmu_event[SMMU_DFX_EVENT_MAX_NUM];
} ts_smmu_cfg_t;
 
typedef struct tag_noc_cfg {
    uint32_t event_num;
    uint16_t noc_event[NOC_DFX_EVENT_MAX_NUM];
} ts_noc_cfg_t;

typedef struct tag_ts_profile_command {
    uint32_t cmd_verify;
    uint32_t channel_id;
    uint32_t cmd_type;
    uint32_t buffer_len;   // length of chl buf
    uint64_t buffer_addr;  // chl buf, for ts <--> drv data exchange
    uint32_t buffer_num;
    uint32_t fid;
    uint64_t com_buf_addr;  // 1980 only
    uint32_t com_buf_len;   // 1980 only
    uint32_t sub_channel_id;
    uint32_t data_len;
    uint32_t pid;
    union {
        uint8_t reserved_payload[72];
        ts_ts_cpu_profile_config_t ts_cpu_profile_cfg;
        ts_ai_cpu_profile_config_t ai_cpu_profile_cfg;
        ts_ai_core_profile_config_t ai_core_profile_cfg;
        ts_ai_vector_profile_config_t ai_vector_profile_cfg;
        ts_ts_fw_profile_config_t ts_fw_profile_cfg;
        ts_l2_profile_config_t ts_l2_profile_cfg;
        ts_stars_soc_log_config_t ts_soc_profile_cfg;
        ts_stars_ffts_profile_config_t ts_ffts_profile_cfg;
        ts_stars_soc_profile_config_t ts_stars_profile_cfg;
        ts_stars_biu_perf_config_t ts_biu_perf_profile_cfg;
        ts_hwts_log_config_t hwts_log_cfg;
        ts_profile_base_tlv_t ts_profile_base_tlv_cfg;
    } u;
} ts_profile_command_t;

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* TS_TSCH_DEFINES_PROFILING_H */
