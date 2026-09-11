/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
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
 * Create: 2024-9-4
 */
#ifndef SVM_MEM_STATS_H
#define SVM_MEM_STATS_H

#include <linux/types.h>
#include "ka_base_pub.h"

#include "devmm_proc_info.h"

void devmm_task_mem_stats_show(ka_seq_file_t *seq);
void devmm_dev_mem_stats_log_show(u32 logic_id);
int devmm_dev_mem_stats_procfs_show(ka_seq_file_t *seq, void *offset);
void devmm_mem_stats_va_map(struct devmm_svm_process *svm_proc, u32 logic_id, u64 va);
void devmm_mem_stats_va_unmap(struct devmm_svm_process *svm_proc);

#endif
