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

#ifndef XSMEM_ALGO_SP_H
#define XSMEM_ALGO_SP_H

#include "ascend_hal_define.h"
#include "xsmem_framework.h"

static inline unsigned int sp_get_device_id_from_flags(unsigned long flags)
{
    return (flags >> BUFF_FLAGS_DEVID_OFFSET) & 0xff;
}

static inline void sp_set_device_id_to_flags(unsigned int devid, unsigned long *flags)
{
    *flags |= ((unsigned long)devid << BUFF_FLAGS_DEVID_OFFSET);
}

struct xsm_pool_algo *xsm_get_sp_algo(void);
void *sp_inst_create(void);
int sp_inst_destroy(void *sp_ctrl);
int sp_algo_alloc(void *sp_ctrl, unsigned long alloc_size, unsigned long flag,
    unsigned long *addr, unsigned long *real_size);
int sp_algo_free(void *sp_ctrl, unsigned long addr);
int sp_perm_add(void *sp_ctrl, int pid, unsigned long prop);
int sp_algo_is_sharepool_addr(unsigned long va);

#endif
