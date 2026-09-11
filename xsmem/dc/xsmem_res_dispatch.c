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

#include "xsmem_res_dispatch.h"

struct xsm_id_to_xp *g_xsm_switch_handle = NULL;

void xsmem_id_switch_init(void)
{
    extern struct xsm_id_to_xp xsm_idr_swith;
    xsmem_register_id_switch_func(&xsm_idr_swith);
}

void xsmem_register_id_switch_func(struct xsm_id_to_xp *func_handle)
{
    g_xsm_switch_handle = func_handle;
}

int xsmem_alloc_id(struct xsm_pool *xp, int *id_in)
{
    return g_xsm_switch_handle->alloc(xp, id_in);
}
void xsmem_delete_id(int id)
{
    g_xsm_switch_handle->del(id);
}
struct xsm_pool *xsmem_get_xp_by_id(int id)
{
    return g_xsm_switch_handle->get(id);
}
int xsmem_id_for_each(int (*fn)(int id, void *p, void *data), void *data)
{
    return g_xsm_switch_handle->loop(fn, data);
}
