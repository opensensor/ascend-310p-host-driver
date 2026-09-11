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

#include "securec.h"

#include "dbl/feature_loader.h"
#include "esched.h"
#include "esched_fops.h"
#include "esched_table.h"
#include "esched_table_proc_fs.h"
#include "event_sched_inner.h"
#include "ka_ioctl_pub.h"

u32 sched_dev_num;

static int esched_send_event_check(u32 dev_id, struct esched_table_op_send_event *event_para)
{
    if (event_para->dev_id != dev_id) {
        sched_err("Invalid para. (dev_id=%u; event_dev_id=%u)\n", dev_id, event_para->dev_id);
        return DRV_ERROR_PARA_ERROR;
    }

    if ((event_para->gid >= SCHED_MAX_GRP_NUM) || (event_para->event_id >= SCHED_MAX_EVENT_TYPE_NUM)) {
        sched_err("Invalid para. (gid=%u; event_id=%u)\n", event_para->gid, event_para->event_id);
        return DRV_ERROR_PARA_ERROR;
    }

    if ((event_para->dst_engine >= DST_ENGINE_MAX) || (event_para->policy >= POLICY_MAX)) {
        sched_err("Invalid para. (dst_engine=%u; policy=%u)\n", event_para->dst_engine, event_para->policy);
        return DRV_ERROR_PARA_ERROR;
    }

    switch (event_para->data_src) {
        case ESCHED_DATA_SRC_NONE:
            break;
        case ESCHED_DATA_SRC_RAW_DATA:
            break;
        case ESCHED_DATA_SRC_KEY:
            break;
        case ESCHED_DATA_SRC_USR_CFG:
            if ((event_para->data_len > ESCHED_USR_CFG_DATA_MAX_LEN) || (event_para->data_len == 0)) {
                sched_err("Invalid para. (data_len=%u)\n", event_para->data_len);
                return DRV_ERROR_PARA_ERROR;
            }
            break;
        default:
            sched_err("Invalid para. (dev_id=%u; data_src=%u;)\n", dev_id, (u32)event_para->data_src);
            return DRV_ERROR_PARA_ERROR;
    }

    return 0;
}

static int esched_next_table_check(u32 dev_id, u32 table_id, struct esched_table_op_next_table *table_para)
{
    if (table_para->dev_id != dev_id) {
        sched_err("Invalid next table para. (dev_id=%u)\n", table_para->dev_id);
        return DRV_ERROR_PARA_ERROR;
    }

    if (table_para->table_id == table_id) {
        sched_err("Invalid next table para. (dev_id=%u; table_id=%u)\n", table_para->dev_id, table_para->table_id);
        return DRV_ERROR_PARA_ERROR;
    }

    return 0;
}

static int esched_table_add_entry(u32 dev_id, u32 table_id, int pid, struct esched_table_key *key,
    struct esched_table_entry *entry)
{
    u8 tmp_key[ESCHED_MAX_CQE_SIZE];
    int ret = 0;

    if (dev_id >= sched_dev_num) {
        sched_err("Invalid para. (dev_id=%u; sched_dev_num=%u; table_id=%u)\n", dev_id, sched_dev_num, table_id);
        return DRV_ERROR_PARA_ERROR;
    }

    if ((key == NULL) || (key->key == NULL) || (entry == NULL)) {
        sched_err("Invalid para, null ptr. (table_id=%u)\n", table_id);
        return DRV_ERROR_PARA_ERROR;
    }

    if ((key->key_len == 0) || (key->key_len > ESCHED_MAX_CQE_SIZE)) {
        sched_err("Invalid para. (key_len=%u)\n", key->key_len);
        return DRV_ERROR_PARA_ERROR;
    }

    switch (entry->op) {
        case ESCHED_TABLE_OP_SEND_EVENT:
            ret = esched_send_event_check(dev_id, &entry->send_event);
            break;
        case ESCHED_TABLE_OP_NEXT_TABLE:
            ret = esched_next_table_check(dev_id, table_id, &entry->next_table);
            break;
        case ESCHED_TABLE_OP_DROP:
            break;
        default:
            sched_err("Invalid para. (dev_id=%u; op=%u; table_id=%u)\n", dev_id, (u32)entry->op, table_id);
            return DRV_ERROR_PARA_ERROR;
    }

    if (ret != 0) {
        sched_err("Invalid para. (dev_id=%u; op=%u; table_id=%u)\n", dev_id, (u32)entry->op, table_id);
        return ret;
    }

    ret = copy_from_user_safe(tmp_key, key->key, key->key_len);
    if (ret != 0) {
        sched_err("Copy from user failed. (dev_id=%u; ret=%d; table_id=%u)\n", dev_id, ret, table_id);
        return DRV_ERROR_COPY_USER_FAIL;
    }

    key->key = tmp_key;

    return esched_add_table_entry(dev_id, table_id, pid, key, entry);
}

static int esched_table_update_key(struct esched_table_key *key, u8 *tmp_key)
{
    int ret;

    if ((key == NULL) || (key->key == NULL)) {
        sched_err("Invalid para, null ptr.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    if ((key->key_len == 0) || (key->key_len > ESCHED_MAX_CQE_SIZE)) {
        sched_err("Invalid para. (key_len=%u)\n", key->key_len);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = copy_from_user_safe(tmp_key, key->key, key->key_len);
    if (ret != 0) {
        sched_err("Copy from user failed. (ret=%d)\n", ret);
        return DRV_ERROR_COPY_USER_FAIL;
    }

    key->key = tmp_key;

    return 0;
}

static int esched_table_del_entry(u32 dev_id, u32 table_id, int pid, struct esched_table_key *key)
{
    u8 tmp_key[ESCHED_MAX_CQE_SIZE];
    int ret;

    if (dev_id >= sched_dev_num) {
        sched_err("Invalid para. (dev_id=%u; sched_dev_num=%u; table_id=%u)\n", dev_id, sched_dev_num, table_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = esched_table_update_key(key, tmp_key);
    if (ret != 0) {
        sched_err("Failed to fill key info. (dev_id=%u; ret=%d; table_id=%u)\n", dev_id, ret, table_id);
        return ret;
    }

    return esched_del_table_entry(dev_id, table_id, pid, key);
}

static int esched_table_query_entry_stat(u32 dev_id, u32 table_id, struct esched_table_key *key,
    struct esched_table_key_entry_stat *stat)
{
    u8 tmp_key[ESCHED_MAX_CQE_SIZE];
    int ret;

    if (dev_id >= sched_dev_num) {
        sched_err("Invalid para. (dev_id=%u; sched_dev_num=%u; table_id=%u)\n", dev_id, sched_dev_num, table_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = esched_table_update_key(key, tmp_key);
    if (ret != 0) {
        sched_err("Failed to fill key info. (dev_id=%u; ret=%d; table_id=%u)\n", dev_id, ret, table_id);
        return ret;
    }

    return esched_query_table_entry_stat(dev_id, table_id, key, stat);
}

STATIC int32_t sched_fop_add_table_entry(u32 devid, unsigned long arg)
{
    struct sched_ioctl_para_add_table_entry para;

    if (copy_from_user_safe(&para, (void *)(uintptr_t)arg, sizeof(para)) != 0) {
        return DRV_ERROR_COPY_USER_FAIL;
    }

    return esched_table_add_entry(sched_ioctl_devid(devid, para.dev_id),
        para.table_id, SCHED_CURRENT_TGID, &para.key, &para.entry);
}

STATIC int32_t sched_fop_del_table_entry(u32 devid, unsigned long arg)
{
    struct sched_ioctl_para_del_table_entry para;

    if (copy_from_user_safe(&para, (void *)(uintptr_t)arg, sizeof(para)) != 0) {
        return DRV_ERROR_COPY_USER_FAIL;
    }

    return esched_table_del_entry(sched_ioctl_devid(devid, para.dev_id), para.table_id, SCHED_CURRENT_TGID, &para.key);
}

STATIC int32_t sched_fop_query_table_entry_stat(u32 devid, unsigned long arg)
{
    struct sched_ioctl_para_query_table_entry_stat *usr_para =
        (struct sched_ioctl_para_query_table_entry_stat __ka_user*)arg;
    struct sched_ioctl_para_query_table_entry_stat para = {{0}};
    int ret;

    if (copy_from_user_safe(&para, (void *)(uintptr_t)arg, sizeof(para)) != 0) {
        return DRV_ERROR_COPY_USER_FAIL;
    }

    ret = esched_table_query_entry_stat(sched_ioctl_devid(devid, para.input.chip_id),
        para.input.table_id, &para.input.key, &para.stat);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if (copy_to_user_safe((void *)&usr_para->stat, &para.stat, sizeof(struct esched_table_key_entry_stat)) != 0) {
        return DRV_ERROR_COPY_USER_FAIL;
    }

    return 0;
}

static void esched_table_ioctl_cmd_register(void)
{
    esched_register_ioctl_cmd_func(_KA_IOC_NR(SCHED_ADD_TABLE_ENTRY), sched_fop_add_table_entry);
    esched_register_ioctl_cmd_func(_KA_IOC_NR(SCHED_DEL_TABLE_ENTRY), sched_fop_del_table_entry);
    esched_register_ioctl_cmd_func(_KA_IOC_NR(SCHED_QUERY_TABLE_ENTRY_STAT), sched_fop_query_table_entry_stat);
}

STATIC void esched_table_proc_exit(int pid)
{
    u32 i;

    for (i = 0; i < sched_dev_num; i++) {
        esched_table_recycle_proc_owner_entry(i, pid);
    }
}
DECLAER_FEATURE_AUTO_UNINIT_TASK(esched_table_proc_exit, FEATURE_LOADER_STAGE_5);

static int esched_raw_data_cq_phase_head_check(struct esched_cq_phase_head *phase_head)
{
    u32 mask_bit_num, phase_max;
    u32 addr_width;

    if (phase_head->head.reg_addr == 0) {
        return 0;
    }

    /* The current version supports only the one bit phase. */
    mask_bit_num = esched_get_non_zero_bit_num((u8 *)&phase_head->phase_mask, SCHED_PHASE_MASK_BYTE_LEN);
    if (mask_bit_num != 1) {
        sched_err("Invalid para. (phase_mask=%llu; mask_bit_num=%u)\n", phase_head->phase_mask, mask_bit_num);
        return DRV_ERROR_PARA_ERROR;
    }

    phase_max = (0x1 << mask_bit_num) - 1;
    addr_width = phase_head->addr_width;
    if ((phase_head->init_value > phase_max) || (phase_head->ring_step > phase_max) || (phase_head->ring_step == 0)
        || ((addr_width != ESCHED_ADDR_WIDTH_32_BITS) && (addr_width != ESCHED_ADDR_WIDTH_64_BITS))) {
        sched_err("Invalid para. (init_value=%u; ring_step=%u; mask_bit_num=%u; addr_width=%u)\n",
            (u32)phase_head->init_value, (u32)phase_head->ring_step, mask_bit_num, addr_width);
        return DRV_ERROR_PARA_ERROR;
    }

    return 0;
}

static int esched_raw_data_cq_phase_check(struct esched_raw_data_cq *cq_data)
{
    struct esched_cq_phase *cq_phase = &cq_data->cq_phase;
    u32 mask_bit_num, phase_max;
    int ret;

    if (cq_phase->mask == NULL) {
        sched_err("Invalid phase para, null ptr.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    /* The current version supports only the one bit phase. */
    mask_bit_num = esched_get_non_zero_bit_num(cq_phase->mask, cq_data->cqe_size);
    if (mask_bit_num != 1) {
        sched_err("Invalid phase para. (mask_bit_num=%u)\n", mask_bit_num);
        return DRV_ERROR_PARA_ERROR;
    }

    phase_max = (0x1U << mask_bit_num) - 1;
    if ((cq_phase->init_value > phase_max) || (cq_phase->ring_step > phase_max) || (cq_phase->ring_step == 0)) {
        sched_err("Invalid phase para. (init_value=%u; ring_step=%u; mask_bit_num=%u)\n",
            (u32)cq_phase->init_value, (u32)cq_phase->ring_step, mask_bit_num);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = esched_raw_data_cq_phase_head_check(&cq_phase->phase_head);
    if (ret != 0) {
        return ret;
    }

    return 0;
}

static int esched_raw_data_cq_check(struct esched_table_raw_data *raw_data)
{
    struct esched_raw_data_cq *cq_data = &raw_data->cq_data;
    int ret;

    /* The current version supports only the virtual address of the kernel. */
    if ((cq_data->addr_type != ESCHED_ADDR_TYPE_VIR) || (cq_data->stream_id != 0)) {
        sched_err("Invalid para. (addr_type=%u; stream_id=%u)\n", cq_data->addr_type, (u32)cq_data->stream_id);
        return DRV_ERROR_PARA_ERROR;
    }

    if ((cq_data->cqe_size > ESCHED_MAX_CQE_SIZE) || (cq_data->cqe_size == 0) ||
        ((cq_data->cqe_size % ESCHED_CQE_SIZE_ALIGN) != 0)) {
        sched_err("Invalid para. (cqe_size=%u)\n", cq_data->cqe_size);
        return DRV_ERROR_PARA_ERROR;
    }

    if ((cq_data->cq_depth > ESCHED_MAX_CQ_DEPTH) || (cq_data->cq_depth == 0)) {
        sched_err("Invalid para. (cq_depth=%u)\n", cq_data->cq_depth);
        return DRV_ERROR_PARA_ERROR;
    }

    if (cq_data->cq_addr == 0) {
        sched_err("Invalid para. (cq_addr=0x%pK)\n", (void *)(uintptr_t)cq_data->cq_addr);
        return DRV_ERROR_PARA_ERROR;
    }

    if (raw_data->raw_data_key_mask_len != cq_data->cqe_size) {
        sched_err("Invalid para. (cqe_size=%u; raw_data_key_mask_len=%u)\n",
            cq_data->cqe_size, raw_data->raw_data_key_mask_len);
        return DRV_ERROR_PARA_ERROR;
    }

    /* The current version supports only the cq type phase. */
    switch (cq_data->type) {
        case ESCHED_CQ_TYPE_PHASE:
            ret = esched_raw_data_cq_phase_check(cq_data);
            if (ret != 0) {
                return ret;
            }
            break;
        default:
            sched_err("Invalid para. (type=%u)\n", cq_data->type);
            return DRV_ERROR_PARA_ERROR;
    }

    return 0;
}

int hal_esched_alloc_table(u32 dev_id, struct esched_table_raw_data *raw_data, u32 *table_id)
{
    int ret;
    u32 endian_type;

    if (dev_id >= sched_dev_num) {
        sched_err("Invalid para. (dev_id=%u; sched_dev_num=%u)\n", dev_id, sched_dev_num);
        return DRV_ERROR_PARA_ERROR;
    }

    if ((raw_data == NULL) || (raw_data->raw_data_key_mask == NULL) || (raw_data->name == NULL) || (table_id == NULL)) {
        sched_err("Invalid para, null ptr.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    endian_type = raw_data->endian_type;
    if ((raw_data->max_entry_num > ESCHED_MAX_ENTRY_NUM) || (raw_data->max_entry_num == 0)
        || ((endian_type != ESCHED_ADDR_LITTLE_ENDIAN_TYPE) && (endian_type != ESCHED_ADDR_BIG_ENDIAN_TYPE))) {
        sched_err("Invalid para. (dev_id=%u; max_entry_num=%u; endian_type=%u)\n", dev_id, raw_data->max_entry_num,
            endian_type);
        return DRV_ERROR_PARA_ERROR;
    }

    switch (raw_data->raw_data_type) {
        case RAW_DATA_TYPE_CQ:
            ret = esched_raw_data_cq_check(raw_data);
            if (ret != 0) {
                sched_err("Invalid raw data para. (dev_id=%u; raw_data_type=%u)\n", dev_id, raw_data->raw_data_type);
                return ret;
            }
            break;
        default:
            sched_err("Invalid para. (dev_id=%u; raw_data_type=%u)\n", dev_id, raw_data->raw_data_type);
            return DRV_ERROR_PARA_ERROR;
    }

    return esched_alloc_table(dev_id, raw_data, table_id);
}
KA_EXPORT_SYMBOL_GPL(hal_esched_alloc_table);

int hal_esched_free_table(u32 dev_id, u32 table_id)
{
    if (dev_id >= sched_dev_num) {
        sched_err("Invalid para. (dev_id=%u; sched_dev_num=%u)\n", dev_id, sched_dev_num);
        return DRV_ERROR_PARA_ERROR;
    }

    return esched_free_table(dev_id, table_id);
}
KA_EXPORT_SYMBOL_GPL(hal_esched_free_table);


static int esched_hw_info_check(struct esched_hw_info *hw_info)
{
    u32 addr_width, endian_type;

    if ((hw_info == NULL) || (hw_info->name == NULL)) {
        sched_err("Invalid para, null ptr.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    if (hw_info->irq_clr_reg == 0) {
        return 0;
    }

    addr_width = hw_info->addr_width;
    endian_type = hw_info->endian_type;

    /* The current version supports only the virtual address of the kernel. */
    if ((hw_info->addr_type != ESCHED_ADDR_TYPE_VIR)
        || ((addr_width != ESCHED_ADDR_WIDTH_32_BITS) && (addr_width != ESCHED_ADDR_WIDTH_64_BITS))
        || ((endian_type != ESCHED_ADDR_LITTLE_ENDIAN_TYPE) && (endian_type != ESCHED_ADDR_BIG_ENDIAN_TYPE))) {
        sched_err("Invalid para, (addr_type=%u; addr_width=%u; endian_type=%u).\n", hw_info->addr_type,
            addr_width, endian_type);
        return DRV_ERROR_PARA_ERROR;
    }

    return 0;
}

int hal_esched_hw_bind_table(u32 dev_id, struct esched_hw_info *hw_info, u32 table_id)
{
    int ret;

    if (dev_id >= sched_dev_num) {
        sched_err("Invalid para. (dev_id=%u; sched_dev_num=%u)\n", dev_id, sched_dev_num);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = esched_hw_info_check(hw_info);
    if (ret != 0) {
        return DRV_ERROR_PARA_ERROR;
    }

    return esched_hw_bind_table(dev_id, hw_info, table_id);
}
KA_EXPORT_SYMBOL_GPL(hal_esched_hw_bind_table);

int hal_esched_hw_unbind_table(u32 dev_id, struct esched_hw_info *hw_info)
{
    if (dev_id >= sched_dev_num) {
        sched_err("Invalid para. (dev_id=%u; sched_dev_num=%u)\n", dev_id, sched_dev_num);
        return DRV_ERROR_PARA_ERROR;
    }

    if (hw_info == NULL) {
        sched_err("Invalid para, null ptr.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    return esched_hw_unbind_table(dev_id, hw_info);
}
KA_EXPORT_SYMBOL_GPL(hal_esched_hw_unbind_table);

STATIC int esched_table_dev_init(u32 dev_id)
{
    int ret = 0;

    ret = esched_table_mng_init(dev_id);
    if (ret == 0) {
        sched_dev_num++;
    }

    return ret;
}
DECLAER_FEATURE_AUTO_INIT_DEV(esched_table_dev_init, FEATURE_LOADER_STAGE_5);

STATIC void esched_table_dev_uninit(u32 dev_id)
{
    esched_table_mng_uninit(dev_id);
    sched_dev_num--;
}
DECLAER_FEATURE_AUTO_UNINIT_DEV(esched_table_dev_uninit, FEATURE_LOADER_STAGE_5);

STATIC int esched_table_init(void)
{
    sched_dev_num = 0;
    esched_table_mng_clear();
    esched_table_ioctl_cmd_register();
    esched_table_proc_fs_init();

    sched_info("Table init success.\n");
    return DRV_ERROR_NONE;
}
DECLAER_FEATURE_AUTO_INIT(esched_table_init, FEATURE_LOADER_STAGE_5);

STATIC void esched_table_uninit(void)
{
    esched_table_proc_fs_uninit();
    sched_info("Table uninit success.\n");
}
DECLAER_FEATURE_AUTO_UNINIT(esched_table_uninit, FEATURE_LOADER_STAGE_5);

