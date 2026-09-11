/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
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
 * Create: 2022-11-29
 */

#include "svm_rbtree.h"

#ifndef EMU_ST
ka_rb_node_t *devmm_rb_first(ka_rb_root_t *root)
{
    if (KA_BASE_RB_EMPTY_ROOT(root)) {
        return NULL;
    }

    return ka_base_rb_first(root);
}
 
ka_rb_node_t *devmm_rb_next(ka_rb_node_t *node)
{
    if (KA_BASE_RB_EMPTY_NODE(node)) {
        return NULL;
    }

    return ka_base_rb_next(node);
}
#endif
int devmm_rb_erase(ka_rb_root_t *root, ka_rb_node_t *node)
{
    int ret = -ENODEV;

    if (KA_BASE_RB_EMPTY_NODE(node) == false) {
        ka_base_rb_erase(node, root);
        KA_BASE_RB_CLEAR_NODE(node);
        ret = 0;
    }
    return ret;
}

int devmm_rb_insert(ka_rb_root_t *root, ka_rb_node_t *node, rb_handle_func get_handle)
{
    ka_rb_node_t **cur_node = ka_base_get_rb_root_node_addr(root);
    ka_rb_node_t *parent = NULL;
    u64 handle = get_handle(node);

    /* Figure out where to put new node */
    while (*cur_node) {
        u64 tmp_handle = get_handle(*cur_node);

        parent = *cur_node;
        if (handle < tmp_handle) {
            cur_node = ka_base_get_rb_node_left_addr(*cur_node);
        } else if (handle > tmp_handle) {
            cur_node = ka_base_get_rb_node_right_addr(*cur_node);
        } else {
            return -EINVAL;
        }
    }

    /* Add new node and rebalance tree. */
    ka_base_rb_link_node(node, parent, cur_node);
    ka_base_rb_insert_color(node, root);
    return 0;
}

int devmm_rb_insert_by_range(ka_rb_root_t *root, ka_rb_node_t *node, rb_range_handle_func get_range)
{
    ka_rb_node_t **cur_node = ka_base_get_rb_root_node_addr(root);
    ka_rb_node_t *parent = NULL;
    struct rb_range_handle range;
    get_range(node, &range);

    /* Figure out where to put new node */
    while (*cur_node) {
        struct rb_range_handle tmp_range;
        get_range(*cur_node, &tmp_range);

        parent = *cur_node;
        if (range.end < tmp_range.start) {
            cur_node = ka_base_get_rb_node_left_addr(*cur_node);
        } else if (range.start > tmp_range.end) {
            cur_node = ka_base_get_rb_node_right_addr(*cur_node);
        } else {
            return -EINVAL;
        }
    }

    /* Add new node and rebalance tree. */
    ka_base_rb_link_node(node, parent, cur_node);
    ka_base_rb_insert_color(node, root);
    return 0;
}

ka_rb_node_t *devmm_rb_search(ka_rb_root_t *root, u64 handle, rb_handle_func get_handle)
{
    ka_rb_node_t *node = NULL;

    node = ka_base_get_rb_root_node(root);
    while (node != NULL) {
        u64 tmp_handle = get_handle(node);
        if (handle < tmp_handle) {
            node = ka_base_get_rb_node_left(node);
        } else if (handle > tmp_handle) {
            node = ka_base_get_rb_node_right(node);
        } else {
            return node;
        }
    }

    return NULL;
}

ka_rb_node_t *devmm_rb_search_by_range(ka_rb_root_t *root, struct rb_range_handle *range,
    rb_range_handle_func get_range)
{
    ka_rb_node_t *node = ka_base_get_rb_root_node(root);

    while (node != NULL) {
        struct rb_range_handle tmp_range;

        get_range(node, &tmp_range);
        if (range->end < tmp_range.start) {
            node = ka_base_get_rb_node_left(node);
        } else if (range->start > tmp_range.end) {
            node = ka_base_get_rb_node_right(node);
        } else if (range->start >= tmp_range.start && range->end <= tmp_range.end) {
            return node;
        } else {
            return NULL;
        }
    }
    return NULL;
}

/* condition can be NULL */
ka_rb_node_t *devmm_rb_erase_one_node(ka_rb_root_t *root, rb_erase_condition condition)
{
    ka_rb_node_t *node = NULL;

    if (KA_BASE_RB_EMPTY_ROOT(root) == true) {
        return NULL;
    }

    node = ka_base_rb_first(root);
    while (node != NULL) {
        if (condition == NULL) {
            break;
        }

        if (condition(node) == true) {
            break;
        }
        node = ka_base_rb_next(node);
    }

    if (node != NULL) {
        ka_base_rb_erase(node, root);
        KA_BASE_RB_CLEAR_NODE(node);
    }

    return node;
}

void devmm_rb_erase_all_node(ka_rb_root_t *root, rb_release_func release)
{
    ka_rb_node_t *node = NULL;
    ka_rb_node_t *next_node = NULL;

    if (KA_BASE_RB_EMPTY_ROOT(root) == true) {
        return;
    }

    node = ka_base_rb_first(root);
    while (node != NULL) {
        next_node = ka_base_rb_next(node);
        ka_base_rb_erase(node, root);
        KA_BASE_RB_CLEAR_NODE(node);

        release(node);
        node = next_node;
    }
}

