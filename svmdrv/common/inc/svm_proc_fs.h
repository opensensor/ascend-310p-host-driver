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

#ifndef SVM_PROC_FS_H
#define SVM_PROC_FS_H

#include "devmm_proc_info.h"

#define DEVMM_PROC_FS_NAME_LEN 32
#define DEVMM_PROC_FS_MODE 0444

void devmm_proc_fs_init(struct devmm_svm_dev *svm_dev);
void devmm_proc_fs_uninit(void);
void devmm_proc_fs_add_task(struct devmm_svm_process *svm_proc);
void devmm_proc_fs_del_task(struct devmm_svm_process *svm_proc);
int devmm_info_show(ka_seq_file_t *seq, void *offset);
ka_proc_dir_entry_t *devmm_get_top_entry(void);
void devmm_dev_proc_fs_init(void);
void devmm_dev_proc_fs_uninit(void);
void devmm_dev_proc_fs_create(u32 logic_id);

#endif /* SVM_PROC_FS_H */
