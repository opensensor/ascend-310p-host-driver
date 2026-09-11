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
* Create: 2022-7-18
*/

#ifndef SUBSYS_TS_H__
#define SUBSYS_TS_H__
#include <linux/types.h>

#include "soc_resmng.h"

struct soc_resmng_ts {
    ka_mutex_t mutex;

    ka_atomic_t ts_status;
    ka_list_head_t io_bases_head;
    ka_list_head_t rsv_mems_head;
    struct soc_irq_info irq_infos[TS_IRQ_TYPE_MAX];
    struct soc_mia_res_info res_info[MIA_MAX_RES_TYPE];
    struct soc_mia_res_info_ex res_info_ex[MIA_MAX_RES_TYPE];
};

int subsys_ts_set_rsv_mem(struct soc_resmng_ts *ts_resmng, const char *name, struct soc_rsv_mem_info *rsv_mem);
int subsys_ts_get_rsv_mem(struct soc_resmng_ts *ts_resmng, const char *name, struct soc_rsv_mem_info *rsv_mem);

int subsys_ts_set_reg_base(struct soc_resmng_ts *ts_resmng, const char *name, struct soc_reg_base_info *io_base);
int subsys_ts_get_reg_base(struct soc_resmng_ts *ts_resmng, const char *name, struct soc_reg_base_info *io_base);

int subsys_ts_set_irq_num(struct soc_resmng_ts *ts_resmng, u32 irq_type, u32 irq_num);
int subsys_ts_get_irq_num(struct soc_resmng_ts *ts_resmng, u32 irq_type, u32 *irq_num);

int subsys_ts_set_irq_by_index(struct soc_resmng_ts *ts_resmng, u32 irq_type, u32 index, u32 irq);
int subsys_ts_get_irq_by_index(struct soc_resmng_ts *ts_resmng, u32 irq_type, u32 index, u32 *irq);

int subsys_ts_set_irq(struct soc_resmng_ts *ts_resmng, u32 irq_type, u32 irq);
int subsys_ts_get_irq(struct soc_resmng_ts *ts_resmng, u32 irq_type, u32 *irq);

int subsys_ts_set_tscpu_to_taishan_irq(struct soc_resmng_ts *ts_resmng, u32 irq_type, u32 irq,
    u32 tscpu_to_taishan_irq);
int subsys_ts_get_tscpu_to_taishan_irq(struct soc_resmng_ts *ts_resmng, u32 irq_type, u32 irq,
    u32 *tscpu_to_taishan_irq);

int subsys_ts_set_hwirq(struct soc_resmng_ts *ts_resmng, u32 irq_type, u32 irq, u32 hwirq);
int subsys_ts_get_hwirq(struct soc_resmng_ts *ts_resmng, u32 irq_type, u32 irq, u32 *hwirq);

void subsys_ts_set_ts_status(struct soc_resmng_ts *ts_resmng, u32 status);
void subsys_ts_get_ts_status(struct soc_resmng_ts *ts_resmng, u32 *status);

int subsys_ts_set_mia_res(struct soc_resmng_ts *ts_resmng, u32 type, u64 bitmap, u32 unit_per_bit);
int subsys_ts_get_mia_res(struct soc_resmng_ts *ts_resmng, u32 type, u64 *bitmap, u32 *unit_per_bit);
int subsys_ts_set_mia_res_ex(struct soc_resmng_ts *ts_resmng, u32 type, struct soc_mia_res_info_ex *info);
int subsys_ts_get_mia_res_ex(struct soc_resmng_ts *ts_resmng, u32 type, struct soc_mia_res_info_ex *info);

#endif /* SUBSYS_TS_H__ */
