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
 * Create: 2023-12-15
 */
#ifndef SVM_SHMEM_PRCFS_H
#define SVM_SHMEM_PRCFS_H

struct devmm_ipc_node;

#ifdef CONFIG_PROC_FS

void svm_shmem_profs_init(void);
void svm_shmem_profs_uninit(void);
void devmm_ipc_procfs_add_node(struct devmm_ipc_node *node);
void devmm_ipc_procfs_del_node(struct devmm_ipc_node *node);
void devmm_ipc_profs_init(void);
void devmm_ipc_profs_uninit(void);

#else /* !CONFIG_PROC_FS */

static inline void svm_shmem_profs_init(void)
{
}

static inline void svm_shmem_profs_uninit(void)
{
}

static inline void devmm_ipc_procfs_add_node(struct devmm_ipc_node *node)
{
}

static inline void devmm_ipc_procfs_del_node(struct devmm_ipc_node *node)
{
}

static inline void devmm_ipc_profs_init(void)
{
}

static inline void devmm_ipc_profs_uninit(void)
{
}

#endif /* CONFIG_PROC_FS */

#endif /* SVM_SHMEM_PRCFS_H */
