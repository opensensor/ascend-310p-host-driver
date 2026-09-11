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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/hashtable.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/vmalloc.h>

#ifndef DEVMNG_UT
#include "devdrv_common.h"
#include "dbl_mem_alloc_interface.h"
#include "devdrv_nsm.h"

static DEFINE_SPINLOCK(devdrv_hash_notify_lock);
static DEFINE_HASHTABLE(ipc_notify_hashtable, 4);

STATIC int devdrv_ipc_notify_create(const char *ipc_name, u32 key, void **ipc_node, u32 flag);
STATIC int devdrv_ipc_notify_find(const char *ipc_name, u32 key, void **ipc_node);
STATIC int devdrv_ipc_notify_del(const char *ipc_name, u32 key);

int (* const ipc_create_handler[DEVDRV_IPC_TYPE_MAX])(const char *ipc_name, u32 key, void **ipc_node, u32 flag) = {
    [DEVDRV_IPC_NOTIFY] = devdrv_ipc_notify_create,
};

int (* const ipc_find_handler[DEVDRV_IPC_TYPE_MAX])(const char *ipc_name, u32 key, void **ipc_node) = {
    [DEVDRV_IPC_NOTIFY] = devdrv_ipc_notify_find,
};

int (* const ipc_del_handler[DEVDRV_IPC_TYPE_MAX])(const char *ipc_name, u32 key) = {
    [DEVDRV_IPC_NOTIFY] = devdrv_ipc_notify_del,
};

int devdrv_creat_ipc_name(char *ipc_name, unsigned int len)
{
    return 0;
}
EXPORT_SYMBOL(devdrv_creat_ipc_name);

u32 devdrv_mc_get_str_elfhash(const char *ipc_name, size_t len)
{
    u32 hash = 0;
    u32 tmp_hash = 0;
    u32 tmp_len = 0;
    const u8 *ipc_char = (u8 *)ipc_name;

    while ((*ipc_char != 0) && (tmp_len < len)) {
        hash = (hash << 4) + *ipc_char;
        tmp_hash = hash & 0xF0000000;

        if (!tmp_hash) {
            hash ^= tmp_hash >> 24;
            hash ^= tmp_hash;
        }
        ipc_char++;
        tmp_len += sizeof(u8);
    }

    return hash;
}

STATIC int devdrv_ipc_notify_create(const char *ipc_name, u32 key, void **ipc_node, u32 flag)
{
    struct ipc_notify_node *notify_node = NULL;
    int i;

    spin_lock(&devdrv_hash_notify_lock);
    /*lint -e666 */
    hash_for_each_possible(ipc_notify_hashtable, notify_node, link, key) if (!strcmp(notify_node->name, ipc_name)) {
        spin_unlock(&devdrv_hash_notify_lock);
        return -EINVAL;
    }
    notify_node = dbl_kzalloc(sizeof(*notify_node), GFP_ATOMIC | __GFP_ACCOUNT);
    if (notify_node == NULL) {
        spin_unlock(&devdrv_hash_notify_lock);
        return -ENOMEM;
    }

    for (i = 0; i < DEVDRV_HCCL_NAME_SIZE; i++) {
        notify_node->name[i] = ipc_name[i];
    }

    hash_add(ipc_notify_hashtable, &notify_node->link, key);
    /*lint +e666 */
    spin_unlock(&devdrv_hash_notify_lock);

    *ipc_node = (void *)notify_node;

    return 0;
}

int devdrv_ipc_create(const char *ipc_name, void **ipc_node, enum devdrv_ipc_type ipc_type, u32 flag)
{
    u32 key = 0;
    int ret;

    if (ipc_name == NULL || ipc_node == NULL) {
        return -EINVAL;
    }
    if (ipc_type >= DEVDRV_IPC_TYPE_MAX || ipc_create_handler[ipc_type] == NULL) {
        devdrv_drv_err("devdrv_ipc_type = %d\n", ipc_type);
        return -ENODEV;
    }

    key = devdrv_mc_get_str_elfhash(ipc_name, strlen(ipc_name) + 1);
    ret = ipc_create_handler[ipc_type](ipc_name, key, ipc_node, flag);

    return ret;
}
EXPORT_SYMBOL(devdrv_ipc_create);

STATIC int devdrv_ipc_notify_find(const char *ipc_name, u32 key, void **ipc_node)
{
    struct ipc_notify_node *notify_node = NULL;

    spin_lock(&devdrv_hash_notify_lock);
    /*lint -e666 */
    hash_for_each_possible(ipc_notify_hashtable, notify_node, link, key) if (!strcmp(notify_node->name, ipc_name)) {
        spin_unlock(&devdrv_hash_notify_lock);
        *ipc_node = notify_node;
        return 0;
    }
    /*lint +e666 */
    spin_unlock(&devdrv_hash_notify_lock);

    return -EINVAL;
}

int devdrv_ipc_find(const char *ipc_name, void **ipc_node, enum devdrv_ipc_type ipc_type)
{
    u32 key = 0;
    int ret;

    if (ipc_name == NULL || ipc_node == NULL) {
        return -EINVAL;
    }
    if (ipc_type >= DEVDRV_IPC_TYPE_MAX || ipc_find_handler[ipc_type] == NULL) {
        devdrv_drv_err("invalid parameter, ipc_type = %d\n", ipc_type);
        return -ENODEV;
    }

    key = devdrv_mc_get_str_elfhash(ipc_name, strlen(ipc_name) + 1);
    ret = ipc_find_handler[ipc_type](ipc_name, key, ipc_node);

    return ret;
}
EXPORT_SYMBOL(devdrv_ipc_find);

STATIC int devdrv_ipc_notify_del(const char *ipc_name, u32 key)
{
    struct ipc_notify_node *notify_node = NULL;

    spin_lock(&devdrv_hash_notify_lock);
    /*lint -e666 */
    hash_for_each_possible(ipc_notify_hashtable, notify_node, link, key) if (!strcmp(notify_node->name, ipc_name)) {
        hash_del(&notify_node->link);
        spin_unlock(&devdrv_hash_notify_lock);
        goto node_free;
    }
    /*lint +e666 */
    spin_unlock(&devdrv_hash_notify_lock);

    return -EINVAL;

node_free:
    dbl_kfree(notify_node);
    notify_node = NULL;
    return 0;
}

int devdrv_ipc_del(const char *ipc_name, u32 max_len, enum devdrv_ipc_type ipc_type)
{
    u32 key = 0;
    int ret;

    if (ipc_name == NULL) {
        return -EINVAL;
    }
    if (ipc_type >= DEVDRV_IPC_TYPE_MAX || ipc_del_handler[ipc_type] == NULL || (strlen(ipc_name) >= max_len)) {
        devdrv_drv_err("invalid parameter, devdrv_ipc_type = %d\n", ipc_type);
        return -ENODEV;
    }

    key = devdrv_mc_get_str_elfhash(ipc_name, strlen(ipc_name) + 1);
    ret = ipc_del_handler[ipc_type](ipc_name, key);

    return ret;
}
EXPORT_SYMBOL(devdrv_ipc_del);
#else
int devdrv_ipc_notify_del(const char *ipc_name, u32 key)
{
    return 0;
}
#endif
