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
#ifndef __DRV_COMM_H__
#define __DRV_COMM_H__

/* makefile容许的驱动宏定义
MK_DRV_MDC          MDC场景
MK_DRV_HOST         HOST软件
MK_DRV_DEVICE       DEVICE软件
MK_CHIP_ASCEND310   310芯片
MK_CHIP_ASCEND910   910芯片
MK_CHIP_ASCEND610   610芯片
*/

#define DRV_TRUE                1
#define DRV_FALSE               0
#define DRV_OK                  0

#define DRV_MAX_DEV_NUM_HOST    64  /* host侧支持最大的芯片数量 */
#define DRV_MAX_DEV_NUM_DEVICE  4   /* device侧最大的device数量，SMP系统才存在多个芯片 */
#define DRV_MAX_DEV_NUM         64  /* 不区分host或device时使用此宏，host和device共代码时 */
#define DRV_MAX_APP_PROC_NUM    32  /* 支持最大的APP进程数 */
#define DRV_MAX_VIRT_DEV_NUM    16  /* 每个芯片支持最大的虚拟设备数 */
#define DRV_MAX_FID_PER_DEV     17  /* 每个芯片支持最大的FID数 */

#define DRV_MODE_DC             0   /* DC场景 */
#define DRV_MODE_MDC            1   /* MDC场景 */
#define DRV_ENV_HOST            0   /* 当前是HOST侧 */
#define DRV_ENV_DEVICE          1   /* 当前是DEVICE侧 */

/* 芯片类型定义 */
typedef enum tagDRV_CHIP_TYPE_E {
    CHIP_TYPE_UNKNOW        = 0,
    CHIP_TYPE_ASCEND310     = 1, /* mini v1 */
    CHIP_TYPE_ASCEND910     = 2, /* cloud v1 */
    CHIP_TYPE_ASCEND610_310P = 3, /* mini v2 */
    CHIP_TYPE_ASCEND910B     = 4, /* cloud v2 */
    CHIP_TYPE_END
}DRV_CHIP_TYPE_E;

/* 运行模式 */
typedef enum tagDRV_RUN_MODE_E {
    RUN_MODE_NORMAL    = 0,
    RUN_MODE_DOCKER_PM = 1, /* 多容器场景，物理机 */
    RUN_MODE_PM        = 2, /* 多虚拟机场景，物理机 */
    RUN_MODE_VM        = 3, /* 多虚拟机场景，虚拟机 */
    RUN_MODE_END
}DRV_RUN_MODE_E;

#if defined(CFG_FEATURE_BOARD_ID_EXPAND)
#define BOARD_ID_REG_VALUE_MASK       0xFFFFFF
#else
#define BOARD_ID_REG_VALUE_MASK       0xFFFF
#endif

#ifdef CFG_SOC_MDC_V51_LITE
#define SLOT_ID_REG_VALUE_MASK       0x10000000
#endif
/* board id定义 */
#define DRV_BOARD_ID_MDC_START          0     /* MDC单板起始boardid */
#define DRV_BOARD_ID_MDC_END            99    /* MDC单板结束boardid */
#define DRV_BOARD_ID_MODULE_START       100   /* 模块单板起始boardid */
#define DRV_BOARD_ID_MODULE_END         199   /* 模块单板结束boardid */
#define DRV_BOARD_ID_PCIE_CARD_START    200   /* PCIE卡单板起始boardid */
#define DRV_BOARD_ID_PCIE_CARD_END      299   /* PCIE卡结束boardid */
#define DRV_BOARD_ID_NETWORK_START      700   /* 网络产品单板起始boardid */
#define DRV_BOARD_ID_NETWORK_END        799   /* 网络产品单板结束boardid */
#define DRV_BOARD_ID_CUSTOMER_START     800   /* 终端产品单板起始boardid */
#define DRV_BOARD_ID_CUSTOMER_END       899   /* 终端产品单板结束boardid */
#define DRV_BOARD_ID_EVB_START          900   /* EVB单板起始boardid */
#define DRV_BOARD_ID_EVB_END            949   /* EVB单板结束boardid */
#define DRV_BOARD_ID_FPGA_START         950   /* FPGA单板起始boardid */
#define DRV_BOARD_ID_FPGA_END           999   /* FPGA单板结束boardid */
#define DRV_BOARD_ID_IS_MDC(boardid) \
    (((boardid) >= DRV_BOARD_ID_MDC_START) && ((boardid) <= DRV_BOARD_ID_MDC_END))
#define DRV_BOARD_ID_IS_MODULE(boardid) \
    (((boardid) >= DRV_BOARD_ID_MODULE_START) && ((boardid) <= DRV_BOARD_ID_MODULE_END))
#define DRV_BOARD_ID_IS_PCIE_CARD(boardid) \
    (((boardid) >= DRV_BOARD_ID_PCIE_CARD_START) && ((boardid) <= DRV_BOARD_ID_PCIE_CARD_END))
#define DRV_BOARD_ID_IS_NETWORK(boardid) \
    (((boardid) >= DRV_BOARD_ID_NETWORK_START) && ((boardid) <= DRV_BOARD_ID_NETWORK_END))
#define DRV_BOARD_ID_IS_CUSTOMER(boardid) \
    (((boardid) >= DRV_BOARD_ID_CUSTOMER_START) && ((boardid) <= DRV_BOARD_ID_CUSTOMER_END))
#define DRV_BOARD_ID_IS_EVB(boardid) \
    (((boardid) >= DRV_BOARD_ID_EVB_START) && ((boardid) <= DRV_BOARD_ID_EVB_END))
#define DRV_BOARD_ID_IS_FPGA(boardid) \
    (((boardid) >= DRV_BOARD_ID_FPGA_START) && ((boardid) <= DRV_BOARD_ID_FPGA_END))

/* 芯片类型获取,支持内核态和用户态，支持host和device获取，
  host时在设备枚举前获取到的是CHIP_TYPE_UNKNOW类型 */
DRV_CHIP_TYPE_E drv_get_chip_type(unsigned int devid);

/* 获取当前是否是MDC模式 */
unsigned int drv_is_mdc_mode(void);

/* 获取run包的安装目录 */
int drv_get_install_path(char *install_path, int str_len);

/* 获取当前运行模式 */
DRV_RUN_MODE_E drv_get_run_mode(void);

/* 获取当前是host还是device */
unsigned int drv_get_host_device_env(void);

#endif
