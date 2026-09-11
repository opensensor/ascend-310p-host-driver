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

#ifndef XSMEM_ALGO_VMA_H
#define XSMEM_ALGO_VMA_H

#include "xsmem_framework.h"
#include "ka_common_pub.h"

struct xsm_pool_algo *xsm_get_vma_algo(void);
void *vma_inst_create(unsigned long pool_size);
int vma_inst_destroy(void *vma_ctrl);
void vma_algo_show(void *vma_ctrl, ka_seq_file_t *seq);
int vma_algo_alloc(void *vma_ctrl, unsigned long alloc_size,
    unsigned long *addr, unsigned long *real_size);
int vma_algo_free(void *vma_ctrl, unsigned long addr, unsigned long real_size);

#endif
