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
#ifndef SVM_RBTREE_H
#define SVM_RBTREE_H

#include "ka_base_pub.h"

struct rb_range_handle {
    u64 start;
    u64 end;
};

typedef u64 (*rb_handle_func)(ka_rb_node_t *node);
typedef void (*rb_range_handle_func)(ka_rb_node_t *node, struct rb_range_handle *range_handle);

typedef bool (*rb_erase_condition)(ka_rb_node_t *node);
typedef void (*rb_release_func)(ka_rb_node_t *node);

ka_rb_node_t *devmm_rb_first(ka_rb_root_t *root);
ka_rb_node_t *devmm_rb_next(ka_rb_node_t *node);

int devmm_rb_erase(ka_rb_root_t *root, ka_rb_node_t *node);
int devmm_rb_insert(ka_rb_root_t *root, ka_rb_node_t *node, rb_handle_func get_handle);
int devmm_rb_insert_by_range(ka_rb_root_t *root, ka_rb_node_t *node, rb_range_handle_func get_range);
ka_rb_node_t *devmm_rb_search(ka_rb_root_t *root, u64 handle, rb_handle_func get_handle);
ka_rb_node_t *devmm_rb_search_by_range(ka_rb_root_t *root, struct rb_range_handle *range,
    rb_range_handle_func get_range);

ka_rb_node_t *devmm_rb_erase_one_node(ka_rb_root_t *root, rb_erase_condition condition);
void devmm_rb_erase_all_node(ka_rb_root_t *root, rb_release_func release);

#endif /* SVM_RBTREE_H */
