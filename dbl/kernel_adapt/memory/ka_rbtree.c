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
 * Create: 2024-7-10
 */

#include <linux/sched.h>
#include "ka_rbtree.h"

int ka_rb_erase(struct rb_root *root, struct rb_node *node)
{
    int ret = -ENODEV;

    if (RB_EMPTY_NODE(node) == false) {
        rb_erase(node, root);
        RB_CLEAR_NODE(node);
        ret = 0;
    }

    return ret;
}

int ka_rb_insert(struct rb_root *root, struct rb_node *node, rb_handle_func get_handle)
{
    struct rb_node **cur_node = &root->rb_node;
    struct rb_node *parent = NULL;
    unsigned long handle = get_handle(node);

    /* Figure out where to put new node */
    while (*cur_node) {
        unsigned long tmp_handle = get_handle(*cur_node);

        parent = *cur_node;
        if (handle < tmp_handle) {
            cur_node = &((*cur_node)->rb_left);
        } else if (handle > tmp_handle) {
            cur_node = &((*cur_node)->rb_right);
        } else {
            return -EINVAL;
        }
    }

    /* Add new node and rebalance tree. */
    rb_link_node(node, parent, cur_node);
    rb_insert_color(node, root);
    return 0;
}

struct rb_node *ka_rb_search(struct rb_root *root, unsigned long handle, rb_handle_func get_handle)
{
    struct rb_node *node = NULL;

    node = root->rb_node;
    while (node != NULL) {
        unsigned long tmp_handle = get_handle(node);
        if (handle < tmp_handle) {
            node = node->rb_left;
        } else if (handle > tmp_handle) {
            node = node->rb_right;
        } else {
            return node;
        }
    }

    return NULL;
}

struct rb_node *ka_rb_erase_one_node(struct rb_root *root)
{
    struct rb_node *node = NULL;

    if (RB_EMPTY_ROOT(root) == true) {
        return NULL;
    }
    node = rb_first(root);
    if (node != NULL) {
        rb_erase(node, root);
        RB_CLEAR_NODE(node);
    }
    return node;
}

