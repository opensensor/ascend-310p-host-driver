/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
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
 * Create: 2022-07-15
 */

#ifndef ESCHED_VF_ADAPT_H
#define ESCHED_VF_ADAPT_H

/* use for esched_vf.c */
int esched_fill_vf_info(struct esched_vf_info *vf_info, struct vmngd_client_instance *instance);
int esched_init_vf_ctx(struct sched_vf_ctx *vf_ctx, struct esched_vf_info *vf_info);
int esched_uninit_vf_ctx(struct sched_vf_ctx *vf_ctx, struct esched_vf_info *vf_info);
int esched_get_pfvf_id_by_devid(u32 dev_id, u32 *chip_id, u32 *vfid);

#endif

