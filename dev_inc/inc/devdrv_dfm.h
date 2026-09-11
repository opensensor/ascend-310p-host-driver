/*
 * Copyright (C) 2019-2022. Huawei Technologies Co., Ltd. All rights reserved.
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
#ifndef __DEVDRV_DFM_H__
#define __DEVDRV_DFM_H__

#ifndef UT_TEST
#include "drv_log.h"
#endif
#include "device/bbox_pub_cloud.h"
#include "device/bbox_pub_mini.h"

#define INIT_SUCCESS 0
#define UNINIT_SUCCESS 1

#define DFM_OK 0
#define DFM_ERROR 1

#define DFM_MODULE_ID_DRIVER 1
#define DFM_MODULE_ID_DVPP 10

#define DFM_SUBMODULE_ID_DEVMNG 0  /* 设备管理 */
#define DFM_SUBMODULE_ID_PCIE 1    /* PCIE */
#define DFM_SUBMODULE_ID_USB 2     /* USB */
#define DFM_SUBMODULE_ID_NET 3     /* 网络相关，RGMII/MAC/ETH */
#define DFM_SUBMODULE_ID_AICPU 4   /* AICPU DRV，RGMII/MAC/ETH */
#define DFM_SUBMODULE_ID_APPMOND 5 /* APPMOND */
#define DFM_SUBMODULE_ID_UPGRADE 6 /* UPGRADE */
#define DFM_SUBMODULE_ID_STORAGE 8 /* STORAGE */
#if defined(CFG_SOC_PLATFORM_MINI) && !defined(CFG_SOC_PLATFORM_MINIV2) && !defined(CFG_SOC_PLATFORM_MINIV3)
#define DFM_SUBMODULE_ID_END 6
#else
#define DFM_SUBMODULE_ID_END 9
#endif
#define FW_SYNC_FAIL 0xA2020000U

#define TIME_NS_TO_US 1000

#define DVPP_JPEGD_IRQ_FAIL 0xA6140001U
#define DVPP_JPEGE_IRQ_FAIL 0xA6140002U
#define DVPP_VPC_IRQ_FAIL 0xA6140003U
#define DVPP_PNGD_IRQ_FAIL 0xA6140004U
#define DVPP_VDEC_IRQ_FAIL 0xA6140005U
#define DVPP_VENC_IRQ_FAIL 0xA6140006U
#if defined(CFG_SOC_PLATFORM_MINI) && !defined(CFG_SOC_PLATFORM_MINIV2) && !defined(CFG_SOC_PLATFORM_MINIV3)
#define DVPP_JPEGD_PROC_FAIL 0xA4140007U
#define DVPP_JPEGE_PROC_FAIL 0xA4140008U
#define DVPP_VPC_PROC_FAIL 0xA4140009U
#define DVPP_PNGD_PROC_FAIL 0xA414000AU
#define DVPP_VDEC_PROC_FAIL 0xA414000BU
#define DVPP_VENC_PROC_FAIL 0xA414000CU
#define DVPP_CMD_LIST_PROC_FAIL 0xA414000DU
#define DVPP_BUS_ERROR 0xA614000EU
#define AICPU_DRV_INIT_FAIL_CODE (0xA6024001U)
#define AICPU_DRV_RUN_FAIL_CODE (0xA6024002U)
#else
#define DVPP_JPEGD_PROC_FAIL 0xA2140007U
#define DVPP_JPEGE_PROC_FAIL 0xA2140008U
#define DVPP_VPC_PROC_FAIL 0xA2140009U
#define DVPP_PNGD_PROC_FAIL 0xA214000AU
#define DVPP_VDEC_PROC_FAIL 0xA214000BU
#define DVPP_VENC_PROC_FAIL 0xA214000CU
#define DVPP_CMD_LIST_PROC_FAIL 0xA214000DU
#define DVPP_BUS_ERROR 0xA214000EU
#define AICPU_DRV_INIT_FAIL_CODE (0xA8024001U)
#define AICPU_DRV_RUN_FAIL_CODE (0xA8024002U)
#endif

#define DFM_AGENTDRV_DMA_PROBE_FAIL 0xA8021000U
#define DFM_AGENTDRV_SDIO_PROBE_FAIL 0xA8021001U
#define DFM_AGENTDRV_PM_PROBE_FAIL 0xA8021002U
#define DFM_AGENTDRV_LINKDOWN_FAIL 0xA8021003U

#define DFM_HIGMAC_E_TX_TIMEOUT 0xA6023001U
#define DFM_HIGMAC_E_RX_INCONST 0xA6023002U
#define DFM_HIGMAC_E_TX_INCONST 0xA6023003U
#define DFM_HIGMAC_E_TX_BD_ERROR 0xA6023004U

#define DFM_EXECP_ID_UFS_ERROR 0xA802D004U

#define DFM_EXECP_ID_USB_TRB 0xA6022001U

#define DFM_AGENTDRV_DMA_FAIL 0xA8021004U

#define APPMON_IDLE_RECOVERY 0x94025021U
#define APPMON_IDLE_HEART_BEAT_LOST 0xA4025021U
#define APPMON_IDLE_RAISE_FAIL 0xA6025022U
#define APPMON_SLOGD_RECOVERY 0x94025041U
#define APPMON_SLOGD_HEART_BEAT_LOST 0xA4025041U
#define APPMON_SLOGD_RAISE_FAIL 0xA6025042U
#define APPMON_SKLOGD_RECOVERY 0x94025061U
#define APPMON_SKLOGD_HEART_BEAT_LOST 0xA4025061U
#define APPMON_SKLOGD_RAISE_FAIL 0xA8025062U
#define APPMON_DMP_RECOVERY 0x94025081U
#define APPMON_DMP_HEART_BEAT_LOST 0xA4025081U
#define APPMON_DMP_RAISE_FAIL 0xA8025082U
#define APPMON_TSDAEMON_RECOVERY 0x960250A1U
#define APPMON_TSDAEMON_HEART_BEAT_LOST 0xA60250A1U
#define APPMON_TSDAEMON_RAISE_FAIL 0xA80250A2U
#define APPMON_DEVMM_RECOVERY 0x960250C1U
#define APPMON_DEVMM_HEART_BEAT_LOST 0xA60250C1U
#define APPMON_DEVMM_RAISE_FAIL 0xA80250C2U
#define APPMON_LOG_RECOVERY 0x940250E1U
#define APPMON_LOG_HEART_BEAT_LOST 0xA40250E1U
#define APPMON_LOG_RAISE_FAIL 0xA60250E2U
#define APPMON_HDCD_RECOVERY 0x94025101U
#define APPMON_HDCD_HEART_BEAT_LOST 0xA4025101U
#define APPMON_HDCD_RAISE_FAIL 0xA8025102U

#define ECC_EXCEED_16_WARNING   0xA4028801U
#define ECC_REACH_64_WARNING    0xA8028801U
#define ECC_APPEAR_WARNING      0xA8028802U

#define DFM_MAX_DAVINCI_NUM 4


// dvpp recover excep id

#define CLR_EXCEP_TYPE 0xCFFFFFFFU
#define SET_RECOVER_TYPE 0x10000000U


#define DVPP_JPEGD_PROC_RECOVER_ID ((DVPP_JPEGD_PROC_FAIL & CLR_EXCEP_TYPE) | SET_RECOVER_TYPE)
#define DVPP_JPEGE_PROC_RECOVER_ID ((DVPP_JPEGE_PROC_FAIL & CLR_EXCEP_TYPE) | SET_RECOVER_TYPE)
#define DVPP_VPC_PROC_RECOVER_ID ((DVPP_VPC_PROC_FAIL & CLR_EXCEP_TYPE) | SET_RECOVER_TYPE)
#define DVPP_PNGD_PROC_RECOVER_ID ((DVPP_PNGD_PROC_FAIL & CLR_EXCEP_TYPE) | SET_RECOVER_TYPE)
#define DVPP_VDEC_PROC_RECOVER_ID ((DVPP_VDEC_PROC_FAIL & CLR_EXCEP_TYPE) | SET_RECOVER_TYPE)
#define DVPP_VENC_PROC_RECOVER_ID ((DVPP_VENC_PROC_FAIL & CLR_EXCEP_TYPE) | SET_RECOVER_TYPE)
#define DVPP_CMD_LIST_PROC_RECOVER_ID ((DVPP_CMD_LIST_PROC_FAIL & CLR_EXCEP_TYPE) | SET_RECOVER_TYPE)

void dfm_system_error_report_ex(unsigned int dev_id, unsigned int excep_id, unsigned int arg);

#if (defined (CFG_SOC_PLATFORM_MINI) && !defined(CFG_SOC_PLATFORM_MINIV2) && !defined(CFG_SOC_PLATFORM_MINIV3) && \
    !defined(DEVMNG_UT) && !defined(LOG_UT)) || defined (DVPP_310_UTEST)
typedef void (*dfm_dump)(unsigned int excep_id, unsigned int etype, unsigned int module_id, char **black_box_info);
typedef void (*dfm_dump_ex)(unsigned int excep_id, unsigned int etype, unsigned int module_id,
    char **black_box_info);
int dfm_write_black_box(unsigned int module_id, unsigned int sub_module_id, const char *black_box_info);
void dfm_system_error_report(unsigned int excep_id, unsigned int arg);
#else
typedef void (*dfm_dump)(u32 dev_id, u32 excep_id, u32 etype, u32 module_id, char **black_box_info);
typedef void (*dfm_dump_done)(u32 dev_id, u32 excep_id, u32 etype, u32 module_id);
typedef void (*dfm_dump_ex)(unsigned int dev_id, unsigned int excep_id, unsigned int etype, unsigned int module_id,
    char **black_box_info);
int dfm_stub_print(u32 dev_id, u32 module_id, u32 type);
int dfm_write_black_box(u32 dev_id, u32 module_id, u32 sub_module_id, const char *black_box_info);
void dfm_system_error_report(u32 dev_id, u32 excep_id, u32 arg);
#endif

struct dfm_module_register {
    unsigned int module_id;     /* 模块id */
    unsigned int sub_module_id; /* 子模块id */
    dfm_dump ops_dump;          /* dump函数指针 */
    dfm_dump_ex ops_dump_ex;          /* dump函数指针 */
};

struct dfm_exception_info {
    unsigned int excep_id;         /* 异常id */
    unsigned int module_id;        /* 模块id */
    unsigned int exce_type;        /* 异常类型 */
    unsigned int process_priority; /* 异常级别 RDR_ERR/RDR_WARN/RDR_OTHER */
    unsigned int reboot_priority;  /* 重启优先级 RDR_REBOOT_NOW/RDR_REBOOT_WAIT/RDR_REBOOT_NO */
    char *module_name;             /* 模块名 */
    char *desc;                    /* 异常描述 */
};

#define USB_ETHER_DUMP_BUF_SIZE 512
typedef int (*dfm_ops_write_black_box)(unsigned int module_id, unsigned int sub_module_id, const char *black_box_info);
typedef void (*dfm_ops_system_error_report)(unsigned int excep_id, unsigned int arg);
typedef int (*dfm_ops_unregister_module)(unsigned int module_id, unsigned int sub_module_id);
struct dfm_usb_module_register_info {
    dfm_ops_write_black_box ops_write_black_box;         /* 输入:写黑匣子静态内存 */
    dfm_ops_system_error_report ops_system_error_report; /* 输入:上报异常给RDR */
    dfm_ops_unregister_module ops_unregister_module;     /* 输入:模块去注册 */
    dfm_dump ops_dump;                                   /* 输出:dump接口 */
};
int dfm_usb_module_register(void);
int dfm_register_module(struct dfm_module_register *dfm_module_info);
int dfm_unregister_module(u32 module_id, u32 sub_module_id);
int dfm_register_exception(struct dfm_exception_info *excep, unsigned int num);
int dfm_unregister_exception(struct dfm_exception_info *excep, unsigned int num);

void dfm_reset(unsigned int excep_id, unsigned char etype, unsigned char module_id);

int dfm_init(void);
void dfm_exit(void);

#define module_dfm "dfm"
#ifndef UT_TEST
#define dfm_err(fmt, ...)   drv_err(module_dfm, \
    "<%s:%d:%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__)
#define dfm_warn(fmt, ...)  drv_warn(module_dfm, \
    "<%s:%d:%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__)
#define dfm_inf(fmt, ...)   drv_info(module_dfm, \
    "<%s:%d:%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__)
#define dfm_debug(fmt, ...) drv_pr_debug(module_dfm, \
    "<%s:%d:%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__)
#else
#define dfm_printk(level, module, fmt, ...) \
    (void)printk(level " [%s] [%s %d] " fmt, module, __func__, __LINE__, ##__VA_ARGS__)

#define dfm_err(fmt...) dfm_printk("err", module_dfm, fmt)
#define dfm_warn(fmt...) dfm_printk("warn", module_dfm, fmt)
#define dfm_inf(fmt...) dfm_printk("info", module_dfm, fmt)
#define dfm_debug(fmt...) dfm_printk("debug", module_dfm, fmt)
#endif

#endif
