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

#define ASCEND_CHIP_NUM_MAX             4
#define ASCEND_DIE_NUM_MAX              1
/* The actual MAX number of davinci devices on the device side. */
/* AMP: 1 SMP: 4 */
#define ASCEND_DEVICE_DAVINCI_NUM_MAX   4

#define BASEADDR_EACH_CHIP_OFFSET       0x200000000000ULL   /* 32T */
#define BASEADDR_EACH_DIE_OFFSET        0x0ULL              /* No use */
#define BASE_HW_INFO_MEM_ADDR           0                   /* No use */

/* not used */
#define DMS_TS_IPC_CHAN_ID  -1
#define DMS_LP_IPC_CHAN_ID  -1
#define SC_PAD_INFO_BASE 0

#define CORE_NUM_PER_CHIP 16
/* sharemem baseaddr */
#define SHAREMEM_BASE_ADDR 0

/* not used */
#define DMS_AI_CORE_NUM -1
#define DMS_AI_VECTOR_NUM -1

/* chip die offset */
#define ASCEND_CHIP_ADDR_OFFSET 0x200000000000

/* L3T register */
/* L3T number */
#define L3T_TOTAL_NUM 0x4

/* L3T register base addr */
#define L3_TAG0_REG_BASE 0x000089060000
#define L3_TAG1_REG_BASE 0x00008B060000
#define L3_TAG2_REG_BASE 0x000089070000
#define L3_TAG3_REG_BASE 0x00008B070000

/* bbox ddr dump address */
#define ASCEND_PLATFORM_MEMDUMP_ADDR 0x06E00000
#define ASCEND_PLATFORM_MEMDUMP_SIZE 0x00F00000

/* Bbox export register feature */
#define PCIE_DDR_READ_REG_BASE 0x5F300000
#define PCIE_DDR_READ_REG_SIZE 0x500000

/* HCCS profiling */
#define HCCS_NUM 3
#define HCCS_PHY_ADDR_INTERVAL 0x10000L

#define HLLC_HYDRA_RX_CH0_FLIT_CNT_OFFSET 0x11c0
#define HLLC_HYDRA_RX_CH1_FLIT_CNT_OFFSET 0x11c8
#define HLLC_HYDRA_RX_CH2_FLIT_CNT_OFFSET 0x11d0

#define PHY_TX_CH0_FLIT_CNT_OFFSET 0x12C0
#define PHY_TX_CH1_FLIT_CNT_OFFSET 0x12C8
#define PHY_TX_CH2_FLIT_CNT_OFFSET 0x12D0

#define HCCS_REG_LEN 0x2000

#define HCCS_PACKET_LEN 0x20

#define HCCS_DEV0_PHY_BASE_ADDR 0x000200080000
#define HCCS_DEV1_PHY_BASE_ADDR 0x000200080000
#define HCCS_DEV2_PHY_BASE_ADDR 0x000200080000
#define HCCS_DEV3_PHY_BASE_ADDR 0x000200080000

/* sys_ctrl address */
#define SYSCTL_REG_BASE_ADDR      0x80000000U
#define SYSCTL_REG_SIZE           0x10000

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
