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
 * Create: 2024-09-12
 */

#ifndef PCIE_RC_SENTRY_H
#define PCIE_RC_SENTRY_H

#include <linux/types.h>

int drv_pcierc_enter_sentry_proc(void);
int drv_pcierc_exit_sentry_proc(void);

#endif
