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

#include "multi_rbtree.h"

static ka_rb_node_t *rbtree_insert_get_node(ka_rb_root_t *root, ka_rb_node_t *node, u64 key)
{
    ka_rb_node_t **tmp = ka_base_get_rb_root_node_addr(root);
    ka_rb_node_t *parent = NULL;

    while (*tmp != NULL) {
        struct multi_rb_node *cur_node = ka_base_rb_entry(*tmp, struct multi_rb_node, multi_rbtree_node);

        parent = *tmp;
        if (key < cur_node->key) {
            tmp = ka_base_get_rb_node_left_addr(*tmp);
        } else if (key > cur_node->key) {
            tmp = ka_base_get_rb_node_right_addr(*tmp);
        } else {
            /* return the same key */
            return *tmp;
        }
    }

    /* Add new node and rebalance tree. */
    ka_base_rb_link_node(node, parent, tmp);
    ka_base_rb_insert_color(node, root);

    return node;
}

void multi_rbtree_insert(ka_rb_root_t *root, struct multi_rb_node *node, u64 key)
{
    ka_rb_node_t *rb_node = NULL;

    node->key = key;
    rb_node = rbtree_insert_get_node(root, &node->multi_rbtree_node, key);
    if (rb_node != &node->multi_rbtree_node) {
        struct multi_rb_node *tmp = NULL;

        node->is_list_first = false;
        tmp = ka_base_rb_entry(rb_node, struct multi_rb_node, multi_rbtree_node);
        ka_list_add_tail(&node->list, &tmp->list);
    } else {
        KA_INIT_LIST_HEAD(&node->list);
        node->is_list_first = true;
    }
}

void multi_rbtree_erase(ka_rb_root_t *root, struct multi_rb_node *node)
{
    if (ka_list_empty(&node->list) == 1) {
        ka_base_rb_erase(&node->multi_rbtree_node, root);
    } else {
        if (node->is_list_first) {
            struct multi_rb_node *tmp = NULL;

            tmp = ka_base_rb_entry(node->list.next, struct multi_rb_node, list);
            ka_base_rb_replace_node(&node->multi_rbtree_node, &tmp->multi_rbtree_node, root);
            tmp->is_list_first = true;
        }
        ka_list_del(&node->list);
    }
}

static inline struct multi_rb_node *multi_rbtree_get_with_list(struct multi_rb_node *node)
{
    return (ka_list_empty(&node->list) == 1) ? node :
        ka_base_rb_entry(node->list.prev, struct multi_rb_node, list);
}

static ka_rb_node_t *rbtree_get(ka_rb_root_t *root, u64 key)
{
    ka_rb_node_t *tmp = ka_base_get_rb_root_node(root);

    while (tmp != NULL) {
        struct multi_rb_node *cur_node = NULL;

        cur_node = ka_base_rb_entry(tmp, struct multi_rb_node, multi_rbtree_node);
        if (key < cur_node->key) {
            tmp = ka_base_get_rb_node_left(tmp);
        } else if (key > cur_node->key) {
            tmp = ka_base_get_rb_node_right(tmp);
        } else {
            return tmp;
        }
    }
    return NULL;
}

struct multi_rb_node *multi_rbtree_get(ka_rb_root_t *root, u64 key)
{
    ka_rb_node_t *rb_node = NULL;

    rb_node = rbtree_get(root, key);
    if (rb_node != NULL) {
        struct multi_rb_node *tmp = NULL;

        tmp = ka_base_rb_entry(rb_node, struct multi_rb_node, multi_rbtree_node);
        return multi_rbtree_get_with_list(tmp);
    }

    return NULL;
}

static ka_rb_node_t *rbtree_get_upper_bound(ka_rb_root_t *root, u64 key)
{
    struct multi_rb_node *tmp_upper = NULL;
    ka_rb_node_t *tmp = ka_base_get_rb_root_node(root);

    while (tmp != NULL) {
        struct multi_rb_node *cur_node = NULL;

        cur_node = ka_base_rb_entry(tmp, struct multi_rb_node, multi_rbtree_node);
        if (key < cur_node->key) {
            tmp_upper = cur_node;
            tmp = ka_base_get_rb_node_left(tmp);
        } else if (key > cur_node->key) {
            tmp = ka_base_get_rb_node_right(tmp);
        } else {
            return ka_base_rb_next(&cur_node->multi_rbtree_node);
        }
    }
    return (tmp_upper != NULL) ? &tmp_upper->multi_rbtree_node : NULL;
}

struct multi_rb_node *multi_rbtree_get_upper_bound(ka_rb_root_t *root, u64 key)
{
    ka_rb_node_t *rb_node = NULL;

    rb_node = rbtree_get_upper_bound(root, key);
    if (rb_node != NULL) {
        struct multi_rb_node *tmp = NULL;

        tmp = ka_base_rb_entry(rb_node, struct multi_rb_node, multi_rbtree_node);
        return multi_rbtree_get_with_list(tmp);
    }

    return NULL;
}

struct multi_rb_node *multi_rbtree_get_node_from_rb_node(ka_rb_node_t *node)
{
    return ka_base_rb_entry(node, struct multi_rb_node, multi_rbtree_node);
}
