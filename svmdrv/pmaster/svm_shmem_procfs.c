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
#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/version.h>

#include "securec.h"
#include "svm_shmem_node.h"
#include "svm_shmem_procfs.h"

#define PROC_FS_TOP_NAME "svm_shmem"
#define PROC_FS_MODE 0400

static ka_proc_dir_entry_t *g_svm_shmem_top_entry = NULL;

static int devmm_ipc_procfs_sum_show(ka_seq_file_t *seq, void *offset)
{
#ifndef EMU_ST
    struct devmm_ipc_node *node = (struct devmm_ipc_node *)seq->private;
    struct ipc_node_wlist *wlist = NULL;
    u32 stamp = (u32)ka_jiffies;
    int i = 0;

    ka_fs_seq_printf(seq, "Ipc node detail:\n");
    ka_fs_seq_printf(seq, "name=%s;sdid=%u;devid=%u;vfid=%u;pid=%d;vptr=0x%llx;len=%ld;page_size=%ld;is_huge=%d;"
        "is_reserve=%d;need_set_wlist=%d;valid=%d;key=%u;ref=%d;mem_repair=%d\n",
        node->attr.name, node->attr.sdid, node->attr.inst.devid, node->attr.inst.vfid, node->attr.pid,
        node->attr.vptr, node->attr.len, node->attr.page_size, node->attr.is_huge, node->attr.is_reserve_addr,
        node->attr.need_set_wlist, node->valid, node->key, ka_base_kref_read(&node->ref), node->mem_repair_record);
    ka_fs_seq_printf(seq, "White List:\n");
    ka_list_for_each_entry(wlist, &node->wlist_head, list) {
        ka_fs_seq_printf(seq, "   [%d]:sdid=%u;pid=%d;set_time=%lu;vptr=0x%llx\n",
            i++, wlist->sdid, wlist->pid, wlist->set_time, wlist->vptr);
        devmm_try_cond_resched(&stamp);
    }
#endif
    return 0;
}

static int devmm_ipc_procfs_ops_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, devmm_ipc_procfs_sum_show, ka_base_pde_data(inode));
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 5, 0)
static const ka_file_operations_t devmm_ipc_procfs_ops = {
    .owner = KA_THIS_MODULE,
    .open    = devmm_ipc_procfs_ops_open,
    .read    = ka_fs_seq_read,
    .llseek  = ka_fs_seq_lseek,
    .release = ka_fs_single_release,
};
#else
static const ka_procfs_ops_t devmm_ipc_procfs_ops = {
    .proc_open    = devmm_ipc_procfs_ops_open,
    .proc_read    = ka_fs_seq_read,
    .proc_lseek   = ka_fs_seq_lseek,
    .proc_release = ka_fs_single_release,
};
#endif

void devmm_ipc_procfs_add_node(struct devmm_ipc_node *node)
{
    if (g_svm_shmem_top_entry != NULL) {
        node->entry = ka_fs_proc_create_data(node->attr.name, PROC_FS_MODE,
            g_svm_shmem_top_entry, &devmm_ipc_procfs_ops, node);
    }
}

void devmm_ipc_procfs_del_node(struct devmm_ipc_node *node)
{
    if (node->entry != NULL) {
        ka_fs_proc_remove(node->entry);
        node->entry = NULL;
    }
}

void devmm_ipc_profs_init(void)
{
    g_svm_shmem_top_entry = ka_fs_proc_mkdir(PROC_FS_TOP_NAME, NULL);
    if (g_svm_shmem_top_entry == NULL) {
        devmm_drv_warn("Create top entry dir fail\n");
    }
}

void devmm_ipc_profs_uninit(void)
{
    if (g_svm_shmem_top_entry != NULL) {
        (void)ka_fs_remove_proc_subtree(PROC_FS_TOP_NAME, NULL);
        g_svm_shmem_top_entry = NULL;
    }
}

#endif /* CONFIG_PROC_FS */
