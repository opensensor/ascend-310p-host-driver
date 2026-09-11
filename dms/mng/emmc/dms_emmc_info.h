/*
* Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
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
* Create: 2023-3-3
*/

#ifndef __DMS_EMMC_INFO_H
#define __DMS_EMMC_INFO_H

#define DMS_MODULE_EMMC "dms_module_emmc"

#define MAX_EMMC_INFO_TYPE 3
#define DMS_EMMC_MAX_PI_LEN 7
#define DMS_EMMC_FW_LEN 8
#define DMS_EMMC_RESERVE_LEN 8
#define DMS_EMMC_MANUF_STATUS_LEN 512
struct dms_emmc_status_stru {
    unsigned int manufacturer_id;                /* emmc device manufacturer id */
    unsigned char product_name[DMS_EMMC_MAX_PI_LEN]; /* emmc device product identification */
    unsigned char timing_interface;         /* emmc speed mode: 9-HS200; 10-HS400 (see define in linux/mmc/host.h) */
    unsigned int serial_number;             /* emmc device serial number */
    unsigned int fault_status;              /* emmc device exception status */
    unsigned int device_life_time_a;        /* emmc device life time estimation type A value */
    unsigned int device_life_time_b;        /* emmc device life time estimation type B value */
    unsigned int pre_eol_info;              /* emmc device life time reflected by average reserved blocks */
    unsigned int spec_version;              /* emmc device specification version */
    unsigned int device_version;            /* emmc device device version */
    unsigned int total_capacity;            /* total raw device capacity (unit: 512 bytes) */
    unsigned char fw_ver[DMS_EMMC_FW_LEN];      /* product revision level */
};

typedef enum {
    DMS_EMMC_SUB_CMD_STATUS = 0x01,       /* Standard protocol information */
    DMS_EMMC_SUB_CMD_MANUF_STATUS = 0x02, /* Manufacturer custom information */
    DMS_EMMC_SUB_CMD_INVALID = 0xFF,
} DMS_EMMC_SUB_CMD;

int dms_emmc_init(void);
void dms_emmc_uninit(void);

#endif