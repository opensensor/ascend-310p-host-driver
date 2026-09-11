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

#include "ka_fs_pub.h"

#include "securec.h"
#include "esched.h"
#include "esched_table.h"

#define PROC_FS_NAME_LEN 32
#define PROC_FS_MODE 0400
#define BYTE_NUM_PER_LINE 16

ka_proc_dir_entry_t *top_entry = NULL;

static void esched_table_proc_fs_format_dir_name(char *name, int len, const char *prefix, u32 id)
{
    if (sprintf_s(name, len, "%s_%u", prefix, id) <= 0) {
#ifndef EMU_ST
        sched_warn("Sprintf_s warn. (prefix=%s; id=%u)\n", prefix, id);
#endif
    }
}

static void esched_table_value_show(ka_seq_file_t *seq, u8 *value, u32 len)
{
    u32 i;

    for (i = 0; i < len; i++) {
        ka_fs_seq_printf(seq, "%02x ", (u32)value[i]);
    }
}

static void esched_table_mask_bit_show(ka_seq_file_t *seq, struct esched_table_mask_bit *mask_bit, u32 len)
{
    u32 i;

    for (i = 0; i < len; i++) {
        ka_fs_seq_printf(seq, "bit %u, byte offset %u, bit offset %u\n", i, (u32)mask_bit[i].byte_offset,
            (u32)mask_bit[i].bit_offset);
    }
}

static void esched_table_key_mask_show(ka_seq_file_t *seq, struct esched_table_key_mask *key_mask)
{
    ka_fs_seq_printf(seq, "\n");
    ka_fs_seq_printf(seq, "key mask:\n");
    esched_table_value_show(seq, key_mask->mask, key_mask->mask_len);
    ka_fs_seq_printf(seq, "\n");
    ka_fs_seq_printf(seq, "mask_len %u, key_len %u, mask_bit_num %u\n",
        key_mask->mask_len, key_mask->key_len, key_mask->mask_bit_num);
    esched_table_mask_bit_show(seq, key_mask->mask_bit, key_mask->mask_bit_num);
}

static void esched_table_phase_head_show(ka_seq_file_t *seq, struct esched_cq_phase_head_ctrl *head)
{
    ka_fs_seq_printf(seq, "phase_head:\n");
    ka_fs_seq_printf(seq, "reg_addr %pK, addr_width %u, endian_type %u, offset %u, mask 0x%llx, overlay value 0x%llx\n",
        head->reg_addr, head->addr_width, head->endian_type, head->offset, head->mask, head->overlay_value);
    ka_fs_seq_printf(seq, "phase_value: mask 0x%llx, offset %u, phase %u, ring_step %u, phase_range %u\n",
        head->head_phase.mask, (u32)head->head_phase.offset, (u32)head->head_phase.phase,
        (u32)head->head_phase.ring_step, (u32)head->head_phase.phase_range);
}

static void esched_table_cq_ctrl_show(ka_seq_file_t *seq, struct esched_cq_ctrl *cq_ctrl)
{
    ka_fs_seq_printf(seq, "\n");
    ka_fs_seq_printf(seq, "cq_ctrl:\n");
    ka_fs_seq_printf(seq, "type %u, cq_depth %u, cqe_size %u, head %u\n",
        cq_ctrl->type, cq_ctrl->cq_depth, cq_ctrl->cqe_size, cq_ctrl->head);
    ka_fs_seq_printf(seq, "phase %u, ring_step %u, mask %x, phase_range %u, mask_bit_num %u\n",
        (u32)cq_ctrl->cq_phase.phase, (u32)cq_ctrl->cq_phase.ring_step, (u32)cq_ctrl->cq_phase.mask,
        (u32)cq_ctrl->cq_phase.phase_range, cq_ctrl->cq_phase.mask_bit_num);
    esched_table_mask_bit_show(seq, &cq_ctrl->cq_phase.mask_bit, 1);
    esched_table_phase_head_show(seq, &cq_ctrl->cq_phase.phase_head);
}

static void esched_table_stat_show(ka_seq_file_t *seq, struct esched_table_stat *stat)
{
    ka_fs_seq_printf(seq, "\n");
    ka_fs_seq_printf(seq, "stat:\n");
    ka_fs_seq_printf(seq, "task_run_num:              %llu\n", stat->task_run_num);
    ka_fs_seq_printf(seq, "send_event_num:            %llu\n", stat->send_event_num);
    ka_fs_seq_printf(seq, "send_event_fail_num:       %llu\n", stat->send_event_fail_num);
    ka_fs_seq_printf(seq, "next_table_num:            %llu\n", stat->next_table_num);
    ka_fs_seq_printf(seq, "next_table_mismatch_num:   %llu\n", stat->next_table_mismatch_num);
    ka_fs_seq_printf(seq, "drop_num:                  %llu\n", stat->drop_num);
    ka_fs_seq_printf(seq, "key_entry_mismatch_num:    %llu\n", stat->key_entry_mismatch_num);
}

static void esched_table_key_entry_show(ka_seq_file_t *seq, u32 id,
    struct esched_table_key_entry *key_entry, u32 key_len)
{
    ka_fs_seq_printf(seq, "id %8u, match num %16llu, pid %8d; ", id, key_entry->stat.match_num, key_entry->pid);
    ka_fs_seq_printf(seq, "key: ");
    esched_table_value_show(seq, key_entry->key, key_len);
    ka_fs_seq_printf(seq, "; entry: ");
    if (key_entry->entry.op == ESCHED_TABLE_OP_SEND_EVENT) {
        struct esched_table_op_send_event *send_event = &key_entry->entry.send_event;
        ka_fs_seq_printf(seq, "op 'send event', dev_id %u, dst_engine %u, dev_id %u, gid %u, ",
            send_event->dev_id, send_event->dst_engine, send_event->policy, send_event->gid);
        ka_fs_seq_printf(seq, "event_id %u, sub_event_id %u, data_src %u",
            send_event->event_id, send_event->sub_event_id, send_event->data_src);
        if (send_event->data_src == ESCHED_DATA_SRC_USR_CFG) {
            ka_fs_seq_printf(seq, " user cfg data: ");
            esched_table_value_show(seq, send_event->data, send_event->data_len);
        }
    } else if (key_entry->entry.op == ESCHED_TABLE_OP_NEXT_TABLE) {
        struct esched_table_op_next_table *next_table = &key_entry->entry.next_table;
        ka_fs_seq_printf(seq, "op 'next table', dev_id %u, table_id %u", next_table->dev_id, next_table->table_id);
    } else if (key_entry->entry.op == ESCHED_TABLE_OP_DROP) {
        ka_fs_seq_printf(seq, "op 'drop'");
    }
    ka_fs_seq_printf(seq, "\n");
}

static void esched_table_entry_show(ka_seq_file_t *seq, struct esched_table *table)
{
    struct esched_table_key_entry *key_entry = NULL;
    ka_hlist_node_t *hnode = NULL;
    u32 bkt, id = 0;

    ka_fs_seq_printf(seq, "\n");
    ka_fs_seq_printf(seq, "entrys:\n");

    ka_task_spin_lock_bh(&table->lock);

    ka_hash_for_each_safe(table->entry_hash_table, bkt, hnode, key_entry, link) {
        esched_table_key_entry_show(seq, id, key_entry, table->key_mask.key_len);
        id++;
    }

    ka_task_spin_unlock_bh(&table->lock);
}

static void esched_table_mismatched_cqe_show(ka_seq_file_t *seq, struct esched_table *table)
{
    u8 *cqe = table->last_mismatched_cqe;
    int size = table->cq_ctrl.cqe_size;
    int index;

    if (table->stat.key_entry_mismatch_num == 0) {
        return;
    }

    ka_fs_seq_printf(seq, "\n");
    ka_fs_seq_printf(seq, "The latest mismatched cqe (size %d):\n", size);
    for (index = 0; index < size; index++) {
        if ((index != 0) && (index % BYTE_NUM_PER_LINE == 0)) {
            ka_fs_seq_printf(seq, "\n");
        }
        ka_fs_seq_printf(seq, "0x%02x ", (u32)cqe[index]);
    }
    ka_fs_seq_printf(seq, "\n");
}

static int esched_table_info_show(ka_seq_file_t *seq, void *offset)
{
    struct esched_table *table = (struct esched_table *)seq->private;

    ka_fs_seq_printf(seq, "id %d, name %s, refcnt %d, entry_num %u, max_entry_num %u\n",
        table->id, table->name, table->refcnt, table->entry_num, table->max_entry_num);

    esched_table_key_mask_show(seq, &table->key_mask);
    esched_table_cq_ctrl_show(seq, &table->cq_ctrl);
    esched_table_stat_show(seq, &table->stat);
    esched_table_mismatched_cqe_show(seq, table);
    esched_table_entry_show(seq, table);

    return 0;
}

STATIC int esched_table_info_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, esched_table_info_show, ka_base_pde_data(inode));
}

STATIC const ka_procfs_ops_t table_info_ops = {
    ka_fs_init_pf_owner(KA_THIS_MODULE)
    ka_fs_init_pf_open(esched_table_info_open)
    ka_fs_init_pf_read(ka_fs_seq_read)
    ka_fs_init_pf_lseek(ka_fs_seq_lseek)
    ka_fs_init_pf_release(ka_fs_single_release)
};

void esched_table_proc_fs_add_table(struct esched_table_mng *tm, struct esched_table *table)
{
    ka_proc_dir_entry_t *entry = NULL;
    char name[PROC_FS_NAME_LEN];

    esched_table_proc_fs_format_dir_name(name, PROC_FS_NAME_LEN, "id", table->id);
    entry = ka_fs_proc_mkdir(name, tm->table_entry);
    if (entry == NULL) {
#ifndef EMU_ST
        sched_warn("Create table entry warn. (table_id=%d)\n", table->id);
#endif
        return;
    }

    (void)ka_fs_proc_create_data("info", PROC_FS_MODE, entry, &table_info_ops, table);
    sched_info("Add table entry. (table=%u; table_id=%d)\n", tm->dev_id, table->id);
}

void esched_table_proc_fs_del_table(struct esched_table_mng *tm, struct esched_table *table)
{
    char name[PROC_FS_NAME_LEN];

    sched_info("Del table entry. (table=%u; table_id=%d)\n", tm->dev_id, table->id);

    esched_table_proc_fs_format_dir_name(name, PROC_FS_NAME_LEN, "id", table->id);
    (void)ka_fs_remove_proc_subtree(name, tm->table_entry);
}

static int esched_table_dev_irq_show(ka_seq_file_t *seq, void *offset)
{
    struct esched_table_mng *tm = (struct esched_table_mng *)seq->private;
    struct esched_irq_ctrl *irq_ctrl = NULL;

    ka_fs_seq_printf(seq, "irq, name, bind table, addr_width, endian_type, irq_clr_reg, val\n");

    ka_task_mutex_lock(&tm->mutex);
    ka_list_for_each_entry(irq_ctrl, &tm->irq_list, node) {
        ka_fs_seq_printf(seq, "%d, %s, %d, %u, %u, %pK, 0x%llx\n", irq_ctrl->irq, irq_ctrl->name, irq_ctrl->table->id,
            irq_ctrl->addr_width, irq_ctrl->endian_type, irq_ctrl->irq_clr_reg, irq_ctrl->val);
    }
    ka_task_mutex_unlock(&tm->mutex);

    return 0;
}

int esched_table_dev_irq_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, esched_table_dev_irq_show, ka_base_pde_data(inode));
}

static const ka_procfs_ops_t dev_irq_ops = {
    ka_fs_init_pf_owner(KA_THIS_MODULE)
    ka_fs_init_pf_open(esched_table_dev_irq_open)
    ka_fs_init_pf_read(ka_fs_seq_read)
    ka_fs_init_pf_lseek(ka_fs_seq_lseek)
    ka_fs_init_pf_release(ka_fs_single_release)
};

void esched_table_proc_fs_add_dev(struct esched_table_mng *tm)
{
    char name[PROC_FS_NAME_LEN];

    esched_table_proc_fs_format_dir_name(name, PROC_FS_NAME_LEN, "dev_", tm->dev_id);

    tm->dev_entry = ka_fs_proc_mkdir(name, top_entry);
    if (tm->dev_entry == NULL) {
        sched_err("Create dev entry dir failed.\n");
        return;
    }

    tm->table_entry = ka_fs_proc_mkdir(name, tm->dev_entry);
    if (tm->table_entry == NULL) {
        sched_err("Create table entry dir failed.\n");
        return;
    }

    (void)ka_fs_proc_create_data("take_over_irq", PROC_FS_MODE, tm->dev_entry, &dev_irq_ops, tm);

    sched_info("Add dev entry. (dev_id=%u)\n", tm->dev_id);

    return;
}

void esched_table_proc_fs_del_dev(struct esched_table_mng *tm)
{
    if (tm->table_entry != NULL) {
        ka_fs_proc_remove(tm->table_entry);
        tm->table_entry = NULL;
    }

    if (tm->dev_entry != NULL) {
        ka_fs_proc_remove(tm->dev_entry);
        tm->dev_entry = NULL;
    }

    sched_info("Del dev entry. (dev_id=%u)\n", tm->dev_id);

    return;
}

void esched_table_proc_fs_init(void)
{
    top_entry = ka_fs_proc_mkdir("irq_sched", NULL);
    if (top_entry == NULL) {
        sched_err("Create top entry dir failed.\n");
        return;
    }
}

void esched_table_proc_fs_uninit(void)
{
    (void)ka_fs_remove_proc_subtree("irq_sched", NULL);
}
