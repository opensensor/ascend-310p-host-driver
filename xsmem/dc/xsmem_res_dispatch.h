/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
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
 * Create: 2022-08-13
 */

#ifndef XSMEM_RES_DISPATCH_H
#define XSMEM_RES_DISPATCH_H

#include "xsmem_framework.h"

struct xsm_id_to_xp {
    int (*alloc)(struct xsm_pool *xp, int *id_in);
    void (*del)(int id);
    struct xsm_pool *(*get)(int id);
    int (*loop)(int (*fn)(int id, void *p, void *data), void *data);
};

void xsmem_id_switch_init(void);
void xsmem_register_id_switch_func(struct xsm_id_to_xp *func_handle);
int xsmem_alloc_id(struct xsm_pool *xp, int *id_in);
void xsmem_delete_id(int id);
struct xsm_pool *xsmem_get_xp_by_id(int id);
int xsmem_id_for_each(int (*fn)(int id, void *p, void *data), void *data);

#endif
