/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2022. All rights reserved.
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
 * Create: 2019-10-15
 */
#ifndef SVM_MASTER_ADVISE_H
#define SVM_MASTER_ADVISE_H

#include "devmm_proc_info.h"
#include "svm_heap_mng.h"

int devmm_ioctl_advise(struct devmm_svm_process *svm_pro, struct devmm_ioctl_arg *arg);
int devmm_ioctl_prefetch(struct devmm_svm_process *svm_pro, struct devmm_ioctl_arg *arg);
void devmm_get_dev_mem_dfx(struct devmm_svm_process *svm_pro, u32 mem_type, struct devmm_ioctl_arg *arg);
int devmm_ipc_page_table_create_process(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap,
    u32 *page_bitmap, struct devmm_ioctl_arg *arg, void *n_attr);

#endif /* SVM_MASTER_ADVISE_H */

