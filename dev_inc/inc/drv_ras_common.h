/*
* Copyright (c) Huawei Technologies Co., Ltd. 2019-2023. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*/

#ifndef _DRV_RAS_COMMON_H_
#define _DRV_RAS_COMMON_H_

/* Same define to bios:EfiArmProcessorHisiliconCommonSectionData */
#define MAX_REGISTER_ARRAY_SIZE 36
typedef struct {
    u8  function;
    u8  device;
    u16 segment;
    u8  bus;
    u8  reserved[3];
} pcie_info;

typedef struct {
    u32              validation_bits;
    u8               version;
    u8               soc_id;
    u8               socket_id;
    u8               totem_id;
    u8               nimbus_id;
    u8               sub_sys_id;    // HISI_SUBSYS_ID
    u8               module_id;     // HISI_MODULE_ID
    u8               sub_module_id;
    u8               device_or_core_id;
    u8               function_or_port_id;
    u16              error_type;
    pcie_info        pcie_info;
    u8               error_severity;
    u8               sub_sys_num;
    u8               reserved[2];
    u32              register_array_size;
    u32              register_array[MAX_REGISTER_ARRAY_SIZE];
} hisi_common_error_info;

typedef enum {
    HISI_SOC_ASCNED_6X = 0x10,
    HISI_SOC_ASCNED_9X = 0x11,
    HISI_SOC_ASCNED_3X = 0x12,
} HISI_SOC_ID;

typedef enum {
    HISI_MODULE_MN = 0,
    HISI_MODULE_PLL = 1,
    HISI_MODULE_SLLC = 2,
    HISI_MODULE_AA = 3,
    HISI_MODULE_SIOE = 4,
    HISI_MODULE_POE = 5,
    HISI_MODULE_CPA = 6,
    HISI_MODULE_DISP = 7,
    HISI_MODULE_GIC = 8,
    HISI_MODULE_ITS = 9,
    HISI_MODULE_AVSBUS = 10,
    HISI_MODULE_CS = 11,
    HISI_MODULE_PPU = 12,
    HISI_MODULE_SMMU = 13,
    HISI_MODULE_PA = 14,
    HISI_MODULE_HLLC = 15,
    HISI_MODULE_DDRC = 16,
    HISI_MODULE_L3TAG = 17,
    HISI_MODULE_L3DATA = 18,
    HISI_MODULE_PCS = 19,
    HISI_MODULE_MATA = 20,
    HISI_MODULE_PCIE_LOCAL = 21,
    HISI_MODULE_SAS = 22,
    HISI_MODULE_SATA = 23,
    HISI_MODULE_NIC = 24,
    HISI_MODULE_ROCE = 25,
    HISI_MODULE_USB = 26,
    HISI_MODULE_ZIP = 27,
    HISI_MODULE_HPRE = 28,
    HISI_MODULE_SEC = 29,
    HISI_MODULE_RDE = 30,
    HISI_MODULE_MEE = 31,
    HISI_MODULE_L4D = 32,
    HISI_MODULE_SCH = 33,
    HISI_MODULE_DJTAG = 34,
    HISI_MODULE_CFG_BUS = 35,
    HISI_MODULE_R52_SENSORHUB = 36,
    HISI_MODULE_R52_LP = 37,
    HISI_MODULE_CPU_TS = 38,
    HISI_MODULE_TSENSOR = 40,
    HISI_MODULE_L2BUFFER = 41,
    HISI_MODULE_AICORE = 42,
    HISI_MODULE_AIVECTOR = 43,
    HISI_MODULE_RING_BRIDGE = 44,
    HISI_MODULE_HILINK = 45,
    HISI_MODULE_HBM = 46,
    HISI_MODULE_ROH = 47,
    HISI_MODULE_ACG2 = 48,
    HISI_MODULE_PMC = 55,
    HISI_MODULE_MAX = 255
} HISI_MODULE_ID;

typedef enum {
    HISI_SUBSYS_HACSUB = 0,
    HISI_SUBSYS_AOSUB = 1,
    HISI_SUBSYS_PERISUB = 2,
    HISI_SUBSYS_SILSUB = 3,
    HISI_SUBSYS_AICORESUB = 4,
    HISI_SUBSYS_AIVECTORSUB = 5,
    HISI_SUBSYS_GPUSUB = 6,
    HISI_SUBSYS_TSSUB = 7,
    HISI_SUBSYS_IOSUB = 8,
    HISI_SUBSYS_ISPSUB = 9,
    HISI_SUBSYS_MEDIASUB = 10,
    HISI_SUBSYS_NICSUB = 11,
    HISI_SUBSYS_PCIESUB = 12,
    HISI_SUBSYS_DSASUB = 13,
    HISI_SUBSYS_DVPPSUB = 14,
    HISI_SUBSYS_MAX = 255
} HISI_SUBSYS_ID;

/**
* @driver base layer interface
* @description: Ras interrupt unmask register write common interface
* @attention  : For device.
* @param [in]: bit_mask write bit mask
* @param [in]: val write val for bit mask
* @return     : 0 for success, others for fail.
*/
int ras_int_unmask_reg_write(u64 reg_addr, u32 bit_mask, u32 val);

#endif
