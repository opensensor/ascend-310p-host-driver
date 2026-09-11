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

#ifndef SOC_RESMNG_H__
#define SOC_RESMNG_H__

#include "ka_memory_pub.h"
#include "ka_base_pub.h"
#include "ka_list_pub.h"
#include "ka_task_pub.h"
#include "ka_kernel_def_pub.h"
#include "ascend_hal_define.h"
#include "soc_res.h"

#define SOC_IRQ_INVALID_VALUE 0xffffffffU

struct soc_reg_base {
    char name[SOC_RESMNG_MAX_NAME_LEN];
    ka_list_head_t list_node;

    struct soc_reg_base_info info;
};

struct soc_rsv_mem {
    char name[SOC_RESMNG_MAX_NAME_LEN];
    ka_list_head_t list_node;

    struct soc_rsv_mem_info info;
};

struct soc_key_data {
    char name[SOC_RESMNG_MAX_NAME_LEN];
    ka_list_head_t list_node;

    u64 value;
};

struct irq_info {
    u32 irq;
    u32 hwirq;
    u32 tscpu_to_taishan_irq;
};

struct soc_irq_info {
    struct irq_info *irqs;
    u32 irq_num;

    bool valid;
    u32 irq;
    u32 hw_irq;
};

static inline void soc_res_name_copy(char *des_name, const char *src_name)
{
    size_t i;

    for (i = 0; i < ka_base_strnlen(src_name, SOC_RESMNG_MAX_NAME_LEN); i++) {
        des_name[i] = src_name[i];
    }
    des_name[i] = '\0';
}

int resmng_irqs_create(struct soc_irq_info *info, u32 irq_num);
void resmng_irqs_destroy(struct soc_irq_info *info);
int find_irq_index(struct soc_irq_info *info, u32 irq);
struct soc_rsv_mem *rsv_mem_node_find(const char *name, ka_list_head_t *rsv_mems_head);
struct soc_reg_base *io_bases_node_find(const char *name, ka_list_head_t *io_bases_head);
int soc_resmng_set_irq(struct res_inst_info *inst, u32 irq_type, u32 irq);
int soc_resmng_get_irq(struct res_inst_info *inst, u32 irq_type, u32 *irq);

#endif /* SOC_RESMNG_H__ */
