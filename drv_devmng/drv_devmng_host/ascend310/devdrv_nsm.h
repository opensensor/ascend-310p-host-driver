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

#ifndef __DEVDRV_NSM_H
#define __DEVDRV_NSM_H

#define DEVDRV_HCCL_NAME_SIZE (64)

struct ipc_notify_node {
    u32 delete_flag; /* 1 means it start to deleting */
    u32 dev_id;
    u32 notify_id;

    u32 key;
    char name[DEVDRV_HCCL_NAME_SIZE];

    atomic_t ref;

    struct hlist_node link;

    struct list_head create_list_node;
    struct list_head open_list_node;
    struct mutex mutex;
};

enum devdrv_ipc_type {
    DEVDRV_IPC_NOTIFY = 0,
    DEVDRV_IPC_TYPE_MAX,
};

int devdrv_ipc_create(const char *ipc_name, void **ipc_node, enum devdrv_ipc_type ipc_type, u32 flag);
int devdrv_ipc_del(const char *ipc_name, u32 max_len, enum devdrv_ipc_type);
int devdrv_ipc_find(const char *ipc_name, void **ipc_node, enum devdrv_ipc_type ipc_type);

#endif /* __DEVDRV_NSM_H */
