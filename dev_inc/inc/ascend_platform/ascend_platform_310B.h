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

#ifndef PLATFORM_CHIP_H
#define PLATFORM_CHIP_H

#include "drv_ipc.h"

#define ASCEND_CHIP_NUM_MAX             1
#define ASCEND_DIE_NUM_MAX              1
/* The actual MAX number of davinci devices on the device side. */
#define ASCEND_DEVICE_DAVINCI_NUM_MAX   1

#define BASEADDR_EACH_CHIP_OFFSET       0x200000000000ULL   /* 32T No used, No meaning */
#define BASEADDR_EACH_DIE_OFFSET        0x0ULL              /* No use */
#define BASE_HW_INFO_MEM_ADDR           0                   /* No use */

#define CORE_NUM_PER_CHIP 4
/* ipc msg send channel */
#define DMS_TS_IPC_CHAN_ID  HISI_RPROC_TX_TS_ACPU1
#define DMS_LP_IPC_CHAN_ID  HISI_RPROC_TX_LP_ACPU0

/* not used */
#define SC_PAD_INFO_BASE 0

/* sharemem baseaddr */
#ifdef CFG_SOC_PLATFORM_MDC_V11
#define SHAREMEM_BASE_ADDR 0x1B20000ULL
#else
#define SHAREMEM_BASE_ADDR   0x3120000ULL
#endif

/* aicore num */
#define DMS_AI_CORE_NUM 1

/* aivector num */
#define DMS_AI_VECTOR_NUM 1

/* L3T register */
/* L3T number */
#define L3T_TOTAL_NUM 0x1

/* L3T register base addr */
#define L3_TAG0_REG_BASE 0x0000C1190000UL
/* adapt for other chips */
#define L3_TAG1_REG_BASE 0
#define L3_TAG2_REG_BASE 0
#define L3_TAG3_REG_BASE 0

/* Miniest spec: aicore/vpc/jpegd/cpu/hbm/l2/mata. */
#define SOC_DEFAULT_AICORE_FREQ         1250
#define SOC_DEFAULT_AICORE_TOTAL_NUM    1
#define SOC_DEFAULT_AICORE_MIN_NUM      1
#define SOC_DEFAULT_AICORE_BITMAP       0x01
#define SOC_DEFAULT_AIVECTOR_FREQ       1500
#define SOC_DEFAULT_AIVECTOR_TOTAL_NUM  1
#define SOC_DEFAULT_AIVECTOR_MIN_NUM    1
#define SOC_DEFAULT_AIVECTOR_BITMAP     0x01
#define SOC_DEFAULT_VPC_TOTAL_NUM       2
#define SOC_DEFAULT_VPC_MIN_NUM         2
#define SOC_DEFAULT_VPC_BITMAP          0x03
#define SOC_DEFAULT_JPEGD_TOTAL_NUM     2
#define SOC_DEFAULT_JPEGD_MIN_NUM       2
#define SOC_DEFAULT_JPEGD_BITMAP        0x03
#define SOC_DEFAULT_CPU_TOTAL_NUM       4
#define SOC_DEFAULT_CPU_MIN_NUM         4
#define SOC_DEFAULT_CPU_BITMAP          0x0F
#define SOC_DEFAULT_HBM_FREQ            0
#define SOC_DEFAULT_HBM_TOTAL_NUM       0
#define SOC_DEFAULT_HBM_MIN_NUM         0
#define SOC_DEFAULT_HBM_BITMAP          0x00
#define SOC_DEFAULT_L2_FREQ             1250
#define SOC_DEFAULT_L2_TOTAL_NUM        2
#define SOC_DEFAULT_L2_MIN_NUM          2
#define SOC_DEFAULT_L2_BITMAP           0x03
#define SOC_DEFAULT_MATA_MIN_NUM        1
#define SOC_DEFAULT_SPEC_SINGLE_DIE     "Ascend310B"
#define SOC_DEFAULT_SPEC_MULTI_DIE      "Ascend310B"

/* bbox ddr dump address */
#define ASCEND_PLATFORM_MEMDUMP_ADDR 0x23340000
#define ASCEND_PLATFORM_MEMDUMP_SIZE 0x00F00000

/* Bbox export register feature */
#define PCIE_DDR_READ_REG_BASE 0x22240000
#define PCIE_DDR_READ_REG_SIZE 0x500000

/* HCCS profiling */
#define HCCS_NUM 0

/* sys_ctrl address */
#define SYSCTL_REG_BASE_ADDR      0xC0140000U
#define SYSCTL_REG_SIZE           0x10000

#define DRV_BOARD_ID_REG_OFFSET   0xEC80

/* Chip info */
#define SOC_CHIP_INFO_REG_BASE    0xC014F000UL /* SYSCTL_REG_BASE_ADDR + 0xF000 */
#define SOC_CHIP_INFO_MAP_SIZE    0x1000
#define SOC_CHIP_INFO_REG_OFFSET  0xFF8
#define SOC_ACC_TYPE_REG_OFFSET   0

typedef struct soc_chip_ver_reg {
    unsigned int chip_ver : 4;
    unsigned int chip_name : 16;
    unsigned int reserved : 12;
} soc_chip_ver_reg_t;

#endif
