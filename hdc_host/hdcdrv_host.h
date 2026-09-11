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

#ifndef _HDCDRV_HOST_H_
#define _HDCDRV_HOST_H_

#include "hdcdrv_adapter.h"

#define PCI_VENDOR_ID_HUAWEI 0x19e5
#define DEVDRV_TRANS_CHAN_TYPE 2U /* trans msg chan type: normal or fast */
#ifdef CFG_FEATURE_HDC_REG_MEM
#define HDCDRV_HOTRESET_CHECK_MAX_CNT 125
#define HDCDRV_HOTRESET_CHECK_PCIE_STAT_CNT 50
#else
#define HDCDRV_HOTRESET_CHECK_MAX_CNT 500
#endif
#define HDCDRV_HOTRESET_CHECK_DELAY_MS 40

typedef int (*container_virtual_to_physical_devid)(u32, u32 *, u32 *);
typedef int (*devdrv_manager_get_run_env)(ka_mnt_namespace_t *mnt_ns);
typedef int (*hdcdrv_is_in_container)(void);
typedef int (*get_container_id)(u32  *docker_id);

int devdrv_manager_container_logical_id_to_physical_id(u32 logical_dev_id, u32 *physical_dev_id, u32 *vfid);
#ifdef CFG_FEATURE_NOT_SUPPORT_UDA
int devdrv_manager_container_get_docker_id(u32 *docker_id);
int devdrv_manager_container_is_in_container(void);
#endif

#endif
