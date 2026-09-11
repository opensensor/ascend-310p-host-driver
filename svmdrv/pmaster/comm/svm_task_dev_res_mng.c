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
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/rwsem.h>
#include <linux/kref.h>
#include <linux/rbtree.h>

#include "devmm_proc_info.h"
#include "svm_master_proc_mng.h"
#include "svm_master_convert.h"
#include "svm_master_dma_desc_mng.h"
#include "devmm_mem_alloc_interface.h"
#include "svm_task_dev_res_mng.h"

struct devmm_task_dev_res_node *devmm_task_dev_res_node_create(struct devmm_svm_process *svm_proc,
    struct svm_id_inst *id_inst)
{
    struct devmm_svm_proc_master *master_data = (struct devmm_svm_proc_master *)svm_proc->priv_data;
    struct devmm_task_dev_res_info *info = &master_data->task_dev_res_info;
    struct devmm_task_dev_res_node *node = NULL;

    node = devmm_kzalloc_ex(sizeof(struct devmm_task_dev_res_node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (node == NULL) {
        devmm_drv_err("Kzalloc failed.\n");
        return NULL;
    }

    node->dev_res_mng = devmm_dev_res_mng_get(id_inst);
    if (node->dev_res_mng == NULL) {
        /* calculate_group vm may fail, can not print err */
        devmm_drv_info("Can not get dev res mng. (devid=%u; vfid=%u)\n", id_inst->devid, id_inst->vfid);
        devmm_kfree_ex(node);
        return NULL;
    }

    node->svm_proc = svm_proc;  /* node->svm_proc's access can ensure int task context, so not get ref */
    node->host_pid = svm_proc->process_id.hostpid;
    node->id_inst = *id_inst;
    node->vm_id = svm_proc->process_id.vm_id;

    KA_INIT_LIST_HEAD(&node->task_node);
    KA_INIT_LIST_HEAD(&node->dev_res_node);
    ka_base_kref_init(&node->ref);

    node->convert_rb_info.root = KA_RB_ROOT;
    ka_task_init_rwsem(&node->convert_rb_info.rw_sem);

    ka_task_down_write(&info->rw_sem);
    ka_list_add(&node->task_node, &info->head);
    ka_task_up_write(&info->rw_sem);

    ka_task_down_write(&node->dev_res_mng->task_dev_res_info.rw_sem);
    ka_list_add(&node->dev_res_node, &node->dev_res_mng->task_dev_res_info.head);
    ka_task_up_write(&node->dev_res_mng->task_dev_res_info.rw_sem);
    return node;
}

static void devmm_task_dev_res_node_release(ka_kref_t *kref)
{
    struct devmm_task_dev_res_node *node = ka_container_of(kref, struct devmm_task_dev_res_node, ref);

    devmm_kfree_ex(node);
}

static void devmm_task_dev_res_node_subres_recycle(struct devmm_task_dev_res_node *node)
{
    u32 convert_node_num = 0;

    devmm_convert_nodes_destroy_by_task_release(node, &convert_node_num);

    devmm_drv_debug("Proc release destroy info. (host_pid=%d; convert_node_num=%u)\n",
        node->host_pid, convert_node_num);
}

void devmm_task_dev_res_node_destroy(struct devmm_task_dev_res_node *node)
{
    struct devmm_svm_proc_master *master_data = (struct devmm_svm_proc_master *)node->svm_proc->priv_data;
    struct devmm_task_dev_res_info *proc_info = &master_data->task_dev_res_info;

    devmm_task_dev_res_node_subres_recycle(node);

    ka_task_down_write(&proc_info->rw_sem);
    ka_list_del_init(&node->task_node);
    ka_task_up_write(&proc_info->rw_sem);

    ka_task_down_write(&node->dev_res_mng->task_dev_res_info.rw_sem);
    ka_list_del_init(&node->dev_res_node);
    ka_task_up_write(&node->dev_res_mng->task_dev_res_info.rw_sem);

    devmm_dev_res_mng_put(node->dev_res_mng);
    devmm_task_dev_res_node_put(node);
}

int devmm_task_dev_res_node_get(struct devmm_task_dev_res_node *node)
{
    if (ka_base_kref_get_unless_zero(&node->ref) == 0) {
#ifndef EMU_ST
        return -EEXIST;
#endif
    }
    return 0;
}

void devmm_task_dev_res_node_put(struct devmm_task_dev_res_node *node)
{
    ka_base_kref_put(&node->ref, devmm_task_dev_res_node_release);
}

struct devmm_task_dev_res_node *devmm_task_dev_res_node_get_by_task(
    struct devmm_svm_process *svm_proc, struct svm_id_inst *id_inst)
{
    struct devmm_svm_proc_master *master_data = (struct devmm_svm_proc_master *)svm_proc->priv_data;
    struct devmm_task_dev_res_info *info = &master_data->task_dev_res_info;
    struct devmm_task_dev_res_node *tmp = NULL;
    ka_list_head_t *head = &info->head;
    ka_list_head_t *n = NULL;
    ka_list_head_t *pos = NULL;

    ka_task_down_read(&info->rw_sem);
    ka_list_for_each_safe(pos, n, head) {
        tmp = ka_list_entry(pos, struct devmm_task_dev_res_node, task_node);
        if ((tmp->id_inst.devid == id_inst->devid) && (tmp->id_inst.vfid == id_inst->vfid)) {
            int ret = devmm_task_dev_res_node_get(tmp);
            if (ret != 0) {
#ifndef EMU_ST
                break;
#endif
            }
            ka_task_up_read(&info->rw_sem);
            return tmp;
        }
    }
    ka_task_up_read(&info->rw_sem);
    return NULL;
}

static struct devmm_task_dev_res_node *devmm_del_one_task_dev_res_node(struct devmm_task_dev_res_info *info)
{
    struct devmm_task_dev_res_node *node = NULL;
    int ret;

    ka_task_down_write(&info->rw_sem);
    if (ka_list_empty(&info->head) != 0) {
        ka_task_up_write(&info->rw_sem);
        return NULL;
    }

    node = ka_list_last_entry(&info->head, struct devmm_task_dev_res_node, task_node);
    ka_list_del_init(&node->task_node);
    ret = devmm_task_dev_res_node_get(node);  /* get node ref in lock, or the node will be release */
    ka_task_up_write(&info->rw_sem);
    if (ret != 0) {
#ifndef EMU_ST
        return NULL;
#endif
    }
    return node;
}

void devmm_task_dev_res_nodes_destroy_by_task(struct devmm_svm_process *svm_proc)
{
    struct devmm_svm_proc_master *master_data = (struct devmm_svm_proc_master *)svm_proc->priv_data;
    struct devmm_task_dev_res_info *info = &master_data->task_dev_res_info;
    struct devmm_task_dev_res_node *node = NULL;
    u32 stamp = (u32)ka_jiffies;

    while (1) {
        node = devmm_del_one_task_dev_res_node(info);
        if (node == NULL) {
            break;
        }
        devmm_task_dev_res_node_destroy(node);
        devmm_task_dev_res_node_put(node);
        devmm_try_cond_resched(&stamp);
    }
}
