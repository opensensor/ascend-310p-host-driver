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
#ifndef TSDRV_PLAT_DEV_H
#define TSDRV_PLAT_DEV_H

#include <linux/types.h>

void *plat_dev_set_handshake(u32 devid);
void plat_dev_reset_handshake(u32 devid);
int plat_dev_init(u32 devid, void *priv_data);
void *plat_dev_uniniting(u32 devid);
int plat_dev_uninit(u32 devid);
void plat_dev_setup(void);
void plat_dev_cleanup(void);
void *plat_get_priv(u32 devid);

#endif /* __TSDRV_PLAT_DEV_H */
