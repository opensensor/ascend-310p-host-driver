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

#define ASCEND_CHIP_NUM_MAX             16
#define ASCEND_DIE_NUM_MAX              2
/* The actual MAX number of davinci devices on the device side. */
#define ASCEND_DEVICE_DAVINCI_NUM_MAX   2

#define BASEADDR_EACH_CHIP_OFFSET       0x80000000000ULL    /* 8T */
#define BASEADDR_EACH_DIE_OFFSET        0x10000000000ULL    /* 1T */
#define BASE_HW_INFO_MEM_ADDR           0x36EFD800

#define BASE_DEVMNG_INFO_MEM_ADDR       0x36E00000

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
#define SOC_DEFAULT_SPEC_SINGLE_DIE     "Ascend910B4"
#define SOC_DEFAULT_SPEC_MULTI_DIE      "Ascend910_9372"

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

/* PCS status */
#define ST_CH_PCS_LANE_MODE_CHANGE_OFFSET 0x860
#define ST_PCS_MODE_WORKING_X4 0x2
#define ST_PCS_USE_WORKING_X4 0xF
#define PCS_STATUS_OFFSET 0
#define PCS_INDEX_OFFSET 8
#define PCS_MODE_WORKING_OFFSET 16
#define PCS_USE_WORKING_OFFSET 24

#define PCS_NUM 8
#define PCS0_BASE_ADDR 0x000703B90000
#define PCS1_BASE_ADDR 0x000703BA0000
#define PCS2_BASE_ADDR 0x000703BB0000
#define PCS3_BASE_ADDR 0x000703BC0000
#define PCS4_BASE_ADDR 0x000703BD0000
#define PCS5_BASE_ADDR 0x000703BE0000
#define PCS6_BASE_ADDR 0x000703BF0000
#define PCS7_BASE_ADDR 0x000703C00000

#define HDLC_LINK_NUM 2 /* x4: 1 HDLC have 2link, x8: 1 HDLC have 1link */
#define HDLC_REG_SIZE 4
#define HDLC0_BASE_ADDR 0x000601910000
#define HDLC1_BASE_ADDR 0x000601920000
#define HDLC2_BASE_ADDR 0x000601930000
#define HDLC3_BASE_ADDR 0x000601940000

#define HDLC_FSM_ADDR         0x1068
#define HDLC_RETRY_CNT_ADDR   0x1078
#define HDLC_CRC_ERR_CNT_ADDR 0x1090
#define HDLC_TX_CNT_ADDR      0x10B0
#define HDLC_RX_CNT_ADDR      0x10F0
#define HDLC_REG_MAP_SIZE     0x1100
#define HDLC_INIT_SUCCESS     0x0003

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

#define SIO_SLLC_MAX_NUM 2
#define SIO_SLLC0_BASE_ADDR 0x000601AE0000
#define SIO_SLLC1_BASE_ADDR 0x000601AF0000

#define SIO_SLLC0_RETRY_BASE_ADDR 0x000601AE5700
#define SIO_SLLC1_RETRY_BASE_ADDR 0x000601AF5700

/* start address is (SIO_SLLC0_BASE_ADDR - 0x5700) */
#define SIO_SLLC_RETRY_EN_OFFSET 0

#define SIO_SLLC_TX_RETRY_ST_CNT_OFFSET 0x0010
#define SIO_SLLC_RX_RETRY_ST_CNT_OFFSET 0x0014
#define SIO_SLLC_RETRY_CNT_MASK 0x00FF

#define REG_STARS_GLOBAL_BASE_ADDR (0x6a0000000ULL)
#define REG_STARS_PREFETCH_BUFFER_ADDR (0x10000000)
#define REG_STARS_SQ_MAP1_ADDR (REG_STARS_GLOBAL_BASE_ADDR + REG_STARS_PREFETCH_BUFFER_ADDR + 0x220)
#define REG_STARS_SQ_MAP1_OFFSET (0x1000)

#endif
