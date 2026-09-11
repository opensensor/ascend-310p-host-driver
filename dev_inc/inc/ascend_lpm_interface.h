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
 * Create: 2024-04-15
 */

#ifndef ASCEND_LPM_INTERFACE_H
#define ASCEND_LPM_INTERFACE_H
#include <linux/export.h>
#include <linux/types.h>
int devdrv_get_ecc_statistics(unsigned long arg);
int devdrv_query_ddr_statistic_from_lp(unsigned int dev_id, unsigned int type, void *out_msg);
int devdrv_get_freq_from_lp_memory(unsigned int dev_id, unsigned int type, unsigned int *freq);
#endif
