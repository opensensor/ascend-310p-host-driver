/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
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
 * Create: 2024-03-04
 */

#ifndef MULTI_RBTREE_H
#define MULTI_RBTREE_H

#include "ka_base_pub.h"
#include "ka_list_pub.h"

struct multi_rb_node {
    ka_rb_node_t multi_rbtree_node;
    u64 key;

    ka_list_head_t list;
    bool is_list_first;
};

void multi_rbtree_insert(ka_rb_root_t *root, struct multi_rb_node *node, u64 key);
void multi_rbtree_erase(ka_rb_root_t *root, struct multi_rb_node *node);
struct multi_rb_node *multi_rbtree_get(ka_rb_root_t *root, u64 key);
struct multi_rb_node *multi_rbtree_get_upper_bound(ka_rb_root_t *root, u64 key);
struct multi_rb_node *multi_rbtree_get_node_from_rb_node(ka_rb_node_t *node);

#endif

