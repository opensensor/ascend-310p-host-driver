/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2023. All rights reserved.
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
#ifdef CFG_FEATURE_IPC_NOTIFY
#include <linux/types.h>
#include <linux/platform_device.h>
#ifdef AOS_LLVM_BUILD
#endif
#include "ka_hashtable_pub.h"
#include "ka_task_pub.h"
#include "tsdrv_common.h"
#include "tsdrv_log.h"
#include "devdrv_nsm.h"
#include "devdrv_driver_adapt.h"
#include "ascend_hal_define.h"
#ifndef AOS_LLVM_BUILD
static KA_TASK_DEFINE_SPINLOCK(devdrv_hash_notify_lock);
#else
static AOS_DEFINE_SPINLOCK(devdrv_hash_notify_lock);
#endif
static KA_DEFINE_HASHTABLE(ipc_notify_hashtable, 4); /* hash shifts 4 bits */
STATIC int devdrv_ipc_notify_create(const char *ipc_name, u32 tag, void **ipc_node, u32 flag);
STATIC int devdrv_ipc_notify_find(const char *ipc_name, u32 tag, void **ipc_node);
STATIC int devdrv_ipc_notify_del(const char *ipc_name, u32 tag);

int (*const ipc_create_handler[DEVDRV_IPC_TYPE_MAX])(const char *ipc_name, u32 tag, void **ipc_node, u32 flag) = {
    [DEVDRV_IPC_NOTIFY] = devdrv_ipc_notify_create,
};

int (*const ipc_find_handler[DEVDRV_IPC_TYPE_MAX])(const char *ipc_name, u32 tag, void **ipc_node) = {
    [DEVDRV_IPC_NOTIFY] = devdrv_ipc_notify_find,
};

int (*const ipc_del_handler[DEVDRV_IPC_TYPE_MAX])(const char *ipc_name, u32 tag) = {
    [DEVDRV_IPC_NOTIFY] = devdrv_ipc_notify_del,
};
#ifndef AOS_LLVM_BUILD
STATIC ka_atomic64_t ipc_name_ref = ATOMIC64_INIT(0);

int devdrv_creat_ipc_name(char *ipc_name, unsigned int len)
{
    char random[RANDOM_LENGTH] = {0};
    u64 tmp_ref;
    int offset;
    ka_pid_t tgid;
    int ret;
    int i;

    if ((ipc_name == NULL) || (len < DEVDRV_IPC_NAME_SIZE)) {
        TSDRV_PRINT_ERR("ipc is null(%d) or invalid len(%u).\n", (ipc_name == NULL), len);
        return -EINVAL;
    }

    tgid = current->tgid;
    tmp_ref = (u64)ka_base_atomic64_inc_return(&ipc_name_ref);
    offset = snprintf_s(ipc_name, DEVDRV_IPC_NAME_SIZE, DEVDRV_IPC_NAME_SIZE - 1, "%08x%016x", tgid, tmp_ref);
    if (offset < 0) {
        TSDRV_PRINT_ERR("snprintf failed, offset(%d).\n", offset);
        return -EINVAL;
    }
    for (i = 0; i < RANDOM_LENGTH; i++) {
        ret = snprintf_s(ipc_name + offset, DEVDRV_IPC_NAME_SIZE - offset, DEVDRV_IPC_NAME_SIZE - 1 - offset,
            "%02x", (u8)random[i]);
        if (ret < 0) {
            TSDRV_PRINT_ERR("snprintf failed, ret(%d).\n", ret);
            return -EINVAL;
        }
        offset += ret;
    }
    ipc_name[DEVDRV_IPC_NAME_SIZE - 1] = '\0';

    return 0;
}

KA_EXPORT_SYMBOL_GPL(devdrv_creat_ipc_name);
#endif
STATIC u32 devdrv_mc_get_str_elfhash(const char *ipc_name, size_t len)
{
    u32 hash = 0;
    u32 tmp_hash = 0;
    u32 tmp_len = 0;
    const u8 *ipc_char = (u8 *)ipc_name;

    while ((*ipc_char != 0) && (tmp_len < len)) {
        hash = (hash << 4) + *ipc_char; /* hash shifts left 4 bits */
        tmp_hash = hash & 0xF0000000;

        if (!tmp_hash) {
            hash ^= tmp_hash >> 24; /* tmp_hash shifts right 24 bits */
            hash ^= tmp_hash;
        }
        ipc_char++;
        tmp_len += sizeof(u8);
    }

    return hash;
}

STATIC int devdrv_ipc_notify_create(const char *ipc_name, u32 tag, void **ipc_node, u32 flag)
{
    struct ipc_notify_node *notify_node = NULL;
    int i;

    ka_task_spin_lock(&devdrv_hash_notify_lock);
    /*lint -e666 */
    ka_hash_for_each_possible(ipc_notify_hashtable, notify_node, link, tag) {
        if (!ka_base_strcmp(notify_node->name, ipc_name)) {
            ka_task_spin_unlock(&devdrv_hash_notify_lock);
            return -EINVAL;
        }
    }
    notify_node = tsdrv_kzalloc(sizeof(*notify_node), KA_GFP_ATOMIC | __KA_GFP_ACCOUNT);
    if (notify_node == NULL) {
        ka_task_spin_unlock(&devdrv_hash_notify_lock);
        return -ENOMEM;
    }

    for (i = 0; i < DEVDRV_IPC_NAME_SIZE; i++) {
        notify_node->name[i] = ipc_name[i];
    }

    for (i = 0; i < DEVDRV_PID_MAX_NUM; i++) {
        notify_node->open_info[i].pid = 0;
        notify_node->open_info[i].set_time = 0;
    }

    if ((flag & TSDRV_FLAG_REMOTE_ID) != 0) {
        notify_node->flag |= TSDRV_FLAG_REMOTE_ID;
    }

    notify_node->need_wlist = true;
    TSDRV_PRINT_DEBUG("devid=%u; tsid=%u; notify_id=%u; id_type=%u; flag=%u; name=%s\n",
        notify_node->dev_id, notify_node->tsid, notify_node->notify_id, notify_node->type,
        notify_node->flag, notify_node->name);

    ka_hash_add(ipc_notify_hashtable, &notify_node->link, tag);
    /*lint +e666 */
    ka_task_spin_unlock(&devdrv_hash_notify_lock);

    *ipc_node = (void *)notify_node;

    return 0;
}
#ifndef AOS_LLVM_BUILD
int devdrv_ipc_create(char *ipc_name, void **ipc_node, enum devdrv_ipc_type ipc_type, u32 flag)
{
    u32 tag;
    int ret;

    if ((ipc_name == NULL) || (ipc_node == NULL)) {
        return -EINVAL;
    }
    if ((ipc_type >= DEVDRV_IPC_TYPE_MAX) || (ipc_create_handler[ipc_type] == NULL)) {
        TSDRV_PRINT_ERR("devdrv_ipc_type = %d\n", (int)ipc_type);
        return -ENODEV;
    }

    ret = devdrv_creat_ipc_name(ipc_name, DEVDRV_IPC_NAME_SIZE);
    if (ret != 0) {
        TSDRV_PRINT_ERR("ipc name creat failed, ret(%d).\n", ret);
        return -EINVAL;
    }
    tag = devdrv_mc_get_str_elfhash(ipc_name, ka_base_strlen(ipc_name) + 1);
    ret = ipc_create_handler[ipc_type](ipc_name, tag, ipc_node, flag);

    return ret;
}
#endif

STATIC int devdrv_ipc_notify_find(const char *ipc_name, u32 tag, void **ipc_node)
{
    struct ipc_notify_node *notify_node = NULL;

    ka_task_spin_lock(&devdrv_hash_notify_lock);
    /* lint -e666 */
    ka_hash_for_each_possible(ipc_notify_hashtable, notify_node, link, tag) {
        if (!ka_base_strcmp(notify_node->name, ipc_name)) {
            ka_task_spin_unlock(&devdrv_hash_notify_lock);
            *ipc_node = notify_node;
            return 0;
        }
    }
    /* lint +e666 */
    ka_task_spin_unlock(&devdrv_hash_notify_lock);

    return -EINVAL;
}

#ifndef TSDRV_UT
bool devdrv_ipc_notify_is_belong_to_proc(u32 dev_id, u32 tsid, int pid, int res_type, u32 res_id)
{
    struct ipc_notify_node *notify_node = NULL;
    u32 i, bkt;

    ka_task_spin_lock(&devdrv_hash_notify_lock);
    /* lint -e666 */
    ka_hash_for_each(ipc_notify_hashtable, bkt, notify_node, link) {
        if ((notify_node->dev_id == dev_id) && (notify_node->tsid == tsid) &&
            (notify_node->type == res_type) && (notify_node->notify_id == res_id)) {
            for (i = 0; i < DEVDRV_PID_MAX_NUM; i++) {
                if ((notify_node->open_info[i].pid == pid) && (ka_base_atomic_read(&notify_node->open_info[i].ref) > 0)) {
                    ka_task_spin_unlock(&devdrv_hash_notify_lock);
                    return true;
                };
            }
        }
    }
    /* lint +e666 */
    ka_task_spin_unlock(&devdrv_hash_notify_lock);

    TSDRV_PRINT_ERR("Check failed. (dev_id=%u; tsid=%u; pid=%u; res_type=%u; res_id=%u)\n",
        dev_id, tsid, pid, res_type, res_id);
    return false;
}
#endif

int devdrv_ipc_find(const char *ipc_name, void **ipc_node, enum devdrv_ipc_type ipc_type)
{
    u32 tag;
    size_t name_len;
    int ret;

    if ((ipc_name == NULL) || (ipc_node == NULL)) {
        return -EINVAL;
    }

    name_len = ka_base_strnlen(ipc_name, DEVDRV_IPC_NAME_SIZE);
    if ((name_len == 0) || (name_len >= DEVDRV_IPC_NAME_SIZE)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Length out of range. (name_len=%lu)\n", name_len);
        return -EINVAL;
#endif
    }

    if ((ipc_type >= DEVDRV_IPC_TYPE_MAX) || (ipc_find_handler[ipc_type] == NULL)) {
        TSDRV_PRINT_ERR("invalid parameter, ipc_type = %d\n", (int)ipc_type);
        return -ENODEV;
    }

    tag = devdrv_mc_get_str_elfhash(ipc_name, ka_base_strlen(ipc_name) + 1);
    ret = ipc_find_handler[ipc_type](ipc_name, tag, ipc_node);

    return ret;
}

STATIC int devdrv_ipc_notify_del(const char *ipc_name, u32 tag)
{
    struct ipc_notify_node *notify_node = NULL;

    ka_task_spin_lock(&devdrv_hash_notify_lock);
    /* lint -e666 */
    ka_hash_for_each_possible(ipc_notify_hashtable, notify_node, link, tag) {
        if (!ka_base_strcmp(notify_node->name, ipc_name)) {
            ka_hash_del(&notify_node->link);
            ka_task_spin_unlock(&devdrv_hash_notify_lock);
            goto node_free;
        }
    }
    /* lint +e666 */
    ka_task_spin_unlock(&devdrv_hash_notify_lock);

    return -EINVAL;

node_free:
    (void)memset_s(notify_node->name, DEVDRV_IPC_NAME_SIZE, 0, DEVDRV_IPC_NAME_SIZE);
    tsdrv_kfree(notify_node);
    notify_node = NULL;
    return 0;
}

int devdrv_ipc_del(const char *ipc_name, u32 max_len, enum devdrv_ipc_type ipc_type)
{
    u32 tag;
    int ret;

    if (ipc_name == NULL) {
        return -EINVAL;
    }
    if ((ipc_type >= DEVDRV_IPC_TYPE_MAX) || (ipc_del_handler[ipc_type] == NULL) || (strlen(ipc_name) >= max_len)) {
        TSDRV_PRINT_ERR("invalid parameter, devdrv_ipc_type = %d\n", (int)ipc_type);
        return -ENODEV;
    }

    tag = devdrv_mc_get_str_elfhash(ipc_name, strlen(ipc_name) + 1);
    ret = ipc_del_handler[ipc_type](ipc_name, tag);

    return ret;
}
#endif
