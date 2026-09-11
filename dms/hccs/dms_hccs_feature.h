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
* Create: 2022-11-11
*/

#ifndef HCCS_FEATURE_H
#define HCCS_FEATURE_H

#include <linux/time.h>

#define DMS_HCCS_INFO_RESERVED_BYTES 4
typedef struct hccs_info_struct {
    unsigned int pcs_status;
    unsigned int hdlc_status;
    unsigned char reserved[DMS_HCCS_INFO_RESERVED_BYTES];
} hccs_info_t;

#define DMS_HCCS_MAX_PCS_NUM        (16)
typedef struct hccs_port_lane_details {
    unsigned int hccs_port_pcs_bitmap;
    unsigned int pcs_lane_bitmap[DMS_HCCS_MAX_PCS_NUM];
    unsigned int reserve[DMS_HCCS_MAX_PCS_NUM];
} hccs_lane_info_t;

typedef struct {
    unsigned int tx_cnt[DMS_HCCS_MAX_PCS_NUM];
    unsigned int rx_cnt[DMS_HCCS_MAX_PCS_NUM];
    unsigned int crc_err_cnt[DMS_HCCS_MAX_PCS_NUM];
    unsigned int retry_cnt[DMS_HCCS_MAX_PCS_NUM];
    unsigned int reserve[DMS_HCCS_MAX_PCS_NUM * 3];
} hccs_statistic_info_t;

typedef struct {
    unsigned long long tx_cnt[DMS_HCCS_MAX_PCS_NUM];
    unsigned long long rx_cnt[DMS_HCCS_MAX_PCS_NUM];
    unsigned long long crc_err_cnt[DMS_HCCS_MAX_PCS_NUM];
    unsigned long long retry_cnt[DMS_HCCS_MAX_PCS_NUM];
    unsigned long long reserve[DMS_HCCS_MAX_PCS_NUM * 3];
} hccs_statistic_info_ext_t;

typedef struct hpcs_status_reg {
    unsigned int st_pcs_mode_change_done : 1;
    unsigned int reserved_1 : 3;
    unsigned int st_pcs_mode_working : 2;
    unsigned int reserved_2 : 2;
    unsigned int st_pcs_use_working : 8;
    unsigned int reserved_3 : 16;
} hccs_pcs_status_reg_t;

int dms_get_hccs_status_by_dev_id(unsigned int dev_id, hccs_info_t *hccs_status);
int dms_get_hccs_lane_details(unsigned int dev_id, hccs_lane_info_t *hccs_lane_info);
int dms_get_hpcs_status_by_dev_id(unsigned int dev_id, unsigned long long pcs_bitmap, unsigned long long phy_addr_offset, hccs_info_t *hccs_status);
int dms_get_hdlc_status_by_dev_id(unsigned int dev_id, unsigned long long pcs_bitmap, unsigned long long phy_addr_offset, hccs_info_t *hccs_status);

#ifdef CFG_FEATURE_GET_PCS_BITMAP_BY_BOARD_TYPE
int dms_get_hpcs_bitmap_by_board_type(unsigned int dev_id, unsigned long long *bitmap);
#define DMS_GET_HPCS_BITMAP dms_get_hpcs_bitmap_by_board_type
#else
int dms_get_hpcs_bitmap_default(unsigned int dev_id, unsigned long long *bitmap);
#define DMS_GET_HPCS_BITMAP dms_get_hpcs_bitmap_default
#endif

#endif