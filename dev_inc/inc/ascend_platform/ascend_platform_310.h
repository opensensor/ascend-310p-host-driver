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
* Create: 2022-8-29
*/

#ifndef PLATFORM_CHIP_H
#define PLATFORM_CHIP_H

#define ASCEND_CHIP_NUM_MAX             1
#define ASCEND_DIE_NUM_MAX              1

#define BASEADDR_EACH_CHIP_OFFSET       0x200000000000ULL   /* 32T */
#define BASEADDR_EACH_DIE_OFFSET        0x0ULL              /* No use */
#define BASE_HW_INFO_MEM_ADDR           0                   /* No use */

#define CORE_NUM_PER_CHIP 8

/* not used */
#define DMS_TS_IPC_CHAN_ID  -1
#define DMS_LP_IPC_CHAN_ID  -1
#define SC_PAD_INFO_BASE 0

/* sharemem baseaddr */
#define SHAREMEM_BASE_ADDR 0

/* bbox ddr dump address */
#define ASCEND_PLATFORM_MEMDUMP_ADDR 0x06E00000
#define ASCEND_PLATFORM_MEMDUMP_SIZE 0x00F00000

/* not used */
#define DMS_AI_CORE_NUM -1
#define DMS_AI_VECTOR_NUM -1

/* adapt for other chips */
/* L3T register */
/* L3T number */
#define L3T_TOTAL_NUM 0

/* L3T register base addr */
#define L3_TAG0_REG_BASE 0
#define L3_TAG1_REG_BASE 0
#define L3_TAG2_REG_BASE 0
#define L3_TAG3_REG_BASE 0

/* HCCS profiling */
#define HCCS_NUM 0

/* sys_ctrl address */
#define SYSCTL_REG_BASE_ADDR      0x1100c0000U
#define SYSCTL_REG_SIZE           0x10000

/* Chip info */
#define SOC_CHIP_INFO_REG_BASE    0x10015e000UL
#define SOC_CHIP_INFO_MAP_SIZE    0x1000
#define SOC_CHIP_INFO_REG_OFFSET  0X1C
#define SOC_ACC_TYPE_REG_OFFSET   0

typedef struct soc_chip_ver_reg {
    unsigned int chip_ver : 12;
    unsigned int chip_name : 16;
    unsigned int reserved : 4;
} soc_chip_ver_reg_t;

#endif