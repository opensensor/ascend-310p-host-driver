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
#ifndef DEVDRV_CONFIG_P2P_H
#define DEVDRV_CONFIG_P2P_H

#include <linux/types.h>
#include "devdrv_config_p2p.h"

void devdrv_config_p2p(void);
void devdrv_deconfig_p2p(void);
void tsdrv_init_taskid_share_memory(u32 devid, u32 tsnum, u32 chipid, u32 dieid);

#endif
