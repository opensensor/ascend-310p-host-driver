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

#include <linux/string.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/list.h>

#include "urd_kv.h"
#include "urd_feature.h"
#include "securec.h"
#include "urd_define.h"

#include "ka_base_pub.h"
#include "ka_memory_pub.h"
#include "ka_list_pub.h"

struct dms_kv_table g_kv_cb = {0};

#define CHECK_ARG(key, data, data_len, ret)                                           \
    do {                                                                              \
        if (((key) == NULL) || ((data) == NULL) || ((data_len) > KV_VALUE_MAX_LEN)   \
            || ((data_len) <= 0)) {                                                   \
            dms_err("Invalid Parameter.\n");                                          \
            return -EINVAL;                                                           \
        }                                                                             \
        if (ka_base_strnlen(key, KV_KEY_MAX_LEN) >= KV_KEY_MAX_LEN) {                         \
            dms_err("Key too long. (max=%d)\n", KV_KEY_MAX_LEN);                      \
            return -EINVAL;                                                           \
        }                                                                             \
    } while (0)

static void free_kv(struct dms_kv_node* kv)
{
    if (kv != NULL) {
        kfree(kv->data);
        kv->data = NULL;

        kfree(kv->key);
        kv->key = NULL;
        kfree(kv);
    }
}

static inline unsigned int dms_kv_hash_33(const char* tag)
{
    unsigned int hash = 0;
    while (*tag) {
        hash = (hash << 5) + hash + *tag++; /* 5 hash left offset */
    }
    return hash;
}

static inline ka_hlist_head_t *dms_kv_get_table_head(const char *key)
{
    return &g_kv_cb.table[dms_kv_hash_33(key) % KV_TABLE_SIZE];
}

static struct dms_kv_node *dms_kv_new_node(const char *key, unsigned int key_len, void *value, int len)
{
    struct dms_kv_node* node = NULL;
    int ret;

    node = kzalloc(sizeof(struct dms_kv_node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (node == NULL) {
        dms_err("kmalloc failed. (size=%lu; key=\"%s\")\n", sizeof(struct dms_kv_node), key);
        return NULL;
    }
    node->key = kzalloc(sizeof(char) * (key_len + 1), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (node->key == NULL) {
        dms_err("kmalloc failed. (size=%lu; key=\"%s\")\n", (sizeof(char) * (key_len + 1)), key);
        goto free_node;
    }
    ret = strcpy_s(node->key, (key_len + 1), key);
    if (ret != 0) {
        dms_err("strcpy failed. (ret=%d; size=%lu; key=\"%s\")\n", ret, ka_base_strlen(key), key);
        goto free_key;
    }

    node->data = kzalloc(len, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (node->data == NULL) {
        dms_err("kmalloc failed. (size=%d; key=\"%s\")\n", len, key);
        goto free_key;
    }
    ret = memcpy_s(node->data, len, value, len);
    if (ret != 0) {
        dms_err("memcpy failed. (ret=%d; size=%d; key=\"%s\")\n", ret, len, key);
        goto free_data;
    }
    node->data_len = len;
    return node;
free_data:
    kfree(node->data);
    node->data = NULL;
free_key:
    kfree(node->key);
    node->key = NULL;
free_node:
    kfree(node);
    node = NULL;
    return NULL;
}

static struct dms_kv_node* dms_kv_search(const char* key)
{
    ka_hlist_head_t *table_head = NULL;
    struct dms_kv_node* node = NULL;
    ka_hlist_node_t *tmp = NULL;
    table_head = dms_kv_get_table_head(key);
    ka_hlist_for_each_entry_safe(node, tmp, table_head, hnode)
    {
        if (ka_base_strncmp(node->key, key, ka_base_strlen(key)) == 0) {
            break;
        }
    }
    return node;
}

static int dms_kv_add(const char* key, void* value, int len)
{
    ka_hlist_head_t *table_head = NULL;
    struct dms_kv_node* node = NULL;

    node = dms_kv_new_node(key, ka_base_strlen(key), value, len);
    if (node == NULL) {
        dms_err("Create new node failed. (key=\"%s\")\n", key);
        return -ENOMEM;
    }
    table_head = dms_kv_get_table_head(key);
    ka_hlist_add_head(&node->hnode, table_head);
    return 0;
}

int dms_kv_del(const char* key)
{
    struct dms_kv_node* node = NULL;
    if (key == NULL) {
        dms_err("Invalid Parameter.\n");
        return -EINVAL;
    }
    ka_task_mutex_lock(&g_kv_cb.lock);
    node = dms_kv_search(key);
    if (node != NULL) {
        ka_hlist_del(&node->hnode);
        ka_task_mutex_unlock(&g_kv_cb.lock);
        free_kv(node);
        return 0;
    }
    ka_task_mutex_unlock(&g_kv_cb.lock);
    dms_err("No such device node. (key=\"%s\")\n", key);
    return -ENODEV;
}
int dms_kv_set(const char* key, void* value, int len)
{
    ka_hlist_head_t *table_head = NULL;
    struct dms_kv_node* node = NULL;
    ka_hlist_node_t *tmp = NULL;
    int ret;
    CHECK_ARG(key, value, len, -EINVAL);
    ka_task_mutex_lock(&g_kv_cb.lock);
    table_head = dms_kv_get_table_head(key);
    ka_hlist_for_each_entry_safe(node, tmp, table_head, hnode)
    {
        if (strcmp(node->key, key) == 0) {
            ka_task_mutex_unlock(&g_kv_cb.lock);
            dms_err("Repeated registration of the same node, (key=\"%s\")\n", key);
            return -EINVAL;
        }
    }
    ret = dms_kv_add(key, value, len);
    ka_task_mutex_unlock(&g_kv_cb.lock);
    return ret;
}

int dms_kv_get(const char* key, void* value, int len)
{
    struct dms_kv_node* node = NULL;
    int ret;
    CHECK_ARG(key, value, len, -EINVAL);
    ka_task_mutex_lock(&g_kv_cb.lock);
    node = dms_kv_search(key);
    if (node != NULL) {
        ret = memcpy_s(value, len, node->data, node->data_len);
        ka_task_mutex_unlock(&g_kv_cb.lock);
        return ret;
    }
    ka_task_mutex_unlock(&g_kv_cb.lock);
    dms_debug("No such device node. (key=\"%s\")\n", key);
    return -ENODEV;
}

int dms_kv_get_ex(const char* key, void* value, int len)
{
    struct dms_kv_node* node = NULL;
    int ret;
    CHECK_ARG(key, value, len, -EINVAL);
    ka_task_mutex_lock(&g_kv_cb.lock);
    node = dms_kv_search(key);
    if (node == NULL) {
        ka_task_mutex_unlock(&g_kv_cb.lock);
        dms_debug("No such device node. (key=\"%s\")\n", key);
        return -ENODEV;
    }

    ret = memcpy_s(value, len, node->data, node->data_len);
    if (ret != 0) {
        ka_task_mutex_unlock(&g_kv_cb.lock);
        dms_err("Failed to memcpy node data. (key=\"%s\"; ret=%d)\n", key, ret);
        return -EINVAL;
    }

    ret = feature_inc_work(*(DMS_FEATURE_NODE_S**)value);
    if (ret != 0) {
        ka_task_mutex_unlock(&g_kv_cb.lock);
        return ret;
    }

    ka_task_mutex_unlock(&g_kv_cb.lock);
    return ret;
}

void dms_kv_dump(char *buf, ssize_t *offset, key_dump_handle handle)
{
    ka_hlist_head_t *table_head = NULL;
    struct dms_kv_node* node = NULL;
    ka_hlist_node_t *tmp = NULL;
    int i;
    ssize_t ret;

    ka_task_mutex_lock(&g_kv_cb.lock);
    for (i = 0; i < KV_TABLE_SIZE; i++) {
        table_head = &g_kv_cb.table[i];
        ka_hlist_for_each_entry_safe(node, tmp, table_head, hnode)
        {
            ret = handle(node->data, buf, offset);
            if (ret < 0) {
                ka_task_mutex_unlock(&g_kv_cb.lock);
                return;
            }
        }
    }
    ka_task_mutex_unlock(&g_kv_cb.lock);
}

int dms_kv_init(void)
{
    int i;

    g_kv_cb.table = kzalloc(sizeof(ka_hlist_head_t) * KV_TABLE_SIZE, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (g_kv_cb.table == NULL) {
        dms_err("kmalloc failed. (size=%lu)\n", (sizeof(ka_hlist_head_t) * KV_TABLE_SIZE));
        return -ENOMEM;
    }

    for (i = 0; i < KV_TABLE_SIZE; i++) {
        KA_INIT_HLIST_HEAD(&g_kv_cb.table[i]);
    }
    ka_task_mutex_init(&g_kv_cb.lock);
    return 0;
}

void dms_kv_uninit(void)
{
    ka_hlist_head_t *table_head = NULL;
    struct dms_kv_node* node = NULL;
    ka_hlist_node_t *tmp = NULL;
    int i;

    ka_task_mutex_lock(&g_kv_cb.lock);
    for (i = 0; i < KV_TABLE_SIZE; i++) {
        table_head = &g_kv_cb.table[i];
        ka_hlist_for_each_entry_safe(node, tmp, table_head, hnode)
        {
            ka_hlist_del_init(&node->hnode);
            free_kv(node);
            node = NULL;
        }
    }
    ka_task_mutex_unlock(&g_kv_cb.lock);

    ka_task_mutex_destroy(&g_kv_cb.lock);
    kfree(g_kv_cb.table);
    g_kv_cb.table = NULL;
    return;
}

