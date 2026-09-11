/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Description:
 * Author: huawei
 * Create: 2024-7-20
 */

#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/version.h>
#include <linux/uaccess.h>

#include "ka_memory_mng.h"
#include "ka_module_init.h"
#include "ka_proc_fs.h"

#define KA_PROC_FS_NAME_LEN 32
#define KA_PROC_FS_MODE 0444

static struct proc_dir_entry *ka_task_entry = NULL;

STATIC int ka_enable_mem_record_show(struct seq_file *seq, void *offset)
{
    seq_printf(seq, "%d\n", (ka_is_enable_mem_record() == true));
    return 0;
}

STATIC int ka_enable_mem_record_ops_open(struct inode *inode, struct file *file)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
    return single_open(file, ka_enable_mem_record_show, pde_data(inode));
#else
    return single_open(file, ka_enable_mem_record_show, PDE_DATA(inode));
#endif
}

#define MEM_RECORD_DISABLE 0
STATIC ssize_t ka_enable_record_ops_write(struct file *filp, const char __user *ubuf, size_t count, loff_t *ppos)
{
    char ch[2] = {0}; /* 2 bytes long */
    long val;

    if ((ppos == NULL) || (*ppos != 0) || (count != sizeof(ch)) || (ubuf == NULL)) {
        return -EINVAL;
    }
    if (copy_from_user(ch, ubuf, count)) {
        return -ENOMEM;
    }
    ch[count - 1] = '\0';
    if (kstrtol(ch, 10, &val)) {
        return -EFAULT;
    }
    ka_mem_record_status_reset(val != MEM_RECORD_DISABLE);
    ka_debug("Enable_memory.(enable_is_true=%d).\n", (ka_is_enable_mem_record() == true));

    return (ssize_t)count;
}

STATIC int ka_get_alloc_mem_desc_open(struct inode *inode, struct file *file)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
    return single_open(file, ka_mem_stats_show, pde_data(inode));
#else
    return single_open(file, ka_mem_stats_show, PDE_DATA(inode));
#endif
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 5, 0)
static const struct file_operations ka_enable_mem_desc_ops = {
    .owner = THIS_MODULE,
    .open = ka_enable_mem_record_ops_open,
    .read = seq_read,
    .write = ka_enable_record_ops_write,
    .llseek  = seq_lseek,
    .release = single_release,
};
static const struct file_operations ka_mem_ops = {
    .owner = THIS_MODULE,
    .open = ka_get_alloc_mem_desc_open,
    .read = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};
#else
static const struct proc_ops ka_enable_mem_desc_ops = {
    .proc_open    = ka_enable_mem_record_ops_open,
    .proc_read    = seq_read,
    .proc_write   = ka_enable_record_ops_write,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};
static const struct proc_ops ka_mem_ops = {
    .proc_open    = ka_get_alloc_mem_desc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};
#endif

void ka_proc_fs_init(void)
{
    ka_task_entry = proc_mkdir("ka", NULL);
    if (ka_task_entry != NULL) {
        proc_create_data("ka_enable_mem_record", KA_PROC_FS_MODE, ka_task_entry, &ka_enable_mem_desc_ops,
            (void *)(uintptr_t)ka_is_enable_mem_record());
        proc_create_data("ka_mem_stats", KA_PROC_FS_MODE, ka_task_entry, &ka_mem_ops, (void *)(uintptr_t)ka_is_enable_mem_record());
    }
}

void ka_proc_fs_uninit(void)
{
    if (ka_task_entry != NULL) {
        remove_proc_subtree("ka", NULL);
    }
}

