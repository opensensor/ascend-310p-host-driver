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
 * Create: 2024-08-01
 */
 
#ifndef XSMEM_DRV_ALLOC_INTERFACE_H
#define XSMEM_DRV_ALLOC_INTERFACE_H
 
#include "ka_memory.h"
#include "ascend_hal_define.h"
#define xsmem_drv_kmalloc(size, flags)  ka_kmalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_BUF_MANAGER, KA_SUB_MODULE_TYPE_0))
#define xsmem_drv_kzalloc(size, flags)  ka_kzalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_BUF_MANAGER, KA_SUB_MODULE_TYPE_0))
#define xsmem_drv_kcalloc(size, flags)  ka_kcalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_BUF_MANAGER, KA_SUB_MODULE_TYPE_0))
#define xsmem_drv_kfree(addr)  ka_kfree(addr, ka_get_module_id(HAL_MODULE_TYPE_BUF_MANAGER, KA_SUB_MODULE_TYPE_0))
 
#define xsmem_drv_kvzalloc(size, flags)  ka_kvzalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_BUF_MANAGER, KA_SUB_MODULE_TYPE_0))
#define xsmem_drv_kvmalloc(size, flags)  ka_kvmalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_BUF_MANAGER, KA_SUB_MODULE_TYPE_0))
#define xsmem_drv_kvfree(addr)  ka_kvfree(addr, ka_get_module_id(HAL_MODULE_TYPE_BUF_MANAGER, KA_SUB_MODULE_TYPE_0))
#endif