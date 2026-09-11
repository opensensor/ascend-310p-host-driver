/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
* Create: 2025-08-06
*/

#ifndef SVM_FAULT_REPORT_H
#define SVM_FAULT_REPORT_H

#include "ka_system_pub.h"

typedef int (*fault_report_handle)(u32 devid);
static inline void svm_kernel_soft_fault_report(u32 devid)
{
    fault_report_handle handle = (fault_report_handle)(uintptr_t)__ka_system_symbol_get("hal_kernel_drv_soft_fault_report");
    if (handle != NULL) {
        handle(devid);
        __ka_system_symbol_put("hal_kernel_drv_soft_fault_report");
    }
}
#endif

