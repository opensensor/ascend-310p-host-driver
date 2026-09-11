/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2023. All rights reserved.
 * Description:
 * Author: Huawei
 * Create: 2021-7-20
 * File Name     : dms_cmd_def.h
 * Version       : Initial Draft
 * Author        :
 * Created       : 2021-7-20
 * Last Modified :
 * Description   : head file

 *  History       :
 * 1.Date        : 2021-7-20
 *    Author      :
 *    Modification: Created file
 */


#ifndef __DMS_CMD_DEF_H__
#define __DMS_CMD_DEF_H__
#include "dev_mon_cmd_def.h"

#define ZERO_CMD 0
#define MAKE_UP_COMMAND(fun, cmd) (((fun) << 16) | (cmd))
#define MAKE_UP_DAVINCI_COMMAND(cmd)  ((DEV_MON_SMB_FUN_CODE_DAVINCI << 16) | (cmd))
#define MAKE_UP_COMMON_COMMAND(cmd)  ((DEV_MON_SMB_FUN_CODE_COMMON << 16) | (cmd))
#define MAKE_UP_INDEPENDENCE_COMMAND(main, sub) (((main) << 12) | (sub))

/* common */
#define DMS_GET_HEALTH_CMD MAKE_UP_COMMON_COMMAND(DEV_MON_CMD_GET_HEALTH_STATE)
#define DMS_GET_ERR_CODE_CMD MAKE_UP_COMMON_COMMAND(DEV_MON_CMD_GET_ERROR_CODE)
#define DMS_GET_CHIP_TEMP_CMD MAKE_UP_COMMON_COMMAND(DEV_MON_CMD_GET_CHIP_TEMP)
#define DMS_GET_POWER_CMD MAKE_UP_COMMON_COMMAND(DEV_MON_CMD_GET_POWER)
#define DMS_GET_PCIE_VID_CMD MAKE_UP_COMMON_COMMAND(DEV_MON_CMD_GET_VID)
#define DMS_GET_PCIE_DID_CMD MAKE_UP_COMMON_COMMAND(DEV_MON_CMD_GET_DID)
#define DMS_GET_PCIE_SUBVID_CMD MAKE_UP_COMMON_COMMAND(DEV_MON_CMD_GET_SUBVID)
#define DMS_GET_PCIE_SUBDID_CMD MAKE_UP_COMMON_COMMAND(DEV_MON_CMD_GET_SUBDID)
#define DMS_GET_CHIP_VOLT_CMD MAKE_UP_COMMON_COMMAND(DEV_MON_CMD_GET_CHIP_VOLT)
#define DMS_GET_BOARD_ID_CMD MAKE_UP_COMMON_COMMAND(DEV_MON_CMD_GET_BOARD_ID)
#define DMS_GET_ECC_STAT_CMD MAKE_UP_COMMON_COMMAND(DEV_MON_CMD_QUERY_ECC_STAT)

/* davinci */
#define DMS_GET_D_INFO_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_GET_D_INFO)
#define DMS_GET_FLASH_INFO_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_GET_FLASH_D_INFO)

#define DMS_GET_SENSOR_INFO_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_GET_MINI_SENSOR_INFO)
#define DMS_GET_GET_USER_CONFIG_CMD MAKE_UP_DAVINCI_COMMAND(DMP_MON_CMD_GET_USER_CONFIG)
#define DMS_GET_SET_USER_CONFIG_CMD MAKE_UP_DAVINCI_COMMAND(DMP_MON_CMD_SET_USER_CONFIG)
#define DMS_GET_CNTPCT_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_GET_CNTPCT)
#define DMS_GET_GET_ERRSTR_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_GET_ERRSTR)
#define DMS_GET_GET_LLC_PERF_PARA MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_GET_LLC_PERF_PARA)
#define DMS_GET_NETWORK_HEALTH_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_GET_NETWORK_HEALTH)
#define DMS_GET_ISOLATED_PAGES_INFO_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_GET_ISOLATED_PAGES_INFO)
#define DMS_CLEAR_ISOLATED_INFO_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_CLEAR_ISOLATED_INFO)
#define DMS_GET_MULTI_ECC_TIME_INFO_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_GET_MULTI_ECC_TIME_INFO)
#define DMS_GET_ECC_RECORD_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_GET_ECC_RECORD_INFO)
#define DMS_GET_GET_DIE_ID_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_GET_DIE_ID)
#define DMS_GET_GET_NET_DEV_INFO_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_GET_NET_DEV_INFO)
#define DMS_GET_GET_AICPU_INFO_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_GET_AICPU_INFO)
#define DMS_GET_SET_REVOCATION_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_SET_REVOCATION)

#define DMS_GET_GET_DEVICE_INFO_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_GET_DEVICE_INFO)
#define DMS_GET_SET_DEVICE_INFO_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_SET_DEVICE_INFO)
#define DMS_GET_GET_DETECT_INFO_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_GET_DETECT_INFO)
#define DMS_GET_SET_DETECT_INFO_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_SET_DETECT_INFO)
#define DMS_GET_GET_REBOOT_REASON_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_GET_REBOOT_REASON)
#define DMS_BIND_HOST_PID_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_BIND_HOST_PID)
#define DMS_UNBIND_HOST_PID_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_UNBIND_HOST_PID)
#define DMS_QUERY_DEV_PID_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_QUERY_DEV_PID)
#define DMS_GET_PROCESS_SIGN MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_GET_PROCESS_SIGN)
#define DMS_GET_BIST_INFO_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_GET_BIST_INFO)
#define DMS_CTRL_DEVICE_NODE_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_CTRL_DEVICE_NODE)
#define DMS_GET_ALL_DEVICE_NODE_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_GET_ALL_DEVICE_NODE)
#define DMS_GET_FLASH_CONTENT MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_GET_FLASH_CONTENT)
#define DMS_SET_FLASH_CONTENT MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_SET_FLASH_CONTENT)
#define DMS_SET_BIST_INFO_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_SET_BIST_INFO)
#define DMS_FAULT_INJECT_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_FAULT_INJECT)
#define DMS_QUERY_HOST_PID_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_QUERY_HOST_PID)
#define DMS_GET_UDIS_DEVICE_INFO_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_GET_UDIS_DEVICE_INFO)
#define DMS_SET_UDIS_DEVICE_INFO_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_SET_UDIS_DEVICE_INFO)
#define DMS_GET_CHIP_EXPAND_VERSION_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_GET_CHIP_EXPAND_VERSION)

#define DMS_FILTER_SET_CAN_CONFIG   "main_cmd=0x3"
#define DMS_MAKE_UP_FILTER_DEVICE_INFO(f, main_cmd) do { \
    (f)->filter_len = sprintf_s((f)->filter, sizeof((f)->filter), "main_cmd=0x%x", main_cmd); \
} while (0)

#define DMS_MAKE_UP_FILTER_DEVICE_INFO_EX(f, main_cmd, sub_cmd) do { \
    (f)->filter_len = (unsigned int)sprintf_s((f)->filter, sizeof((f)->filter), "main_cmd=0x%x,sub_cmd=0x%x", main_cmd, sub_cmd); \
} while (0)

#define DMS_MAKE_UP_FILTER_HAL_DEV_INFO_EX(f, module, info) do { \
    (f)->filter_len = sprintf_s((f)->filter, sizeof((f)->filter), "module=0x%x,info=0x%x",\
        (unsigned int)(module), (unsigned int)(info)); \
} while (0)

#define DMS_CREATE_CAPABILITY_GROUP_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_CREATE_CAPABILITY_GROUP)
#define DMS_DELETE_CAPABILITY_GROUP_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_DELETE_CAPABILITY_GROUP)
#define DMS_GET_CAPABILITY_GROUP_INFO_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_GET_CAPABILITY_GROUP_INFO)
#define DMS_GET_LAST_BOOTSTATE_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_GET_LAST_BOOTSTATE)
#define DMS_GET_GET_HISSSTATUS_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_GET_HISSSTATUS)
#define DMS_GET_GET_LPSTATUS_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_GET_LPSTATUS)
#define DMS_GET_SET_POWER_STATE_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_SET_POWER_STATE)
#define DMS_GET_GET_CAN_STATUS_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_GET_CAN_STATUS)
#define DMS_GET_GET_UFS_STATUS_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_GET_UFS_STATUS)
#define DMS_GET_GET_SENSORHUB_STATUS_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_GET_SENSORHUB_STATUS)
#define DMS_GET_GET_UFS_CONFIG_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_GET_UFS_CONFIG)
#define DMS_GET_SET_UFS_CONFIG_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_SET_UFS_CONFIG)
#define DMS_GET_GET_SENSORHUB_CONFIG_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_GET_SENSORHUB_CONFIG)
#define DMS_GET_GET_GPIO_STATUS_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_GET_GPIO_STATUS)
#define DMS_GET_GET_SOC_HW_FAULT_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_GET_SOC_HW_FAULT)
#define DMS_GET_GET_SAFETYISLAND_STATUS_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_GET_SAFETYISLAND_STATUS)

#define DMS_GET_GET_EMMC_INFO_CMD MAKE_UP_DAVINCI_COMMAND(DEV_MON_CMD_D_GET_EMMC_INFO)

#define DMS_MASK_USAGE   "data[0]=0x01,data[1]=0x02"
#define DMS_MASK_FREQ    "data[0]=0x01,data[1]=0x03"

#define DAVINCI_INTF_MODULE_DMS "DMS"

#define DMS_MAGIC 'V'
#define DMS_IOCTL_CMD _IO(DMS_MAGIC, 1)

#define DMS_MAIN_CMD_BASIC 'E'
#define DMS_SUBCMD_GET_FAULT_EVENT 0
#define DMS_SUBCMD_GET_EVENT_CODE 1
#define DMS_SUBCMD_GET_HEALTH_CODE 2
#define DMS_SUBCMD_QUERY_STR 3
#define DMS_GET_FAULT_EVENT    _IO(DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_FAULT_EVENT)
#define DMS_GET_EVENT_CODE     _IO(DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_EVENT_CODE)

#define DMS_SUBCMD_CLEAR_EVENT 4
#define DMS_SUBCMD_DISABLE_EVENT 5
#define DMS_SUBCMD_ENABLE_EVENT 6

#define DMS_SUBCMD_GET_PCIE_INFO 7

#define DMS_POWER_PRE_HOTRESET 8
#define DMS_SUBCMD_POWER_PCIE_PRE_RESET 9
#define DMS_SUBCMD_POWER_PCIE_HOT_RESET 10

#define DMS_GET_VDEVICE_INFO 11
#define DMS_GET_AI_DDR_INFO 12
#define DMS_SUBCMD_GET_AI_INFO_FROM_TS 13
#define DMS_GET_AI_INFO_FROM_TS    _IO(DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_AI_INFO_FROM_TS)
#define DMS_SUBCMD_GET_DEV_TOPOLOGY  14
#define DMS_SUBCMD_GET_DEV_SPLIT_MODE  15
#define DMS_SUBCMD_SRIOV_SWITCH  16
#define DMS_SUBCMD_GET_HCCL_DEVICE_INFO  17
#define DMS_SUBCMD_GET_CORE_SPEC_INFO  18
#define DMS_SUBCMD_GET_CORE_INUSE_INFO  19
#define DMS_SUBCMD_GET_HISTORY_FAULT_EVENT 20
#define DMS_SET_TIME_ZONE_SYNC 21
#define DMS_GET_HISTORY_FAULT_EVENT     _IO(DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_HISTORY_FAULT_EVENT)
#define DMS_SUBCMD_GET_DEVICES_TOPOLOGY 22

#define DMS_SUBCMD_GET_HOST_OSC_FREQ 23
#define DMS_SUBCMD_GET_DEV_OSC_FREQ 24
#define DMS_GET_OSC_FREQ_INFO_HOST      _IO(DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_HOST_OSC_FREQ)
#define DMS_GET_OSC_FREQ_INFO_DEVICE    _IO(DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_DEV_OSC_FREQ)

#define DMS_SUBCMD_GET_DEV_INIT_STATUS 25
#define DMS_SUBCMD_INJECT_FAULT 26

#define DMS_SUBCMD_GET_FAULT_INJECT_INFO 27
#define DMS_GET_FAULT_INJECT_INFO     _IO(DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_FAULT_INJECT_INFO)

#define DMS_SUBCMD_GET_FAULT_EVENT_OBJ 28
#define DMS_SUBCMD_GET_CHIP_INFO 29
#define DMS_GET_CHIP_INFO     _IO(DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_CHIP_INFO)
#define DMS_SUBCMD_GET_DEVICE_STATE 30
#define DMS_SUBCMD_GET_BOARD_ID_HOST 31
#define DMS_SUBCMD_GET_SLOT_ID_HOST 32
#define DMS_SUBCMD_GET_BOM_ID_HOST 33
#define DMS_SUBCMD_GET_PCB_ID_HOST 34
#define DMS_SUBCMD_GET_PHY_DEVICE_INFO 58

#define DMS_MAIN_CMD_SOC 'F'
#define DMS_SUBCMD_GET_PCB_ID    0
#define DMS_SUBCMD_GET_BOM_ID    1
#define DMS_SUBCMD_GET_SLOT_ID   2
#define DMS_SUBCMD_GET_CPU_INFO  3
#define DMS_SUBCMD_GET_CPU_UTILIZATION  4
#define DMS_GET_CPU_INFO    _IO(DMS_MAIN_CMD_SOC, DMS_SUBCMD_GET_CPU_INFO)
#define DMS_GET_CPU_UTILIZATION    _IO(DMS_MAIN_CMD_SOC, DMS_SUBCMD_GET_CPU_UTILIZATION)
#define DMS_SUBCMD_GET_SOC_INFO  7

#define DMS_MAIN_CMD_MEMORY 'G'
#define DMS_SUBCMD_DDR_BW_UTIL_RATE 0
#define DMS_SUBCMD_HBM_BW_UTIL_RATE 1
#define DMS_SUBCMD_DDR_FREQUENCY 4
#define DMS_SUBCMD_CGROUP_MEM_INFO 5
#define DMS_SUBCMD_HBM_TEMPERATURE  6
#define DMS_SUBCMD_SERVICE_MEM_INFO 32
#define DMS_SUBCMD_SYSTEM_MEM_INFO 33
#define DMS_SUBCMD_DDR_MEM_INFO 34
#define DMS_SUBCMD_HBM_MEM_INFO 35
#define DMS_SUBCMD_HBM_GET_VA   36
#define DMS_SUBCMD_GET_FAULT_SYSCNT 37

#define DMS_MAIN_CMD_LPM 'H'
#define DMS_SUBCMD_GET_TEMPERATURE 0
#define DMS_SUBCMD_GET_POWER 1
#define DMS_SUBCMD_GET_VOLTAGE 2
#define DMS_SUBCMD_GET_FREQUENCY 3
#define DMS_SUBCMD_GET_TSENSOR 4
#define DMS_SUBCMD_GET_MAX_FREQUENCY 5
#define DMS_SUBCMD_PASS_THROUGTH_MCU 6
#define DMS_SUBCMD_GET_LP_STATUS 7
#define DMS_SUBCMD_GET_AIC_VOL_CUR 8
#define DMS_SUBCMD_GET_HYBIRD_VOL_CUR 9
#define DMS_SUBCMD_GET_TAISHAN_VOL_CUR 10
#define DMS_SUBCMD_GET_DDR_VOL_CUR 11
#define DMS_SUBCMD_GET_ACG 12
#define DMS_SUBCMD_GET_TEMP_DDR 13
#define DMS_SUBCMD_GET_DDR_THOLD 14
#define DMS_SUBCMD_GET_SOC_THOLD 15
#define DMS_SUBCMD_GET_SOC_MIN_THOLD 16
#define DMS_SUBCMD_SET_TEMP_THOLD 17

#define DMS_MAIN_CMD_PRODUCT 'I'
#define DMS_SUBCMD_GET_VRD_INFO 2

#define DMS_MAIN_CMD_SOFT_FAULT 'J'
#define DMS_SUBCMD_SENSOR_NODE_REGISTER 0
#define DMS_SUBCMD_SENSOR_NODE_UNREGISTER 1
#define DMS_SUBCMD_SENSOR_NODE_UPDATE_VAL 2
#define DMS_SENSOR_NODE_REGISTER _IO(DMS_MAIN_CMD_SOFT_FAULT, DMS_SUBCMD_SENSOR_NODE_REGISTER)
#define DMS_SENSOR_NODE_UNREGISTER _IO(DMS_MAIN_CMD_SOFT_FAULT, DMS_SUBCMD_SENSOR_NODE_UNREGISTER)
#define DMS_SENSOR_NODE_UPDATE_VAL _IO(DMS_MAIN_CMD_SOFT_FAULT, DMS_SUBCMD_SENSOR_NODE_UPDATE_VAL)


#define DMS_MAIN_CMD_TRS 'K'
#define DMS_SUBCMD_CREATE_GROUP  0
#define DMS_SUBCMD_DEL_GROUP     1
#define DMS_SUBCMD_GET_GROUP     2
#define DMS_SUBCMD_GET_AI_INFO   3
#define DMS_SUBCMD_GET_TS_HB_STATUS   4
#define DMS_SUBCMD_GET_AI_INFO_ASYN  5
#define DMS_SUBCMD_SET_AI_INFO_ASYN  6
#define DMS_SUBCMD_SET_STL_RUNNING_STATUS  7
#define DMS_SUBCMD_UPDATE_TS_PATCH  8
#define DMS_SUBCMD_LOAD_TS_PATCH    9

#define DMS_MAIN_CMD_SENSORHUB 'L'
#define DMS_SUBCMD_GET_SENSORHUB_STATUS  0
#define DMS_SUBCMD_GET_SENSORHUB_CONFIG  1

#define DMS_MAIN_CMD_ISP 'M'
#define DMS_SUBCMD_GET_ISP_STATUS 0
#define DMS_SUBCMD_GET_ISP_CONFIG 1

#define DMS_MAIN_CMD_CAN 'N'
#define DMS_SUBCMD_GET_CAN_STATUS  0

#define DMS_MAIN_CMD_BBOX 'O'
#define DMS_SUBCMD_GET_LAST_BOOT_STATE 0
#define DMS_SUBCMD_GET_LOG_DUMP_INFO 1
#define DMS_SUBCMD_GET_PCIE_LOG_DUMP_INFO 2

#define DMS_MAIN_CMD_EMMC 'P'
#define DMS_SUBCMD_GET_EMMC_INFO 0

#define DMS_MAIN_CMD_PCIE 'Q'
#define DMS_SUBCMD_GET_PCIE_LINK_INFO 0

#define DMS_MAIN_CMD_QOS 'R'
#define DMS_SUBCMD_GET_CONFIG_INFO 0

#define DMS_MAIN_CMD_LOG 'S'
#define DMS_SUBCMD_GET_LOG_INFO 0

#define DMS_MAIN_CMD_SILS 'T'
#define DMS_SUBCMD_GET_SILS_EMU_INFO 0
#define DMS_SUBCMD_GET_SILS_HEALTH_STATUS 1
#define DMS_SUBCMD_SET_SILS_PMUWDG_INFO 2
#define DMS_SUBCMD_GET_SILS_PMUWDG_INFO 3
#define DMS_SUBCMD_SET_SILS_HARDWARE_TEST 4
#define DMS_SUBCMD_GET_SILS_HARDWARE_TEST 5

#define DMS_MAIN_CMD_P2P_COM 'U'
#define DMS_SUBCMD_P2P_COM_LINK_INFO 0
#define DMS_SUBCMD_P2P_COM_FORCE_LINKDOWN 1

#define DMS_MAIN_CMD_FLASH 'V'
#define DMS_SUBCMD_GET_FLASH_ERASE_COUNT 0

#define DMS_MAIN_CMD_DVPP 0x60000
#define DMS_SUBCMD_GET_DVPP_STATUS  0
#define DMS_SUBCMD_GET_DVPP_RATE  1

#define DMS_MAIN_CMD_LP             0x00008 /* DSMI_MAIN_CMD_LP */
#define DMS_SUBCMD_LP_SUSPEND       0
#define DMS_MAIN_CMD_TEMP           0x00032 /* DSMI_MAIN_CMD_TEMP */

#define DMS_SUBCMD_LP_VOLTAGE_CURRENT_AICORE    0
#define DMS_SUBCMD_LP_VOLTAGE_CURRENT_HYBRID    1
#define DMS_SUBCMD_LP_VOLTAGE_CURRENT_TAISHAN   2
#define DMS_SUBCMD_LP_VOLTAGE_CURRENT_DDR       3
#define DMS_SUBCMD_LP_ACG                       4
#define DMS_SUBCMD_LP_STATUS        5
#define DMS_SUBCMD_LP_GET_ALL_TOPS  6
#define DMS_SUBCMD_LP_SET_TOPS      7
#define DMS_SUBCMD_LP_GET_CUR_TOPS  8
#define DMS_SUBCMD_LP_AICORE_FREQREDUC_CAUSE  9
#define DMS_SUBCMD_LP_GET_POWER_INFO 10
#define DMS_SUBCMD_LP_SET_IDLE_SWITCH 11
#define DMS_SUBCMD_LP_SET_STRESS_TEST 12 /* 2024-12-30 reserve */
#define DMS_SUBCMD_LP_GET_AIC_CPM     13 /* 2024-12-30 reserve */
#define DMS_SUBCMD_LP_GET_BUS_CPM     14 /* 2024-12-30 reserve */
#define DMS_SUBCMD_LP_FEATURE_SWITCH  15
#define DMS_SUBCMD_LP_INNER_START           0x100 /* start for inner used sub_cmd, not for product */
#define DMS_SUBCMD_LP_INNER_SET_STRESS_TEST 0x101
#define DMS_SUBCMD_LP_INNER_GET_AIC_CPM     0x102
#define DMS_SUBCMD_LP_INNER_GET_BUS_CPM     0x103
#define DMS_SUBCMD_LP_INNER_GET_STRESS_FREQ_VOLT 0x104
#define DMS_SUBCMD_LP_INNER_GET_TEMP_INFO   0x105
#define DMS_SUBCMD_LP_INNER_GET_AIC_INFO    0x106
#define DMS_SUBCMD_LP_INNER_GET_BUS_INFO    0x107
#define DMS_SUBCMD_LP_INNER_GET_HBM_INFO    0x108

#define DMS_SUBCMD_LP_SET_LPTEST    1000    /* for lptest debug */
#define DMS_SUBCMD_LP_GET_LPTEST    1001

#define DMS_SUBCMD_TEMP_DDR           0
#define DMS_SUBCMD_TEMP_DDR_THOLD     1
#define DMS_SUBCMD_TEMP_SOC_THOLD     2
#define DMS_SUBCMD_TEMP_SOC_MIN_THOLD 3

/* HCCS */
#define DMS_FILTER_HCCS "main_cmd=0x10"
#define DMS_FILTER_HCCS_CREDIT_INFO "main_cmd=0x10,sub_cmd=0x4"
#define DMS_HCCS_SUB_CMD_STATUS     0
#define DMS_HCCS_SUB_CMD_LANE_INFO  1
#define DMS_HCCS_SUB_CMD_PING_INFO  2
#define DMS_HCCS_SUB_CMD_STATISTIC_INFO  3
#define DMS_HCCS_SUB_CMD_CREDIT_INFO  4
#define DMS_HCCS_SUB_CMD_STATISTIC_INFO_EXT  5

#define FILTER_MAX_LEN 128

#define DMS_BIST_CMD_GET_RSLT       0
#define DMS_BIST_CMD_SET_MODE       1
#define DMS_BIST_CMD_GET_VEC_CNT    2
#define DMS_BIST_CMD_GET_VEC_TIME   3


#define DMS_DTM_OPCODE_GET_INFO_LIST      0
#define DMS_DTM_OPCODE_GET_STATE          1
#define DMS_DTM_OPCODE_GET_CAPACITY       2
#define DMS_DTM_OPCODE_SET_POWER_STATE    3
#define DMS_DTM_OPCODE_SCAN               4
#define DMS_DTM_OPCODE_FAULT_DIAG         5
#define DMS_DTM_OPCODE_EVENT_NOTIFY       6
#define DMS_DTM_OPCODE_GET_LINK_STATE     7
#define DMS_DTM_OPCODE_SET_LINK_STATE     8
#define DMS_DTM_OPCODE_FAULT_INJECT       9
#define DMS_DTM_OPCODE_ENABLE_DEVICE      10
#define DMS_DTM_OPCODE_DISABLE_DEVICE     11
#define DMS_DTM_OPCODE_SET_POWER_INFO     12
#define DMS_DTM_OPCODE_GET_POWER_INFO     13

#define DMS_DTM_POWER_CAPABILITY     0
#define DMS_CONTENT_RTC_CONFIG     0

/* Fault inject */
#define DMS_FAULT_INJECT_SUB_CMD_MEMORY 0

struct dms_ioctl_arg {
    unsigned int msg_source;
    unsigned int main_cmd;
    unsigned int sub_cmd;
    const char *filter;
    unsigned int filter_len;
    void *input;
    unsigned int input_len;
    void *output;
    unsigned int output_len;
};

#define DMS_MAX_EVENT_NAME_LENGTH 256
#define DMS_MAX_EVENT_DATA_LENGTH 32
#define DMS_MAX_EVENT_ARRAY_LENGTH 128
#define DMS_MAX_EVENT_INFO_NUM 4

struct dms_event_para {
    unsigned int event_code;
    int pid;

    unsigned int event_id;
    unsigned short deviceid;
    unsigned short node_type;
    unsigned char node_id;
    unsigned short sub_node_type;
    unsigned char sub_node_id;
    unsigned char severity;
    unsigned char assertion;
    unsigned short sensor_num;
    int event_serial_num;
    int notify_serial_num;
    unsigned long long alarm_raised_time;
    char event_name[DMS_MAX_EVENT_NAME_LENGTH];
    char additional_info[DMS_MAX_EVENT_DATA_LENGTH];
    unsigned char event_info[DMS_MAX_EVENT_INFO_NUM];
};

struct devdrv_event_obj_para {
    unsigned int event_count;
    struct dms_event_para dms_event[DMS_MAX_EVENT_ARRAY_LENGTH];
};

enum cmd_source {
    FROM_DSMI = 0,
    FROM_HAL = 1,
    FROM_KERNEL = 2
};
struct dms_read_event_ioctl {
    enum cmd_source cmd_src;
    int timeout;
};

struct dms_event_ioctrl {
    unsigned int devid;
    unsigned int event_code;
};

struct dms_power_state_st {
    unsigned int dev_id;
    int state;
};

struct dms_get_gpio {
    unsigned int dev_id;
    unsigned int gpio_num;
};

struct dms_set_device_info_in {
    unsigned int dev_id;
    unsigned int sub_cmd;
    const void *buff;
    unsigned int buff_size;
};

struct dms_set_device_info_in_multi_packet {
    unsigned int dev_id;
    unsigned int sub_cmd;
    unsigned char buff[300];
    unsigned int buff_size;
    unsigned int current_packet;
    unsigned int total_size;
};

struct dms_get_device_info_in {
    unsigned int dev_id;
    unsigned int sub_cmd;
    void *buff;
    unsigned int buff_size;
};
struct dms_hal_device_info_stru {
    unsigned int dev_id; /* in */
    int module_type;     /* in */
    int info_type;       /* in */
    unsigned int buff_size;     /* in&out */
    unsigned char payload[300]; /* in&out, valid length is buff_size */
};
#define DMS_HAL_DEV_INFO_HEAD_LEN ((size_t)&((struct dms_hal_device_info_stru *)0)->payload)

struct dms_get_device_info_out {
    unsigned int out_size;
};
#define DMS_FILTER_LENGTH 256

struct dms_filter_st {
    char filter[DMS_FILTER_LENGTH];
    unsigned int filter_len;
};


struct dms_get_vdevice_info_in {
    unsigned int dev_id;
    unsigned int vfid;
};

struct dms_get_vdevice_info_out {
    unsigned int total_core;
    unsigned int core_num;
    unsigned long mem_size;
    unsigned int bitmap;
};

struct dms_get_ddr_info_in {
    unsigned int dev_id;
    unsigned int vfid;
};

struct dms_get_ddr_info_out {
    unsigned long total;
    unsigned long free;
};

struct dms_get_dev_topology_in {
    unsigned int dev_id1;
    unsigned int dev_id2;
};

struct dms_set_bist_info_in {
    unsigned int dev_id;
    unsigned int buff_size;
    unsigned char *buff;
};

struct dms_get_bist_info_in {
    unsigned int dev_id;
    unsigned int sub_cmd;
    unsigned int size;
    unsigned char *buff;
};

struct dms_get_bist_info_out {
    unsigned int size;
};

struct dms_flash_content_in {
    int dev_id;
    unsigned int type;
    unsigned char *buf;
    unsigned int size;
    unsigned int offset;
};

struct dms_ctrl_device_node_in {
    unsigned int dev_id;
    int node_type;
    int node_id;
    unsigned int sub_cmd;
    void *in_buf;
    unsigned int in_size;
};

struct dms_get_all_device_node_in {
    unsigned int dev_id;
    unsigned int capability;
    struct dsmi_dtm_node_s *node_info;
    unsigned int size;
};

struct dms_get_all_device_node_out {
    unsigned int out_size;
};

struct dms_fault_inject_in {
    unsigned int dev_id;
    unsigned int vfid;
    unsigned int buff_size;
    const void *buff;
};

struct dms_sriov_switch_in {
    unsigned int dev_id;
    int sriov_switch;
};

struct dms_get_cpu_utilization_in {
    unsigned int index;
    unsigned int num;
};

struct dms_mem_info_in {
    unsigned int dev_id;
    unsigned int vfid;
};

struct dms_mem_info_out {
    unsigned long long total_size;
    unsigned long long use_size;
    unsigned long long free_size;
    unsigned long long hpage_2M_num;
    unsigned long long hpage_1G_num;
    unsigned int mem_util;
};

struct dms_cgroup_mem_info_out {
    unsigned long long limit_in_bytes;
    unsigned long long max_usage_in_bytes;
    unsigned long long usage_in_bytes;
};

struct dms_pass_through_mcu_in {
    unsigned int dev_id;
    unsigned int rw_flag;
    unsigned int buf_len;
    unsigned char *buf;
};

#define MCU_RESP_LEN  28
struct dms_pass_through_mcu_out {
    unsigned int dev_id;
    unsigned int response_len;
    unsigned char response_data[MCU_RESP_LEN];
};

#define DMS_FILTER_TS_INFO "main_cmd=0xb"
struct dms_ts_fault_mask {
    unsigned int type;
    unsigned int mask_switch;
};

#define DSMI_SIO_SLLC_INDEX_BIT (0xFF000000U)
#define DSMI_SIO_SUB_CMD_BIT    (0x00FFFFFFU)

struct dms_sio_crc_err_info {
    unsigned char sllc_index;       /* input parameter */
    unsigned short tx_error_count;
    unsigned short rx_error_count;
};

struct dms_device_state_out {
    unsigned long buf_size;
    unsigned char buf[];
};

struct dms_update_ts_patch {
    unsigned int devid;
    unsigned int update_type;   /* 0:install; 1: uninstall */
};

typedef struct dms_sils_info_out {
    unsigned int size;
} dms_sils_info_out_t;

#define SILS_RESERVE_LEN  8
typedef struct dms_sils_info_in {
    unsigned int dev_id;
    unsigned int size;
    unsigned int resv[SILS_RESERVE_LEN];
    void *buff;
} dms_sils_info_in_t;

#define UDIS_MAX_NAME_LEN 16
struct udis_get_ioctl_in {
    unsigned int devid;
    unsigned int module_type;
    char name[UDIS_MAX_NAME_LEN];
    unsigned int in_len;
    void *data;
};

struct udis_get_ioctl_out {
    unsigned long long last_update_time;
    unsigned int acc_ctrl;
    unsigned int out_len;
};

struct udis_set_ioctl_in {
    unsigned int devid;
    unsigned int module_type;
    char name[UDIS_MAX_NAME_LEN];
    unsigned int acc_ctrl;
    unsigned long long last_update_time;
    unsigned int data_len;
    const void *data;
};

struct dms_get_phy_dev_info_out {
    unsigned int chip_id;
    unsigned int die_id;
    unsigned int reserved[32];
};

#endif
