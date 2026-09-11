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

#define ASCEND_CHIP_NUM_MAX             2
#define ASCEND_DIE_NUM_MAX              1
/* The actual MAX number of davinci devices on the device side. */
#define ASCEND_DEVICE_DAVINCI_NUM_MAX   2

#define BASEADDR_EACH_CHIP_OFFSET       0x8000000000ULL     /* 512G */
#define BASEADDR_EACH_DIE_OFFSET        0x0ULL              /* No use */
#define BASE_HW_INFO_MEM_ADDR           0                   /* No use */

/* not used */
#define DMS_TS_IPC_CHAN_ID  -1
#define DMS_LP_IPC_CHAN_ID  -1

#define CORE_NUM_PER_CHIP 8
/* sharemem baseaddr */
#define SHAREMEM_BASE_ADDR 0

/* aicore num */
#define DMS_AI_CORE_NUM 10

/* aivector num */
#define DMS_AI_VECTOR_NUM 8

/* L3T register */
/* L3T number */
#ifdef CFG_SOC_MDC_V51_LITE
#define L3T_TOTAL_NUM 0x2

/* L3T register base addr */
#define L3_TAG0_REG_BASE 0x000081170000
#define L3_TAG1_REG_BASE 0x000081180000
#define L3_TAG2_REG_BASE 0
#define L3_TAG3_REG_BASE 0
#else
#define L3T_TOTAL_NUM 0x4

/* L3T register base addr */
#define L3_TAG0_REG_BASE 0x000081170000
#define L3_TAG1_REG_BASE 0x000081180000
#define L3_TAG2_REG_BASE 0x000081190000
#define L3_TAG3_REG_BASE 0x0000811A0000
#endif

/* chip die offset */
#define ASCEND_CHIP_ADDR_OFFSET 0x8000000000

/* bbox ddr dump address */
#define ASCEND_PLATFORM_MEMDUMP_ADDR 0x2F900000
#define ASCEND_PLATFORM_MEMDUMP_SIZE 0x01E00000

/* Bbox export register feature */
#define PCIE_DDR_READ_REG_BASE 0xC00000
#define PCIE_DDR_READ_REG_SIZE 0x200000

/* HCCS profiling */
#define HCCS_NUM 0

#define PCS_NUM 1
#define PCS0_BASE_ADDR 0x812A0000

#define HDLC_LINK_NUM 1
#define HDLC_REG_SIZE 4
#define HDLC0_BASE_ADDR 0x812B1000

#define HDLC_RETRY_CNT_ADDR         0x4A0
#define HDLC_CRC_ERR_CNT_ADDR       0x7C0
#define HDLC_TX_CNT_CH0_ADDR        0x5C0
#define HDLC_TX_CNT_CH1_ADDR        0x5C8
#define HDLC_TX_CNT_CH2_ADDR        0x5D0
#define HDLC_RX_CNT_CH0_ADDR        0x1C0
#define HDLC_RX_CNT_CH1_ADDR        0x1C8
#define HDLC_RX_CNT_CH2_ADDR        0x1D0
#define HDLC_REG_MAP_SIZE           0x1000

/* sys_ctrl address */
#define SYSCTL_REG_BASE_ADDR      0x80000000U
#define DRV_BOARD_ID_REG_OFFSET   0xF07C

#define SYSCTL_REG_SIZE           0x10000

#define SC_PAD_INFO_BASE 0x8000E000

/* Chip info */
#define SOC_CHIP_INFO_REG_BASE    0x8000F000UL /* SYSCTL_REG_BASE_ADDR + 0xF000 */
#define SOC_CHIP_INFO_MAP_SIZE    0x1000
#define SOC_CHIP_INFO_REG_OFFSET  0xFF8
#define SOC_ACC_TYPE_REG_OFFSET   0

typedef struct soc_chip_ver_reg {
    unsigned int chip_ver : 4;
    unsigned int chip_name : 16;
    unsigned int reserved : 12;
} soc_chip_ver_reg_t;

#endif
