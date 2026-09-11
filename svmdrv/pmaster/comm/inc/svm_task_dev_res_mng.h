/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
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
 * Create: 2022-11-29
 */
#ifndef SVM_TASK_DEV_RES_MNG_H
#define SVM_TASK_DEV_RES_MNG_H

#include "svm_rbtree.h"
#include "devmm_proc_info.h"
#include "svm_dev_res_mng.h"
#include "devmm_common.h"

struct devmm_convert_node_rb_info {
    ka_rw_semaphore_t rw_sem;
    ka_rb_root_t root;
};

struct devmm_task_dev_res_node {
    struct devmm_svm_process *svm_proc;
    struct devmm_dev_res_mng *dev_res_mng;

    ka_list_head_t task_node;
    ka_list_head_t dev_res_node;

    ka_kref_t ref;

    struct svm_id_inst id_inst;
    int host_pid;
    u32 vm_id;

    struct devmm_convert_node_rb_info convert_rb_info;
};

struct devmm_task_dev_res_node *devmm_task_dev_res_node_create(struct devmm_svm_process *svm_proc,
    struct svm_id_inst *id_inst);
void devmm_task_dev_res_node_destroy(struct devmm_task_dev_res_node *node);

int devmm_task_dev_res_node_get(struct devmm_task_dev_res_node *node);
void devmm_task_dev_res_node_put(struct devmm_task_dev_res_node *node);
struct devmm_task_dev_res_node *devmm_task_dev_res_node_get_by_task(
    struct devmm_svm_process *svm_proc, struct svm_id_inst *id_inst);
void devmm_task_dev_res_nodes_destroy_by_task(struct devmm_svm_process *svm_proc);

#endif /* SVM_TASK_DEV_RES_MNG_H */