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

#ifndef DEVDRV_ID_CONFIG_DFX_H
#define DEVDRV_ID_CONFIG_DFX_H

#include <linux/types.h>
#include "tsdrv_dev_dfx.h"
#include "ka_task_pub.h"
#ifndef AOS_LLVM_BUILD
int id_config_dfx_create(u32 devid, u32 fid, struct dentry *parent);
#else
int id_config_dfx_create(u32 devid, u32 fid, ka_proc_dir_entry_t *parent);
#endif
#endif /* DEVDRV_ID_CONFIG_DFX_H */
