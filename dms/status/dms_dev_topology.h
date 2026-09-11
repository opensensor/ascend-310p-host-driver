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
* Create: 2022-08-15
*/

#ifndef DMS_DEV_TOPOLOGY_H
#define DMS_DEV_TOPOLOGY_H

#define DMS_PCIE_CARD 0x10
#define DMS_EVB_CARD 0
#define MODULE_CARD_DEVICE_NUM 8

typedef struct __dms_topology_check {
    int topology_type;
    int (*topology_check_handler)(unsigned int dev_id1, unsigned int dev_id2, bool *result);
} dms_topology_check_t;

#ifdef CFG_FEATURE_TOPOLOGY_BY_SMP
#define DMS_TOPOLOGY_CHECK_HCCS dms_topology_check_hccs_by_smp
#endif

#ifdef CFG_FEATURE_TOPOLOGY_BY_HCCS_LINK_STATUS
#define DMS_TOPOLOGY_CHECK_HCCS dms_topology_check_hccs_by_hccs_link_status
#endif

#ifdef CFG_FEATURE_CHIP_DIE
#define DMS_TOPOLOGY_CHECK_SIO dms_topology_check_sio
#define DMS_TOPOLOGY_CHECK_HCCS_SW dms_topology_check_hccs_sw
#endif

int dms_feature_get_dev_topology(void *feature, char *in, u32 in_len, char *out, u32 out_len);
int dms_feature_get_phy_devices_topology(void *feature, char *in, u32 in_len, char *out, u32 out_len);

#endif
