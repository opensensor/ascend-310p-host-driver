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
* Create: 2022-8-29
*/


/*
* NOTICE: ALL changes in this file are David's compile adaptations, cannot be used for software development!
* NOTICE: ALL changes in this file are David's compile adaptations, cannot be used for software development!
* NOTICE: ALL changes in this file are David's compile adaptations, cannot be used for software development!
*/
#ifndef PLATFORM_CHIP_H
#define PLATFORM_CHIP_H

#include "drv_ipc.h"

#define ASCEND_CHIP_NUM_MAX             16
#define ASCEND_DIE_NUM_MAX              2
/* The actual MAX number of davinci devices on the device side. */
#define ASCEND_DEVICE_DAVINCI_NUM_MAX   1

#define BASEADDR_EACH_CHIP_OFFSET       0x80000000000ULL    /* 8T */
#define BASEADDR_EACH_DIE_OFFSET        0x10000000000ULL    /* 1T */
#define BASE_HW_INFO_MEM_ADDR           0x36EFD800

#define CORE_NUM_PER_CHIP 8
/* ipc msg send channel */
#define DMS_TS_IPC_CHAN_ID  HISI_RPROC_TX_TS_MBX3
#define DMS_LP_IPC_CHAN_ID  HISI_RPROC_TX_IMU_MBX28

/* not used */
#define SC_PAD_INFO_BASE 0

/* sharemem baseaddr */
#define SHAREMEM_BASE_ADDR  0x29220000

/* aicore num */
#define DMS_AI_CORE_NUM 25

/* aivector num */
#define DMS_AI_VECTOR_NUM 50

/* chip die offset */
#define ASCEND_CHIP_ADDR_OFFSET 0x10000000000

/* L3T register */
/* L3T number */
#define L3T_TOTAL_NUM 0x2

/* L3T register base addr */
#define L3_TAG0_REG_BASE 0x0007039E0000
#define L3_TAG1_REG_BASE 0x0007039F0000
/* adapt for other chips */
#define L3_TAG2_REG_BASE 0
#define L3_TAG3_REG_BASE 0

/* Miniest spec: aicore/vpc/jpegd/cpu/hbm/l2/mata. */
#define SOC_DEFAULT_AICORE_FREQ         1500
#define SOC_DEFAULT_AICORE_TOTAL_NUM    25
#define SOC_DEFAULT_AICORE_MIN_NUM      2
#define SOC_DEFAULT_AICORE_BITMAP       0x0C
#define SOC_DEFAULT_AIVECTOR_FREQ       0
#define SOC_DEFAULT_AIVECTOR_TOTAL_NUM  0
#define SOC_DEFAULT_AIVECTOR_MIN_NUM    0
#define SOC_DEFAULT_AIVECTOR_BITMAP     0
#define SOC_DEFAULT_VPC_TOTAL_NUM       10
#define SOC_DEFAULT_VPC_MIN_NUM         2
#define SOC_DEFAULT_VPC_BITMAP          0x03
#define SOC_DEFAULT_JPEGD_TOTAL_NUM     14
#define SOC_DEFAULT_JPEGD_MIN_NUM       4
#define SOC_DEFAULT_JPEGD_BITMAP        0x0F
#define SOC_DEFAULT_CPU_TOTAL_NUM       8
#define SOC_DEFAULT_CPU_MIN_NUM         2
#define SOC_DEFAULT_CPU_BITMAP          0x11
#define SOC_DEFAULT_HBM_FREQ            16000
#define SOC_DEFAULT_HBM_TOTAL_NUM       4
#define SOC_DEFAULT_HBM_MIN_NUM         1
#define SOC_DEFAULT_HBM_BITMAP          0x01
#define SOC_DEFAULT_L2_FREQ             16000
#define SOC_DEFAULT_L2_TOTAL_NUM        32
#define SOC_DEFAULT_L2_MIN_NUM          8
#define SOC_DEFAULT_L2_BITMAP           0x0F0F
#define SOC_DEFAULT_MATA_MIN_NUM        2
#define SOC_DEFAULT_SPEC_SINGLE_DIE     "Ascend910_9591"
#define SOC_DEFAULT_SPEC_MULTI_DIE      "Ascend910_9591"

/* mainboard id */
#define MAINBOARD_ID_DUAL_SERVER_SECOND 0x2
#define DUAL_SERVER_EACH_CHIP_COUNT 8

/* bbox ddr dump address */
#define ASCEND_PLATFORM_MEMDUMP_ADDR 0x36400000
#define ASCEND_PLATFORM_MEMDUMP_SIZE 0x00900000

/* Bbox export register feature */
#define PCIE_DDR_READ_REG_BASE 0x37100000
#define PCIE_DDR_READ_REG_SIZE 0x00800000

/* HCCS profiling */
#define HCCS_NUM 4
#define HCCS_PHY_ADDR_INTERVAL 0x10000L

#define HLLC_HYDRA_RX_CH0_FLIT_CNT_OFFSET 0x10E8
#define HLLC_HYDRA_RX_CH1_FLIT_CNT_OFFSET 0x10EC
#define HLLC_HYDRA_RX_CH2_FLIT_CNT_OFFSET 0

#define PHY_TX_CH0_FLIT_CNT_OFFSET 0x10D8
#define PHY_TX_CH1_FLIT_CNT_OFFSET 0x10DC
#define PHY_TX_CH2_FLIT_CNT_OFFSET 0xFFFFFFFF

#define HCCS_REG_LEN 0x2000

#define HCCS_PACKET_LEN 0x20

#define HCCS_DEV0_PHY_BASE_ADDR 0x000601910000
#define HCCS_DEV1_PHY_BASE_ADDR 0x000601910000
#define HCCS_DEV2_PHY_BASE_ADDR 0x0
#define HCCS_DEV3_PHY_BASE_ADDR 0x0

/* sys_ctrl address */
#define SYSCTL_REG_BASE_ADDR      0x80000000U
#define SYSCTL_REG_SIZE           0x10000

/* Chip info */
#define SOC_CHIP_INFO_REG_BASE    0x8000F000UL /* SYSCTL_REG_BASE_ADDR + 0xF000 */
#define SOC_CHIP_INFO_MAP_SIZE    0x1000
#define SOC_CHIP_INFO_REG_OFFSET  0xFF8
#define SOC_ACC_TYPE_REG_OFFSET   0x85C /* distinguish 910B or 910_93 */
#define SOC_ACC_TYPE_VALUE_910B   0x5A5A5A5AUL
#define SOC_ACC_TYPE_VALUE_910_93 0xA5A5A5A5UL

typedef struct soc_chip_ver_reg {
    unsigned int chip_ver : 4;
    unsigned int chip_name : 16;
    unsigned int reserved : 12;
} soc_chip_ver_reg_t;

#endif
