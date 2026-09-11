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

#ifndef TSDRV_ID_H
#define TSDRV_ID_H

#include "devdrv_common.h"
#include "tsdrv_kernel_common.h"
#include "ka_task_pub.h"
void tsdrv_dev_id_info_init(void);
int tsdrv_get_ts_id_range(u32 devid, u32 tsid, enum tsdrv_id_type id_type, u32 *min, u32 *max);

/**
 * Obtains the maximum resource ID. If @id_type is invalid, 0 is returned.
 */
u32 tsdrv_get_model_id_max_num(u32 devid, u32 tsid);
u32 tsdrv_get_stream_id_max_num(u32 devid, u32 tsid);
int tsdrv_inc_ts_id_cur_num(u32 devid, u32 tsid, enum tsdrv_id_type id_type, u32 num);
int tsdrv_dec_ts_id_cur_num(u32 devid, u32 tsid, enum tsdrv_id_type id_type, u32 num);
int tsdrv_get_ts_id_cur_num(u32 devid, u32 tsid, enum tsdrv_id_type id_type, u32 *cur_num);
void tsdrv_dev_set_id_range(struct devdrv_info *dev_info, u32 tsnum);
void tsdrv_set_ids_cur_num(u32 devid, u32 tsid);
u32 tsdrv_get_max_id_num(enum tsdrv_id_type id_type);

#endif /* TSDRV_ID_H */

