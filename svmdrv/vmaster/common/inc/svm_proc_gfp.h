/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
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
 * Create: 2023-07-25
 */
#ifndef SVM_PROC_GFP_H
#define SVM_PROC_GFP_H

#include "svm_gfp.h"
#include "svm_page_cnt_stats.h"
#include "devmm_proc_info.h"

int devmm_proc_alloc_pages(struct devmm_svm_process *svm_proc,
    struct devmm_phy_addr_attr *attr, ka_page_t **pages, u64 pg_num);
void devmm_proc_free_pages(struct devmm_svm_process *svm_proc,
    struct devmm_phy_addr_attr *attr, ka_page_t **pages, u64 pg_num);

#endif
