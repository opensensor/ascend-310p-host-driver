/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
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
 * Create: 2024-07-31
 */
 
#ifndef LOG_DRV_AGENT_MEM_ALLOC_INTERFACE_H
#define LOG_DRV_AGENT_MEM_ALLOC_INTERFACE_H

#ifndef LOG_UT
#include "ka_memory.h"
#include "ascend_hal_define.h"

#define log_drv_vzalloc(size)  ka_vzalloc(size, ka_get_module_id(HAL_MODULE_TYPE_LOG, KA_SUB_MODULE_TYPE_0))
#define log_drv_vfree(addr) ka_vfree(addr, ka_get_module_id(HAL_MODULE_TYPE_LOG, KA_SUB_MODULE_TYPE_0))
#else
#define log_drv_vzalloc(size)  ka_vzalloc(size)
#define log_drv_vfree(addr) ka_vfree(addr)
#endif

#endif