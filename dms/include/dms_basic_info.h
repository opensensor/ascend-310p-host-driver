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


#ifndef __DMS_BASIC_INFO_H__
#define __DMS_BASIC_INFO_H__

#include "devdrv_interface.h"

extern int devdrv_manager_container_logical_id_to_physical_id(u32 logical_dev_id, u32 *physical_dev_id, u32 *vfid);
extern int devdrv_get_pcie_id_info(u32 devid, struct devdrv_pcie_id_info *pcie_id_info);

struct boardid_in {
    unsigned int dev_id;
};

struct boardid_out {
    unsigned int board_id;
};
#define DEVDRV_GPIO_NAME "gpio-read"

#define DMS_MODULE_BASIC_INFO  "dms_basic"
INIT_MODULE_FUNC(DMS_MODULE_BASIC_INFO);
EXIT_MODULE_FUNC(DMS_MODULE_BASIC_INFO);
#endif
