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

#ifndef __DMS_SYSFS_H__
#define __DMS_SYSFS_H__
#include "dms_define.h"
/* dms */
#ifdef AOS_LLVM_BUILD
void dms_procfs_init(void);
void dms_procfs_uninit(void);
#else
void dms_sysfs_init(void);
void dms_sysfs_uninit(void);
#endif
#endif