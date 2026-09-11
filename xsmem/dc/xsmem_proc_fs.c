/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
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
 * Create: 2022-08-13
 */

#include "securec.h"
#include "xsmem_res_dispatch.h"
#include "xsmem_framework_log.h"
#include "xsmem_framework.h"
#include "xsmem_proc_fs.h"
#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_list_pub.h"
#include "ka_fs_pub.h"
#include "ka_kernel_def_pub.h"

#define PROC_FS_NAME_LEN 32
#define PROC_FS_MODE 0444

extern ka_mutex_t xsmem_mutex;
extern ka_idr_t xsmem_idr;

static ka_proc_dir_entry_t *top_entry = NULL;
static ka_proc_dir_entry_t *xp_entry = NULL;
static ka_proc_dir_entry_t *task_entry = NULL;

static int task_node_show(ka_seq_file_t *seq, void *offset)
{
    struct xsm_task_pool_node *node = (struct xsm_task_pool_node *)ka_fs_get_seq_file_private(seq);
    struct xsm_pool *xp = node->pool;
    struct xsm_task_block_node *task_blk_node = NULL;
    int num = 0;

    ka_fs_seq_printf(seq, "task id %d pid %d\n", node->task_id, node->task->pid);
    ka_fs_seq_printf(seq, "block id: offset alloc_size real_size alloc_pid refcnt\n");

    ka_task_mutex_lock(&xp->xp_block_mutex);
    ka_list_for_each_entry(task_blk_node, &node->task_blk_head, node_list) {
        struct xsm_block *blk = task_blk_node->blk;
        ka_fs_seq_printf(seq, "    %-4d:%-8pK %-8lu %-8lu %-4d %-4ld\n", blk->id,
            (void *)(uintptr_t)blk->offset, blk->alloc_size, blk->real_size, blk->pid, blk->refcnt);
        num++;
    }
    ka_task_mutex_unlock(&xp->xp_block_mutex);

    ka_fs_seq_printf(seq, "total block %d, total alloc size %llu, total real size %llu\n",
        num, node->alloc_size, node->real_alloc_size);

    return 0;
}

STATIC int task_node_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, task_node_show, ka_base_pde_data(inode));
}

static ka_procfs_ops_t task_node_ops = {
    ka_fs_init_pf_owner(KA_THIS_MODULE)
    ka_fs_init_pf_open(task_node_open)
    ka_fs_init_pf_read(ka_fs_seq_read)
    ka_fs_init_pf_lseek(ka_fs_seq_lseek)
    ka_fs_init_pf_release(ka_fs_single_release)
};

static void proc_fs_format_task_dir_name(const struct xsm_task *task, char *name, int len)
{
    if (sprintf_s(name, (u32)len, "%d", task->pid) <= 0) {
        xsmem_warn("pid %d sprintf_s failed\n", task->pid);
    }
}

static ka_proc_dir_entry_t *proc_fs_mk_task_dir(struct xsm_task *task, ka_proc_dir_entry_t *parent)
{
    char name[PROC_FS_NAME_LEN];

    proc_fs_format_task_dir_name(task, name, PROC_FS_NAME_LEN);
    return ka_fs_proc_mkdir((const char *)name, parent);
}

static void proc_fs_rm_task_dir(struct xsm_task *task, ka_proc_dir_entry_t *parent)
{
    char name[PROC_FS_NAME_LEN];

    proc_fs_format_task_dir_name(task, name, PROC_FS_NAME_LEN);
    (void)ka_fs_remove_proc_subtree((const char *)name, parent);
}

void proc_fs_xsmem_pool_add_task(struct xsm_task_pool_node *node)
{
    ka_proc_dir_entry_t *entry = NULL;
    struct xsm_pool *xp = node->pool;
    struct xsm_task *task = node->task;

    if (xp->entry != NULL) {
        entry = proc_fs_mk_task_dir(task, xp->entry);
        if (entry != NULL) {
            (void)ka_fs_proc_create_data("block", PROC_FS_MODE, entry, &task_node_ops, node);
        }
    }

    if (task->entry != NULL) {
        entry = ka_fs_proc_mkdir(xp->key, task->entry);
        if (entry != NULL) {
            (void)ka_fs_proc_create_data("block", PROC_FS_MODE, entry, &task_node_ops, node);
        }
    }
}

void proc_fs_xsmem_pool_del_task(struct xsm_task_pool_node *node)
{
    struct xsm_pool *xp = node->pool;
    struct xsm_task *task = node->task;

    if (xp->entry != NULL) {
        proc_fs_rm_task_dir(task, xp->entry);
    }
    if (task->entry != NULL) {
        (void)ka_fs_remove_proc_subtree(xp->key, task->entry);
    }
}

void proc_fs_add_xsmem_pool(struct xsm_pool *xp)
{
    if (xp_entry != NULL) {
        xp->entry = ka_fs_proc_mkdir(xp->key, xp_entry);
    }
}

void proc_fs_del_xsmem_pool(struct xsm_pool *xp)
{
    if (xp_entry != NULL) {
        (void)ka_fs_remove_proc_subtree(xp->key, xp_entry);
    }
}

static void per_task_info_show(ka_seq_file_t *seq, struct xsm_task *task)
{
    struct xsm_task_pool_node *node = NULL;
    u64 alloc_size = 0, real_alloc_size = 0;

    ka_fs_seq_printf(seq, "pool id(name): alloc_size real_size peak_size\n");

    ka_list_for_each_entry(node, &task->node_list_head, task_node) {
        /* can not modify the string format, because udf&aicpu will parse it for dfx. */
        ka_fs_seq_printf(seq, "%d(%s) %-8llu %-8llu %-8llu\n", node->pool->pool_id, node->pool->key,
                node->alloc_size, node->real_alloc_size, node->alloc_peak_size);
        alloc_size += node->alloc_size;
        real_alloc_size += node->real_alloc_size;
    }

    ka_fs_seq_printf(seq, "summary: %-8llu %-8llu\n", alloc_size, real_alloc_size);
}

static int task_info_show(ka_seq_file_t *seq, void *offset)
{
    struct xsm_task *task = (struct xsm_task *)seq->private;

    ka_task_mutex_lock(&task->mutex);
    ka_fs_seq_printf(seq, "task pid %d pool cnt %d\n", task->pid, task->attached_pool_count);
    per_task_info_show(seq, task);
    ka_task_mutex_unlock(&task->mutex);

    return 0;
}

STATIC int task_sum_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, task_info_show, ka_base_pde_data(inode));
}

static ka_procfs_ops_t task_sum_ops = {
    ka_fs_init_pf_owner(KA_THIS_MODULE)
    ka_fs_init_pf_open(task_sum_open)
    ka_fs_init_pf_read(ka_fs_seq_read)
    ka_fs_init_pf_lseek(ka_fs_seq_lseek)
    ka_fs_init_pf_release(ka_fs_single_release)
};

void proc_fs_add_task(struct xsm_task *task)
{
    if (task_entry == NULL) {
        return;
    }

    task->entry = proc_fs_mk_task_dir(task, task_entry);
    if (task->entry != NULL) {
        (void)ka_fs_proc_create_data("summary", PROC_FS_MODE, task->entry, &task_sum_ops, task);
    }
}

void proc_fs_del_task(struct xsm_task *task)
{
    if (task_entry == NULL) {
        return;
    }

    proc_fs_rm_task_dir(task, task_entry);
}

static int per_pool_info_show(int id, void *p, void *data)
{
    struct xsm_pool *xp = p;
    ka_seq_file_t *seq = data;

    ka_fs_seq_printf(seq, "id:%d\n    key:%s, create_pid %d, refcnt:%d, algo:%s, alloc_size %lu, real_alloc_size %lu, "
        "alloc_peak_size %lu\n", id, xp->key, xp->create_pid, ka_base_atomic_read(&xp->refcnt), xp->algo->name,
        xp->alloc_size, xp->real_alloc_size, xp->alloc_peak_size);
    if (xp->algo->xsm_pool_show) {
        ka_task_mutex_lock(&xp->xp_block_mutex);
        xp->algo->xsm_pool_show(xp, seq);
        ka_task_mutex_unlock(&xp->xp_block_mutex);
    }

    return 0;
}

static int pool_info_show(ka_seq_file_t *seq, void *offset)
{
    int ret;

    ka_fs_seq_printf(seq, "pool list:\n");

    ka_task_mutex_lock(&xsmem_mutex);
    ret = ka_base_idr_for_each(&xsmem_idr, per_pool_info_show, seq);
    ka_task_mutex_unlock(&xsmem_mutex);

    return ret;
}

STATIC int xsmem_pool_sum_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, pool_info_show, ka_base_pde_data(inode));
}

static ka_procfs_ops_t pool_sum_ops = {
    ka_fs_init_pf_owner(KA_THIS_MODULE)
    ka_fs_init_pf_open(xsmem_pool_sum_open)
    ka_fs_init_pf_read(ka_fs_seq_read)
    ka_fs_init_pf_lseek(ka_fs_seq_lseek)
    ka_fs_init_pf_release(ka_fs_single_release)
};

void xsmem_proc_fs_init(void)
{
    top_entry = ka_fs_proc_mkdir("xsmem", NULL);
    if (top_entry != NULL) {
        xp_entry = ka_fs_proc_mkdir("pool", top_entry);
        if (xp_entry != NULL) {
            (void)ka_fs_proc_create_data("summary", PROC_FS_MODE, xp_entry, &pool_sum_ops, NULL);
        }
        task_entry = ka_fs_proc_mkdir("task", top_entry);
    }
}

void xsmem_proc_fs_uninit(void)
{
    if (top_entry != NULL) {
        (void)ka_fs_remove_proc_subtree("xsmem", NULL);
    }
}
