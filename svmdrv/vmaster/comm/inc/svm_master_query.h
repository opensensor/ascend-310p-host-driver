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

#ifndef SVM_MASTER_QUERY_H
#define SVM_MASTER_QUERY_H
#include <linux/mutex.h>
#include <linux/types.h>

void devmm_mem_free_preprocess_by_dev_and_va(struct devmm_svm_process *svm_proc, u32 devid, u64 free_va, u64 free_len);
void devmm_mem_free_preprocess_by_dev(struct devmm_svm_process *svm_proc, u32 devid);

#endif /* __SVM_MASTER_QUERY_H__ */
