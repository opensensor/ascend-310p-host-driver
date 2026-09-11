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
#ifndef SVM_MASTER_MEMCPY_H
#define SVM_MASTER_MEMCPY_H

#include "svm_cmd.h"
#include "devmm_proc_info.h"

int devmm_ioctl_memcpy_proc(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
int devmm_ioctl_memcpy_batch(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
int devmm_ioctl_async_memcpy_proc(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
int devmm_ioctl_memcpy2d_proc(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
void devmm_find_memcpy_dir(enum devmm_copy_direction *dir, struct devmm_memory_attributes *src_attr,
    struct devmm_memory_attributes *dst_attr);
int devmm_memcpy_d2d_process(struct devmm_svm_process *src_proc, struct devmm_memory_attributes *src_attr,
    struct devmm_svm_process *dst_proc, struct devmm_memory_attributes *dst_attr, struct devmm_mem_copy_para *para);
int devmm_check_va_direction(enum devmm_copy_direction para_dir,
    enum devmm_copy_direction real_dir, u32 task_mode, bool is_memcpy_batch, struct devmm_memory_attributes *dst_attr);
void devmm_init_task_para(struct devmm_mem_copy_convrt_para *task_para, bool last_seq_flag,
    bool create_msg, bool is_2d, u32 task_mode);
int devmm_ioctl_memcpy_proc(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
int devmm_check_memcpy2d_input(enum devmm_copy_direction dir, u64 spitch, u64 dpitch,
    u64 width, u64 height);
int devmm_ioctl_cpy_result_refresh(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
int devmm_ioctl_sumbit_convert_dma(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
int devmm_ioctl_wait_convert_dma_result(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
bool devmm_check_va_is_async_cpying(struct devmm_svm_process *svm_proc, u64 va);

#endif /* SVM_MASTER_MEMCPY_H */

