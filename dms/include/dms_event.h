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

#ifndef __DMS_EVENT_H__
#define __DMS_EVENT_H__
#include <linux/hashtable.h>

#include "ka_hashtable_pub.h"

#define DMS_EVENT_CMD_NAME "DMS_EVENT"

#define DMS_EVENT_ERROR_ARRAY_NUM (128UL)

#define EVENT_ID_HASH_TABLE_BIT 8
#define EVENT_ID_HASH_TABLE_SIZE (0x1 << EVENT_ID_HASH_TABLE_BIT)
#define EVENT_ID_HASH_TABLE_MASK (EVENT_ID_HASH_TABLE_SIZE - 1)

struct dms_converge_event_list {
    struct list_head head;
    unsigned int event_num;
    unsigned int health_code;
    struct mutex lock;
};

struct dms_sensor_reported_list {
    struct list_head head;
    unsigned int reported_num;
    struct mutex lock;
};

struct dms_converge_htable {
    KA_DECLARE_HASHTABLE(htable, EVENT_ID_HASH_TABLE_BIT);
    struct mutex lock;

    bool converge_switch;
    bool converge_validity;
    bool need_free;
};

struct dms_event_dfx_table {
    atomic_t recv_from_sensor[DMS_EVENT_TYPE_MAX];
    atomic_t report_to_consumer[DMS_EVENT_TYPE_MAX];
    struct list_head mask_list;
    struct mutex lock;
};

enum dms_event_poll_flag {
    EVENT_POLL_EXIT = 0,
    EVENT_POLL_WORK,
    EVENT_POLL_STOP,
    EVENT_POLL_MAX
};

typedef int (*add_exception_handle)(u32 devid, u32 code, struct timespec stamp);

int dms_event_set_add_exception_handle(add_exception_handle func);
int dms_event_mask_by_phyid(u32 phyid, u32 event_id, u8 mask);

int dms_event_init(void);
void dms_event_exit(void);
void dms_event_release_proc(int tgid, int pid);
void dms_event_cb_release(int owner_pid);
int dms_event_is_converge(void);
int dms_event_clear_by_phyid(u32 phyid);
void dms_event_set_poll_task_flag(enum dms_event_poll_flag flag);

#endif
