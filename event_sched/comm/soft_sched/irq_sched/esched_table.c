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

#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_list_pub.h"
#include "ka_system_pub.h"
#include "ka_driver_pub.h"
#include "ka_compiler_pub.h"

#include "securec.h"
#include "esched.h"
#include "esched_table_proc_fs.h"
#include "esched_table.h"

#define TASKLET_DEVID_OFFSET 32

#ifdef CFG_BUILD_DEBUG
const char * const esched_hlt_irq_name_prefix = "hlttest";
#endif

struct esched_table_mng *table_mng[SCHED_MAX_CHIP_NUM];

void esched_table_mng_clear(void)
{
    u32 i;

    for (i = 0; i < SCHED_MAX_CHIP_NUM; i++) {
        table_mng[i] = NULL;
    }
}

STATIC struct esched_table_mng *esched_get_table_mng(u32 dev_id)
{
    return table_mng[dev_id];
}

u32 esched_get_non_zero_bit_num(u8 *mask, u32 len)
{
    u32 i, j;
    u32 bit_num = 0;

    for (i = 0; i < len; i++) {
        if (mask[i] == 0) {
            continue;
        }

        for (j = 0; j < BYTE_WIDTH; j++) {
            if ((mask[i] & (0x1U << j)) != 0) {
                bit_num++;
            }
        }
    }

    return bit_num;
}

/* If mask==0, offset is -1. */
STATIC int esched_get_first_non_zero_bit_offset(u8 *mask, u32 len)
{
    u32 i, j;
    int offset = -1;

    for (i = 0; i < len; i++) {
        if (mask[i] == 0) {
            continue;
        }

        for (j = 0; j < BYTE_WIDTH; j++) {
            if ((mask[i] & (0x1 << j)) != 0) {
                offset = i * BYTE_WIDTH + j;
                return offset;
            }
        }
    }

    return offset;
}

static void esched_table_mask_bit_init(u8 *mask, u32 len, struct esched_table_mask_bit *mask_bit, u32 mask_bit_num)
{
    u32 i, j, bit_num = 0;

    for (i = 0; i < len; i++) {
        if (mask[i] == 0) {
            continue;
        }

        for (j = 0; j < BYTE_WIDTH; j++) {
            if ((mask[i] & (0x1U << j)) == 0) {
                continue;
            }

            mask_bit[bit_num].byte_offset = i;
            mask_bit[bit_num].bit_offset = j;
            bit_num++;

            if (mask_bit_num == bit_num) {
                return;
            }
        }
    }
}

static void esched_table_memcpy(u8 *dst, u8 *src, u32 len)
{
    u32 i;
    for (i = 0; i < len; i++) {
        dst[i] = src[i];
    }
}

static void esched_table_key_mask_init(struct esched_table_key_mask *key_mask, struct esched_table_raw_data *raw_data,
    struct esched_table_mask_bit *mask_bit, u32 mask_bit_num)
{
    key_mask->mask_len = raw_data->raw_data_key_mask_len;
    key_mask->mask_bit = mask_bit;
    key_mask->mask_bit_num = mask_bit_num;
    key_mask->key_len = KA_DRIVER_ALIGN(mask_bit_num, BYTE_WIDTH) / BYTE_WIDTH;
    esched_table_memcpy(key_mask->mask, raw_data->raw_data_key_mask, key_mask->mask_len);
    esched_table_mask_bit_init(key_mask->mask, key_mask->mask_len, key_mask->mask_bit, mask_bit_num);
}

static void esched_table_phase_head_init(struct esched_cq_phase_head_ctrl *phase_head,
    struct esched_raw_data_cq *cq_data)
{
    int offset;

    if (cq_data->cq_phase.phase_head.head.reg_addr == 0) {
        phase_head->reg_addr = NULL;
        (void)offset;
        return;
    }

    offset = esched_get_first_non_zero_bit_offset((u8 *)&cq_data->cq_phase.phase_head.phase_mask,
        SCHED_PHASE_MASK_BYTE_LEN);

    phase_head->reg_addr = (void __iomem *)cq_data->cq_phase.phase_head.head.reg_addr;
    phase_head->addr_width = cq_data->cq_phase.phase_head.addr_width;
    phase_head->overlay_value = cq_data->cq_phase.phase_head.overlay_value;
    phase_head->offset = cq_data->cq_phase.phase_head.head.offset;
    phase_head->mask = cq_data->cq_phase.phase_head.head.mask;

    phase_head->head_phase.mask = cq_data->cq_phase.phase_head.phase_mask;
    phase_head->head_phase.offset = offset;
    phase_head->head_phase.phase = cq_data->cq_phase.phase_head.init_value;
    phase_head->head_phase.ring_step = cq_data->cq_phase.phase_head.ring_step;
    phase_head->head_phase.phase_range = 0x1 << 1; /* The current version supports only the one bit phase. */
}

static void esched_table_cq_phase_init(struct esched_cq_ctrl *cq_ctrl, struct esched_raw_data_cq *cq_data)
{
    u32 mask_bit_num = esched_get_non_zero_bit_num(cq_data->cq_phase.mask, cq_data->cqe_size);

    esched_table_phase_head_init(&cq_ctrl->cq_phase.phase_head, cq_data);

    cq_ctrl->cq_phase.phase = cq_data->cq_phase.init_value;
    cq_ctrl->cq_phase.ring_step = cq_data->cq_phase.ring_step;
    cq_ctrl->cq_phase.phase_range = 0x1U << mask_bit_num;
    cq_ctrl->cq_phase.mask_bit_num = mask_bit_num;
    esched_table_mask_bit_init(cq_data->cq_phase.mask, cq_data->cqe_size, &cq_ctrl->cq_phase.mask_bit, mask_bit_num);
    cq_ctrl->cq_phase.mask = cq_data->cq_phase.mask[cq_ctrl->cq_phase.mask_bit.byte_offset];
}

static void esched_table_cq_ctrl_init(struct esched_cq_ctrl *cq_ctrl, struct esched_raw_data_cq *cq_data)
{
    cq_ctrl->type = cq_data->type;
    cq_ctrl->cq_addr = (u8 *)cq_data->cq_addr;
    cq_ctrl->cq_depth = cq_data->cq_depth;
    cq_ctrl->cqe_size = cq_data->cqe_size;
    cq_ctrl->head = 0;
    cq_ctrl->round = 0;

    esched_table_cq_phase_init(cq_ctrl, cq_data);
}

static int esched_table_key_check(struct esched_table *table, struct esched_table_key *key)
{
    struct esched_table_key_mask *key_mask = &table->key_mask;
    u32 offset;

    if (table->key_mask.key_len != key->key_len) {
        sched_err("Invalid para. (dev_id=%u; table_id=%d; mask_bit_num=%u; key_len=%u)\n",
            table->dev_id, table->id, key_mask->mask_bit_num, key->key_len);
        return DRV_ERROR_PARA_ERROR;
    }

    offset = table->key_mask.mask_bit_num % BYTE_WIDTH;
    if ((offset != 0) && ((key->key[key->key_len - 1] >> offset) != 0)) {
        sched_err("Invalid para. (dev_id=%u; table_id=%d; mask_bit_num=%u; key_len=%u; key_high=%c)\n",
            table->dev_id, table->id, key_mask->mask_bit_num, key->key_len, key->key[key->key_len - 1]);
        return DRV_ERROR_PARA_ERROR;
    }

    return 0;
}

static u32 esched_table_construct_key(u8 *key, u32 key_len)
{
    return ka_base_jhash(key, key_len, 0) & SCHED_TABLES_HASH_TABLE_MASK;
}

static struct esched_table_key_entry *esched_find_table_key_entry(struct esched_table *table, u8 *key)
{
    struct esched_table_key_entry *key_entry = NULL;
    u32 key_len = table->key_mask.key_len;
    u32 key_tag = esched_table_construct_key(key, key_len);

    ka_hash_for_each_possible(table->entry_hash_table, key_entry, link, key_tag) {
        if (ka_base_memcmp(key_entry->key, key, key_len) == 0) {
            return key_entry;
        }
    }

    return NULL;
}

static struct esched_table_key_entry *esched_create_entry(struct esched_table *table, int pid,
    struct esched_table_key *key, struct esched_table_entry *entry)
{
    struct esched_table_key_entry *key_entry = NULL;
    u32 key_tag = esched_table_construct_key(key->key, key->key_len);

    key_entry = (struct esched_table_key_entry *)sched_kzalloc(sizeof(struct esched_table_key_entry),
        KA_GFP_ATOMIC | __KA_GFP_ACCOUNT);
    if (key_entry == NULL) {
        sched_err("Failed to alloc memory. (size=0x%lx)\n", sizeof(struct esched_table_key_entry));
        return NULL;
    }

    key_entry->pid = pid;
    key_entry->entry = *entry;
    esched_table_memcpy(key_entry->key, key->key, key->key_len);

    ka_hash_add(table->entry_hash_table, &key_entry->link, key_tag);

    return key_entry;
}

static void esched_destroy_entry(struct esched_table_key_entry *key_entry)
{
    ka_hash_del(&key_entry->link);
    sched_kfree(key_entry);
}

static void esched_table_free_all_key_entry(struct esched_table *table)
{
    struct esched_table_key_entry *key_entry = NULL;
    ka_hlist_node_t *hnode = NULL;
    u32 bkt, entry_num = 0;

    ka_hash_for_each_safe(table->entry_hash_table, bkt, hnode, key_entry, link) {
        esched_destroy_entry(key_entry);
        entry_num++;
    }

    if (entry_num > 0) {
        sched_info("Free entry. (dev_id=%u; table_id=%d; entry_num=%u)\n", table->dev_id, table->id, entry_num);
    }
}

static void esched_table_free_proc_key_entry(struct esched_table *table, int pid)
{
    struct esched_table_key_entry *key_entry = NULL;
    ka_hlist_node_t *hnode = NULL;
    u32 bkt, entry_num = 0;

    ka_hash_for_each_safe(table->entry_hash_table, bkt, hnode, key_entry, link) {
        if (key_entry->pid == pid) {
            esched_destroy_entry(key_entry);
            entry_num++;
        }
    }

    if (entry_num > 0) {
        sched_info("Free proc entry. (dev_id=%u; table_id=%d; pid=%d; entry_num=%u)\n",
            table->dev_id, table->id, pid, entry_num);
    }
}

static struct esched_table *esched_create_table(u32 dev_id, struct esched_table_raw_data *raw_data)
{
    struct esched_table *table = NULL;
    struct esched_table_mask_bit *mask_bit = NULL;
    u32 mask_bit_num = esched_get_non_zero_bit_num(raw_data->raw_data_key_mask, raw_data->raw_data_key_mask_len);
    unsigned long size;
    int ret;

    size = sizeof(struct esched_table) + sizeof(struct esched_table_mask_bit) * mask_bit_num;
    table = (struct esched_table *)sched_vzalloc(size);
    if (table == NULL) {
        sched_err("Failed to alloc memory. (size=0x%lx)\n", size);
        return NULL;
    }

    mask_bit = (struct esched_table_mask_bit *)(table + 1);

    ka_hash_init(table->entry_hash_table);
    ka_task_spin_lock_init(&table->lock);
    table->refcnt = 0;
    table->dev_id = dev_id;
    table->entry_num = 0;
    table->max_entry_num = raw_data->max_entry_num;
    table->endian_type = raw_data->endian_type;
    ret = strcpy_s(table->name, ESCHED_MAX_NAME_LEN, raw_data->name);
    if (ret != 0) {
        sched_vfree(table);
        sched_err("Call strcpy_s failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return NULL;
    }
    table->name[ESCHED_MAX_NAME_LEN - 1] = '\0';

    esched_table_key_mask_init(&table->key_mask, raw_data, mask_bit, mask_bit_num);
    esched_table_cq_ctrl_init(&table->cq_ctrl, &raw_data->cq_data);

    return table;
}

static void esched_destroy_table(struct esched_table *table)
{
    sched_info("Destroy table. (dev_id=%u; table_id=%d)\n", table->dev_id, table->id);
    esched_table_free_all_key_entry(table);
    sched_vfree(table);
}

static int esched_add_table(u32 dev_id, struct esched_table *table)
{
    struct esched_table_mng *tm = esched_get_table_mng(dev_id);
    int id;

    id = ka_base_idr_alloc(&tm->table_idr, table, 0, ESCHED_TABLE_MAX_ID, KA_GFP_ATOMIC);
    if (id < 0) {
        return DRV_ERROR_NO_RESOURCES;
    }
    table->id = id;
    table->refcnt++;

    tm->table_num++;

    return 0;
}

static void esched_del_table(u32 dev_id, struct esched_table *table)
{
    struct esched_table_mng *tm = esched_get_table_mng(dev_id);

    tm->table_num--;

    (void)ka_base_idr_remove(&tm->table_idr, table->id);
    table->refcnt--;
}

static struct esched_table *esched_table_get(u32 dev_id, u32 table_id)
{
    struct esched_table_mng *tm = esched_get_table_mng(dev_id);
    struct esched_table *table = NULL;

    ka_task_spin_lock_bh(&tm->lock);
    table = ka_base_idr_find(&tm->table_idr, table_id);
    if (table != NULL) {
        table->refcnt++;
    }
    ka_task_spin_unlock_bh(&tm->lock);

    return table;
}

static void esched_table_put(u32 dev_id, struct esched_table *table)
{
    struct esched_table_mng *tm = esched_get_table_mng(dev_id);
    int destroy_flag = 0;

    ka_task_spin_lock_bh(&tm->lock);
    table->refcnt--;
    if (table->refcnt <= 0) {
        destroy_flag = 1;
    }
    ka_task_spin_unlock_bh(&tm->lock);

    if (destroy_flag == 1) {
        esched_destroy_table(table);
    }
}

static bool esched_cq_phase_is_cqe_valid(struct esched_cq_phase_ctrl *cq_phase, u8 *cqe_addr)
{
    u8 phase = (cqe_addr[cq_phase->mask_bit.byte_offset] & cq_phase->mask) >> cq_phase->mask_bit.bit_offset;
    return (phase == cq_phase->phase);
}

static void esched_table_reg_wr(void __iomem *reg_addr, u64 value, u32 addr_width, u32 endian_type)
{
    if (ka_likely(addr_width == ESCHED_ADDR_WIDTH_32_BITS)) {
        if (ka_likely(endian_type == ESCHED_ADDR_BIG_ENDIAN_TYPE)) {
            ka_mm_writel(ka_system_cpu_to_be32((u32)value), reg_addr);
        } else {
            ka_mm_writel(ka_system_cpu_to_le32((u32)value), reg_addr);
        }
    } else {
        if (ka_likely(endian_type == ESCHED_ADDR_BIG_ENDIAN_TYPE)) {
            ka_mm_writeq(ka_system_cpu_to_be64(value), reg_addr);
        } else {
            ka_mm_writeq(ka_system_cpu_to_le64(value), reg_addr);
        }
    }

    sched_debug("Write reg. (value=0x%llx; addr_width=%u; endian_type=%u)\n", value, addr_width, endian_type);
}

static void esched_notice_hw_cq_finish(struct esched_table *table)
{
    struct esched_cq_phase_ctrl *cq_phase = &table->cq_ctrl.cq_phase;
    struct esched_cq_ctrl *cq_ctrl;
    u32 offset, mask;
    u64 pre_value, value;
    u64 phase_value = 0;
    u64 head;

    if (cq_phase->phase_head.reg_addr == NULL) {
        return;
    }

    cq_ctrl = &table->cq_ctrl;
    offset = cq_phase->phase_head.offset;
    mask = cq_phase->phase_head.mask;
    pre_value = cq_phase->phase_head.overlay_value;

    if (cq_phase->phase_head.head_phase.mask != 0) {
        head = (u64)cq_ctrl->head;
        phase_value = ((u64)cq_phase->phase_head.head_phase.phase << cq_phase->phase_head.head_phase.offset) &
            cq_phase->phase_head.head_phase.mask;
    } else {
        head = (u64)cq_ctrl->round * cq_ctrl->cq_depth + cq_ctrl->head;
    }

    value = ((head << offset) & mask) | pre_value | phase_value;

    esched_table_reg_wr(cq_phase->phase_head.reg_addr, value, cq_phase->phase_head.addr_width,
        table->endian_type);
}

static void esched_notice_hw_finish(struct esched_table *table)
{
    esched_notice_hw_cq_finish(table);
}

static void esched_cq_phase_update_head(struct esched_cq_ctrl *cq_ctrl)
{
    u8 phase;
    u8 head_phase = cq_ctrl->cq_phase.phase_head.head_phase.phase;
    u8 phase_range = cq_ctrl->cq_phase.phase_head.head_phase.phase_range;
    u8 ring_step = cq_ctrl->cq_phase.phase_head.head_phase.ring_step;

    cq_ctrl->head = (cq_ctrl->head + 1) % cq_ctrl->cq_depth;
    if (cq_ctrl->head == 0) {
        cq_ctrl->round++;
        phase = (cq_ctrl->cq_phase.phase + cq_ctrl->cq_phase.ring_step) % cq_ctrl->cq_phase.phase_range;
        cq_ctrl->cq_phase.phase = phase;
        cq_ctrl->cq_phase.phase_head.head_phase.phase = (head_phase + ring_step) % phase_range;
    }

    sched_debug("Update head.(round=%u; head=%u; phase=%u; head_phase=%u)\n", cq_ctrl->round, cq_ctrl->head,
        (u32)cq_ctrl->cq_phase.phase, (u32)head_phase);
}

static void esched_raw_data_to_key(u8 *data_addr, struct esched_table_key_mask *key_mask, u8 *key)
{
    struct esched_table_mask_bit *mask_bit = key_mask->mask_bit;
    u32 i, key_index = 0, key_offset = 0;

    key[key_index] = 0;
    for (i = 0; i < key_mask->mask_bit_num; i++) {
        key[key_index] |= ((data_addr[mask_bit->byte_offset] >> mask_bit->bit_offset) & 0x1U) << key_offset;
        mask_bit++;
        key_offset++;

        /* change to next byte to store key */
        if (key_offset == BYTE_WIDTH) {
            key_index++;
            key_offset = 0;
            key[key_index] = 0;
        }
    }
}

static void esched_update_mismatched_cqe(struct esched_table *table, const u8 *cqe, const int size)
{
    int ret;
    ret = memcpy_s((void *)table->last_mismatched_cqe, size, (void *)cqe, size);
    if (ret != 0) {
#ifndef EMU_ST
        sched_warn("Update mismatched cqe warn, can be ignored.\n");
#endif
    }
}

static struct esched_table_key_entry *esched_cq_phase_get_key_entry(struct esched_table *table)
{
    struct esched_table_key_entry *key_entry = NULL;
    struct esched_cq_ctrl *cq_ctrl = &table->cq_ctrl;
    u8 *cqe_addr = NULL;
    u8 key[ESCHED_MAX_KEY_LEN];

retry:
    cqe_addr = cq_ctrl->cq_addr + (u64)cq_ctrl->cqe_size * cq_ctrl->head;

    if (!esched_cq_phase_is_cqe_valid(&cq_ctrl->cq_phase, cqe_addr)) {
        return NULL;
    }

    esched_cq_phase_update_head(cq_ctrl);
    esched_raw_data_to_key(cqe_addr, &table->key_mask, key);

    key_entry = esched_find_table_key_entry(table, key);
    if (key_entry != NULL) {
        key_entry->raw_data = cqe_addr;
        key_entry->raw_data_len = cq_ctrl->cqe_size;
        key_entry->stat.match_num++;
    } else {
        esched_update_mismatched_cqe(table, cqe_addr, cq_ctrl->cqe_size);
        table->stat.key_entry_mismatch_num++;
        goto retry;
    }

    return key_entry;
}

static struct esched_table_key_entry *esched_cq_get_key_entry(struct esched_table *table)
{
    return esched_cq_phase_get_key_entry(table);
}

static struct esched_table_key_entry *esched_get_key_entry(struct esched_table *table)
{
    return esched_cq_get_key_entry(table);
}

static void esched_table_op_handle_send_event(struct esched_table *table, struct esched_table_key_entry *key_entry)
{
    struct esched_table_op_send_event *para = &key_entry->entry.send_event;
    struct sched_published_event publish_event;
    int ret;

    table->stat.send_event_num++;

    publish_event.event_info.dst_engine = para->dst_engine;
    publish_event.event_info.policy = para->policy;
    publish_event.event_info.pid = key_entry->pid;
    publish_event.event_info.gid = para->gid;
    publish_event.event_info.event_id = para->event_id;
    publish_event.event_info.subevent_id = para->sub_event_id;

    if (para->data_src == ESCHED_DATA_SRC_RAW_DATA) {
        publish_event.event_info.msg = key_entry->raw_data;
        publish_event.event_info.msg_len = key_entry->raw_data_len;
    } else if (para->data_src == ESCHED_DATA_SRC_KEY) {
        publish_event.event_info.msg = key_entry->key;
        publish_event.event_info.msg_len = table->key_mask.key_len;
    } else if (para->data_src == ESCHED_DATA_SRC_USR_CFG) {
        publish_event.event_info.msg = para->data;
        publish_event.event_info.msg_len = para->data_len;
    } else {
        publish_event.event_info.msg = NULL;
        publish_event.event_info.msg_len = 0;
    }

    publish_event.event_func.event_ack_func = NULL;
    publish_event.event_func.event_finish_func = NULL;

    ret = sched_submit_event(para->dev_id, &publish_event);
    if (ka_unlikely(ret)) {
        table->stat.send_event_fail_num++;
        sched_err("Submit event failed. (dev_id=%u; table_id=%d; ret=%d)\n", table->dev_id, table->id, ret);
    }
}

static void esched_table_op_handle_next_table(struct esched_table *table, struct esched_table_key_entry *key_entry)
{
    struct esched_table_op_next_table *para = &key_entry->entry.next_table;
    struct esched_table *next_table = NULL;

    table->stat.next_table_num++;

    next_table = esched_table_get(para->dev_id, para->table_id);
    if (next_table == NULL) {
        table->stat.next_table_mismatch_num++;
        sched_err("Get next table failed. (cur_dev_id=%u; cur_table_id=%d; dev_id=%u; table_id=%u)\n",
            table->dev_id, table->id, para->dev_id, para->table_id);
        return;
    }

    ka_system_tasklet_schedule(&next_table->task);

    esched_table_put(para->dev_id, next_table);
}

static void esched_table_op_handle_drop(struct esched_table *table, struct esched_table_key_entry *key_entry)
{
    table->stat.drop_num++;
}

static void (*const esched_entry_op_handles[ESCHED_TABLE_OP_MAX])(struct esched_table *table,
    struct esched_table_key_entry *key_entry) = {
    [ESCHED_TABLE_OP_SEND_EVENT] = esched_table_op_handle_send_event,
    [ESCHED_TABLE_OP_NEXT_TABLE] = esched_table_op_handle_next_table,
    [ESCHED_TABLE_OP_DROP] = esched_table_op_handle_drop,
};

static void esched_table_tasklet(unsigned long data)
{
    struct esched_table_key_entry *key_entry = NULL;
    struct esched_table *table = NULL;
    u32 dev_id = (u32)(data >> TASKLET_DEVID_OFFSET);
    u32 table_id = (u32)data;
    u32 num = 0;

    table = esched_table_get(dev_id, table_id);
    if (table == NULL) {
        sched_err("Get table failed. (dev_id=%u; table_id=%u)\n", dev_id, table_id);
        return;
    }

    table->stat.task_run_num++;

    ka_task_spin_lock_bh(&table->lock);

    while (1) {
        key_entry = esched_get_key_entry(table);
        if (key_entry == NULL) {
            if (num == 0) {
                sched_warn("Get key entry warn. (dev_id=%u; table_id=%u)\n", dev_id, table_id);
            }
            break;
        }

        esched_entry_op_handles[key_entry->entry.op](table, key_entry);

        num++;
        if (num >= ESCHED_TASK_HANDLE_ENTRY_NUM) {
            ka_system_tasklet_schedule(&table->task);
            break;
        }
    }

    if (num > 0) {
        esched_notice_hw_finish(table);
    }

    ka_task_spin_unlock_bh(&table->lock);
    esched_table_put(dev_id, table);
}

int esched_add_table_entry(u32 dev_id, u32 table_id, int pid, struct esched_table_key *key,
    struct esched_table_entry *entry)
{
    struct esched_table_key_entry *key_entry = NULL;
    struct esched_table *table = NULL;
    int ret;

    table = esched_table_get(dev_id, table_id);
    if (table == NULL) {
        sched_err("Get table failed. (dev_id=%u; table_id=%u)\n", dev_id, table_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ka_task_spin_lock_bh(&table->lock);

    if (table->entry_num >= table->max_entry_num) {
        sched_err("Entry out of range. (dev_id=%u; table_id=%u; entry_num=%u; max_entry_num=%u)\n",
            dev_id, table_id, table->entry_num, table->max_entry_num);
        ret = DRV_ERROR_NO_RESOURCES;
        goto out;
    }

    ret = esched_table_key_check(table, key);
    if (ret != 0) {
        sched_err("Invalid key para. (dev_id=%u; table_id=%u)\n", dev_id, table_id);
        goto out;
    }

    key_entry = esched_find_table_key_entry(table, key->key);
    if (key_entry != NULL) {
        sched_err("Repeat key. (dev_id=%u; table_id=%u)\n", dev_id, table_id);
        ret = DRV_ERROR_REPEATED_USERD;
        goto out;
    }

    key_entry = esched_create_entry(table, pid, key, entry);
    if (key_entry == NULL) {
        ret = DRV_ERROR_OUT_OF_MEMORY;
        goto out;
    }

    table->entry_num++;
    sched_debug("Add table entry success. (dev_id=%u; table_id=%u; pid=%d; entry_num=%u)\n",
        dev_id, table_id, pid, table->entry_num);

out:
    ka_task_spin_unlock_bh(&table->lock);
    esched_table_put(dev_id, table);
    return ret;
}

int esched_del_table_entry(u32 dev_id, u32 table_id, int pid, struct esched_table_key *key)
{
    struct esched_table_key_entry *key_entry = NULL;
    struct esched_table *table = NULL;
    int ret;

    table = esched_table_get(dev_id, table_id);
    if (table == NULL) {
        sched_err("Get table failed. (dev_id=%u; table_id=%u)\n", dev_id, table_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ka_task_spin_lock_bh(&table->lock);

    ret = esched_table_key_check(table, key);
    if (ret != 0) {
        sched_err("Invalid key para. (dev_id=%u; table_id=%u)\n", dev_id, table_id);
        goto out;
    }

    key_entry = esched_find_table_key_entry(table, key->key);
    if (key_entry == NULL) {
        sched_err("Key not find. (dev_id=%u; table_id=%u)\n", dev_id, table_id);
        ret = DRV_ERROR_PARA_ERROR;
        goto out;
    }

    esched_destroy_entry(key_entry);
    table->entry_num--;

    sched_debug("Del table entry success. (dev_id=%u; table_id=%u; pid=%d; entry_num=%u)\n",
        dev_id, table_id, pid, table->entry_num);

out:
    ka_task_spin_unlock_bh(&table->lock);
    esched_table_put(dev_id, table);
    return ret;
}

int esched_query_table_entry_stat(u32 dev_id, u32 table_id, struct esched_table_key *key,
    struct esched_table_key_entry_stat *stat)
{
    struct esched_table_key_entry *key_entry = NULL;
    struct esched_table *table = NULL;

    table = esched_table_get(dev_id, table_id);
    if (table == NULL) {
        sched_err("Get table failed. (dev_id=%u; table_id=%u)\n", dev_id, table_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ka_task_spin_lock_bh(&table->lock);
    key_entry = esched_find_table_key_entry(table, key->key);
    if (key_entry == NULL) {
        ka_task_spin_unlock_bh(&table->lock);
        esched_table_put(dev_id, table);
        sched_err("Invalid key. (dev_id=%u; table_id=%u)\n", dev_id, table_id);
        return DRV_ERROR_PARA_ERROR;
    }

    stat->matchNum = key_entry->stat.match_num;

    ka_task_spin_unlock_bh(&table->lock);
    esched_table_put(dev_id, table);
    return 0;
}

void esched_table_recycle_proc_owner_entry(u32 dev_id, int pid)
{
    struct esched_table_mng *tm = esched_get_table_mng(dev_id);
    struct esched_table *table = NULL;
    u32 *table_ids = NULL;
    int i, j = 0;

    table_ids = (u32 *)sched_vmalloc(ESCHED_TABLE_MAX_ID * sizeof(u32 *), KA_GFP_KERNEL | __KA_GFP_ACCOUNT, KA_PAGE_KERNEL);
    if (table_ids == NULL) {
        sched_err("Failed to alloc memory. (size=0x%lx)\n", ESCHED_TABLE_MAX_ID * sizeof(u32 *));
        return;
    }

    /* To avoid affecting the tasklet processing process, can not free table proc entry in tm->lock */
    ka_task_spin_lock_bh(&tm->lock);
    ka_base_idr_for_each_entry(&tm->table_idr, table, i) {
        if (j < ESCHED_TABLE_MAX_ID) {
            table_ids[j] = (u32)table->id;
            j++;
        }
    }
    ka_task_spin_unlock_bh(&tm->lock);

    for (i = 0; i < j; i++) {
        table = esched_table_get(dev_id, table_ids[i]);
        if (table == NULL) {
            continue;
        }

        ka_task_spin_lock_bh(&table->lock);
        esched_table_free_proc_key_entry(table, pid);
        ka_task_spin_unlock_bh(&table->lock);

        esched_table_put(dev_id, table);
    }

    sched_vfree(table_ids);
}

int esched_alloc_table(u32 dev_id, struct esched_table_raw_data *raw_data, u32 *table_id)
{
    struct esched_table_mng *tm = esched_get_table_mng(dev_id);
    struct esched_table *table = NULL;
    unsigned long tasklet_para;
    int ret;

    table = esched_create_table(dev_id, raw_data);
    if (table == NULL) {
        sched_err("Failed to create table. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    ka_task_spin_lock_bh(&tm->lock);
    ret = esched_add_table(dev_id, table);
    if (ret != 0) {
        ka_task_spin_unlock_bh(&tm->lock);
        esched_destroy_table(table);
        sched_err("Failed to add table. (dev_id=%u)\n", dev_id);
        return ret;
    }
    ka_task_spin_unlock_bh(&tm->lock);

    sched_info("Alloc table success. (dev_id=%u; table_id=%d; name=%s)\n", dev_id, table->id, table->name);

    *table_id = (u32)table->id;
    tasklet_para = ((unsigned long)dev_id << TASKLET_DEVID_OFFSET) | *table_id;
    ka_system_tasklet_init(&table->task, esched_table_tasklet, tasklet_para);
    esched_table_proc_fs_add_table(tm, table);

    return 0;
}

int esched_free_table(u32 dev_id, u32 table_id)
{
    struct esched_table_mng *tm = esched_get_table_mng(dev_id);
    struct esched_table *table = NULL;
    int destroy_flag = 0;

    ka_task_spin_lock_bh(&tm->lock);
    table = ka_base_idr_find(&tm->table_idr, table_id);
    if (table == NULL) {
        ka_task_spin_unlock_bh(&tm->lock);
        sched_err("Invalid para. (dev_id=%u; table_id=%u)\n", dev_id, table_id);
        return DRV_ERROR_PARA_ERROR;
    }

    esched_del_table(dev_id, table);
    if (table->refcnt <= 0) {
        destroy_flag = 1;
    }
    ka_task_spin_unlock_bh(&tm->lock);

    esched_table_proc_fs_del_table(tm, table);
    ka_system_tasklet_kill(&table->task);

    if (destroy_flag == 1) {
        esched_destroy_table(table);
    }

    sched_info("Free table success. (dev_id=%u; table_id=%u)\n", dev_id, table_id);
    return 0;
}

STATIC void esched_free_all_table(u32 dev_id)
{
    struct esched_table_mng *tm = esched_get_table_mng(dev_id);
    struct esched_table *table = NULL;
    int i;

    ka_base_idr_for_each_entry(&tm->table_idr, table, i) {
        sched_info("Recycle table. (dev_id=%u; table_id=%d)\n", dev_id, table->id);
        (void)esched_free_table(dev_id, (u32)table->id);
    }
}

/* emu st use, not add static */
STATIC struct esched_irq_ctrl *esched_find_irq_ctrl(u32 dev_id, int irq)
{
    struct esched_table_mng *tm = esched_get_table_mng(dev_id);
    struct esched_irq_ctrl *irq_ctrl = NULL;

    ka_list_for_each_entry(irq_ctrl, &tm->irq_list, node) {
        if (irq_ctrl->irq == irq) {
            return irq_ctrl;
        }
    }

    return NULL;
}

static struct esched_irq_ctrl *esched_find_irq_ctrl_by_table(u32 dev_id, int table_id)
{
    struct esched_table_mng *tm = esched_get_table_mng(dev_id);
    struct esched_irq_ctrl *irq_ctrl = NULL;

    ka_list_for_each_entry(irq_ctrl, &tm->irq_list, node) {
        if (irq_ctrl->table->id == table_id) {
            return irq_ctrl;
        }
    }

    return NULL;
}

static struct esched_irq_ctrl *esched_create_irq_ctrl(u32 dev_id, int irq)
{
    struct esched_table_mng *tm = esched_get_table_mng(dev_id);
    struct esched_irq_ctrl *irq_ctrl = NULL;

    irq_ctrl = (struct esched_irq_ctrl *)sched_vmalloc(sizeof(struct esched_irq_ctrl),
        KA_GFP_KERNEL | __KA_GFP_ACCOUNT, KA_PAGE_KERNEL);
    if (irq_ctrl == NULL) {
        return NULL;
    }

    irq_ctrl->irq = irq;
    ka_list_add_tail(&irq_ctrl->node, &tm->irq_list);

    return irq_ctrl;
}

static void esched_destroy_irq_ctrl(u32 dev_id, struct esched_irq_ctrl *irq_ctrl)
{
    ka_list_del(&irq_ctrl->node);
    sched_vfree(irq_ctrl);
}

static void esched_irq_clear(struct esched_irq_ctrl *irq_ctrl)
{
    if (irq_ctrl->irq_clr_reg != NULL) {
        esched_table_reg_wr(irq_ctrl->irq_clr_reg, irq_ctrl->val, irq_ctrl->addr_width, irq_ctrl->endian_type);
    }
}

STATIC ka_irqreturn_t esched_table_interrupt(int irq, void *data)
{
    struct esched_irq_ctrl *irq_ctrl = (struct esched_irq_ctrl *)data;

    esched_irq_clear(irq_ctrl);

    ka_system_tasklet_schedule(&irq_ctrl->table->task);
    return KA_IRQ_HANDLED;
}

static int esched_irq_ctrl_init(struct esched_irq_ctrl *irq_ctrl, struct esched_table *table,
    struct esched_hw_info *hw_info)
{
    int ret;

    irq_ctrl->table = table;
    ret = strcpy_s(irq_ctrl->name, ESCHED_MAX_NAME_LEN, hw_info->name);
    if (ret != 0) {
        sched_err("Call strcpy_s failed. (ret=%d)\n", ret);
        return DRV_ERROR_INNER_ERR;
    }
    irq_ctrl->name[ESCHED_MAX_NAME_LEN - 1] = '\0';

    if (hw_info->irq_clr_reg == 0) {
        irq_ctrl->irq_clr_reg = NULL;
        return 0;
    }

    irq_ctrl->addr_width = hw_info->addr_width;
    irq_ctrl->val = hw_info->val;
    irq_ctrl->endian_type = hw_info->endian_type;
    irq_ctrl->irq_clr_reg = (void __iomem *)hw_info->irq_clr_reg;

    return 0;
}

int esched_hw_bind_table(u32 dev_id, struct esched_hw_info *hw_info, u32 table_id)
{
    struct esched_table_mng *tm = esched_get_table_mng(dev_id);
    struct esched_irq_ctrl *irq_ctrl = NULL;
    struct esched_table *table = NULL;
    int ret, irq = hw_info->irq;

    ka_task_mutex_lock(&tm->mutex);

    irq_ctrl = esched_find_irq_ctrl_by_table(dev_id, table_id);
    if (irq_ctrl != NULL) {
        ka_task_mutex_unlock(&tm->mutex);
        sched_err("Table has been bind. (dev_id=%u; table_id=%u; irq_ctrl->irq=%d)\n", dev_id, table_id, irq_ctrl->irq);
        return DRV_ERROR_REPEATED_USERD;
    }

    irq_ctrl = esched_find_irq_ctrl(dev_id, irq);
    if (irq_ctrl != NULL) {
        ka_task_mutex_unlock(&tm->mutex);
        sched_err("Irq has been bind. (dev_id=%u; table_id=%u; irq=%d)\n", dev_id, table_id, irq);
        return DRV_ERROR_REPEATED_USERD;
    }

    table = esched_table_get(dev_id, table_id);
    if (table == NULL) {
        ka_task_mutex_unlock(&tm->mutex);
        sched_err("Invalid para. (dev_id=%u; table_id=%u; irq=%d)\n", dev_id, table_id, irq);
        return DRV_ERROR_PARA_ERROR;
    }

    irq_ctrl = esched_create_irq_ctrl(dev_id, irq);
    if (irq_ctrl == NULL) {
        esched_table_put(dev_id, table);
        ka_task_mutex_unlock(&tm->mutex);
        sched_err("Add irq failed. (dev_id=%u; table_id=%u; irq=%d)\n", dev_id, table_id, irq);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    ret = esched_irq_ctrl_init(irq_ctrl, table, hw_info);
    if (ret != 0) {
        esched_destroy_irq_ctrl(dev_id, irq_ctrl);
        esched_table_put(dev_id, table);
        ka_task_mutex_unlock(&tm->mutex);
        sched_err("Failed to init irq.(dev_id=%u; table_id=%u; irq=%d; ret=%d)\n", dev_id, table_id, irq, ret);
        return ret;
    }

#ifdef CFG_BUILD_DEBUG
    if (ka_base_strncmp(irq_ctrl->name, esched_hlt_irq_name_prefix, ka_base_strlen(esched_hlt_irq_name_prefix)) == 0) {
        ka_task_mutex_unlock(&tm->mutex);
        sched_info("Test irq. (dev_id=%u; table_id=%u; irq=%d; name=%s)\n", dev_id, table_id, irq, irq_ctrl->name);
        return 0;
    }
#endif

    ret = ka_system_request_irq((u32)irq, esched_table_interrupt, 0, "esched_take_over", (void *)irq_ctrl);
    if (ret != 0) {
        esched_destroy_irq_ctrl(dev_id, irq_ctrl);
        esched_table_put(dev_id, table);
        ka_task_mutex_unlock(&tm->mutex);
        sched_err("Call request_irq failed. (dev_id=%u; table_id=%u; irq=%d; ret=%d)\n", dev_id, table_id, irq, ret);
        return ret;
    }

    ka_task_mutex_unlock(&tm->mutex);

    sched_info("Bind irq success. (dev_id=%u; table_id=%u; irq=%d; name=%s)\n", dev_id, table_id, irq, irq_ctrl->name);

    return 0;
}

int esched_hw_unbind_table(u32 dev_id, struct esched_hw_info *hw_info)
{
    struct esched_table_mng *tm = esched_get_table_mng(dev_id);
    struct esched_irq_ctrl *irq_ctrl = NULL;
    int irq = hw_info->irq;

    ka_task_mutex_lock(&tm->mutex);

    irq_ctrl = esched_find_irq_ctrl(dev_id, irq);
    if (irq_ctrl == NULL) {
        ka_task_mutex_unlock(&tm->mutex);
        sched_err("Invalid para, irq has not been bind. (dev_id=%u; irq=%d)\n", dev_id, irq);
        return DRV_ERROR_PARA_ERROR;
    }

    sched_info("Unbind irq success. (dev_id=%u; table_id=%d; irq=%d)\n", dev_id, irq_ctrl->table->id, irq);

#ifdef CFG_BUILD_DEBUG
    if (ka_base_strncmp(irq_ctrl->name, esched_hlt_irq_name_prefix, ka_base_strlen(esched_hlt_irq_name_prefix)) == 0) {
        esched_table_put(dev_id, irq_ctrl->table);
        esched_destroy_irq_ctrl(dev_id, irq_ctrl);
        ka_task_mutex_unlock(&tm->mutex);
        return 0;
    }
#endif

    (void)ka_system_free_irq((u32)irq_ctrl->irq, (void *)irq_ctrl);
    esched_table_put(dev_id, irq_ctrl->table);
    esched_destroy_irq_ctrl(dev_id, irq_ctrl);

    ka_task_mutex_unlock(&tm->mutex);
    return 0;
}

static void esched_all_hw_unbind_table(u32 dev_id)
{
    struct esched_table_mng *tm = esched_get_table_mng(dev_id);
    struct esched_irq_ctrl *irq_ctrl = NULL, *tmp = NULL;
    struct esched_hw_info hw_info;

    ka_list_for_each_entry_safe(irq_ctrl, tmp, &tm->irq_list, node) {
        sched_info("Recycle hw. (dev_id=%u; table_id=%d; irq=%d)\n", dev_id, irq_ctrl->table->id, irq_ctrl->irq);
        hw_info.irq = irq_ctrl->irq;
        (void)esched_hw_unbind_table(dev_id, &hw_info);
    }
}

int esched_table_mng_init(u32 dev_id)
{
    struct esched_table_mng *tmp = NULL;

    if ((dev_id >= SCHED_MAX_CHIP_NUM) || (table_mng[dev_id] != NULL)) {
        sched_err("The devid is invalid or already inited. (dev_id=%u; max=%d)\n", dev_id, SCHED_MAX_CHIP_NUM);
        return DRV_ERROR_INVALID_DEVICE;
    }

    tmp = (struct esched_table_mng *)sched_vzalloc(sizeof(struct esched_table_mng));
    if (tmp == NULL) {
        sched_err("Failed to alloc memory. (size=0x%lx; dev_id=%u)\n", sizeof(struct esched_table_mng), dev_id);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    tmp->dev_id = dev_id;
    tmp->table_num = 0;
    KA_INIT_LIST_HEAD(&tmp->irq_list);
    ka_base_idr_init(&tmp->table_idr);
    ka_task_mutex_init(&tmp->mutex);
    ka_task_spin_lock_init(&tmp->lock);
    esched_table_proc_fs_add_dev(tmp);

    table_mng[dev_id] = tmp;
    sched_info("Table mng init success. (dev_id=%u)\n", dev_id);
    return 0;
}

void esched_table_mng_uninit(u32 dev_id)
{
    if ((dev_id >= SCHED_MAX_CHIP_NUM) || (table_mng[dev_id] == NULL)) {
        sched_err("The devid is invalid or uninited. (dev_id=%u; max=%d)\n", dev_id, SCHED_MAX_CHIP_NUM);
        return;
    }

    esched_all_hw_unbind_table(dev_id);
    esched_free_all_table(dev_id);
    esched_table_proc_fs_del_dev(table_mng[dev_id]);

    sched_vfree(table_mng[dev_id]);
    table_mng[dev_id] = NULL;

    sched_info("Table mng uninit success. (dev_id=%u)\n", dev_id);
}

