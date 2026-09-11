/*
* Copyright (c) Huawei Technologies Co., Ltd. 2019-2022. All rights reserved.
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
* Create: 2019-10-15
*/

#ifndef __DEVDRV_MANAGER_COMMON_H
#define __DEVDRV_MANAGER_COMMON_H


#include <linux/list.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/types.h>
#ifndef AOS_LLVM_BUILD
#include <linux/uio_driver.h>
#endif
#include <linux/notifier.h>
#ifndef AOS_LLVM_BUILD
#include <linux/radix-tree.h>
#endif
#include <linux/hashtable.h>
#include <linux/wait.h>

#include "ka_hashtable_pub.h"
#include "ka_base_pub.h"
#include "ka_ioctl_pub.h"
#include "ka_memory_pub.h"

#include "devdrv_common.h"
#include "devdrv_platform_resource.h"
#include "devdrv_manager_comm.h"
#include "kernel_version_adapt.h"

#define DAVINCI_INTF_MODULE_DEVMNG "DEVMNG"
#define PCI_VENDOR_ID_HUAWEI 0x19e5

#ifndef __KA_GFP_ACCOUNT
#ifdef __GFP_KMEMCG
#define __KA_GFP_ACCOUNT __GFP_KMEMCG /* for linux version 3.10 */
#endif
#ifdef __GFP_NOACCOUNT
#define __KA_GFP_ACCOUNT 0 /* for linux version 4.1 */
#endif
#endif
/* manager */
#define DEVDRV_MANAGER_MAGIC                                    'M'
#define DEVDRV_MANAGER_GET_PCIINFO                              _KA_IO(DEVDRV_MANAGER_MAGIC, 1)
#define DEVDRV_MANAGER_GET_DEVNUM                               _KA_IO(DEVDRV_MANAGER_MAGIC, 2)
#define DEVDRV_MANAGER_GET_PLATINFO                             _KA_IO(DEVDRV_MANAGER_MAGIC, 3)
#define DEVDRV_MANAGER_SVMVA_TO_DEVID                           _KA_IO(DEVDRV_MANAGER_MAGIC, 4)    /* RSVD */
#define DEVDRV_MANAGER_GET_CHANNELINFO                          _KA_IO(DEVDRV_MANAGER_MAGIC, 5)    /* RSVD */
#define DEVDRV_MANAGER_CONFIG_CQ                                _KA_IO(DEVDRV_MANAGER_MAGIC, 6)    /* RSVD */
#define DEVDRV_MANAGER_DEVICE_STATUS                            _KA_IO(DEVDRV_MANAGER_MAGIC, 7)

#define DEVDRV_MANAGER_GET_CORE_SPEC                            _KA_IO(DEVDRV_MANAGER_MAGIC, 17)
#define DEVDRV_MANAGER_GET_CORE_INUSE                           _KA_IO(DEVDRV_MANAGER_MAGIC, 18)
#define DEVDRV_MANAGER_GET_DEVIDS                               _KA_IO(DEVDRV_MANAGER_MAGIC, 19)
#define DEVDRV_MANAGER_GET_DEVINFO                              _KA_IO(DEVDRV_MANAGER_MAGIC, 20)
#define DEVDRV_MANAGER_GET_PCIE_ID_INFO                         _KA_IO(DEVDRV_MANAGER_MAGIC, 21)
#define DEVDRV_MANAGER_GET_FLASH_COUNT                          _KA_IO(DEVDRV_MANAGER_MAGIC, 22)

#define DEVDRV_MANAGER_GET_VOLTAGE                              _KA_IO(DEVDRV_MANAGER_MAGIC, 24)   /* RSVD */
#define DEVDRV_MANAGER_GET_TEMPERATURE                          _KA_IO(DEVDRV_MANAGER_MAGIC, 25)   /* RSVD */
#define DEVDRV_MANAGER_GET_AI_USE_RATE                          _KA_IO(DEVDRV_MANAGER_MAGIC, 26)   /* RSVD */
#define DEVDRV_MANAGER_GET_FREQUENCY                            _KA_IO(DEVDRV_MANAGER_MAGIC, 27)   /* RSVD */
#define DEVDRV_MANAGER_GET_POWER                                _KA_IO(DEVDRV_MANAGER_MAGIC, 28)
#define DEVDRV_MANAGER_GET_HEALTH_CODE                          _KA_IO(DEVDRV_MANAGER_MAGIC, 29)
#define DEVDRV_MANAGER_GET_ERROR_CODE                           _KA_IO(DEVDRV_MANAGER_MAGIC, 30)
#define DEVDRV_MANAGER_GET_DDR_CAPACITY                         _KA_IO(DEVDRV_MANAGER_MAGIC, 31)
#define DEVDRV_MANAGER_LPM3_SMOKE                               _KA_IO(DEVDRV_MANAGER_MAGIC, 32)
#define DEVDRV_MANAGER_BLACK_BOX_GET_EXCEPTION                  _KA_IO(DEVDRV_MANAGER_MAGIC, 33)
#define DEVDRV_MANAGER_DEVICE_MEMORY_DUMP                       _KA_IO(DEVDRV_MANAGER_MAGIC, 34)
#define DEVDRV_MANAGER_DEVICE_RESET_INFORM                      _KA_IO(DEVDRV_MANAGER_MAGIC, 35)
#define DEVDRV_MANAGER_GET_MODULE_STATUS                        _KA_IO(DEVDRV_MANAGER_MAGIC, 36)
#define DEVDRV_MANAGER_GET_DEVICE_DMA_ADDR                      _KA_IO(DEVDRV_MANAGER_MAGIC, 37)   /* RSVD */
#define DEVDRV_MANAGER_GET_INTERRUPT_INFO                       _KA_IO(DEVDRV_MANAGER_MAGIC, 38)   /* RSVD */
#define DEVDRV_MANAGER_REG_DDR_READ                             _KA_IO(DEVDRV_MANAGER_MAGIC, 39)
#define DEVDRV_MANAGER_GPIOIRQ_REGISTER                         _KA_IO(DEVDRV_MANAGER_MAGIC, 40)   /* RSVD */
#define DEVDRV_MANAGER_GPIOIRQ_WAIT_INT                         _KA_IO(DEVDRV_MANAGER_MAGIC, 41)   /* RSVD */
#define DEVDRV_MANAGER_GET_MINI_DEVID                           _KA_IO(DEVDRV_MANAGER_MAGIC, 42)
#define DEVDRV_MANAGER_GET_MINI_BOARD_ID                        _KA_IO(DEVDRV_MANAGER_MAGIC, 43)
#define DEVDRV_MANAGER_PCIE_PRE_RESET                           _KA_IO(DEVDRV_MANAGER_MAGIC, 44)
#define DEVDRV_MANAGER_PCIE_RESCAN                              _KA_IO(DEVDRV_MANAGER_MAGIC, 45)
#define DEVDRV_MANAGER_ALLOC_HOST_DMA_ADDR                      _KA_IO(DEVDRV_MANAGER_MAGIC, 46)
#define DEVDRV_MANAGER_FREE_HOST_DMA_ADDR                       _KA_IO(DEVDRV_MANAGER_MAGIC, 47)   /* RSVD */
#define DEVDRV_MANAGER_PCIE_READ                                _KA_IO(DEVDRV_MANAGER_MAGIC, 48)
#define DEVDRV_MANAGER_PCIE_SRAM_WRITE                          _KA_IO(DEVDRV_MANAGER_MAGIC, 49)   /* RSVD */
#define DEVDRV_MANAGER_PCIE_WRITE                               _KA_IO(DEVDRV_MANAGER_MAGIC, 50)
#define DEVDRV_MANAGER_GET_EMMC_VOLTAGE                         _KA_IO(DEVDRV_MANAGER_MAGIC, 51)
#define DEVDRV_MANAGER_GET_DEVICE_BOOT_STATUS                   _KA_IO(DEVDRV_MANAGER_MAGIC, 52)
#define DEVDRV_MANAGER_ENABLE_EFUSE_LDO                         _KA_IO(DEVDRV_MANAGER_MAGIC, 53)
#define DEVDRV_MANAGER_DISABLE_EFUSE_LDO                        _KA_IO(DEVDRV_MANAGER_MAGIC, 54)
#define DEVDRV_MANAGER_GET_TSENSOR                              _KA_IO(DEVDRV_MANAGER_MAGIC, 55)   /* RSVD */
#define DEVDRV_MANAGER_PCIE_SRAM_READ                           _KA_IO(DEVDRV_MANAGER_MAGIC, 56)
#define DEVDRV_MANAGER_CHECK_CANCEL_STATUS                      _KA_IO(DEVDRV_MANAGER_MAGIC, 57)   /* RSVD */
#define DEVDRV_MANAGER_CONTAINER_CMD                            _KA_IO(DEVDRV_MANAGER_MAGIC, 58)
#define DEVDRV_MANAGER_GET_HOST_PHY_MACH_FLAG                   _KA_IO(DEVDRV_MANAGER_MAGIC, 59)
#define DEVDRV_MANAGER_LOAD_KERNEL_LIB                          _KA_IO(DEVDRV_MANAGER_MAGIC, 60)   /* RSVD */
#define DEVDRV_MANAGER_GET_KERNEL_LIB                           _KA_IO(DEVDRV_MANAGER_MAGIC, 61)
#define DEVDRV_MANAGER_GET_LOCAL_DEVICEIDS                      _KA_IO(DEVDRV_MANAGER_MAGIC, 62)
#define DEVDRV_MANAGER_IMU_SMOKE                                _KA_IO(DEVDRV_MANAGER_MAGIC, 63)
#define DEVDRV_MANAGER_SYSTEM_RDY_CHECK                         _KA_IO(DEVDRV_MANAGER_MAGIC, 64)
#define DEVDRV_MANAGER_IPC_NOTIFY_CREATE                        _KA_IO(DEVDRV_MANAGER_MAGIC, 65)
#define DEVDRV_MANAGER_IPC_NOTIFY_OPEN                          _KA_IO(DEVDRV_MANAGER_MAGIC, 66)
#define DEVDRV_MANAGER_IPC_NOTIFY_CLOSE                         _KA_IO(DEVDRV_MANAGER_MAGIC, 67)
#define DEVDRV_MANAGER_IPC_NOTIFY_DESTROY                       _KA_IO(DEVDRV_MANAGER_MAGIC, 68)
#define DEVDRV_MANAGER_SET_NEW_TIME                             _KA_IO(DEVDRV_MANAGER_MAGIC, 69)   /* RSVD */
#define DEVDRV_MANAGER_CONFIG_DISABLE_WAKELOCK                  _KA_IO(DEVDRV_MANAGER_MAGIC, 70)   /* RSVD */
#define DEVDRV_MANAGER_PCIE_DDR_READ                            _KA_IO(DEVDRV_MANAGER_MAGIC, 71)
#define DEVDRV_MANAGER_PCIE_DDR_WRITE                           _KA_IO(DEVDRV_MANAGER_MAGIC, 72)   /* RSVD */

#define DEVDRV_MANAGER_GET_CPU_INFO                             _KA_IO(DEVDRV_MANAGER_MAGIC, 74)
#define DEVDRV_MANAGER_INVAILD_TLB                              _KA_IO(DEVDRV_MANAGER_MAGIC, 75)   /* RSVD */
#define DEVDRV_MANAGER_SEND_TO_IMU                              _KA_IO(DEVDRV_MANAGER_MAGIC, 76)
#define DEVDRV_MANAGER_RECV_FROM_IMU                            _KA_IO(DEVDRV_MANAGER_MAGIC, 77)
#define DEVDRV_MANAGER_GET_HBM_CAPACITY                         _KA_IO(DEVDRV_MANAGER_MAGIC, 78)   /* RSVD */
#define DEVDRV_MANAGER_GET_DDR_BW_UTILIZATION                   _KA_IO(DEVDRV_MANAGER_MAGIC, 79)   /* RSVD */
#define DEVDRV_MANAGER_GET_HBM_BW_UTILIZATION                   _KA_IO(DEVDRV_MANAGER_MAGIC, 80)
#define DEVDRV_MANAGER_GET_IMU_INFO                             _KA_IO(DEVDRV_MANAGER_MAGIC, 81)
#define DEVDRV_MANAGER_GET_ECC_STATICS                          _KA_IO(DEVDRV_MANAGER_MAGIC, 82)   /* RSVD */
#define DEVDRV_MANAGER_CONFIG_ECC_ENABLE                        _KA_IO(DEVDRV_MANAGER_MAGIC, 83)
#define DEVDRV_MANAGER_GET_DDR_UTILIZATION                      _KA_IO(DEVDRV_MANAGER_MAGIC, 84)   /* RSVD */
#define DEVDRV_MANAGER_GET_HBM_UTILIZATION                      _KA_IO(DEVDRV_MANAGER_MAGIC, 85)   /* RSVD */
#define DEVDRV_MANAGER_GET_PMU_VOLTAGE                          _KA_IO(DEVDRV_MANAGER_MAGIC, 86)
#define DEVDRV_MANAGER_GET_BOOT_DEV_ID                          _KA_IO(DEVDRV_MANAGER_MAGIC, 87)
#define DEVDRV_MANAGER_CFG_DDR_STAT_INFO                        _KA_IO(DEVDRV_MANAGER_MAGIC, 88)   /* RSVD */
#define DEVDRV_MANAGER_GET_PMU_DIEID                            _KA_IO(DEVDRV_MANAGER_MAGIC, 89)
#define DEVDRV_MANAGER_GET_PROBE_NUM                            _KA_IO(DEVDRV_MANAGER_MAGIC, 90)
#define DEVDRV_MANAGER_DEBUG_INFORM                             _KA_IO(DEVDRV_MANAGER_MAGIC, 91)
#define DEVDRV_MANAGER_COMPUTE_POWER                            _KA_IO(DEVDRV_MANAGER_MAGIC, 92)
#define DEVDRV_MANAGER_GET_DEVID_BY_LOCALDEVID                  _KA_IO(DEVDRV_MANAGER_MAGIC, 93)
#define DEVDRV_MANAGER_SYNC_MATRIX_DAEMON_READY                 _KA_IO(DEVDRV_MANAGER_MAGIC, 94)
#define DEVDRV_MANAGER_GET_CONTAINER_DEVIDS                     _KA_IO(DEVDRV_MANAGER_MAGIC, 95)
#define DEVDRV_MANAGER_GET_DECFREQ_REASON                       _KA_IO(DEVDRV_MANAGER_MAGIC, 96)   /* RSVD */
#define DEVDRV_MANAGER_GET_BBOX_ERRSTR                          _KA_IO(DEVDRV_MANAGER_MAGIC, 97)
#define DEVDRV_MANAGER_GET_LLC_PERF_PARA                        _KA_IO(DEVDRV_MANAGER_MAGIC, 98)
#define DEVDRV_MANAGER_PCIE_IMU_DDR_READ                        _KA_IO(DEVDRV_MANAGER_MAGIC, 99)
#define DEVDRV_MANAGER_GET_SLOT_ID                              _KA_IO(DEVDRV_MANAGER_MAGIC, 100)
#define DEVDRV_MANAGER_PCIE_HOT_RESET                           _KA_IO(DEVDRV_MANAGER_MAGIC, 101)
#define DEVDRV_MANAGER_GET_SOC_DIE_ID                           _KA_IO(DEVDRV_MANAGER_MAGIC, 102)   /* RSVD */
#define DEVDRV_MANAGER_GET_CHIP_INFO                            _KA_IO(DEVDRV_MANAGER_MAGIC, 103)
#define DEVDRV_MANAGER_RST_I2C_CTROLLER                         _KA_IO(DEVDRV_MANAGER_MAGIC, 104)
#define DEVDRV_MANAGER_GET_XLOADER_BOOT_INFO                    _KA_IO(DEVDRV_MANAGER_MAGIC, 105)
#define DEVDRV_MANAGER_GET_GPIO_STATE                           _KA_IO(DEVDRV_MANAGER_MAGIC, 106)
#define DEVDRV_MANAGER_APPMON_BBOX_EXCEPTION_CMD                _KA_IO(DEVDRV_MANAGER_MAGIC, 107)
#define DEVDRV_MANAGER_GET_CONTAINER_FLAG                       _KA_IO(DEVDRV_MANAGER_MAGIC, 108)
#define DEVDRV_MANAGER_IPC_NOTIFY_SET_PID                       _KA_IO(DEVDRV_MANAGER_MAGIC, 109)
#define DEVDRV_MANAGER_IPC_NOTIFY_RECORD                        _KA_IO(DEVDRV_MANAGER_MAGIC, 110)
#define DEVDRV_MANAGER_P2P_ATTR                                 _KA_IO(DEVDRV_MANAGER_MAGIC, 111)
#define DEVDRV_MANAGER_GET_PROCESS_SIGN                         _KA_IO(DEVDRV_MANAGER_MAGIC, 112)
#define DEVDRV_MANAGER_GET_MASTER_DEV_IN_THE_SAME_OS            _KA_IO(DEVDRV_MANAGER_MAGIC, 113)
#define DEVDRV_MANAGER_GET_LOCAL_DEV_ID_BY_HOST_DEV_ID          _KA_IO(DEVDRV_MANAGER_MAGIC, 114)
#define DEVDRV_MANAGER_GET_FAULT_REPORT                         _KA_IO(DEVDRV_MANAGER_MAGIC, 115)   /* RSVD */
#define DEVDRV_MANAGER_GET_FLASH_INFO                           _KA_IO(DEVDRV_MANAGER_MAGIC, 116)

#define DEVDRV_MANAGER_GET_HISSSTATUS                           _KA_IO(DEVDRV_MANAGER_MAGIC, 118)
#define DEVDRV_MANAGER_GET_LPSTATUS                             _KA_IO(DEVDRV_MANAGER_MAGIC, 119)   /* RSVD */
#define DEVDRV_MANAGER_GET_VECTOR_INFO                          _KA_IO(DEVDRV_MANAGER_MAGIC, 120)   /* RSVD */
#define DEVDRV_MANAGER_GET_UARTSTATUS                           _KA_IO(DEVDRV_MANAGER_MAGIC, 121)   /* RSVD */
#define DEVDRV_MANAGER_GET_CANSTATUS                            _KA_IO(DEVDRV_MANAGER_MAGIC, 122)   /* RSVD */
#define DEVDRV_MANAGER_GET_UFSSTATUS                            _KA_IO(DEVDRV_MANAGER_MAGIC, 123)
#define DEVDRV_MANAGER_GET_SENSORHUBSTATUS                      _KA_IO(DEVDRV_MANAGER_MAGIC, 124)   /* RSVD */
#define DEVDRV_MANAGER_GET_ISPSTATUS                            _KA_IO(DEVDRV_MANAGER_MAGIC, 125)   /* RSVD */
#define DEVDRV_MANAGER_GET_CANCONFIG                            _KA_IO(DEVDRV_MANAGER_MAGIC, 126)   /* RSVD */
#define DEVDRV_MANAGER_GET_UFSINFO                              _KA_IO(DEVDRV_MANAGER_MAGIC, 127)
#define DEVDRV_MANAGER_SET_UFSINFO                              _KA_IO(DEVDRV_MANAGER_MAGIC, 128)
#define DEVDRV_MANAGER_GET_SENSORHUBCONFIG                      _KA_IO(DEVDRV_MANAGER_MAGIC, 129)   /* RSVD */
#define DEVDRV_MANAGER_GET_EMU_SUBSYS_STATUS                    _KA_IO(DEVDRV_MANAGER_MAGIC, 130)   /* RSVD */
#define DEVDRV_MANAGER_GET_SAFETYISLAND_STATUS                  _KA_IO(DEVDRV_MANAGER_MAGIC, 131)   /* RSVD */
#define DEVDRV_MANAGER_GET_ISPCONFIG                            _KA_IO(DEVDRV_MANAGER_MAGIC, 132)   /* RSVD */
#define DEVDRV_MANAGER_GET_TSDRV_DEV_COM_INFO                   _KA_IO(DEVDRV_MANAGER_MAGIC, 133)
#define DEVDRV_MANAGER_GET_ALL_TEMPERATURE                      _KA_IO(DEVDRV_MANAGER_MAGIC, 134)
#define DEVDRV_MANAGER_SAFETYISLAND_IPC_MSG_SEND                _KA_IO(DEVDRV_MANAGER_MAGIC, 135)   /* RSVD */
#define DEVDRV_MANAGER_SAFETYISLAND_IPC_MSG_RECV                _KA_IO(DEVDRV_MANAGER_MAGIC, 136)   /* RSVD */
#define DEVDRV_MANAGER_GET_DVPP_STATUS                          _KA_IO(DEVDRV_MANAGER_MAGIC, 137)   /* RSVD */
#define DEVDRV_MANAGER_GET_TS_GROUP_NUM                         _KA_IO(DEVDRV_MANAGER_MAGIC, 138)
#define DEVDRV_MANAGER_GET_CAPABILITY_GROUP_INFO                _KA_IO(DEVDRV_MANAGER_MAGIC, 139)
#define DEVDRV_MANAGER_DELETE_CAPABILITY_GROUP                  _KA_IO(DEVDRV_MANAGER_MAGIC, 140)
#define DEVDRV_MANAGER_CREATE_CAPABILITY_GROUP                  _KA_IO(DEVDRV_MANAGER_MAGIC, 141)
#define DEVDRV_MANAGER_PASSTHRU_MCU                             _KA_IO(DEVDRV_MANAGER_MAGIC, 142)   /* RSVD */
#define DEVDRV_MANAGER_GET_P2P_CAPABILITY                       _KA_IO(DEVDRV_MANAGER_MAGIC, 143)
#define DEVDRV_MANAGER_SET_CAN_CONFIG                           _KA_IO(DEVDRV_MANAGER_MAGIC, 144)   /* RSVD */
#define DEVDRV_MANAGER_GET_CAN_CONFIG                           _KA_IO(DEVDRV_MANAGER_MAGIC, 145)   /* RSVD */
#define DEVDRV_MANAGER_GET_SUBSYS_TEMPERATURE                   _KA_IO(DEVDRV_MANAGER_MAGIC, 146)   /* RSVD */
#define DEVDRV_MANAGER_GET_DVPP_RATIO                           _KA_IO(DEVDRV_MANAGER_MAGIC, 147)   /* RSVD */
#define DEVDRV_MANAGER_GET_ETH_ID                               _KA_IO(DEVDRV_MANAGER_MAGIC, 148)
#define DEVDRV_MANAGER_BIND_PID_ID                              _KA_IO(DEVDRV_MANAGER_MAGIC, 149)
#define DEVDRV_MANAGER_GET_H2D_DEVINFO                          _KA_IO(DEVDRV_MANAGER_MAGIC, 150)
#define DEVDRV_MANAGER_GET_DEV_INFO_BY_PHYID                    _KA_IO(DEVDRV_MANAGER_MAGIC, 151)
#define DEVDRV_MANAGER_GET_POWER_STATE                          _KA_IO(DEVDRV_MANAGER_MAGIC, 152)
#define DEVDRV_MANAGER_GET_PROBE_LIST                           _KA_IO(DEVDRV_MANAGER_MAGIC, 153)
#define DEVDRV_MANAGER_GET_CONSOLE_LOG_LEVEL                    _KA_IO(DEVDRV_MANAGER_MAGIC, 154)
#define DEVDRV_MANAGER_GET_TEMP_THOLD                           _KA_IO(DEVDRV_MANAGER_MAGIC, 155)   /* RSVD */
#define DEVDRV_MANAGER_SET_TEMP_THOLD                           _KA_IO(DEVDRV_MANAGER_MAGIC, 156)   /* RSVD */
#define DEVDRV_MANAGER_GET_LP_INFO                              _KA_IO(DEVDRV_MANAGER_MAGIC, 157)   /* RSVD */
#define DEVDRV_MANAGER_CREATE_VDEV                              _KA_IO(DEVDRV_MANAGER_MAGIC, 158)
#define DEVDRV_MANAGER_DESTROY_VDEV                             _KA_IO(DEVDRV_MANAGER_MAGIC, 159)
#define DEVDRV_MANAGER_GET_VDEVINFO                             _KA_IO(DEVDRV_MANAGER_MAGIC, 160)
#define DEVDRV_MANAGER_IPC_UNIFY_MSG                            _KA_IO(DEVDRV_MANAGER_MAGIC, 161)
#define DEVDRV_MANAGER_UPDATE_STARTUP_STATUS                    _KA_IO(DEVDRV_MANAGER_MAGIC, 162)
#define DEVDRV_MANAGER_GET_STARTUP_STATUS                       _KA_IO(DEVDRV_MANAGER_MAGIC, 163)
#define DEVDRV_MANAGER_GET_DEVICE_HEALTH_STATUS                 _KA_IO(DEVDRV_MANAGER_MAGIC, 164)
#define DEVDRV_MANAGER_QUERY_HOST_PID                           _KA_IO(DEVDRV_MANAGER_MAGIC, 165)
#define DEVDRV_MANAGER_EQUIPMENT_SET_SILS_INFO                  _KA_IO(DEVDRV_MANAGER_MAGIC, 166)   /* RSVD */
#define DEVDRV_MANAGER_EQUIPMENT_GET_SILS_INFO                  _KA_IO(DEVDRV_MANAGER_MAGIC, 167)   /* RSVD */
#define DEVDRV_MANAGER_GET_DDR_BASE_INFO                        _KA_IO(DEVDRV_MANAGER_MAGIC, 168)   /* RSVD */
#define DEVDRV_MANAGER_GET_DDR_MANUFACTURES_INFO                _KA_IO(DEVDRV_MANAGER_MAGIC, 169)   /* RSVD */
#define DEVDRV_MANAGER_GET_BOOTSTRAP                            _KA_IO(DEVDRV_MANAGER_MAGIC, 170)
#define DEVDRV_MANAGER_FLASH_USER_CMD                           _KA_IO(DEVDRV_MANAGER_MAGIC, 171)
#define DEVDRV_MANAGER_FLASH_ROOT_CMD                           _KA_IO(DEVDRV_MANAGER_MAGIC, 172)
#define DEVDRV_MANAGER_PCIE_BBOX_HDR_READ                       _KA_IO(DEVDRV_MANAGER_MAGIC, 173)
#define DEVDRV_MANAGER_BOARD_INFO_MEM                           _KA_IO(DEVDRV_MANAGER_MAGIC, 174)
#define DEVDRV_MANAGER_GET_DEV_RESOURCE_INFO                    _KA_IO(DEVDRV_MANAGER_MAGIC, 175)
#define DEVDRV_MANAGER_REG_VMNG_CLIENT                          _KA_IO(DEVDRV_MANAGER_MAGIC, 176)
#define DEVDRV_MANAGER_GET_TS_INFO                              _KA_IO(DEVDRV_MANAGER_MAGIC, 177)   /* RSVD */
#define DEVDRV_MANAGER_GET_CHIP_COUNT                           _KA_IO(DEVDRV_MANAGER_MAGIC, 178)
#define DEVDRV_MANAGER_GET_CHIP_LIST                            _KA_IO(DEVDRV_MANAGER_MAGIC, 179)
#define DEVDRV_MANAGER_GET_DEVICE_FROM_CHIP                     _KA_IO(DEVDRV_MANAGER_MAGIC, 180)
#define DEVDRV_MANAGER_GET_CHIP_FROM_DEVICE                     _KA_IO(DEVDRV_MANAGER_MAGIC, 181)
#define DEVDRV_MANAGER_QUERY_DEV_PID                            _KA_IO(DEVDRV_MANAGER_MAGIC, 182)
#define DEVDRV_MANAGER_GET_QOS_INFO                             _KA_IO(DEVDRV_MANAGER_MAGIC, 183)   /* RSVD */
#define DEVDRV_MANAGER_SET_QOS_INFO                             _KA_IO(DEVDRV_MANAGER_MAGIC, 184)   /* RSVD */
#define DEVDRV_MANAGER_SET_SVM_VDEVINFO                         _KA_IO(DEVDRV_MANAGER_MAGIC, 185)
#define DEVDRV_MANAGER_UNBIND_PID_ID                            _KA_IO(DEVDRV_MANAGER_MAGIC, 186)
#define DEVDRV_MANAGER_GET_DEVICE_VF_MAX                        _KA_IO(DEVDRV_MANAGER_MAGIC, 187)
#define DEVDRV_MANAGER_GET_SVM_VDEVINFO                         _KA_IO(DEVDRV_MANAGER_MAGIC, 188)
#define DEVDRV_MANAGER_GET_DEVICE_VF_LIST                       _KA_IO(DEVDRV_MANAGER_MAGIC, 189)
#define DEVDRV_MANAGER_SET_VDEVMODE                             _KA_IO(DEVDRV_MANAGER_MAGIC, 190)
#define DEVDRV_MANAGER_GET_VDEVMODE                             _KA_IO(DEVDRV_MANAGER_MAGIC, 191)
#define DEVDRV_MANAGER_SET_SIGN                                 _KA_IO(DEVDRV_MANAGER_MAGIC, 192)
#define DEVDRV_MANAGER_GET_SIGN                                 _KA_IO(DEVDRV_MANAGER_MAGIC, 193)
#define DEVDRV_MANAGER_GET_SILSINFO                             _KA_IO(DEVDRV_MANAGER_MAGIC, 194)   /* RSVD */
#define DEVDRV_MANAGER_SET_SILSINFO                             _KA_IO(DEVDRV_MANAGER_MAGIC, 195)   /* RSVD */
#define DEVDRV_MANAGER_GET_VDEVNUM                              _KA_IO(DEVDRV_MANAGER_MAGIC, 196)
#define DEVDRV_MANAGER_GET_VDEVIDS                              _KA_IO(DEVDRV_MANAGER_MAGIC, 197)
#define DEVDRV_MANAGER_TS_LOG_DUMP                              _KA_IO(DEVDRV_MANAGER_MAGIC, 198)
#define DEVDRV_MANAGER_GET_CORE_UTILIZATION                     _KA_IO(DEVDRV_MANAGER_MAGIC, 199)
#define DEVDRV_MANAGER_GET_OSC_FREQ                             _KA_IO(DEVDRV_MANAGER_MAGIC, 200)

#define DEVDRV_MANAGER_GET_CURRENT_AIC_FREQ                     _KA_IO(DEVDRV_MANAGER_MAGIC, 202)
#define DEVDRV_MANAGER_GET_HOST_KERN_LOG                        _KA_IO(DEVDRV_MANAGER_MAGIC, 203)
#define DEVDRV_MANAGER_DEVICE_VMCORE_DUMP                       _KA_IO(DEVDRV_MANAGER_MAGIC, 204)
#define DEVDRV_MANAGER_CONFIG_DEVICE_SHARE                      _KA_IO(DEVDRV_MANAGER_MAGIC, 205)
#define DEVDRV_MANAGER_IPC_NOTIFY_SET_ATTR                      _KA_IO(DEVDRV_MANAGER_MAGIC, 206)
#define DEVDRV_MANAGER_IPC_NOTIFY_GET_INFO                      _KA_IO(DEVDRV_MANAGER_MAGIC, 207)
#define DEVDRV_MANAGER_IPC_NOTIFY_GET_ATTR                      _KA_IO(DEVDRV_MANAGER_MAGIC, 208)
#define DEVDRV_MANAGER_CMD_MAX_NR                               209

#define DEVDRV_MANAGER_IPC_NOTIFY_CMD_NUM \
    (_KA_IOC_NR(DEVDRV_MANAGER_IPC_NOTIFY_DESTROY) - _KA_IOC_NR(DEVDRV_MANAGER_IPC_NOTIFY_CREATE))

#if defined (CFG_SOC_PLATFORM_MINI) && !defined(CFG_SOC_PLATFORM_MINIV2) && !defined(DEVMNG_UT) && !defined(LOG_UT)
#define DEVDRV_CONTIANER_NUM_OF_LONG ((DEVDRV_MAX_DAVINCI_NUM - 1) / 64 + 1)
struct devdrv_device_info {
    u64 dump_ddr_dma_addr;
    u64 cpu_system_count;
    u64 dev_nominal_osc_freq;
    u64 monotonic_raw_time_ns;

    u32 ai_cpu_broken_map;
    u32 ai_core_broken_map;
    u64 aicore_bitmap;
    u32 ctrl_cpu_ip;
    u32 ctrl_cpu_id;
    u32 ctrl_cpu_core_num;
    u32 ctrl_cpu_occupy_bitmap;
    u32 ctrl_cpu_endian_little;
    u32 ts_cpu_core_num;
    u32 ai_cpu_core_num;
    u32 ai_core_num;
    u32 ai_cpu_core_id;
    u32 ai_core_id;
    u32 aicpu_occupy_bitmap;
    u32 hardware_version;
    u32 ts_load_fail;
    u32 dump_ddr_size;
    u32 ffts_type;
    u32 chip_name;
    u32 chip_version;
    u32 chip_info;

    u8 env_type;
    u8 ai_cpu_ready_num;
    u8 ai_core_ready_num;
    u8 ai_subsys_ip_map;
};
#else
#define DEVDRV_CONTIANER_NUM_OF_LONG ((VDAVINCI_MAX_VDEV_ID - 1) / 64 + 1)
#define MAX_CHIP_NAME 32
#define DEVMNG_SHM_INFO_RANDOM_SIZE 24
struct devdrv_device_info { /* for device sync to host */
    u64 dump_ddr_dma_addr;
    u64 aicore_freq;
    u64 cpu_system_count;
    u64 dev_nominal_osc_freq;
    u64 monotonic_raw_time_ns;
    u64 vector_core_freq;
    u64 reg_ddr_dma_addr;
    u64 computing_power[DEVDRV_MAX_COMPUTING_POWER_TYPE];

    u32 ai_cpu_broken_map;
    u32 ai_core_broken_map;
    u64 aicore_bitmap;
    u32 ai_subsys_ip_map;

    u32 ctrl_cpu_ip;
    u32 ctrl_cpu_id;
    u32 ctrl_cpu_core_num;
    u32 ctrl_cpu_occupy_bitmap;
    u32 ctrl_cpu_endian_little;
    u32 ts_cpu_core_num;
    u32 ai_cpu_core_num;
    u32 ai_core_num;
    u32 ai_cpu_core_id;
    u32 ai_core_id;
    u32 aicpu_occupy_bitmap;

    u32 hardware_version;
    u32 ts_load_fail;
    u32 capability;
    u32 dump_ddr_size;
    u32 vector_core_num;
    u32 reg_ddr_size;
    u32 ffts_type;

    u32 chip_name;
    u32 chip_version;
    u32 chip_info;
    u8 soc_version[MAX_CHIP_NAME];
    u8 chip_id;
    u8 multi_chip;
    u8 multi_die;
    u8 mainboard_id;
    u16 connect_type;
    u16 board_id;
    u32 die_id;

    u8 env_type;
    u8 ai_cpu_ready_num;
    u8 ai_core_ready_num;
    u8 ai_core_num_level;
    u8 ai_core_freq_level;
    u8 template_name[MAX_CHIP_NAME];
    u8 resv[3]; /* 3 bytes for aligned */
    u16 server_id;
    u16 scale_type;
    u32 super_pod_id;
    u16 addr_mode;
    char random_number[DEVMNG_SHM_INFO_RANDOM_SIZE];
};
#endif
#define MAX_EXCEPTION_THREAD 2
struct devdrv_black_box {
    struct semaphore black_box_sema[MAX_EXCEPTION_THREAD];
    struct list_head exception_list[MAX_EXCEPTION_THREAD];
    u32 exception_num[MAX_EXCEPTION_THREAD];
    pid_t black_box_pid[MAX_EXCEPTION_THREAD];
    spinlock_t spinlock;
    u8 thread_should_stop;
};
#ifndef AOS_LLVM_BUILD
/* only use in device */
struct devdrv_lib_serve_master {
    ka_radix_tree_root_t lib_tree;  // host_pid is tag, client context is item
    struct mutex lock;                // lock when initing
    u32 status;                       // flag whether lib_tree is inited
};
#endif
#ifdef CFG_SOC_PLATFORM_MDC_V51
#define DEVDRV_MAX_NODE_NUM 1
#else

#ifdef CFG_FEATURE_VF_USE_DEVID
#define DEVDRV_MAX_NODE_NUM DEVDRV_MAX_DAVINCI_NUM
#else
#define DEVDRV_MAX_NODE_NUM 4
#endif

#endif

#define DEVDRV_PROC_HASH_TABLE_BIT 10
#define DEVDRV_PROC_HASH_TABLE_SIZE (0x1 << DEVDRV_PROC_HASH_TABLE_BIT)
#define DEVDRV_PROC_HASH_TABLE_MASK (DEVDRV_PROC_HASH_TABLE_SIZE - 1)

#ifndef BITS_PER_LONG_LONG
#define BITS_PER_LONG_LONG 64
#endif

struct devdrv_manager_info {
    /* number of devices probed by pcie */
    u32 prob_num;
    u64 prob_device_bitmap[DEVDRV_MAX_DAVINCI_NUM/BITS_PER_LONG_LONG + 1];

    u32 num_dev;
    u32 pf_num;
    u32 vf_num;

    u32 plat_info;                           /* 0:device side, 1: host side */
    u32 machine_mode;                        /* 0:RC_MODE,  1: EP_MODE */
    u32 amp_or_smp;                          /* 0:AMP_MODE, 1: SMP_MODE */
    u32 dev_id_flag[DEVDRV_MAX_DAVINCI_NUM]; /* get devce id from host */
    u32 dev_id[DEVDRV_MAX_DAVINCI_NUM];      /* device id assigned by host device driver */

    struct device *dev; /* device manager dev */
    spinlock_t spinlock;
    struct workqueue_struct *dev_rdy_work;

    int msg_chan_rdy[DEVDRV_MAX_DAVINCI_NUM]; /* wait for msg channel ready */
    struct device *dma_dev[DEVDRV_MAX_DAVINCI_NUM]; /* device dma dev */
    wait_queue_head_t msg_chan_wait[DEVDRV_MAX_DAVINCI_NUM];
    struct devdrv_info *dev_info[DEVDRV_MAX_DAVINCI_NUM];
    int device_status[DEVDRV_MAX_DAVINCI_NUM];
    struct list_head pm_list_header; /* for power manager */
    struct mutex pm_list_lock;         /* for power manager */

    struct list_head hostpid_list_header;   /* for hostpid check only in host */
    spinlock_t proc_hash_table_lock;
#if (!defined (DEVMNG_UT)) && (!defined (DEVDRV_MANAGER_HOST_UT_TEST))
    KA_DECLARE_HASHTABLE(proc_hash_table, DEVDRV_PROC_HASH_TABLE_BIT); /* a process can only run on one numa node */
#endif
    struct mutex devdrv_sign_list_lock; /* for hostpid check */
    u32 devdrv_sign_count[MAX_DOCKER_NUM + 1U]; /* for hostpid check , 0~68 : container, 69 :non-container */

    struct list_head msg_pm_list_header; /* for logdrv and other module register on lowpower case */
    spinlock_t msg_pm_list_lock;         /* for logdrv and other module register on lowpower case */

    struct notifier_block ipc_monitor;
    struct notifier_block m3_ipc_monitor;
    /* for heart beat between TS and driver
     * DEVICE manager use this workqueue to
     * start a exception process.
     */
    struct workqueue_struct *heart_beat_wq;
    struct devdrv_black_box black_box;
    u32 host_type;
    struct tsdrv_drv_ops *drv_ops;
#ifndef AOS_LLVM_BUILD
    struct devdrv_lib_serve_master lib_serve;
#endif
    struct devdrv_container_table *container_table;
};

#define DEVDRV_HEART_BEAT_SQ_CMD 0xAABBCCDDU
#define DEVDRV_HEART_BEAT_CYCLE 6                               /* second */
#define DEVDRV_HEART_BEAT_TIMEOUT (DEVDRV_HEART_BEAT_CYCLE * 2) /* second */
#define DEVDRV_HEART_BEAT_THRESHOLD 3
#define DEVDRV_HEART_BEAT_MAX_QUEUE 20

#define DEVDRV_MATEBOOK_WINDOWS_HOST 0x12121212
#define DEVDRV_LINUX_LINUX_HOST 0x23232323
#define DEVDRV_WILL_HOST_REBOOT 0x26262626

#ifndef DEVDRV_HEART_BEAT_STRUCT__
#define DEVDRV_HEART_BEAT_STRUCT__

struct devdrv_aicore_msg {
    u32 syspcie_sysdma_status; /* upper 16 bit: syspcie, lower 16 bit: sysdma */
    u32 aicpu_heart_beat_exception;
    u32 aicore_bitmap; /* every bit identify one aicore, bit0 for core0, value 0 is ok */
    u32 ts_status;
};
#endif

#define DEVDRV_HB_SQ_RSV 16
/* Notice: The size must be the same as that when the SQ is applied for, which is 128 Bytes. */
struct devdrv_heart_beat_sq {
    u32 number;              /* increment counter */
    u32 cmd;                 /* always 0xAABBCCDD */
    struct timespec64 stamp; /* system time */
    u32 devid;
    u32 reserved;           /* used for judge different host-type */
    struct timespec64 wall; /* wall time */
    u64 cntpct;             /* ccpu sys counter */
    time64_t time_zone_interval;   /* unit: second */
    u32 rsv[DEVDRV_HB_SQ_RSV];
};

struct devdrv_heart_beat_cq {
    u32 number;                /* increment counter */
    u32 cmd;                   /* always 0xAABBCCDD */
    u32 syspcie_sysdma_status; /* upper 16 bit: syspcie, lower 16 bit: sysdma */
    u32 aicpu_heart_beat_exception;
    u32 aicore_bitmap; /* every bit identify one aicore, bit0 for core0, value 0 is ok */
    u32 ts_status;     /* ts working status, 0 is ok */

    u32 report_type; /* 0: heart beat report, 1: exception report */
    u32 exception_code;
    struct timespec64 exception_time;
};

#define DEVDRV_AI_SUBSYS_INIT_CHECK_SRAM_OFFSET 0
#define DEVDRV_AI_SUBSYS_INIT_CHECK_SDMA_OFFSET 1
#define DEVDRV_AI_SUBSYS_INIT_CHECK_BS_OFFSET 2
#define DEVDRV_AI_SUBSYS_INIT_CHECK_L2_BUF0_OFFSET 3
#define DEVDRV_AI_SUBSYS_INIT_CHECK_L2_BUF1_OFFSET 4

#define DEVDRV_AI_SUBSYS_SDMA_WORKING_STATUS_OFFSET 5
#define DEVDRV_AI_SUBSYS_SPCIE_WORKING_STATUS_OFFSET 6
#define DEVDRV_AI_SUBSYS_INIT_CHECK_AI_CORE_OFFSET 7
#define DEVDRV_AI_SUBSYS_INIT_CHECK_HWTS_OFFSET 8
#define DEVDRV_AI_SUBSYS_INIT_CHECK_DOORBELL_OFFSET 9

#define DEVDRV_MANAGER_MATRIX_INVALID 0
#define DEVDRV_MANAGER_MATRIX_VALID 1

#define DEVDRV_HEARTBEAT_IS_WORKING 1
#define DEVDRV_HEARTBEAT_NO_WORKING 0

#ifdef CFG_SOC_PLATFORM_MINI
struct devdrv_ts_ai_ready_info {
    u8 ai_cpu_ready_num;
    u8 ai_cpu_broken_map;
    u8 ai_core_ready_num;
    u8 ai_core_broken_map;
    u8 ai_subsys_ip_map;
    u8 res[3];
};
#else
struct devdrv_ts_ai_ready_info {
    u32 ai_cpu_ready_num;
    u32 ai_cpu_broken_map;
    u32 ai_core_ready_num;
    u32 ai_core_broken_map;
    u32 ai_subsys_ip_map;
    u32 res;
};
#endif

enum {
    DEVDRV_MANAGER_CHAN_H2D_SEND_DEVID,
    DEVDRV_MANAGER_CHAN_D2H_DEVICE_READY,
    DEVDRV_MANAGER_CHAN_D2H_DOWN,
    DEVDRV_MANAGER_CHAN_D2H_SUSNPEND,
    DEVDRV_MANAGER_CHAN_D2H_RESUME,
    DEVDRV_MANAGER_CHAN_D2H_FAIL_TO_SUSPEND,
    DEVDRV_MANAGER_CHAN_D2H_CORE_INFO,
    DEVDRV_MANAGER_CHAN_H2D_RERESH_AICORE_INFO,
    DEVDRV_MANAGER_CHAN_D2H_GET_PCIE_ID_INFO,

    DEVDRV_MANAGER_CHAN_H2D_SYNC_GET_DEVINFO,
    DEVDRV_MANAGER_CHAN_H2D_CONTAINER,
    DEVDRV_MANAGER_CHAN_H2D_GET_TASK_STATUS,

    DEVDRV_MANAGER_CHAN_H2D_SYNC_LOW_POWER,
    DEVDRV_MANAGER_CHAN_D2H_SYNC_MATRIX_READY,
    DEVDRV_MANAGER_CHAN_D2H_CHECK_PROCESS_SIGN,
    DEVDRV_MANAGER_CHAN_H2D_GET_TS_GROUP_INFO,
    DEVDRV_MANAGER_CHAN_D2H_GET_DEVICE_INDEX,
    DEVDRV_MANAGER_CHAN_H2D_WALL_TIME_SYNC,
    DEVDRV_MANAGER_CHAN_H2D_LOCALTIME_SYNC,
    DEVDRV_MANAGER_CHAN_H2D_GET_RESOURCE_INFO,
    DEVDRV_MANAGER_CHAN_H2D_QUERY_DMP_STARTED,
    DEVDRV_MANAGER_CHAN_H2D_QUERY_DEVICE_PID,
    DEVDRV_MANAGER_CHAN_H2D_DMS_EVENT_SUBSCRIBE,
    DEVDRV_MANAGER_CHAN_D2H_DMS_EVENT_DISTRIBUTE,
    DEVDRV_MANAGER_CHAN_H2D_DMS_EVENT_CLEAN,
    DEVDRV_MANAGER_CHAN_H2D_DMS_EVENT_MASK,
    DEVDRV_MANAGER_CHAN_H2D_NOTICE_PROCESS_EXIT,
    DEVDRV_MANAGER_CHAN_H2D_NOTICE_REBOOT,
    DEVDRV_MANAGER_CHAN_D2H_SEND_TSLOG_ADDR,
    DEVDRV_MANAGER_CHAN_D2H_SEND_DEVLOG_ADDR,
    DEVDRV_MANAGER_CHAN_H2D_SYNC_GET_CORE_UTILIZATION,
    DEVDRV_MANAGER_CHAN_PID_MAP_SYNC,
    DEVDRV_MANAGER_CHAN_D2H_SET_HOST_AICPU_NUM,
    DEVDRV_MANAGER_CHAN_H2D_SYNC_URD_FORWARD,
    DEVDRV_MANAGER_CHAN_H2D_HOST_NOTIFY_READY,
    DEVDRV_MANAGER_CHAN_MAX_ID,
};

struct devdrv_pid_map_sync {
    int op; /* 1 add, 0 del */
    pid_t pid;
    unsigned int chip_id;
    unsigned int vfid;
    pid_t host_pid;
    unsigned int cp_type;
};

#define DEVDRV_MANAGER_MSG_VALID 0x5A5A
#define DEVDRV_MANAGER_MSG_INVALID_RESULT 0x1A
#define DEVDRV_MANAGER_MSG_H2D_MAGIC 0x5A5A
#define DEVDRV_MANAGER_MSG_D2H_MAGIC 0xA5A5

#define DEVDRV_MANAGER_MSG_GET_ID_NUM 16
#define DEVDRV_MANAGER_UUID_NUM 16

#ifdef CFG_SOC_PLATFORM_CLOUD
#define DEVDRV_LOAD_KERNEL_TIMEOUT 120000
#else
#define DEVDRV_LOAD_KERNEL_TIMEOUT 15000
#endif

enum {
    MSG_ID_TYPE_STREAM = 0x50,
    MSG_ID_TYPE_EVENT,
    MSG_ID_TYPE_SQ,
    MSG_ID_TYPE_CQ,
    MSG_ID_TYPE_MODEL,
    MSG_ID_TYPE_TASK,
};

enum devdrv_board_type {
    DEVDRV_BOARD_PCIE_MINI,
    DEVDRV_BOARD_PCIE_CLOUD,
    DEVDRV_BOARD_AISERVER,
    DEVDRV_BOARD_FPGA,
    DEVDRV_BOARD_OTHERS,
};

struct devdrv_manager_power_state {
    u16 IsLowPowerState; /* Indicate if it is low power state. 0 is false 1 is Yes */
};

struct devdrv_manager_msg_head {
    u32 dev_id;
    u32 msg_id;
    u16 valid;  /* validity judgement, 0x5A5A is valide */
    u16 result; /* process result from rp, zero for succ, non zero for fail */
    u32 tsid;
    u32 vfid;
};
#if defined (CFG_SOC_PLATFORM_MINI) && !defined(CFG_SOC_PLATFORM_MINIV2) && !defined(DEVMNG_UT) && !defined(LOG_UT)
#define DEVDRV_MANAGER_INFO_LEN 512UL
#else
#define DEVDRV_MANAGER_INFO_LEN 512UL
#endif
#define DEVDRV_MANAGER_INFO_PAYLOAD_LEN (DEVDRV_MANAGER_INFO_LEN - sizeof(struct devdrv_manager_msg_head))

struct devdrv_manager_msg_info {
    struct devdrv_manager_msg_head header;
    u8 payload[DEVDRV_MANAGER_INFO_PAYLOAD_LEN];
};

struct devdrv_manager_msg_resource_info {
    u32 vfid;
    u32 owner_type;
    u32 owner_id;
    u32 info_type;
    u64 value;
    u64 value_ext; /* Extended Value Variable */
};

struct devmng_msg_h2d_info {
    u64 cpu_system_count;
    u64 monotonic_raw_time_ns;
    u32 ffts_type;
    u64 computing_power[DEVDRV_MAX_COMPUTING_POWER_TYPE];
};

typedef enum {
    DEVDRV_PLATFORM = 0,
    DEVDRV_SUSPEND,
    DEVDRV_BUTT
} DEVDRV_TASK;

#define DEVDRV_CONTAINER_MSG_PAYLOAD_LENTH (DEVDRV_MANAGER_INFO_PAYLOAD_LEN - (sizeof(u32) * 2))
#define DEVDRV_MINI_CONTAINER_TFLOP_NUM (DEVDRV_MINI_TOTAL_TFLOP / DEVDRV_MINI_FP16_UNIT)

/* the length of struct devdrv_container_msg must not longger than DEVDRV_MANAGER_INFO_PAYLOAD_LEN */
struct devdrv_container_msg {
    u32 devid;
    u32 cmd;
    u8 payload[DEVDRV_CONTAINER_MSG_PAYLOAD_LENTH];
};

#define DEVDRV_LOAD_KERNEL_SUCC 0
#define DEVDRV_LOAD_KERNEL_FAIL 1

enum devdrv_lib_status {
    DEVDRV_LIB_ORIGIN,
    DEVDRV_LIB_READY,
    DEVDRV_LIB_WAIT,
    DEVDRV_LIB_EXIT,
    DEVDRV_LIB_MAX_STATUS,
};

struct devdrv_lib_serve_client {
    pid_t host_pid;
    pid_t device_pid;
    u32 status;
    u32 proc_state;
    wait_queue_head_t wait;
    struct list_head lib_list;  // link all library info from host
    spinlock_t spinlock;        // lock when proc lib_list
};

struct devdrv_lib_info {
    struct devdrv_load_kernel kernel_info;
    struct list_head list;
};

#define PROCESS_SIGN_LENGTH  49
#define PROCESS_RESV_LENGTH  4
#define RANDOM_SIZE          24
#define DEVDRV_MAX_SIGN_NUM  1024

struct process_sign {
    pid_t tgid;
    char sign[PROCESS_SIGN_LENGTH];
    char resv[PROCESS_RESV_LENGTH];
};

#if !defined(DEVMNG_UT) && !defined(LOG_UT)
struct sq_perf {
    u32 taskid;
    struct timeval tv;
    u32 valid;
};

#define SQ_PERF_BUFF_LEN 10
#define CQ_TASKID_BUFF_LEN 500
struct devdrv_statistic_info {
    u64 cmdcount;
    u64 reportcount;
    u64 cb_cnt_rcv; // callback
    u64 irqwaittimeout;
    u64 send_rcv_max;
    u64 sch_max;
    u64 times;
    u64 send_count;
    u64 rcv_count;
    u64 send_bh_sum;
    u64 send_rcv_sum;
    u64 int_sch_sum;
    struct sq_perf sq_time[SQ_PERF_BUFF_LEN];
    u32 taskids[CQ_TASKID_BUFF_LEN];
    int index;
    u8 perf_on;
};
#endif

#if defined(CFG_SOC_PLATFORM_CLOUD)
    #define DEVDRV_MANGER_MAX_DEVICE_NUM 64
#elif defined(CFG_SOC_PLATFORM_MINIV2)
#if defined(CFG_SOC_PLATFORM_MDC_V51)
    #define DEVDRV_MANGER_MAX_DEVICE_NUM 1
#else
    #define DEVDRV_MANGER_MAX_DEVICE_NUM 2
#endif
#elif defined(CFG_SOC_PLATFORM_MINI)
    #define DEVDRV_MANGER_MAX_DEVICE_NUM 64
#endif

typedef enum {
    GROUP_OPER_SUCCESS = 0,
    GROUP_OPER_COMMON_ERROR = 1,
    GROUP_OPER_INPUT_DATA_NULL,
    GROUP_OPER_GROUP_ID_INVALID,
    GROUP_OPER_STATE_ILLEGAL,
    GROUP_OPER_NO_MORE_GROUP_CREATE,
    GROUP_OPER_NO_LESS_GROUP_DELETE,
    GROUP_OPER_DEFAULT_GROUP_ALREADY_CREATE,
    GROUP_OPER_NO_MORE_VALID_AICORE_CREATE,
    GROUP_OPER_NO_MORE_VALID_AIVECTOR_CREATE,
    GROUP_OPER_NO_MORE_VALID_SDMA_CREATE,
    GROUP_OPER_NO_MORE_VALID_AICPU_CREATE,
    GROUP_OPER_NO_MORE_VALID_ACTIVE_SQ_CREATE,
    GROUP_OPER_NO_LESS_VALID_AICORE_DELETE,
    GROUP_OPER_NO_LESS_VALID_AIVECTOR_DELETE,
    GROUP_OPER_NO_LESS_VALID_SDMA_DELETE,
    GROUP_OPER_NO_LESS_VALID_AICPU_DELETE,
    GROUP_OPER_NO_LESS_VALID_ACTIVE_SQ_DELETE,
    GROUP_OPER_BUILDIN_GROUP_NOT_CREATE,
    GROUP_OPER_SPECIFY_GROUPID_ALREADY_CREATE,
    GROUP_OPER_SPECIFY_GROUPID_NOT_CREATE,
    GROUP_OPER_DISABLE_HWTS_ALLGROUP_FAILED,
    GROUP_OPER_AICORE_POOL_FULL,
    GROUP_OPER_AIVECTOR_POOL_FULL,
    GROUP_OPER_SDMA_POOL_FULL,
    GROUP_OPER_AICPU_POOL_FULL,
    GROUP_OPER_ACTIVE_SQ_POOL_FULL,
    GROUP_OPER_CREATE_NULL_GROUP,
    GROUP_OPER_CREATE_NULL_SQ_GROUP,
    GROUP_OPER_VM_CONFIG_NOT_INIT,
    GROUP_OPER_VM_CONFIG_FAILD,
    GROUP_OPER_DELETE_GROUP_STREAM_RUNNING,
    GROUP_OPER_CREATE_NOT_SUPPORT_IN_DC,
    GROUP_OPER_ERROR_MAX,
} ts_group_oper_error_code_t;

struct ipc_rsp_ts_group_info {
    unsigned char  group_id;
    unsigned char  state;
    unsigned char  extend_attribute; // bit 0=1±íÊ¾ÊÇÄ¬ÈÏgroup
    unsigned char  aicore_number; // 0~9
    unsigned char  aivector_number; // 0~7
    unsigned char  sdma_number; // 0~15
    unsigned char  aicpu_number; // 0~15
    unsigned char  active_sq_number; // 0~31
    unsigned int   aicore_mask; // as output in dsmi_get_capability_group_info/halGetCapabilityGroupInfo
};


enum {
    TS_GROUP_OPERATE_CREATE = 0,
    TS_GROUP_OPERATE_DELETE
};

int devdrv_get_ts_group_info(struct get_ts_group_para *group_para,
                             struct ts_group_info *group_info, int group_info_num);


#ifdef CFG_SOC_PLATFORM_CLOUD
#define MCU_RESP_LEN  28
struct devdrv_mcu_msg {
    unsigned char dev_id;
    unsigned char rw_flag;
    unsigned char send_len;
    unsigned char *send_data;
    unsigned char resp_len;
    unsigned char resp_data[MCU_RESP_LEN];
};
#endif
#define BBOX_ERRSTR_LEN 48

struct devdrv_ts_log {
    u32 devid;
    u32 mem_size;
    dma_addr_t dma_addr;
};

struct devdrv_dev_log {
    u32 devid;
    u32 mem_size;
    dma_addr_t dma_addr;
    u32 log_type;
};

struct devdrv_board_info_cache {
    unsigned int board_id;
    unsigned int pcb_id;
    unsigned int bom_id;
    unsigned int slot_id;
};

struct devdrv_manager_info *devdrv_get_manager_info(void);
int devdrv_manager_register(struct devdrv_info *dev_info);
void devdrv_manager_unregister(struct devdrv_info *dev_info);
int devdrv_get_platformInfo(u32 *info);
int devdrv_get_devnum(u32 *num_dev);
u32 devdrv_manager_get_probe_num_kernel(void);
int devdrv_get_vdevnum(u32 *num_dev);
u32 devdrv_manager_get_devid(u32 local_devid);
u32 devdrv_manager_get_devid_flag(u32 local_devid);
int devdrv_get_devinfo(unsigned int devid, struct devdrv_device_info *info);
int devdrv_get_core_inuse(u32 devid, u32 vfid, struct devdrv_hardware_inuse *inuse);
int devdrv_get_core_spec(u32 devid, u32 vfid, struct devdrv_hardware_spec *spec);
struct devdrv_info *devdrv_manager_get_devdrv_info(u32 dev_id);
int copy_from_user_safe(void *to, const void __user *from, unsigned long n);
int copy_to_user_safe(void __user *to, const void *from, unsigned long n);
void devdrv_timestamp_sync_exit(struct devdrv_info *info);
u32 devdrv_manager_get_devid(u32 local_devid);
u32 devdrv_manager_get_devid_flag(u32 local_devid);
int devdrv_manager_get_product_type(void);
int dev_mnt_vdev_unregister_client(u32 phy_id, u32 vfid);
struct devdrv_board_info_cache *devdrv_get_board_info_host(unsigned int dev_id);

#ifdef CFG_HOST_ENV
void devdrv_host_generate_sdid(struct devdrv_info *dev_info);
#endif
#endif /* __DEVDRV_MANAGER_COMMON_H */
