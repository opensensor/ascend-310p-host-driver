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
#ifndef DEVDRV_NSM_H
#define DEVDRV_NSM_H

#ifdef CFG_FEATURE_IPC_NOTIFY
#include "devdrv_user_common.h"
#include "tsdrv_kernel_common.h"
#include "ka_hashtable_pub.h"
#include "ka_list_pub.h"
#include "ka_memory_pub.h"
#include "ka_task_pub.h"
#include "ka_base_pub.h"
#define RANDOM_LENGTH          20

struct ipc_open_info {
    ka_pid_t pid;
    u64 set_time;
    ka_atomic_t ref;
    struct ipc_notify_node *node;
    ka_list_head_t open_list_node;
};

struct ipc_notify_node {
    u32 status; /* 1 means it start to deleting */
    u32 dev_id;
    u32 tsid;
    u32 notify_id;
    u32 type;

    u32 tag;
    char name[DEVDRV_IPC_NAME_SIZE];
    struct ipc_open_info open_info[DEVDRV_PID_MAX_NUM];
    ka_hlist_node_t link;
    ka_list_head_t create_list_node;
    ka_mutex_t mutex;
    u32 flag;
    bool need_wlist;
};

enum devdrv_ipc_type {
    DEVDRV_IPC_NOTIFY = 0,
    DEVDRV_IPC_TYPE_MAX
};

enum devdrv_ipc_ref_status {
    DEVDRV_IPC_REF_INIT = 1,
    DEVDRV_IPC_REF_MAX
};

int devdrv_ipc_create(char *ipc_name, void **ipc_node, enum devdrv_ipc_type ipc_type, u32 flag);
int devdrv_ipc_del(const char *ipc_name, u32 max_len, enum devdrv_ipc_type ipc_type);
int devdrv_ipc_find(const char *ipc_name, void **ipc_node, enum devdrv_ipc_type ipc_type);
#else
enum devdrv_ipc_type {
    DEVDRV_IPC_NOTIFY = 0,
    DEVDRV_IPC_TYPE_MAX
};
static inline int devdrv_ipc_create(char *ipc_name, void **ipc_node, enum devdrv_ipc_type ipc_type, u32 flag)
{
    return 0;
}
static inline int devdrv_ipc_del(const char *ipc_name, u32 max_len, enum devdrv_ipc_type ipc_type)
{
    return 0;
}
static inline int devdrv_ipc_find(const char *ipc_name, void **ipc_node, enum devdrv_ipc_type ipc_type)
{
    return 0;
}
#endif /* CFG_FEATURE_IPC_NOTIFY */
int devdrv_get_random(char *sign, u32 len);
bool devdrv_ipc_notify_is_belong_to_proc(u32 dev_id, u32 tsid, int pid, int res_type, u32 res_id);
#endif /* DEVDRV_NSM_H */

