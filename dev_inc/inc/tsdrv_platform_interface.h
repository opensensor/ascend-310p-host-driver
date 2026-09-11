/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2023. All rights reserved.
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
 * Create: 2021-3-1
 */

#ifndef DRV_TSDRV_PLATFORM_INTERFACE_H
#define DRV_TSDRV_PLATFORM_INTERFACE_H

int devdrv_get_stars_irq_base(u32 dev_id, int *irq);
int devdrv_get_stars_reg_base(u32 dev_id, u64 *base_addr);
int devdrv_get_tscpu_irq_info(u32 dev_id, u32 *start_irq, u32 *irq_num);

#endif

