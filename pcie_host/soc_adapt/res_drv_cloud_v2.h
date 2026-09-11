/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2021. All rights reserved.
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

#ifndef _RES_DRV_CLOUD_V2_H_
#define _RES_DRV_CLOUD_V2_H_

#include "devdrv_pci.h"

#define DEVDRV_S2S_MAX_CHIP_NUM 8
#define DEVDRV_S2S_MAX_UDEVID_NUM 16
#define DEVDRV_S2S_NON_TRANS_MSG_CHAN_NUM 8 /* host/device must be the same */

/* 1024 bytes for extend, 512 bytes for hisi, and 512 bytes for virtualization */
#define DEVDRV_EXTEND_PARA_ADDR_OFFSET (0x3C9A0)
#define DEVDRV_VIRT_PARA_ADDR_OFFSET (DEVDRV_EXTEND_PARA_ADDR_OFFSET + 512)
#define DEVDRV_VIRT_NUMA_MAGIC (0xFAFAAFAF)
struct devdrv_virt_para {
    int version;
    int numa_node;
};

int devdrv_cloud_v2_res_init(struct devdrv_pci_ctrl *pci_ctrl);

#endif
