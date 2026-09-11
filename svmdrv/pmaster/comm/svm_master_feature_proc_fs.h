/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
 * Author: huawei
 * Create: 2025-02-12
 */
#ifndef SVM_MASTER_FEATURE_PROC_FS_H
#define SVM_MASTER_FEATURE_PROC_FS_H

void devmm_dev_feature_proc_fs_create(ka_proc_dir_entry_t *dev_entry, u32 logic_id);
void devmm_dev_feature_proc_fs_destroy(u32 logic_id);

#endif