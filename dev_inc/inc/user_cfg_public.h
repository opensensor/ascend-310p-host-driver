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
 *
 * Description:
 * Author: huawei
 * Create: 2019-10-15
 */

#ifndef _DEV_USER_CFG_PUBLIC_H_
#define _DEV_USER_CFG_PUBLIC_H_

#include "config.h"

#define NETWORK_ITEM_NAME_NUM 7
#define CERT_ITEM_NAME_S0 "hccp_certs_eqpt_cb"
#define CERT_ITEM_NAME_S1 "hccp_pri_data_cb"
#define CERT_ITEM_NAME_S2 "hccp_certs_revoc_cb"
#define CERT_ITEM_NAME_S3 "hccp_certs_mng_cb"

#define DIGITAL_ITEM_NAME_S0 "hccn_envelope_mng"
#define DIGITAL_ITEM_NAME_S1 "hccn_envelope_pri_standby"
#define DIGITAL_ITEM_NAME_S2 "hccn_envelope_pri_active"

#define VERIFY_ITEM_NUM 1

#if (defined CFG_SOC_PLATFORM_MDC_V11) || (defined CFG_SOC_PLATFORM_MDC_V51)
    #define DEVDRV_UC_CHIP_MAX 1
#else
#ifdef CFG_SOC_PLATFORM_MINIV2
    #define DEVDRV_UC_CHIP_MAX 2
#else
    #define DEVDRV_UC_CHIP_MAX 4
#endif
#endif

/* p2p user config */
#define SZ_1G 0x40000000
#define SZ_2G 0x80000000

#define P2P_MEM_CONFIG_NEED_RUN       0x50325001
#define P2P_MEM_CONFIG_RUN_SUCCESS    0x50325002
#define P2P_MEM_CONFIG_RUN_FAIL       0x50325004
#define P2P_MEM_CONFIG_SYNC_SUCCESS   0x503250FF

#ifndef UC_CFG_NAME_LEN_MAX
#define UC_CFG_NAME_LEN_MAX                 32
#endif

struct user_cfg_ioctl_para {
    unsigned int dev_id;
    int cmd;
    int cfg_index;
    char name[UC_CFG_NAME_LEN_MAX];
    unsigned int buf_size;
    void *buf;
};

typedef enum {
    CHIP_INFO_SOLO = 0,
    CHIP_INFO_DUAL,
} BOOT_CFG_CHIP_INFO;

/* cpu number configuration struct in flash, total 16 Bytes, part for reserved */
typedef struct uc_cpu_cfg_stru {
    unsigned char ctrl_cpu_num;
    unsigned char data_cpu_num;
    unsigned char ai_cpu_num;
    unsigned char reserved_1;
    unsigned int reserved_2[3]; /* 3 word 12 bytes, totally 16 bytes */
} uc_cpu_cfg_t;


/* Use 'C' as magic number */
#define USER_CFG_CTL_IOC_MAGIC      'C'
#define USER_CFG_FLASH_OP           _IOR(USER_CFG_CTL_IOC_MAGIC, 1, int)
#define USER_CFG_GET_BOOT_CFG       _IOR(USER_CFG_CTL_IOC_MAGIC, 2, int)
#define USER_CFG_GET_DTS_CFG        _IOR(USER_CFG_CTL_IOC_MAGIC, 4, int)

#endif /* _DEV_USER_CFG_PUBLIC_H_ */
