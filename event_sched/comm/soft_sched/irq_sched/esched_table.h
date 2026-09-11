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

#ifndef ESCHED_TABLE_H
#define ESCHED_TABLE_H

#include "ka_define.h"
#include "ka_fs_pub.h"
#include "ka_task_pub.h"
#include "ka_hashtable_pub.h"
#include "ka_list_pub.h"
#include "ka_base_pub.h"
#include "ka_system_pub.h"

#include "ascend_kernel_hal.h"
#include "ascend_hal_define.h"

#define ESCHED_TASK_HANDLE_ENTRY_NUM 32
#define ESCHED_MAX_NAME_LEN 32

#define ESCHED_TABLE_MAX_ID (1 << 20)

#define SCHED_TABLES_HASH_TABLE_BIT 10
#define SCHED_TABLES_HASH_TABLE_SIZE (0x1 << SCHED_TABLES_HASH_TABLE_BIT)
#define SCHED_TABLES_HASH_TABLE_MASK (SCHED_TABLES_HASH_TABLE_SIZE - 1)

#define SCHED_PHASE_MASK_BYTE_LEN  8

struct esched_table_entry_stat {
    u64 match_num;
};

struct esched_table_key_entry {
    ka_hlist_node_t link; /* hash find link */
    int pid;
    struct esched_table_entry_stat stat;
    u32 raw_data_len;
    u8 *raw_data;
    u8 key[ESCHED_MAX_KEY_LEN];
    struct esched_table_entry entry;
};

#define BYTE_WIDTH 8
struct esched_table_mask_bit {
    u8 byte_offset;
    u8 bit_offset;
};

struct esched_table_key_mask {
    u8 mask[ESCHED_MAX_KEY_LEN];
    u32 mask_len;
    u32 key_len;
    u32 mask_bit_num;
    struct esched_table_mask_bit *mask_bit;
};

struct esched_head_phase {
    u64 mask;
    u8 offset;
    u8 phase;
    u8 ring_step;
    u8 phase_range;
};

struct esched_cq_phase_head_ctrl {
    u32 addr_width; /* 0: 32 bit, 1: 64 bit */
    u32 endian_type; /* 0: little endian, 1: big endian */
    void __iomem *reg_addr;
    u32 offset;
    u64 mask;
    u64 overlay_value;
    struct esched_head_phase head_phase;
};

struct esched_cq_phase_ctrl {
    u8 phase;
    u8 ring_step;
    u8 mask;
    u8 phase_range;
    u32 mask_bit_num;
    struct esched_table_mask_bit mask_bit;
    struct esched_cq_phase_head_ctrl phase_head;
};

struct esched_cq_ptr_ctrl {
    u8 *tail_addr;
};

struct esched_cq_ctrl {
    u32 type;
    u32 cq_depth;
    u32 cqe_size;
    u32 head;
    u32 round;
    u8 *cq_addr;
    union {
        struct esched_cq_phase_ctrl cq_phase;
        struct esched_cq_ptr_ctrl cq_ptr;
    };
};

struct esched_table_stat {
    u64 task_run_num;
    u64 send_event_num;
    u64 send_event_fail_num;
    u64 next_table_num;
    u64 next_table_mismatch_num;
    u64 drop_num;
    u64 key_entry_mismatch_num;
};

struct esched_table {
    u32 dev_id;
    int id;
    int refcnt;
    u32 entry_num;
    u32 max_entry_num;
    u32 endian_type;  /* 0: little endian, 1: big endian */
    char name[ESCHED_MAX_NAME_LEN];
    struct esched_table_stat stat;
    u8 last_mismatched_cqe[ESCHED_MAX_CQE_SIZE];
    struct esched_table_key_mask key_mask;
    union {
        struct esched_cq_ctrl cq_ctrl;
    };
    ka_tasklet_struct_t task;
    ka_spinlock_t lock;
    KA_DECLARE_HASHTABLE(entry_hash_table, SCHED_TABLES_HASH_TABLE_BIT);
};

struct esched_irq_ctrl {
    ka_list_head_t node;
    int irq;
    char name[ESCHED_MAX_NAME_LEN];
    u32 addr_width;  /* 0: 32 bit, 1: 64 bit */
    u32 endian_type; /* 0: little endian, 1: big endian */
    void __iomem * irq_clr_reg;
    u64 val;
    struct esched_table *table;
};

struct esched_table_mng {
    u32 dev_id;
    u32 table_num;
    ka_spinlock_t lock;
    ka_idr_t table_idr;
    ka_mutex_t mutex;
    ka_list_head_t irq_list;
    ka_proc_dir_entry_t *table_entry;
    ka_proc_dir_entry_t *dev_entry;
};

u32 esched_get_non_zero_bit_num(u8 *mask, u32 len);

void esched_table_mng_clear(void);
int esched_add_table_entry(u32 dev_id, u32 table_id, int pid, struct esched_table_key *key,
    struct esched_table_entry *entry);
int esched_del_table_entry(u32 dev_id, u32 table_id, int pid, struct esched_table_key *key);
int esched_query_table_entry_stat(u32 dev_id, u32 table_id, struct esched_table_key *key,
    struct esched_table_key_entry_stat *stat);
void esched_table_recycle_proc_owner_entry(u32 dev_id, int pid);
int esched_alloc_table(u32 dev_id, struct esched_table_raw_data *raw_data, u32 *table_id);
int esched_free_table(u32 dev_id, u32 table_id);
int esched_hw_bind_table(u32 dev_id, struct esched_hw_info *hw_info, u32 table_id);
int esched_hw_unbind_table(u32 dev_id, struct esched_hw_info *hw_info);
int esched_table_mng_init(u32 dev_id);
void esched_table_mng_uninit(u32 dev_id);

#endif

