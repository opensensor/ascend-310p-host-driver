/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
 * Create: 2025-07-14
 */

#include "securec.h"
#include "dbl_mem_alloc_interface.h"
#include "udis_log.h"
#include "udis_management.h"

#include "ka_memory_pub.h"

struct udis_dma_node *udis_addr_list_find_node(const struct udis_ctrl_block *udis_cb, UDIS_MODULE_TYPE module_type,
    const char *name)
{
    int i;
    struct udis_dma_node *addr_node, *next = NULL;

    for (i = UPDATE_ONLY_ONCE; i < UPDATE_TYPE_MAX; ++i) {
        list_for_each_entry_safe(addr_node, next, &udis_cb->addr_list[i], list) {
            if ((addr_node->module_type == module_type) && (strcmp(addr_node->name, name) == 0)) {
                return addr_node;
            }
        }
    }
    return NULL;
}

int udis_addr_list_add_node(unsigned int udevid, struct udis_ctrl_block *udis_cb,
    const struct udis_dma_node *addr_node)
{
    int ret;
    struct udis_dma_node *new_node = NULL;

    new_node = dbl_kzalloc(sizeof(struct udis_dma_node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (new_node == NULL) {
        udis_err("Failed to malloc for udis_dma_node. (udevid=%u; module_type=%u; name=%s)\n",
            udevid, addr_node->module_type, addr_node->name);
        return -ENOMEM;
    }

    ret = strcpy_s(new_node->name, UDIS_MAX_NAME_LEN, addr_node->name);
    if (ret != 0) {
        udis_err("strncpy_s failed. (module_type=%u; name=%s; ret=%d)\n",
            addr_node->module_type, addr_node->name, ret);
        dbl_kfree(new_node);
        return -ENOMEM;
    }

    new_node->module_type = addr_node->module_type;
    new_node->update_type = addr_node->update_type;
    new_node->dev_dma_addr = addr_node->dev_dma_addr;
    new_node->host_dma_addr = addr_node->host_dma_addr;
    new_node->data_len = addr_node->data_len;

    list_add(&new_node->list, &udis_cb->addr_list[addr_node->update_type]);

    return 0;
}

void udis_addr_list_remove_node(unsigned int udevid, struct udis_ctrl_block *udis_cb,
    const struct udis_dma_node *addr_node)
{
    struct udis_dma_node *node = NULL;

    node = udis_addr_list_find_node(udis_cb, addr_node->module_type, addr_node->name);
    if (node == NULL) {
        udis_err("No matched node found. (udevid=%u; module_type=%u; name=%s)\n", udevid, addr_node->module_type,
            addr_node->name);
        return;
    }

    list_del(&node->list);
    dbl_kfree(node);
    node = NULL;

    return;
}