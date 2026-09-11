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

#include "soc_resmng_log.h"
#include "soc_subsys_ts.h"

int subsys_ts_set_rsv_mem(struct soc_resmng_ts *ts_resmng, const char *name, struct soc_rsv_mem_info *rsv_mem)
{
    struct soc_rsv_mem *mem = NULL;

    ka_task_mutex_lock(&ts_resmng->mutex);
    mem = rsv_mem_node_find(name, &ts_resmng->rsv_mems_head);
    if (mem != NULL) {
        soc_res_name_copy(mem->name, name);
        mem->info = *rsv_mem;
        ka_task_mutex_unlock(&ts_resmng->mutex);
        return 0;
    }

    mem = ka_mm_kzalloc(sizeof(*mem), KA_GFP_KERNEL);
    if (mem == NULL) {
        ka_task_mutex_unlock(&ts_resmng->mutex);
        return -ENOSPC;
    }

    soc_res_name_copy(mem->name, name);
    mem->info = *rsv_mem;

    ka_list_add(&mem->list_node, &ts_resmng->rsv_mems_head);
    ka_task_mutex_unlock(&ts_resmng->mutex);

    return 0;
}

int subsys_ts_get_rsv_mem(struct soc_resmng_ts *ts_resmng, const char *name, struct soc_rsv_mem_info *rsv_mem)
{
    struct soc_rsv_mem *mem = NULL;

    ka_task_mutex_lock(&ts_resmng->mutex);
    mem = rsv_mem_node_find(name, &ts_resmng->rsv_mems_head);
    if (mem == NULL) {
        ka_task_mutex_unlock(&ts_resmng->mutex);
        return -ENOENT;
    }

    *rsv_mem = mem->info;
    ka_task_mutex_unlock(&ts_resmng->mutex);

    return 0;
}

int subsys_ts_set_reg_base(struct soc_resmng_ts *ts_resmng, const char *name,
    struct soc_reg_base_info *io_base)
{
    struct soc_reg_base *reg = NULL;

    ka_task_mutex_lock(&ts_resmng->mutex);
    reg = io_bases_node_find(name, &ts_resmng->io_bases_head);
    if (reg != NULL) {
        soc_res_name_copy(reg->name, name);
        reg->info = *io_base;
        ka_task_mutex_unlock(&ts_resmng->mutex);
        return 0;
    }

    reg = ka_mm_kzalloc(sizeof(*reg), KA_GFP_KERNEL);
    if (reg == NULL) {
        ka_task_mutex_unlock(&ts_resmng->mutex);
        return -ENOSPC;
    }

    soc_res_name_copy(reg->name, name);
    reg->info = *io_base;

    ka_list_add(&reg->list_node, &ts_resmng->io_bases_head);
    ka_task_mutex_unlock(&ts_resmng->mutex);

    return 0;
}

int subsys_ts_get_reg_base(struct soc_resmng_ts *ts_resmng, const char *name,
    struct soc_reg_base_info *io_base)
{
    struct soc_reg_base *reg = NULL;

    ka_task_mutex_lock(&ts_resmng->mutex);
    reg = io_bases_node_find(name, &ts_resmng->io_bases_head);
    if (reg == NULL) {
        ka_task_mutex_unlock(&ts_resmng->mutex);
        return -ENOENT;
    }

    *io_base = reg->info;
    ka_task_mutex_unlock(&ts_resmng->mutex);

    return 0;
}

int subsys_ts_set_irq_num(struct soc_resmng_ts *ts_resmng, u32 irq_type, u32 irq_num)
{
    struct soc_irq_info *info = NULL;
    int ret;

    if (irq_type >= TS_IRQ_TYPE_MAX) {
        soc_err("Param is illegal. (type=%u)\n", irq_type);
        return -EINVAL;
    }

    info = &ts_resmng->irq_infos[irq_type];

    ka_task_mutex_lock(&ts_resmng->mutex);
    if (info->irqs != NULL) {
        resmng_irqs_destroy(info);
    }

    ret = resmng_irqs_create(info, irq_num);
    if (ret != 0) {
        ka_task_mutex_unlock(&ts_resmng->mutex);
        soc_err("Irq_base create failed. (irq_num=%u)\n", irq_num);
        return ret;
    }

    info->irq_num = irq_num;
    ka_task_mutex_unlock(&ts_resmng->mutex);

    return 0;
}

int subsys_ts_get_irq_num(struct soc_resmng_ts *ts_resmng, u32 irq_type, u32 *irq_num)
{
    struct soc_irq_info *info = NULL;

    if (irq_type >= TS_IRQ_TYPE_MAX) {
        soc_err("Param is illegal. (type=%u)\n", irq_type);
        return -EINVAL;
    }

    info = &ts_resmng->irq_infos[irq_type];

    ka_task_mutex_lock(&ts_resmng->mutex);
    if (info->irqs == NULL) {
        ka_task_mutex_unlock(&ts_resmng->mutex);
        return -ENOENT;
    }

    *irq_num = info->irq_num;
    ka_task_mutex_unlock(&ts_resmng->mutex);

    return 0;
}

int subsys_ts_set_irq_by_index(struct soc_resmng_ts *ts_resmng, u32 irq_type, u32 index, u32 irq)
{
    struct soc_irq_info *info = NULL;

    if (irq_type >= TS_IRQ_TYPE_MAX) {
        soc_err("Param is illegal. (type=%u)\n", irq_type);
        return -EINVAL;
    }

    info = &ts_resmng->irq_infos[irq_type];

    ka_task_mutex_lock(&ts_resmng->mutex);
    if (index >= info->irq_num) {
        ka_task_mutex_unlock(&ts_resmng->mutex);
        soc_err("Index is illegal. (index=%u; irq_num=%u; irq_type=%u)\n", index, info->irq_num, irq_type);
        return -EINVAL;
    }

    info->irqs[index].irq = irq;
    ka_task_mutex_unlock(&ts_resmng->mutex);

    return 0;
}

int subsys_ts_get_irq_by_index(struct soc_resmng_ts *ts_resmng, u32 irq_type, u32 index, u32 *irq)
{
    struct soc_irq_info *info = NULL;

    if (irq_type >= TS_IRQ_TYPE_MAX) {
        soc_err("Param is illegal. (type=%u)\n", irq_type);
        return -EINVAL;
    }

    info = &ts_resmng->irq_infos[irq_type];

    ka_task_mutex_lock(&ts_resmng->mutex);
    if (index >= info->irq_num) {
        ka_task_mutex_unlock(&ts_resmng->mutex);
        soc_err("Index is illegal. (index=%u; irq_num=%u; irq_type=%u)\n", index, info->irq_num, irq_type);
        return -EINVAL;
    }

    if (info->irqs[index].irq == SOC_IRQ_INVALID_VALUE) {
        ka_task_mutex_unlock(&ts_resmng->mutex);
        return -ENOENT;
    }

    *irq = info->irqs[index].irq;
    ka_task_mutex_unlock(&ts_resmng->mutex);

    return 0;
}

int subsys_ts_set_irq(struct soc_resmng_ts *ts_resmng, u32 irq_type, u32 irq)
{
    struct soc_irq_info *info = NULL;

    if (irq_type >= TS_IRQ_TYPE_MAX) {
        return -EINVAL;
    }

    info = &ts_resmng->irq_infos[irq_type];

    ka_task_mutex_lock(&ts_resmng->mutex);
    info->irq = irq;
    info->valid = true;
    ka_task_mutex_unlock(&ts_resmng->mutex);

    return 0;
}

int subsys_ts_get_irq(struct soc_resmng_ts *ts_resmng, u32 irq_type, u32 *irq)
{
    struct soc_irq_info *info = NULL;

    if (irq_type >= TS_IRQ_TYPE_MAX) {
        soc_err("Param is illegal. (type=%u)\n", irq_type);
        return -EINVAL;
    }

    info = &ts_resmng->irq_infos[irq_type];

    ka_task_mutex_lock(&ts_resmng->mutex);
    if (info->valid == false) {
        ka_task_mutex_unlock(&ts_resmng->mutex);
        return -ENOENT;
    }

    *irq = info->irq;
    ka_task_mutex_unlock(&ts_resmng->mutex);

    return 0;
}

int subsys_ts_set_hwirq(struct soc_resmng_ts *ts_resmng, u32 irq_type, u32 irq, u32 hwirq)
{
    struct soc_irq_info *info = NULL;
    int index;

    if (irq_type >= TS_IRQ_TYPE_MAX) {
        return -EINVAL;
    }

    info = &ts_resmng->irq_infos[irq_type];

    ka_task_mutex_lock(&ts_resmng->mutex);
    index = find_irq_index(info, irq);
    if (index == info->irq_num) {
        ka_task_mutex_unlock(&ts_resmng->mutex);
        soc_err("No such irq. (type=%u; irq=%u)\n", irq_type, irq);
        return -EINVAL;
    }

    if (info->irqs[index].hwirq != SOC_IRQ_INVALID_VALUE) {
        ka_task_mutex_unlock(&ts_resmng->mutex);
        soc_err("Repeated set. (type=%u; irq=%u)\n", irq_type, irq);
        return -EEXIST;
    }

    info->irqs[index].hwirq = hwirq;
    ka_task_mutex_unlock(&ts_resmng->mutex);

    return 0;
}

int subsys_ts_get_hwirq(struct soc_resmng_ts *ts_resmng, u32 irq_type, u32 irq, u32 *hwirq)
{
    struct soc_irq_info *info = NULL;
    int index;

    if (irq_type >= TS_IRQ_TYPE_MAX) {
        soc_err("Param is illegal. (type=%u)\n", irq_type);
        return -EINVAL;
    }

    info = &ts_resmng->irq_infos[irq_type];

    ka_task_mutex_lock(&ts_resmng->mutex);
    index = find_irq_index(info, irq);
    if (index == info->irq_num) {
        ka_task_mutex_unlock(&ts_resmng->mutex);
        soc_err("No such irq. (type=%u; irq=%u)\n", irq_type, irq);
        return -EINVAL;
    }

    if (info->irqs[index].hwirq == SOC_IRQ_INVALID_VALUE) {
        ka_task_mutex_unlock(&ts_resmng->mutex);
        return -ENOENT;
    }

    *hwirq = info->irqs[index].hwirq;
    ka_task_mutex_unlock(&ts_resmng->mutex);

    return 0;
}

int subsys_ts_set_tscpu_to_taishan_irq(struct soc_resmng_ts *ts_resmng, u32 irq_type, u32 irq, u32 tscpu_to_taishan_irq)
{
    struct soc_irq_info *info = NULL;
    int index;

    if (irq_type >= TS_IRQ_TYPE_MAX) {
        return -EINVAL;
    }

    info = &ts_resmng->irq_infos[irq_type];

    ka_task_mutex_lock(&ts_resmng->mutex);
    index = find_irq_index(info, irq);
    if (index == info->irq_num) {
        ka_task_mutex_unlock(&ts_resmng->mutex);
        soc_err("No such irq. (type=%u; irq=%u)\n", irq_type, irq);
        return -EINVAL;
    }

    if (info->irqs[index].tscpu_to_taishan_irq != SOC_IRQ_INVALID_VALUE) {
        ka_task_mutex_unlock(&ts_resmng->mutex);
        soc_err("Repeated set. (type=%u; irq=%u)\n", irq_type, irq);
        return -EEXIST;
    }

    info->irqs[index].tscpu_to_taishan_irq = tscpu_to_taishan_irq;
    ka_task_mutex_unlock(&ts_resmng->mutex);

    return 0;
}

int subsys_ts_get_tscpu_to_taishan_irq(struct soc_resmng_ts *ts_resmng, u32 irq_type, u32 irq,
    u32 *tscpu_to_taishan_irq)
{
    struct soc_irq_info *info = NULL;
    int index;

    if (irq_type >= TS_IRQ_TYPE_MAX) {
        soc_err("Param is illegal. (type=%u)\n", irq_type);
        return -EINVAL;
    }

    info = &ts_resmng->irq_infos[irq_type];

    ka_task_mutex_lock(&ts_resmng->mutex);
    index = find_irq_index(info, irq);
    if (index == info->irq_num) {
        ka_task_mutex_unlock(&ts_resmng->mutex);
        soc_err("No such irq. (type=%u; irq=%u)\n", irq_type, irq);
        return -EINVAL;
    }

    if (info->irqs[index].tscpu_to_taishan_irq == SOC_IRQ_INVALID_VALUE) {
        ka_task_mutex_unlock(&ts_resmng->mutex);
        return -ENOENT;
    }

    *tscpu_to_taishan_irq = info->irqs[index].tscpu_to_taishan_irq;
    ka_task_mutex_unlock(&ts_resmng->mutex);

    return 0;
}

void subsys_ts_set_ts_status(struct soc_resmng_ts *ts_resmng, u32 status)
{
    ka_base_atomic_set(&ts_resmng->ts_status, status);
}

void subsys_ts_get_ts_status(struct soc_resmng_ts *ts_resmng, u32 *status)
{
    *status = (u32)ka_base_atomic_read(&ts_resmng->ts_status);
}

int subsys_ts_set_mia_res(struct soc_resmng_ts *ts_resmng, u32 type, u64 bitmap, u32 unit_per_bit)
{
    if (type >= MIA_MAX_RES_TYPE) {
        soc_err("Invalid mia res type. (type=%u)\n", type);
        return -EINVAL;
    }

    ka_task_mutex_lock(&ts_resmng->mutex);
    ts_resmng->res_info[type].bitmap = bitmap;
    ts_resmng->res_info[type].unit_per_bit = unit_per_bit;
    ka_task_mutex_unlock(&ts_resmng->mutex);

    return 0;
}

int subsys_ts_get_mia_res(struct soc_resmng_ts *ts_resmng, u32 type, u64 *bitmap, u32 *unit_per_bit)
{
    if (type >= MIA_MAX_RES_TYPE) {
        soc_err("Invalid mia res type. (type=%u)\n", type);
        return -EINVAL;
    }

    ka_task_mutex_lock(&ts_resmng->mutex);
    *bitmap = ts_resmng->res_info[type].bitmap;
    *unit_per_bit = ts_resmng->res_info[type].unit_per_bit;
    ka_task_mutex_unlock(&ts_resmng->mutex);

    return 0;
}

int subsys_ts_set_mia_res_ex(struct soc_resmng_ts *ts_resmng, u32 type, struct soc_mia_res_info_ex *info)
{
    if (type >= MIA_MAX_RES_TYPE) {
        soc_err("Invalid mia res type. (type=%u)\n", type);
        return -EINVAL;
    }

    ka_task_mutex_lock(&ts_resmng->mutex);
    ts_resmng->res_info_ex[type].start = info->start;
    ts_resmng->res_info_ex[type].total_num = info->total_num;
    ts_resmng->res_info_ex[type].bitmap = info->bitmap;
    ts_resmng->res_info_ex[type].unit_per_bit = info->unit_per_bit;
    ka_task_mutex_unlock(&ts_resmng->mutex);

    return 0;
}

int subsys_ts_get_mia_res_ex(struct soc_resmng_ts *ts_resmng, u32 type, struct soc_mia_res_info_ex *info)
{
    if (type >= MIA_MAX_RES_TYPE) {
        return -EINVAL;
    }

    ka_task_mutex_lock(&ts_resmng->mutex);
    info->start = ts_resmng->res_info_ex[type].start;
    info->total_num = ts_resmng->res_info_ex[type].total_num;
    info->bitmap = ts_resmng->res_info_ex[type].bitmap;
    info->unit_per_bit = ts_resmng->res_info_ex[type].unit_per_bit;
    ka_task_mutex_unlock(&ts_resmng->mutex);

    return 0;
}

