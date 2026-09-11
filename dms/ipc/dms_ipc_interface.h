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
#ifndef __DMS_IPC_INTERFACE_H
#define __DMS_IPC_INTERFACE_H

enum dms_lpm_info_type {
    LPM_TEMPERATURE = 0,
    LPM_POWER,
    LPM_VOLTAGE,
    LPM_FREQUENCY,
    LPM_USE_RATE,
    LPM_ECC_STATISTICS,
    LPM_STATUS,
    LPM_AIC_VOL_CUR,
    LPM_HYBIRD_VOL_CUR,
    LPM_TAISHAN_VOL_CUR,
    LPM_DDR_VOL_CUR,
    LPM_ACG,
    LPM_TEMP_DDR,
    LPM_DDR_THOLD,
    LPM_SOC_THOLD,
    LPM_SOC_MIN_THOLD,
    LPM_SET_DDR_TEMP_THOLD,
    LPM_SET_SOC_TEMP_THOLD,
    LPM_MAX_TYPE,
};

enum dms_lpm_core_id {
    LPM_CLUSTER_ID = 0,
    LPM_PERI_ID = 1,
    LPM_TS_ID = 2,
    LPM_DDR_ID = 3,
    LPM_AICORE0_ID = 4,
    LPM_AICORE1_ID = 5,
    LPM_HBM_ID = 6,
    LPM_VECTOR_ID = 7,
    LPM_SOC_ID = 8,
    LPM_N_DIE_ID,
    LPM_THERMAL_THRESHOLD_ID,
    LPM_INVALID_ID,
};

#define DDR_TEMP_TEMPERATURE_SIZE 24

int lpm_get_info_from_ipc(u32 dev_id, u32 info_type, u32 core_id, u8 *data, u32 *len);

#ifdef CFG_REATURE_GET_INFO_FORM_IPC
int lpm_set_info_to_ipc(u32 dev_id, u32 info_type, u32 core_id, u8 *data, u32 *len);
#endif
#endif