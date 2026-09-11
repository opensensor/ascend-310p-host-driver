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
 * Create: 2022-5-31
 */
#include "ka_kernel_def_pub.h"
#include "securec.h"
#include "soc_resmng_log.h"
#include "soc_subsys_ts.h"
#include "soc_resmng.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#define SOC_MAX_TS_NUM  2

struct soc_mia_spec_info {
    u32 vfg_num;
    u32 vf_num;
};

struct soc_dev_resmng {
    u32 devid;
    u32 vfgid;  /* ref the mia device belong to which vfg */

    ka_mutex_t mutex;

    ka_list_head_t io_bases_head;
    ka_list_head_t rsv_mems_head;
    ka_list_head_t key_value_head;
    struct soc_irq_info irq_infos[DEV_IRQ_TYPE_MAX];

    struct soc_mia_spec_info spec_info;  /* set in pf */
    struct soc_mia_res_info res_info[MIA_MAX_RES_TYPE];
    struct soc_mia_res_info_ex res_info_ex[MIA_MAX_RES_TYPE];

    /* The subsys is directly placed here for unified management.
    *  The content of the c file is not exposed to other modules and does not generate coupling.
    */
    struct soc_resmng_ts ts_resmng[SOC_MAX_TS_NUM];

    u32 subsys_num[MAX_SOC_SUBSYS_TYPE];
};

#define SOC_MAX_DAVINCI_NUM   2048
struct soc_dev_resmng *soc_resmng[SOC_MAX_DAVINCI_NUM];
ka_mutex_t soc_resmng_mutex;

static int subtype_maxid_trans[MAX_SOC_SUBSYS_TYPE] = {
    [TS_SUBSYS] = SOC_MAX_TS_NUM
};

struct soc_rsv_mem *rsv_mem_node_find(const char *name, ka_list_head_t *rsv_mems_head)
{
    struct soc_rsv_mem *mem = NULL;
    struct soc_rsv_mem *n = NULL;

    ka_list_for_each_entry_safe(mem, n, rsv_mems_head, list_node) {
        if (ka_base_strcmp(mem->name, name) == 0) {
            return mem;
        }
    }

    return NULL;
}

struct soc_reg_base *io_bases_node_find(const char *name, ka_list_head_t *io_bases_head)
{
    struct soc_reg_base *reg = NULL;
    struct soc_reg_base *n = NULL;

    ka_list_for_each_entry_safe(reg, n, io_bases_head, list_node) {
        if (ka_base_strcmp(reg->name, name) == 0) {
            return reg;
        }
    }

    return NULL;
}

static struct soc_key_data *key_data_node_find(const char *name, ka_list_head_t *key_value_head)
{
    struct soc_key_data *info = NULL;

    ka_list_for_each_entry(info, key_value_head, list_node) {
        if (ka_base_strcmp(info->name, name) == 0) {
            return info;
        }
    }
    return NULL;
}

int find_irq_index(struct soc_irq_info *info, u32 irq)
{
    u32 index;

    for (index = 0; index < info->irq_num; index++) {
        if (info->irqs[index].irq == irq) {
            return index;
        }
    }
    return info->irq_num;
}

int resmng_irqs_create(struct soc_irq_info *info, u32 irq_num)
{
    u32 i;

    info->irqs = (struct irq_info *)ka_mm_kzalloc(sizeof(struct irq_info) * irq_num, KA_GFP_KERNEL);
    if (info->irqs == NULL) {
        soc_err("Kzalloc failed.\n");
        return -ENOMEM;
    }

    for (i = 0; i < irq_num; i++) {
        info->irqs[i].irq = SOC_IRQ_INVALID_VALUE;
        info->irqs[i].hwirq = SOC_IRQ_INVALID_VALUE;
        info->irqs[i].tscpu_to_taishan_irq = SOC_IRQ_INVALID_VALUE;
    }

    return 0;
}

void resmng_irqs_destroy(struct soc_irq_info *info)
{
    if (info->irqs != NULL) {
        ka_mm_kfree(info->irqs);
        info->irqs = NULL;
    }
}

static void ts_resmng_create(struct soc_dev_resmng *resmng)
{
    u32 tsid;

    for (tsid = 0; tsid < SOC_MAX_TS_NUM; tsid++) {
        ka_task_mutex_init(&resmng->ts_resmng[tsid].mutex);
        KA_INIT_LIST_HEAD(&resmng->ts_resmng[tsid].io_bases_head);
        KA_INIT_LIST_HEAD(&resmng->ts_resmng[tsid].rsv_mems_head);
    }
}

static void rsv_mem_node_free(ka_list_head_t *rsv_mems_head)
{
    struct soc_rsv_mem *mem = NULL;
    struct soc_rsv_mem *n = NULL;

    ka_list_for_each_entry_safe(mem, n, rsv_mems_head, list_node) {
        ka_list_del(&mem->list_node);
        ka_mm_kfree(mem);
    }
}

static void io_base_node_free(ka_list_head_t *io_bases_head)
{
    struct soc_reg_base *reg = NULL;
    struct soc_reg_base *n = NULL;

    ka_list_for_each_entry_safe(reg, n, io_bases_head, list_node) {
        ka_list_del(&reg->list_node);
        ka_mm_kfree(reg);
    }
}

static void key_value_node_free(ka_list_head_t *key_value_head)
{
    struct soc_key_data *data = NULL;
    struct soc_key_data *n = NULL;

    ka_list_for_each_entry_safe(data, n, key_value_head, list_node) {
        ka_list_del(&data->list_node);
        ka_mm_kfree(data);
    }
}

static void ts_resmng_destroy(struct soc_dev_resmng *resmng)
{
    u32 tsid;
    u32 irq_type;

    for (tsid = 0; tsid < SOC_MAX_TS_NUM; tsid++) {
        for (irq_type = 0; irq_type < TS_IRQ_TYPE_MAX; irq_type++) {
            resmng_irqs_destroy(&resmng->ts_resmng[tsid].irq_infos[irq_type]);
        }
        rsv_mem_node_free(&resmng->ts_resmng[tsid].rsv_mems_head);
        io_base_node_free(&resmng->ts_resmng[tsid].io_bases_head);
        ka_task_mutex_destroy(&resmng->ts_resmng[tsid].mutex);
    }
}

static struct soc_dev_resmng *resmng_create(u32 devid)
{
    struct soc_dev_resmng *resmng = NULL;

    resmng = (struct soc_dev_resmng *)ka_mm_kzalloc(sizeof(struct soc_dev_resmng), KA_GFP_KERNEL);
    if (resmng == NULL) {
        soc_err("Kmalloc failed. (devid=%u)\n", devid);
        return NULL;
    }

    ka_task_mutex_init(&resmng->mutex);
    KA_INIT_LIST_HEAD(&resmng->io_bases_head);
    KA_INIT_LIST_HEAD(&resmng->rsv_mems_head);
    KA_INIT_LIST_HEAD(&resmng->key_value_head);
    resmng->devid = devid;

    ts_resmng_create(resmng);

    return resmng;
}

static void resmng_destroy(struct soc_dev_resmng *resmng)
{
    ts_resmng_destroy(resmng);

    rsv_mem_node_free(&resmng->rsv_mems_head);
    io_base_node_free(&resmng->io_bases_head);
    key_value_node_free(&resmng->key_value_head);

    ka_task_mutex_destroy(&resmng->mutex);

    ka_mm_kfree(resmng);
}

static struct soc_dev_resmng *get_resmng(u32 devid)
{
    struct soc_dev_resmng *resmng = NULL;

    ka_task_mutex_lock(&soc_resmng_mutex);
    if (soc_resmng[devid] == NULL) {
        resmng = resmng_create(devid);
        if (resmng == NULL) {
            ka_task_mutex_unlock(&soc_resmng_mutex);
            return NULL;
        }
        soc_resmng[devid] = resmng;
    }
    ka_task_mutex_unlock(&soc_resmng_mutex);

    return soc_resmng[devid];
}

static int inst_param_check(struct res_inst_info *inst)
{
    if (inst == NULL) {
        soc_err("Param is NULL.\n");
        return -EINVAL;
    }
    if ((inst->devid >= SOC_MAX_DAVINCI_NUM) || (inst->sub_type >= MAX_SOC_SUBSYS_TYPE)) {
        soc_err("Param is illegal. (devid=%u; subtype=%d)\n", inst->devid, (int)inst->sub_type);
        return -EINVAL;
    }
    if (inst->subid >= (u32)subtype_maxid_trans[inst->sub_type]) {
        soc_err("Param is illegal. (devid=%u; subtype=%d; subid=%u; max=%d)\n",
            inst->devid, (int)inst->sub_type, inst->subid, subtype_maxid_trans[inst->sub_type]);
        return -EINVAL;
    }

    return 0;
}

int soc_resmng_set_rsv_mem(struct res_inst_info *inst, const char *name, struct soc_rsv_mem_info *rsv_mem)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return -EINVAL;
    }

    if ((rsv_mem == NULL) || (name == NULL)) {
        soc_err("Param is NULL. (devid=%u)\n", inst->devid);
        return -EINVAL;
    }

    if (ka_base_strnlen(name, SOC_RESMNG_MAX_NAME_LEN) >= SOC_RESMNG_MAX_NAME_LEN) {
        soc_err("Name len is invalid. (devid=%u)\n", inst->devid);
        return -EINVAL;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_set_rsv_mem(&resmng->ts_resmng[inst->subid], name, rsv_mem);
        if (ret != 0) {
            soc_err("Param is illegal. (devid=%u; tsid=%u; name=%s; ret=%d)\n", resmng->devid, inst->subid, name, ret);
        }
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_set_rsv_mem);

int soc_resmng_get_rsv_mem(struct res_inst_info *inst, const char *name, struct soc_rsv_mem_info *rsv_mem)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return -EINVAL;
    }

    if ((rsv_mem == NULL) || (name == NULL)) {
        soc_err("Param is NULL. (devid=%u)\n", inst->devid);
        return -EINVAL;
    }

    if (ka_base_strnlen(name, SOC_RESMNG_MAX_NAME_LEN) >= SOC_RESMNG_MAX_NAME_LEN) {
        soc_err("Name len is invalid. (devid=%u)\n", inst->devid);
        return -EINVAL;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_get_rsv_mem(&resmng->ts_resmng[inst->subid], name, rsv_mem);
        if (ret != 0) {
            soc_debug("Info. (devid=%u; tsid=%u; name=%s; ret=%d)\n", resmng->devid, inst->subid, name, ret);
        }
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_get_rsv_mem);

int soc_resmng_set_reg_base(struct res_inst_info *inst, const char *name,
    struct soc_reg_base_info *io_base)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return -EINVAL;
    }

    if ((io_base == NULL) || (name == NULL)) {
        soc_err("Param is NULL. (devid=%u)\n", inst->devid);
        return -EINVAL;
    }

    if (ka_base_strnlen(name, SOC_RESMNG_MAX_NAME_LEN) >= SOC_RESMNG_MAX_NAME_LEN) {
        soc_err("Name len is invalid. (devid=%u)\n", inst->devid);
        return -EINVAL;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_set_reg_base(&resmng->ts_resmng[inst->subid], name, io_base);
        if (ret != 0) {
            soc_err("Param is illegal. (devid=%u; tsid=%u; name=%s; ret=%d)\n", resmng->devid, inst->subid, name, ret);
        }
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_set_reg_base);

int soc_resmng_get_reg_base(struct res_inst_info *inst, const char *name,
    struct soc_reg_base_info *io_base)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return -EINVAL;
    }

    if ((io_base == NULL) || (name == NULL)) {
        soc_err("Param is NULL. (devid=%u)\n", inst->devid);
        return -EINVAL;
    }

    if (ka_base_strnlen(name, SOC_RESMNG_MAX_NAME_LEN) >= SOC_RESMNG_MAX_NAME_LEN) {
        soc_err("Name len is invalid. (devid=%u)\n", inst->devid);
        return -EINVAL;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_get_reg_base(&resmng->ts_resmng[inst->subid], name, io_base);
        if (ret != 0) {
            soc_debug("Info. (devid=%u; tsid=%u; name=%s; ret=%d)\n", resmng->devid, inst->subid, name, ret);
        }
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_get_reg_base);

#define SOC_IRQ_NUM_MAX  64
int soc_resmng_set_irq_num(struct res_inst_info *inst, u32 irq_type, u32 irq_num)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return -EINVAL;
    }

    if (irq_num >= SOC_IRQ_NUM_MAX) {
        soc_err("Param is illegal. (devid=%u; type=%u; num=%u)\n", inst->devid, irq_type, irq_num);
        return -EINVAL;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_set_irq_num(&resmng->ts_resmng[inst->subid], irq_type, irq_num);
        if (ret != 0) {
            soc_err("Fail check. (devid=%u; tsid=%u; type=%u; num=%u; ret=%d)\n",
                inst->devid, inst->subid, irq_type, irq_num, ret);
            return ret;
        }
    }
    soc_info("Set success. (irq_num=%u; devid=%u; tsid=%u; type=%u)\n", irq_num, inst->devid, inst->subid, irq_type);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_set_irq_num);

int soc_resmng_get_irq_num(struct res_inst_info *inst, u32 irq_type, u32 *irq_num)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return -EINVAL;
    }

    if (irq_num == NULL) {
        soc_err("Param is NULL. (devid=%u; type=%u)\n", inst->devid, irq_type);
        return -EINVAL;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_get_irq_num(&resmng->ts_resmng[inst->subid], irq_type, irq_num);
        if ((ret != 0) && (ret != -ENOENT)) {
            soc_err("Fail check. (devid=%u; tsid=%u; type=%u; ret=%d)\n", inst->devid, inst->subid, irq_type, ret);
        }
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_get_irq_num);

int soc_resmng_set_irq_by_index(struct res_inst_info *inst, u32 irq_type, u32 index, u32 irq)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return ret;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_set_irq_by_index(&resmng->ts_resmng[inst->subid], irq_type, index, irq);
        if (ret != 0) {
            soc_err("Fail check. (devid=%u; tsid=%u; type=%u; index=%u; ret=%d)\n",
                resmng->devid, inst->subid, irq_type, index, ret);
        }
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_set_irq_by_index);

int soc_resmng_get_irq_by_index(struct res_inst_info *inst, u32 irq_type, u32 index, u32 *irq)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return -EINVAL;
    }

    if (irq == NULL) {
        soc_err("Param is NULL. (devid=%u, irq_type=%u)\n", inst->devid, irq_type);
        return -EINVAL;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_get_irq_by_index(&resmng->ts_resmng[inst->subid], irq_type, index, irq);
        if ((ret != 0) && (ret != -ENOENT)) {
            soc_err("Fail check. (devid=%u; tsid=%u; type=%u; index=%u; ret=%d)\n",
                resmng->devid, inst->subid, irq_type, index, ret);
        }
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_get_irq_by_index);

int soc_resmng_set_irq(struct res_inst_info *inst, u32 irq_type, u32 irq)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return ret;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_set_irq(&resmng->ts_resmng[inst->subid], irq_type, irq);
        if (ret != 0) {
            soc_err("Need set first. (devid=%u; tsid=%u; type=%u; ret=%d)\n",
                resmng->devid, inst->subid, irq_type, ret);
        }
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_set_irq);

int soc_resmng_get_irq(struct res_inst_info *inst, u32 irq_type, u32 *irq)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return -EINVAL;
    }

    if (irq == NULL) {
        soc_err("Param is NULL. (devid=%u; type=%u)\n", inst->devid, irq_type);
        return -EINVAL;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_get_irq(&resmng->ts_resmng[inst->subid], irq_type, irq);
        if ((ret != 0) && (ret != -ENOENT)) {
            soc_err("Set first. (devid=%u; tsid=%u; type=%u; ret=%d)\n", resmng->devid, inst->subid, irq_type, ret);
        }
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_get_irq);

int soc_resmng_set_hwirq(struct res_inst_info *inst, u32 irq_type, u32 irq, u32 hwirq)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return ret;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_set_hwirq(&resmng->ts_resmng[inst->subid], irq_type, irq, hwirq);
        if (ret != 0) {
            soc_err("Set failed. (devid=%u; tsid=%u; type=%u; ret=%d)\n", resmng->devid, inst->subid, irq_type, ret);
        }
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_set_hwirq);

int soc_resmng_get_hwirq(struct res_inst_info *inst, u32 irq_type, u32 irq, u32 *hwirq)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return -EINVAL;
    }

    if (hwirq == NULL) {
        soc_err("Param is NULL. (devid=%u; type=%u)\n", inst->devid, irq_type);
        return -EINVAL;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_get_hwirq(&resmng->ts_resmng[inst->subid], irq_type, irq, hwirq);
        if ((ret != 0) && (ret != -ENOENT)) {
            soc_err("Set first. (devid=%u; tsid=%u; type=%u; ret=%d)\n", resmng->devid, inst->subid, irq_type, ret);
        }
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_get_hwirq);

int soc_resmng_set_tscpu_to_taishan_irq(struct res_inst_info *inst, u32 irq_type, u32 irq, u32 tscpu_to_taishan_irq)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return ret;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_set_tscpu_to_taishan_irq(&resmng->ts_resmng[inst->subid], irq_type, irq, tscpu_to_taishan_irq);
        if (ret != 0) {
            soc_err("Set failed. (devid=%u; tsid=%u; type=%u; ret=%d)\n", resmng->devid, inst->subid, irq_type, ret);
        }
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_set_tscpu_to_taishan_irq);

int soc_resmng_get_tscpu_to_taishan_irq(struct res_inst_info *inst, u32 irq_type, u32 irq, u32 *tscpu_to_taishan_irq)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return -EINVAL;
    }

    if (tscpu_to_taishan_irq == NULL) {
        soc_err("Param is NULL. (devid=%u; type=%u)\n", inst->devid, irq_type);
        return -EINVAL;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_get_tscpu_to_taishan_irq(&resmng->ts_resmng[inst->subid], irq_type, irq, tscpu_to_taishan_irq);
        if ((ret != 0) && (ret != -ENOENT)) {
            soc_err("Set first. (devid=%u; tsid=%u; type=%u; ret=%d)\n", resmng->devid, inst->subid, irq_type, ret);
        }
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_get_tscpu_to_taishan_irq);

static int dev_set_rsv_mem(struct soc_dev_resmng *resmng, const char *name, struct soc_rsv_mem_info *rsv_mem)
{
    struct soc_rsv_mem *rsv_mem_inst = NULL;

    ka_task_mutex_lock(&resmng->mutex);
    rsv_mem_inst = rsv_mem_node_find(name, &resmng->rsv_mems_head);
    if (rsv_mem_inst != NULL) {
        soc_res_name_copy(rsv_mem_inst->name, name);
        rsv_mem_inst->info = *rsv_mem;
        ka_task_mutex_unlock(&resmng->mutex);
        soc_info("Rsv mem set again. (devid=%u)\n", resmng->devid);
        return 0;
    }

    rsv_mem_inst = ka_mm_kzalloc(sizeof(*rsv_mem_inst), KA_GFP_KERNEL);
    if (rsv_mem_inst == NULL) {
        ka_task_mutex_unlock(&resmng->mutex);
        return -ENOSPC;
    }

    soc_res_name_copy(rsv_mem_inst->name, name);
    rsv_mem_inst->info = *rsv_mem;

    ka_list_add(&rsv_mem_inst->list_node, &resmng->io_bases_head);
    ka_task_mutex_unlock(&resmng->mutex);

    return 0;
}

int soc_resmng_dev_set_rsv_mem(u32 devid, const char *name, struct soc_rsv_mem_info *rsv_mem)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    if (devid >= SOC_MAX_DAVINCI_NUM) {
        soc_err("Param is illegal. (devid=%u)\n", devid);
        return -EINVAL;
    }

    if ((rsv_mem == NULL) || (name == NULL)) {
        soc_err("Param is NULL. (devid=%u)\n", devid);
        return -EINVAL;
    }

    if (ka_base_strnlen(name, SOC_RESMNG_MAX_NAME_LEN) >= SOC_RESMNG_MAX_NAME_LEN) {
        soc_err("Name len is invalid. (devid=%u)\n", devid);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ret = dev_set_rsv_mem(resmng, name, rsv_mem);
    if (ret != 0) {
        soc_err("Param is illegal. (devid=%u; name=%s; ret=%d)\n", resmng->devid, name, ret);
    }
    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_set_rsv_mem);

static int dev_get_rsv_mem(struct soc_dev_resmng *resmng, const char *name, struct soc_rsv_mem_info *rsv_mem)
{
    struct soc_rsv_mem *rsv_mem_inst = NULL;

    ka_task_mutex_lock(&resmng->mutex);
    rsv_mem_inst = rsv_mem_node_find(name, &resmng->io_bases_head);
    if (rsv_mem_inst == NULL) {
        ka_task_mutex_unlock(&resmng->mutex);
        return -ENOENT;
    }

    *rsv_mem = rsv_mem_inst->info;
    ka_task_mutex_unlock(&resmng->mutex);

    return 0;
}

int soc_resmng_dev_get_rsv_mem(u32 devid, const char *name, struct soc_rsv_mem_info *rsv_mem)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    if (devid >= SOC_MAX_DAVINCI_NUM) {
        soc_err("Param is illegal. (devid=%u)\n", devid);
        return -EINVAL;
    }

    if ((rsv_mem == NULL) || (name == NULL)) {
        soc_err("Param is NULL. (devid=%u)\n", devid);
        return -EINVAL;
    }

    if (ka_base_strnlen(name, SOC_RESMNG_MAX_NAME_LEN) >= SOC_RESMNG_MAX_NAME_LEN) {
        soc_err("Name len is invalid. (devid=%u)\n", devid);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ret = dev_get_rsv_mem(resmng, name, rsv_mem);
    if (ret != 0) {
        soc_debug("Param is illegal. (devid=%u; name=%s; ret=%d)\n", resmng->devid, name, ret);
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_get_rsv_mem);

static int dev_set_key_value(struct soc_dev_resmng *resmng, const char *name, u64 value)
{
    struct soc_key_data *key_value_list = NULL;

    ka_task_mutex_lock(&resmng->mutex);
    key_value_list = key_data_node_find(name, &resmng->key_value_head);
    if (key_value_list != NULL) {
        key_value_list->value = value;
        ka_task_mutex_unlock(&resmng->mutex);
        soc_info("Normal info set again. (devid=%u)\n", resmng->devid);
        return 0;
    }

    key_value_list = ka_mm_kzalloc(sizeof(*key_value_list), KA_GFP_KERNEL);
    if (key_value_list == NULL) {
        ka_task_mutex_unlock(&resmng->mutex);
        return -ENOSPC;
    }

    soc_res_name_copy(key_value_list->name, name);
    key_value_list->value = value;

    ka_list_add(&key_value_list->list_node, &resmng->key_value_head);
    ka_task_mutex_unlock(&resmng->mutex);

    return 0;
}

int soc_resmng_dev_set_key_value(u32 devid, const char *name, u64 value)
{
    struct soc_dev_resmng *resmng = NULL;
    u32 len;
    int ret;

    if (devid >= SOC_MAX_DAVINCI_NUM) {
        soc_err("Param is illegal. (devid=%u)\n", devid);
        return -EINVAL;
    }

    if (name == NULL) {
        soc_err("Param is NULL. (devid=%u)\n", devid);
        return -EINVAL;
    }

    len = ka_base_strnlen(name, SOC_RESMNG_MAX_NAME_LEN);
    if ((len == (u32)SOC_RESMNG_MAX_NAME_LEN) || (len == 0)) {
        soc_err("Param is invalid. (devid=%u; len=%u)\n", devid, len);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ret = dev_set_key_value(resmng, name, value);
    if (ret != 0) {
        soc_err("Param is illegal. (devid=%u; name=%s; ret=%d)\n", resmng->devid, name, ret);
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_set_key_value);

static int dev_get_key_value(struct soc_dev_resmng *resmng, const char *name, u64 *value)
{
    struct soc_key_data *key_value_list = NULL;

    ka_task_mutex_lock(&resmng->mutex);
    key_value_list = key_data_node_find(name, &resmng->key_value_head);
    if (key_value_list == NULL) {
        ka_task_mutex_unlock(&resmng->mutex);
        return -ENOENT;
    }

    *value = key_value_list->value;
    ka_task_mutex_unlock(&resmng->mutex);

    return 0;
}

int soc_resmng_dev_get_key_value(u32 devid, const char *name, u64 *value)
{
    struct soc_dev_resmng *resmng = NULL;
    u32 len;
    int ret;

    if (devid >= SOC_MAX_DAVINCI_NUM) {
        soc_err("Param is illegal. (devid=%u)\n", devid);
        return -EINVAL;
    }

    if ((value == NULL) || (name == NULL)) {
        soc_err("Param is NULL. (devid=%u)\n", devid);
        return -EINVAL;
    }

    len = ka_base_strnlen(name, SOC_RESMNG_MAX_NAME_LEN);
    if ((len == (u32)SOC_RESMNG_MAX_NAME_LEN) || (len == 0)) {
        soc_err("Param is invalid. (devid=%u; len=%u)\n", devid, len);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ret = dev_get_key_value(resmng, name, value);
    if (ret != 0) {
        soc_err("Param is illegal. (devid=%u; name=%s; ret=%d)\n", resmng->devid, name, ret);
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_get_key_value);

static int dev_set_reg_base(struct soc_dev_resmng *resmng, const char *name, struct soc_reg_base_info *io_base)
{
    struct soc_reg_base *reg = NULL;

    ka_task_mutex_lock(&resmng->mutex);
    reg = io_bases_node_find(name, &resmng->io_bases_head);
    if (reg != NULL) {
        soc_res_name_copy(reg->name, name);
        reg->info = *io_base;
        ka_task_mutex_unlock(&resmng->mutex);
        soc_info("Reg base set again. (devid=%u)\n", resmng->devid);
        return 0;
    }

    reg = ka_mm_kzalloc(sizeof(*reg), KA_GFP_KERNEL);
    if (reg == NULL) {
        ka_task_mutex_unlock(&resmng->mutex);
        return -ENOSPC;
    }

    soc_res_name_copy(reg->name, name);
    reg->info = *io_base;

    ka_list_add(&reg->list_node, &resmng->io_bases_head);
    ka_task_mutex_unlock(&resmng->mutex);

    return 0;
}

int soc_resmng_dev_set_reg_base(u32 devid, const char *name, struct soc_reg_base_info *io_base)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    if (devid >= SOC_MAX_DAVINCI_NUM) {
        soc_err("Param is illegal. (devid=%u)\n", devid);
        return -EINVAL;
    }

    if ((io_base == NULL) || (name == NULL)) {
        soc_err("Param is NULL. (devid=%u)\n", devid);
        return -EINVAL;
    }

    if (ka_base_strnlen(name, SOC_RESMNG_MAX_NAME_LEN) >= SOC_RESMNG_MAX_NAME_LEN) {
        soc_err("Name len is invalid. (devid=%u)\n", devid);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ret = dev_set_reg_base(resmng, name, io_base);
    if (ret != 0) {
        soc_err("Param is illegal. (devid=%u; name=%s; ret=%d)\n", resmng->devid, name, ret);
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_set_reg_base);

static int dev_get_reg_base(struct soc_dev_resmng *resmng, const char *name, struct soc_reg_base_info *io_base)
{
    struct soc_reg_base *reg = NULL;

    ka_task_mutex_lock(&resmng->mutex);
    reg = io_bases_node_find(name, &resmng->io_bases_head);
    if (reg == NULL) {
        ka_task_mutex_unlock(&resmng->mutex);
        return -ENOENT;
    }

    *io_base = reg->info;
    ka_task_mutex_unlock(&resmng->mutex);

    return 0;
}

int soc_resmng_dev_get_reg_base(u32 devid, const char *name, struct soc_reg_base_info *io_base)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    if (devid >= SOC_MAX_DAVINCI_NUM) {
        soc_err("Param is illegal. (devid=%u)\n", devid);
        return -EINVAL;
    }

    if ((io_base == NULL) || (name == NULL)) {
        soc_err("Param is NULL. (devid=%u)\n", devid);
        return -EINVAL;
    }

    if (ka_base_strnlen(name, SOC_RESMNG_MAX_NAME_LEN) >= SOC_RESMNG_MAX_NAME_LEN) {
        soc_err("Name len is invalid. (devid=%u)\n", devid);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ret = dev_get_reg_base(resmng, name, io_base);
    if (ret != 0) {
        soc_debug("Param is illegal. (devid=%u; name=%s; ret=%d)\n", resmng->devid, name, ret);
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_get_reg_base);

int soc_resmng_dev_set_irq_num(u32 devid, u32 irq_type, u32 irq_num)
{
    soc_err("No implement.\n");
    return -EINVAL;
}
int soc_resmng_dev_get_irq_num(u32 devid, u32 irq_type, u32 *irq_num)
{
    soc_err("No implement.\n");
    return -EINVAL;
}
int soc_resmng_dev_set_irq_by_index(u32 devid, u32 irq_type, u32 index, u32 irq)
{
    soc_err("No implement.\n");
    return -EINVAL;
}
int soc_resmng_dev_get_irq_by_index(u32 devid, u32 irq_type, u32 index, u32 *irq)
{
    soc_err("No implement.\n");
    return -EINVAL;
}

int soc_resmng_subsys_set_num(u32 devid, enum soc_sub_type type, u32 subnum)
{
    struct soc_dev_resmng *resmng = NULL;

    if ((devid >= SOC_MAX_DAVINCI_NUM) || (type >= MAX_SOC_SUBSYS_TYPE)) {
        soc_err("Param is illegal. (devid=%u; type=%d)\n", devid, (int)type);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ka_task_mutex_lock(&resmng->mutex);
    resmng->subsys_num[type] = subnum;
    ka_task_mutex_unlock(&resmng->mutex);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_subsys_set_num);

int soc_resmng_subsys_get_num(u32 devid, enum soc_sub_type type, u32 *subnum)
{
    struct soc_dev_resmng *resmng = NULL;

    if ((devid >= SOC_MAX_DAVINCI_NUM) || (type >= MAX_SOC_SUBSYS_TYPE)) {
        soc_err("Param is illegal. (devid=%u; type=%d)\n", devid, (int)type);
        return -EINVAL;
    }

    if (subnum == NULL) {
        soc_err("Param is illegal. (devid=%u; type=%d)\n", devid, (int)type);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ka_task_mutex_lock(&resmng->mutex);
    *subnum = resmng->subsys_num[type];
    ka_task_mutex_unlock(&resmng->mutex);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_subsys_get_num);

int soc_resmng_dev_set_mia_res(u32 devid, enum soc_mia_res_type type, u64 bitmap, u32 unit_per_bit)
{
    struct soc_dev_resmng *resmng = NULL;

    if (devid >= SOC_MAX_DAVINCI_NUM) {
        soc_err("Param is illegal. (devid=%u)\n", devid);
        return -EINVAL;
    }

    if (type >= MIA_MAX_RES_TYPE) {
        soc_err("Invalid res type. (devid=%u; type=%u)\n", devid, type);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ka_task_mutex_lock(&resmng->mutex);
    resmng->res_info[type].bitmap = bitmap;
    resmng->res_info[type].unit_per_bit = unit_per_bit;
    ka_task_mutex_unlock(&resmng->mutex);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_set_mia_res);

int soc_resmng_dev_get_mia_res(u32 devid, enum soc_mia_res_type type, u64 *bitmap, u32 *unit_per_bit)
{
    struct soc_dev_resmng *resmng = NULL;

    if (devid >= SOC_MAX_DAVINCI_NUM) {
        soc_err("Param is illegal. (devid=%u)\n", devid);
        return -EINVAL;
    }

    if ((bitmap == NULL) || (unit_per_bit == NULL) || (type >= MIA_MAX_RES_TYPE)) {
        soc_err("Invalid para or res type. (devid=%u; type=%u)\n", devid, type);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ka_task_mutex_lock(&resmng->mutex);
    *bitmap = resmng->res_info[type].bitmap;
    *unit_per_bit = resmng->res_info[type].unit_per_bit;
    ka_task_mutex_unlock(&resmng->mutex);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_get_mia_res);

int soc_resmng_dev_set_mia_res_ex(u32 devid, enum soc_mia_res_type type, struct soc_mia_res_info_ex *info)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    if (devid >= SOC_MAX_DAVINCI_NUM) {
        soc_err("Param is illegal. (devid=%u)\n", devid);
        return -EINVAL;
    }

    if ((info == NULL) || (type >= MIA_MAX_RES_TYPE)) {
        soc_err("Invalid para or res type. (devid=%u; type=%u)\n", devid, type);
        return -EINVAL;
    }

    /* smooth transition between old and new interfaces, the old interface will be deleted. */
    ret = soc_resmng_dev_set_mia_res(devid, type, info->bitmap, info->unit_per_bit);
    if (ret != 0) {
        return ret;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ka_task_mutex_lock(&resmng->mutex);
    resmng->res_info_ex[type].start = info->start;
    resmng->res_info_ex[type].total_num = info->total_num;
    resmng->res_info_ex[type].bitmap = info->bitmap;
    resmng->res_info_ex[type].unit_per_bit = info->unit_per_bit;
    ka_task_mutex_unlock(&resmng->mutex);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_set_mia_res_ex);

int soc_resmng_dev_get_mia_res_ex(u32 devid, enum soc_mia_res_type type, struct soc_mia_res_info_ex *info)
{
    struct soc_dev_resmng *resmng = NULL;

    if (devid >= SOC_MAX_DAVINCI_NUM) {
        soc_err("Param is illegal. (devid=%u)\n", devid);
        return -EINVAL;
    }

    if ((info == NULL) || (type >= MIA_MAX_RES_TYPE)) {
        soc_err("Invalid para or res type. (devid=%u; type=%u)\n", devid, type);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ka_task_mutex_lock(&resmng->mutex);
    info->start = resmng->res_info_ex[type].start;
    info->total_num = resmng->res_info_ex[type].total_num;
    info->bitmap = resmng->res_info_ex[type].bitmap;
    info->unit_per_bit = resmng->res_info_ex[type].unit_per_bit;
    ka_task_mutex_unlock(&resmng->mutex);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_get_mia_res_ex);

int soc_resmng_dev_set_mia_spec(u32 devid, u32 vfg_num, u32 vf_num)
{
    struct soc_dev_resmng *resmng = NULL;

    if (devid >= SOC_MAX_DAVINCI_NUM) {
        soc_err("Param is illegal. (devid=%u)\n", devid);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ka_task_mutex_lock(&resmng->mutex);
    resmng->spec_info.vfg_num = vfg_num;
    resmng->spec_info.vf_num = vf_num;
    ka_task_mutex_unlock(&resmng->mutex);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_set_mia_spec);

int soc_resmng_dev_get_mia_spec(u32 devid, u32 *vfg_num, u32 *vf_num)
{
    struct soc_dev_resmng *resmng = NULL;

    if (devid >= SOC_MAX_DAVINCI_NUM) {
        soc_err("Param is illegal. (devid=%u)\n", devid);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ka_task_mutex_lock(&resmng->mutex);
    *vfg_num = resmng->spec_info.vfg_num;
    *vf_num = resmng->spec_info.vf_num;
    ka_task_mutex_unlock(&resmng->mutex);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_get_mia_spec);

int soc_resmng_dev_set_mia_info(u32 devid, u32 vfgid)
{
    struct soc_dev_resmng *resmng = NULL;

    if (devid >= SOC_MAX_DAVINCI_NUM) {
        soc_err("Param is illegal. (devid=%u)\n", devid);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ka_task_mutex_lock(&resmng->mutex);
    resmng->vfgid = vfgid;
    ka_task_mutex_unlock(&resmng->mutex);
    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_set_mia_info);

int soc_resmng_dev_get_mia_info(u32 devid, u32 *vfgid)
{
    struct soc_dev_resmng *resmng = NULL;

    if (devid >= SOC_MAX_DAVINCI_NUM) {
        soc_err("Param is illegal. (devid=%u)\n", devid);
        return -EINVAL;
    }

    resmng = get_resmng(devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    ka_task_mutex_lock(&resmng->mutex);
    *vfgid = resmng->vfgid;
    ka_task_mutex_unlock(&resmng->mutex);
    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_dev_get_mia_info);

int soc_resmng_set_mia_res(struct res_inst_info *inst, enum soc_mia_res_type type,
    u64 bitmap, u32 unit_per_bit)
{
    struct soc_dev_resmng *resmng = NULL;
    struct soc_mia_res_info_ex info = {0};
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return -EINVAL;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        info.bitmap = bitmap;
        info.unit_per_bit = unit_per_bit;
        ret = subsys_ts_set_mia_res_ex(&resmng->ts_resmng[inst->subid], type, &info);
        if ((ret != 0) && (ret != -ENOENT)) {
            soc_err("Set first. (devid=%u; tsid=%u; type=%u; ret=%d)\n", resmng->devid, inst->subid, type, ret);
        }
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_set_mia_res);

int soc_resmng_get_mia_res(struct res_inst_info *inst, enum soc_mia_res_type type,
    u64 *bitmap, u32 *unit_per_bit)
{
    struct soc_dev_resmng *resmng = NULL;
    struct soc_mia_res_info_ex info = {0};
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return -EINVAL;
    }

    if ((bitmap == NULL) || (unit_per_bit == NULL)) {
        soc_err("Para is NULL. (devid=%u; type=%u)\n", inst->devid, type);
        return -EINVAL;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_get_mia_res_ex(&resmng->ts_resmng[inst->subid], type, &info);
        if ((ret != 0) && (ret != -ENOENT)) {
            soc_err("Set first. (devid=%u; tsid=%u; type=%u; ret=%d)\n", resmng->devid, inst->subid, type, ret);
        }
        *bitmap = info.bitmap;
        *unit_per_bit = info.unit_per_bit;
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_get_mia_res);

int soc_resmng_set_mia_res_ex(struct res_inst_info *inst, enum soc_mia_res_type type, struct soc_mia_res_info_ex *info)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return -EINVAL;
    }

    if (info == NULL) {
        soc_err("Para is null. (devid=%u; type=%u)\n", inst->devid, type);
        return -EINVAL;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        /* subsys_ts_set_mia_res old interface will be delete */
        ret = subsys_ts_set_mia_res(&resmng->ts_resmng[inst->subid], type, info->bitmap, info->unit_per_bit);
        if (ret != 0) {
            return ret;
        }

        ret = subsys_ts_set_mia_res_ex(&resmng->ts_resmng[inst->subid], type, info);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_set_mia_res_ex);

int soc_resmng_get_mia_res_ex(struct res_inst_info *inst, enum soc_mia_res_type type, struct soc_mia_res_info_ex *info)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return -EINVAL;
    }

    if (info == NULL) {
        soc_err("Para is null. (devid=%u; type=%u)\n", inst->devid, type);
        return -EINVAL;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        ret = subsys_ts_get_mia_res_ex(&resmng->ts_resmng[inst->subid], type, info);
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_get_mia_res_ex);

int soc_resmng_set_ts_status(struct res_inst_info *inst, u32 status)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return ret;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        subsys_ts_set_ts_status(&resmng->ts_resmng[inst->subid], status);
    }

    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_set_ts_status);

int soc_resmng_get_ts_status(struct res_inst_info *inst, u32 *status)
{
    struct soc_dev_resmng *resmng = NULL;
    int ret;

    ret = inst_param_check(inst);
    if (ret != 0) {
        return ret;
    }

    if (status == NULL) {
        soc_err("Param is NULL. (devid=%u)\n", inst->devid);
        return -EINVAL;
    }

    resmng = get_resmng(inst->devid);
    if (resmng == NULL) {
        return -ENOSPC;
    }

    if (inst->sub_type == TS_SUBSYS) {
        subsys_ts_get_ts_status(&resmng->ts_resmng[inst->subid], status);
    }

    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_resmng_get_ts_status);

static  void resmng_init(void)
{
    u32 devid;

    for (devid = 0; devid < SOC_MAX_DAVINCI_NUM; devid++) {
        soc_resmng[devid] = NULL;
    }
}

static void resmng_uninit(void)
{
    u32 devid;

    for (devid = 0; devid < SOC_MAX_DAVINCI_NUM; devid++) {
        if (soc_resmng[devid] != NULL) {
            resmng_destroy(soc_resmng[devid]);
            soc_resmng[devid] = NULL;
        }
    }
}

STATIC int __init resmng_init_module(void)
{
    ka_task_mutex_init(&soc_resmng_mutex);
    resmng_init();
    soc_info("Init_module success\n");

    return 0;
}
ka_module_init(resmng_init_module);

STATIC void __exit resmng_exit_module(void)
{
    resmng_uninit();
    ka_task_mutex_destroy(&soc_resmng_mutex);
    soc_info("Exit_module success\n");
}
ka_module_exit(resmng_exit_module);

KA_MODULE_LICENSE("GPL");
KA_MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
KA_MODULE_DESCRIPTION("soc resmng driver");
