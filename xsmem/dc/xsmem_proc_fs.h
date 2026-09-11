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

#ifndef XSMEM_PROC_FS_H
#define XSMEM_PROC_FS_H

#include "xsmem_framework.h"

void proc_fs_xsmem_pool_add_task(struct xsm_task_pool_node *node);
void proc_fs_xsmem_pool_del_task(struct xsm_task_pool_node *node);
void proc_fs_add_xsmem_pool(struct xsm_pool *xp);
void proc_fs_del_xsmem_pool(struct xsm_pool *xp);
void proc_fs_add_task(struct xsm_task *task);
void proc_fs_del_task(struct xsm_task *task);

void xsmem_proc_fs_init(void);
void xsmem_proc_fs_uninit(void);

#endif
