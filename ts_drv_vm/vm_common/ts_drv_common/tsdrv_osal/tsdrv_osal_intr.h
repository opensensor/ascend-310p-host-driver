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

#ifndef TSDRV_INTR_H
#define TSDRV_INTR_H
#ifndef AOS_LLVM_BUILD
#else
#include <linux/types.h>
#endif

#include "ka_base_pub.h"
#include "ka_system_pub.h"

int tsdrv_request_irq(u32 dev_id, int vector,
    ka_irqreturn_t (*callback_func)(int, void *), void *para, const char *name);
int tsdrv_unrequest_irq(u32 dev_id, int vector, void *para);
void tsdrv_irq_set_affinity_hint(u32 devid, int vector, u32 first_cpu_index, u32 last_cpu_index);
#ifdef CFG_SOC_PLATFORM_HELPER
int tsdrv_get_cpu_index_range(u32 devid, u32 *first_ccpu, u32 *last_ccpu);
#endif


#endif
