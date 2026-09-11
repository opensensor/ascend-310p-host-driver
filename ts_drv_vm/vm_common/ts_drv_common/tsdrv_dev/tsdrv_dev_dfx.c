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
#ifndef AOS_LLVM_BUILD
#include <linux/debugfs.h>
#else
#include <linux/uaccess.h>
#endif
#include <linux/stat.h>

#include "securec.h"
#include "tsdrv_dev_dfx.h"
#include "tsdrv_id_dfx.h"
#include "tsdrv_device.h"
#include "tsdrv_mailbox_dfx.h"
#include "tsdrv_ctx_dfx.h"
#include "tsdrv_sqcq_timestamp.h"
#ifndef AOS_LLVM_BUILD
ssize_t simple_read_from_buffer_safe(void __user *to, size_t count, loff_t *ppos,
    const void *from, size_t available)
{
    if (to == NULL || ppos == NULL) {
        TSDRV_PRINT_ERR("user pointer is NULL.\n");
        return -EINVAL;
    }

    return ka_fs_simple_read_from_buffer(to, count, ppos, from, available);
}
#else
ssize_t simple_read_from_buffer_safe(void __user *to, const void *from, size_t available)
{
    int err;
    if (to == NULL) {
        TSDRV_PRINT_ERR("user pointer is NULL.\n");
        return -EINVAL;
    }
    err = ka_base_copy_to_user(to, from, available);
    if (err != 0) {
        TSDRV_PRINT_ERR("copy_to_user fail err = %d.\n", err);
        return 0;
    }
    return available;
}
#endif
KA_EXPORT_SYMBOL_GPL(simple_read_from_buffer_safe);
#ifndef AOS_LLVM_BUILD
static struct tsdrv_dfx *tsdrv_dfx_root;

struct tsdrv_dfx *tsdrv_get_dfx_root(void)
{
    return tsdrv_dfx_root;
}
KA_EXPORT_SYMBOL_GPL(tsdrv_get_dfx_root);

struct tsdrv_dfx_dev *tsdrv_get_dev_dfx(u32 devid)
{
    return &tsdrv_dfx_root->dfx_dev[devid];
}
KA_EXPORT_SYMBOL_GPL(tsdrv_get_dev_dfx);

struct tsdrv_dfx_fid *tsdrv_get_fid_dfx(u32 devid, u32 fid)
{
    return &tsdrv_dfx_root->dfx_dev[devid].dfx_fid[fid];
}
KA_EXPORT_SYMBOL_GPL(tsdrv_get_fid_dfx);

static int tsdrv_davinci_dfx_dir_create(u32 devid)
{
    struct tsdrv_dfx *dfx_root = tsdrv_get_dfx_root();
    char dir_name[TSDRV_DFX_NAME_LEN] = {0};
    struct tsdrv_dfx_dev *dfx_dev = NULL;
    int err;

    dfx_dev = tsdrv_get_dev_dfx(devid);
    if (dfx_dev->dev_dentry == NULL) {
        err = snprintf_s(dir_name, TSDRV_DFX_NAME_LEN, TSDRV_DFX_NAME_LEN - 1, "davinci%u", devid);
        DRV_CHECK_EXP_ACT(err < 0, return err, "dir_name copy fail, err(%d)\n", err);
        dfx_dev->dev_dentry = ka_fs_debugfs_create_dir(dir_name, dfx_root->root_dentry);
        if (dfx_dev->dev_dentry == NULL) {
            TSDRV_PRINT_ERR("davinci%u dfx dir create fail\n", devid);
            return -ENOMEM;
        }
        dfx_root->dev_num++;
    }
    return 0;
}

static void tsdrv_davinci_dfx_dir_remove(u32 devid)
{
    struct tsdrv_dfx_dev *dfx_dev = NULL;

    dfx_dev = tsdrv_get_dev_dfx(devid);
    if (dfx_dev->dev_dentry == NULL) {
        return;
    }
    if (dfx_dev->fid_num == 0) {
        ka_fs_debugfs_remove_recursive(dfx_dev->dev_dentry);
        dfx_dev->dev_dentry = NULL;
    }
}

static int tsdrv_dfx_dev_open(ka_inode_t *inode, ka_file_t *filp)
{
#ifndef TSDRV_UT
    u32 dfx_id = (u32)(uintptr_t)inode->i_private;
    u32 devid = dfx_id & TSDRV_DFX_DEV_MASK;
    u32 fid = (dfx_id >> TSDRV_DFX_DEV_OFFSET) & TSDRV_DFX_FID_MASK;

    TSDRV_PRINT_DEBUG("devid(%u) fid(%u)\n", devid, fid);
    filp->private_data = (void *)(uintptr_t)dfx_id;
#endif
    return 0;
}

ssize_t tsdrv_dfx_dev_read(ka_file_t *file, char __user *user_buf, size_t count, loff_t *ppos)
{
#ifndef TSDRV_UT
    u32 dfx_id = (u32)(uintptr_t)file->private_data;
    u32 devid = dfx_id & TSDRV_DFX_DEV_MASK;
    u32 fid = (dfx_id >> TSDRV_DFX_DEV_OFFSET) & TSDRV_DFX_FID_MASK;

    TSDRV_PRINT_DEBUG("devid(%u) fid(%u)\n", devid, fid);
#endif
    return 0;
}

STATIC const ka_file_operations_t g_dev_dbgfs_ops = {
    ka_fs_init_f_open(tsdrv_dfx_dev_open)
    ka_fs_init_f_read(tsdrv_dfx_dev_read)
};

static int tsdrv_status_dfx_file_create(u32 devid, u32 fid)
{
    struct tsdrv_dfx_fid *dfx_fid = NULL;
    u32 dfx_id = (fid << TSDRV_DFX_DEV_OFFSET) | devid;

    dfx_fid = tsdrv_get_fid_dfx(devid, fid);
    if (dfx_fid->fid_dentry == NULL) {
        TSDRV_PRINT_ERR("fid entry is NULL, devid(%u) fid(%u)\n", devid, fid);
        return -ENODEV;
    }
    if (ka_fs_debugfs_create_file("status", KA_S_IRUGO, dfx_fid->fid_dentry, (void *)(uintptr_t)dfx_id,
        &g_dev_dbgfs_ops) == NULL) {
        TSDRV_PRINT_ERR("create status fail, devid(%u) fid(%u)", devid, fid);
        return -ENOMEM;
    }
    return 0;
}

#define TSDRV_SQCQ_DFX_ID_BUF_LEN   256U

static int tsdrv_sqcq_dfx_open(ka_inode_t *inode, ka_file_t *filp)
{
#ifndef TSDRV_UT
    u32 dfx_id = (u32)(uintptr_t)inode->i_private;
    u32 devid = dfx_id & TSDRV_DFX_DEV_MASK;
    u32 fid = (dfx_id >> TSDRV_DFX_DEV_OFFSET) & TSDRV_DFX_FID_MASK;

    TSDRV_PRINT_DEBUG("devid(%u) fid(%u)\n", devid, fid);

    filp->private_data = (void *)(uintptr_t)dfx_id;
#endif
    return 0;
}

ssize_t tsdrv_sqcq_dfx_read(ka_file_t *file, char __user *user_buf, size_t count, loff_t *ppos)
{
#ifndef TSDRV_UT
    char buf[TSDRV_SQCQ_DFX_ID_BUF_LEN] = {0};
    u32 dfx_id = (u32)(uintptr_t)file->private_data;
    u32 devid = dfx_id & TSDRV_DFX_DEV_MASK;
    u32 fid = (dfx_id >> TSDRV_DFX_DEV_OFFSET) & TSDRV_DFX_FID_MASK;
    int len;

    len = snprintf_s(buf, TSDRV_SQCQ_DFX_ID_BUF_LEN, TSDRV_SQCQ_DFX_ID_BUF_LEN - 1, "sqcq_dfx_enable: %u\n",
        (u32)tsdrv_sqcq_dfx_is_enabled(devid, fid));
    if (len < 0) {
        TSDRV_PRINT_ERR("snprinf_s failed. (devid=%u; fid=%u; ret=%d)\n", devid, fid, len);
        return 0;
    }

    return simple_read_from_buffer_safe(user_buf, count, ppos, buf, len);
#else
    return 0;
#endif
}

STATIC ssize_t tsdrv_sqcq_dfx_write(ka_file_t *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
#ifndef TSDRV_UT
    u32 dfx_id = (u32)(uintptr_t)file->private_data;
    u32 devid = dfx_id & TSDRV_DFX_DEV_MASK;
    u32 fid = (dfx_id >> TSDRV_DFX_DEV_OFFSET) & TSDRV_DFX_FID_MASK;
    char ch[2] = {0}; /* 2 bytes long */
    long val;
    bool enable;

    if ((ppos == NULL) || (*ppos != 0) || (count != sizeof(ch)) || (user_buf == NULL)) {
        return -EINVAL;
    }

    if (copy_from_user_safe(ch, user_buf, count)) {
        return -ENOMEM;
    }

    ch[count - 1] = '\0';
    if (ka_base_kstrtol(ch, 10, &val)) {
        return -EFAULT;
    }

    enable = (val == 0) ? false : true;
    tsdrv_sqcq_dfx_set_enable_status(devid, fid, enable);

    return (ssize_t)count;
#endif
}

STATIC const ka_file_operations_t g_sqcq_dbgfs_ops = {
    ka_fs_init_f_open(tsdrv_sqcq_dfx_open)
    ka_fs_init_f_read(tsdrv_sqcq_dfx_read)
    ka_fs_init_f_write(tsdrv_sqcq_dfx_write)
};

static int tsdrv_sqcq_dfx_file_create(u32 devid, u32 fid)
{
    struct tsdrv_dfx_fid *dfx_fid = NULL;
    u32 dfx_id = (fid << TSDRV_DFX_DEV_OFFSET) | devid;

    dfx_fid = tsdrv_get_fid_dfx(devid, fid);
    if (dfx_fid->fid_dentry == NULL) {
        TSDRV_PRINT_ERR("fid entry is NULL, devid(%u) fid(%u)\n", devid, fid);
        return -ENODEV;
    }
    if (ka_fs_debugfs_create_file("sqcq_dfx", KA_S_IRUGO, dfx_fid->fid_dentry, (void *)(uintptr_t)dfx_id,
        &g_sqcq_dbgfs_ops) == NULL) {
        TSDRV_PRINT_ERR("create status fail, devid(%u) fid(%u)", devid, fid);
        return -ENOMEM;
    }

    return 0;
}

static int tsdrv_fid_dfx_files_create(u32 devid, u32 fid)
{
    int err;

    err = tsdrv_status_dfx_file_create(devid, fid);
    if (err != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("status dfx file create fail, devid(%u) fid(%u)\n", devid, fid);
        return err;
#endif
    }
    err = tsdrv_id_dfx_file_create(devid, fid);
    if (err != 0) {
#ifndef TSDRV_UT
        /* delete fid dfx isn't necessary, it will be deleted recursively when def dfx dir removed */
        TSDRV_PRINT_ERR("id dfx file create fail, devid(%u) fid(%u)\n", devid, fid);
        return err;
#endif
    }
    err = tsdrv_mailbox_dfx_file_create(devid, fid);
    if (err != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("mbox dfx file create fail, devid(%u) fid(%u)\n", devid, fid);
        return err;
#endif
    }

    err = tsdrv_ctx_dfx_file_create(devid, fid);
    if (err != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("ctx dfx file create fail, devid(%u) fid(%u)\n", devid, fid);
        return err;
#endif
    }

    err = tsdrv_sqcq_dfx_file_create(devid, fid);
    if (err != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("sqcq dfx file create fail. (devid=%u; fid=%u)\n", devid, fid);
        return err;
#endif
    }

    return 0;
}

static int tsdrv_fid_dfx_dir_create(u32 devid, u32 fid)
{
    char dir_name[TSDRV_DFX_NAME_LEN] = {0};
    struct tsdrv_dfx_dev *dfx_dev = NULL;
    struct tsdrv_dfx_fid *dfx_fid = NULL;
    int err;

    dfx_dev = tsdrv_get_dev_dfx(devid);
    if (dfx_dev->dev_dentry == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("dfx dev is NULL devid(%u)\n", devid);
        return -ENODEV;
#endif
    }
    dfx_fid = tsdrv_get_fid_dfx(devid, fid);
    if (dfx_fid->fid_dentry != NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("fid entry is not NULL, devid(%u) fid(%u)\n", devid, fid);
        return -ENODEV;
#endif
    }
    err = snprintf_s(dir_name, TSDRV_DFX_NAME_LEN, TSDRV_DFX_NAME_LEN - 1, "fid%u", fid);
    if (err < 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("dir_name copy fail, err(%d)\n", err);
        return -ENOMEM;
#endif
    }
    dfx_fid->fid_dentry = ka_fs_debugfs_create_dir(dir_name, dfx_dev->dev_dentry);
    if (dfx_fid->fid_dentry == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("fid debuffs create fail, devid(%u) fid(%u)\n", devid, fid);
        return -ENOMEM;
#endif
    }

    err = tsdrv_fid_dfx_files_create(devid, fid);
    if (err != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("dfx_files create fail err = %d, devid(%u) fid(%u)\n", devid, fid, err);
        return -ENODEV;
#endif
    }

    dfx_dev->fid_num++;
    return 0;
}

static void tsdrv_fid_dfx_dir_remove(u32 devid, u32 fid)
{
    struct tsdrv_dfx_dev *dfx_dev = NULL;
    struct tsdrv_dfx_fid *dfx_fid = NULL;

    dfx_dev = tsdrv_get_dev_dfx(devid);
    if (dfx_dev->dev_dentry == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("dfx dev is NULL, devid(%u)\n", devid);
        return;
#endif
    }
    dfx_fid = tsdrv_get_fid_dfx(devid, fid);
    if (dfx_fid->fid_dentry == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("dfx fid is NULL, devid(%u) fid(%u)\n", devid, fid);
        return;
#endif
    }
    ka_fs_debugfs_remove_recursive(dfx_fid->fid_dentry);
    dfx_fid->fid_dentry = NULL;
    dfx_dev->fid_num--;
}

int tsdrv_dev_dfx_dir_create(u32 devid, u32 fid)
{
    struct tsdrv_dfx *dfx_root = tsdrv_get_dfx_root();
    int err;

    if (dfx_root->root_dentry == NULL) {
        TSDRV_PRINT_ERR("dfx_root is NULL\n");
        return -ENODEV;
    }
    ka_task_mutex_lock(&dfx_root->lock);
    err = tsdrv_davinci_dfx_dir_create(devid);
    if (err != 0) {
        ka_task_mutex_unlock(&dfx_root->lock);
        TSDRV_PRINT_ERR("davinci dfx dir create fail\n");
        return -ENODEV;
    }
    err = tsdrv_fid_dfx_dir_create(devid, fid);
    if (err != 0) {
        tsdrv_davinci_dfx_dir_remove(devid);
        ka_task_mutex_unlock(&dfx_root->lock);
        return -ENODEV;
    }
    ka_task_mutex_unlock(&dfx_root->lock);
    return 0;
}

void tsdrv_dev_dfx_dir_remove(u32 devid, u32 fid)
{
    struct tsdrv_dfx *dfx_root = tsdrv_get_dfx_root();

    dfx_root = tsdrv_get_dfx_root();
    if (dfx_root->root_dentry == NULL) {
        TSDRV_PRINT_ERR("dfx_root is NULL\n");
        return;
    }
    ka_task_mutex_lock(&dfx_root->lock);
    tsdrv_fid_dfx_dir_remove(devid, fid);
    tsdrv_davinci_dfx_dir_remove(devid);
    ka_task_mutex_unlock(&dfx_root->lock);
}

int tsdrv_dev_dfx_init(void)
{
    struct tsdrv_dfx *dfx_root = NULL;

    if (tsdrv_dfx_root == NULL) {
        tsdrv_dfx_root = (struct tsdrv_dfx *)tsdrv_kzalloc(sizeof(struct tsdrv_dfx), KA_GFP_KERNEL);
        if (tsdrv_dfx_root == NULL) {
            TSDRV_PRINT_ERR("kzalloc failed.\n");
            return -ENOMEM;
        }
    }

    dfx_root = tsdrv_get_dfx_root();
    dfx_root->root_dentry = ka_fs_debugfs_create_dir("tsdrv", NULL);
    if (dfx_root->root_dentry == NULL) {
#ifndef TSDRV_UT
        if (tsdrv_dfx_root != NULL) {
            tsdrv_kfree(dfx_root);
            tsdrv_dfx_root = NULL;
        }
        TSDRV_PRINT_ERR("tsdrv dbgfs dir create fail\n");
        return -ENODEV;
#endif
    }
    ka_task_mutex_init(&dfx_root->lock);
    dfx_root->dev_num = 0;
    return 0;
}

void tsdrv_dev_dfx_exit(void)
{
    struct tsdrv_dfx *dfx_root = tsdrv_get_dfx_root();

    if (dfx_root == NULL) {
        return;
    }

    if (dfx_root->root_dentry != NULL) {
        ka_fs_debugfs_remove_recursive(dfx_root->root_dentry);
        dfx_root->root_dentry = NULL;
    }
    dfx_root->dev_num = 0;
    ka_task_mutex_destroy(&dfx_root->lock);
    tsdrv_kfree(dfx_root);
    tsdrv_dfx_root = NULL;
}

#else

static struct tsdrv_proc_dfx tsdrv_proc_dfx_root;

struct tsdrv_proc_dfx *tsdrv_get_dfx_root(void)
{
    return &tsdrv_proc_dfx_root;
}
KA_EXPORT_SYMBOL_GPL(tsdrv_get_dfx_root);

struct tsdrv_proc_dfx_dev *tsdrv_get_dev_dfx(u32 devid)
{
    return &tsdrv_proc_dfx_root.dfx_dev[devid];
}
KA_EXPORT_SYMBOL_GPL(tsdrv_get_dev_dfx);

struct tsdrv_proc_dfx_fid *tsdrv_get_fid_dfx(u32 devid, u32 fid)
{
    return &tsdrv_proc_dfx_root.dfx_dev[devid].dfx_fid[fid];
}
KA_EXPORT_SYMBOL_GPL(tsdrv_get_fid_dfx);

static int tsdrv_davinci_dfx_dir_create(u32 devid)
{
    struct tsdrv_proc_dfx *dfx_root = tsdrv_get_dfx_root();
    char dir_name[TSDRV_DFX_NAME_LEN] = {0};
    struct tsdrv_proc_dfx_dev *dfx_dev = NULL;
    int err;

    dfx_dev = tsdrv_get_dev_dfx(devid);
    if (dfx_dev->dev_dentry == NULL) {
        err = snprintf_s(dir_name, TSDRV_DFX_NAME_LEN, TSDRV_DFX_NAME_LEN - 1, "davinci%u", devid);
        DRV_CHECK_EXP_ACT(err < 0, return err, "dir_name copy fail, err(%d)\n", err);
        dfx_dev->dev_dentry = ka_fs_proc_mkdir(dir_name, dfx_root->root_dentry);
        if (dfx_dev->dev_dentry == NULL) {
            TSDRV_PRINT_ERR("davinci%u dfx dir create fail\n", devid);
            return -ENOMEM;
        }
        dfx_root->dev_num++;
    }
    return 0;
}

static void tsdrv_davinci_dfx_dir_remove(u32 devid)
{
    struct tsdrv_proc_dfx_dev *dfx_dev = NULL;

    dfx_dev = tsdrv_get_dev_dfx(devid);
    if (dfx_dev->dev_dentry == NULL) {
        return;
    }
    if (dfx_dev->fid_num == 0) {
        proc_remove(dfx_dev->dev_dentry);
        dfx_dev->dev_dentry = NULL;
    }
}

static int tsdrv_dfx_dev_open(ka_inode_t *inode, ka_file_t *filp)
{
#ifndef TSDRV_UT
    u32 dfx_id = (u32)(uintptr_t)inode->i_private;
    u32 devid = dfx_id & TSDRV_DFX_DEV_MASK;
    u32 fid = (dfx_id >> TSDRV_DFX_DEV_OFFSET) & TSDRV_DFX_FID_MASK;

    TSDRV_PRINT_DEBUG("devid(%u) fid(%u)\n", devid, fid);
    filp->private_data = (void *)(uintptr_t)dfx_id;
#endif
    return 0;
}

ssize_t tsdrv_dfx_dev_read(ka_file_t *file, char __user *user_buf, size_t count, loff_t *ppos)
{
#ifndef TSDRV_UT
    u32 dfx_id = (u32)(uintptr_t)file->private_data;
    u32 devid = dfx_id & TSDRV_DFX_DEV_MASK;
    u32 fid = (dfx_id >> TSDRV_DFX_DEV_OFFSET) & TSDRV_DFX_FID_MASK;

    TSDRV_PRINT_DEBUG("devid(%u) fid(%u)\n", devid, fid);
#endif
    return 0;
}

STATIC const ka_file_operations_t g_dev_dbgfs_ops = {
    ka_fs_init_f_open(tsdrv_dfx_dev_open)
    ka_fs_init_f_read(tsdrv_dfx_dev_read)
};

static int tsdrv_status_dfx_file_create(u32 devid, u32 fid)
{
    struct tsdrv_proc_dfx_fid *dfx_fid = NULL;

    dfx_fid = tsdrv_get_fid_dfx(devid, fid);
    if (dfx_fid->fid_dentry == NULL) {
        TSDRV_PRINT_ERR("fid entry is NULL, devid(%u) fid(%u)\n", devid, fid);
        return -ENODEV;
    }
    if (ka_fs_proc_create("status", TS_DRV_ATTR_RD, dfx_fid->fid_dentry, &g_dev_dbgfs_ops) == NULL) {
        TSDRV_PRINT_ERR("create status fail, devid(%u) fid(%u)", devid, fid);
        return -ENOMEM;
    }
    return 0;
}

static int tsdrv_fid_dfx_files_create(u32 devid, u32 fid)
{
    int err;

    err = tsdrv_status_dfx_file_create(devid, fid);
    if (err != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("status dfx file create fail, devid(%u) fid(%u)\n", devid, fid);
        return err;
#endif
    }
    err = tsdrv_id_dfx_file_create(devid, fid);
    if (err != 0) {
#ifndef TSDRV_UT
        /* delete fid dfx isn't necessary, it will be deleted recursively when def dfx dir removed */
        TSDRV_PRINT_ERR("id dfx file create fail, devid(%u) fid(%u)\n", devid, fid);
        return err;
#endif
    }
    err = tsdrv_mailbox_dfx_file_create(devid, fid);
    if (err != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("mbox dfx file create fail, devid(%u) fid(%u)\n", devid, fid);
        return err;
#endif
    }

    err = tsdrv_ctx_dfx_file_create(devid, fid);
    if (err != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("ctx dfx file create fail, devid(%u) fid(%u)\n", devid, fid);
        return err;
#endif
    }

    return 0;
}

static int tsdrv_fid_dfx_dir_create(u32 devid, u32 fid)
{
    char dir_name[TSDRV_DFX_NAME_LEN] = {0};
    struct tsdrv_proc_dfx_dev *dfx_dev = NULL;
    struct tsdrv_proc_dfx_fid *dfx_fid = NULL;
    int err;

    dfx_dev = tsdrv_get_dev_dfx(devid);
    if (dfx_dev->dev_dentry == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("dfx dev is NULL devid(%u)\n", devid);
        return -ENODEV;
#endif
    }
    dfx_fid = tsdrv_get_fid_dfx(devid, fid);
    if (dfx_fid->fid_dentry != NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("fid entry is not NULL, devid(%u) fid(%u)\n", devid, fid);
        return -ENODEV;
#endif
    }
    err = snprintf_s(dir_name, TSDRV_DFX_NAME_LEN, TSDRV_DFX_NAME_LEN - 1, "fid%u", fid);
    if (err < 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("dir_name copy fail, err(%d)\n", err);
        return -ENOMEM;
#endif
    }
    dfx_fid->fid_dentry = ka_fs_proc_mkdir(dir_name, dfx_dev->dev_dentry);
    if (dfx_fid->fid_dentry == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("fid debuffs create fail, devid(%u) fid(%u)\n", devid, fid);
        return -ENOMEM;
#endif
    }

    err = tsdrv_fid_dfx_files_create(devid, fid);
    if (err != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("dfx_files create fail err = %d, devid(%u) fid(%u)\n", devid, fid, err);
        return -ENODEV;
#endif
    }

    dfx_dev->fid_num++;
    return 0;
}

static void tsdrv_fid_dfx_dir_remove(u32 devid, u32 fid)
{
    struct tsdrv_proc_dfx_dev *dfx_dev = NULL;
    struct tsdrv_proc_dfx_fid *dfx_fid = NULL;

    dfx_dev = tsdrv_get_dev_dfx(devid);
    if (dfx_dev->dev_dentry == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("dfx dev is NULL, devid(%u)\n", devid);
        return;
#endif
    }
    dfx_fid = tsdrv_get_fid_dfx(devid, fid);
    if (dfx_fid->fid_dentry == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("dfx fid is NULL, devid(%u) fid(%u)\n", devid, fid);
        return;
#endif
    }
    ka_fs_proc_remove(dfx_fid->fid_dentry);
    dfx_fid->fid_dentry = NULL;
    dfx_dev->fid_num--;
}

int tsdrv_dev_dfx_dir_create(u32 devid, u32 fid)
{
    struct tsdrv_proc_dfx *dfx_root = tsdrv_get_dfx_root();
    int err;

    if (dfx_root->root_dentry == NULL) {
        TSDRV_PRINT_ERR("dfx_root is NULL\n");
        return -ENODEV;
    }
    ka_task_mutex_lock(&dfx_root->lock);
    err = tsdrv_davinci_dfx_dir_create(devid);
    if (err != 0) {
        ka_task_mutex_unlock(&dfx_root->lock);
        TSDRV_PRINT_ERR("davinci dfx dir create fail\n");
        return -ENODEV;
    }
    err = tsdrv_fid_dfx_dir_create(devid, fid);
    if (err != 0) {
        tsdrv_davinci_dfx_dir_remove(devid);
        ka_task_mutex_unlock(&dfx_root->lock);
        return -ENODEV;
    }
    ka_task_mutex_unlock(&dfx_root->lock);
    return 0;
}

void tsdrv_dev_dfx_dir_remove(u32 devid, u32 fid)
{
    struct tsdrv_proc_dfx *dfx_root = tsdrv_get_dfx_root();

    dfx_root = tsdrv_get_dfx_root();
    if (dfx_root->root_dentry == NULL) {
        TSDRV_PRINT_ERR("dfx_root is NULL\n");
        return;
    }
    ka_task_mutex_lock(&dfx_root->lock);
    tsdrv_fid_dfx_dir_remove(devid, fid);
    tsdrv_davinci_dfx_dir_remove(devid);
    ka_task_mutex_unlock(&dfx_root->lock);
}

int tsdrv_dev_dfx_init(void)
{
    struct tsdrv_proc_dfx *dfx_root = tsdrv_get_dfx_root();

    dfx_root->root_dentry = ka_fs_proc_mkdir("tsdrv", NULL);
    if (dfx_root->root_dentry == NULL) {
        TSDRV_PRINT_ERR("tsdrv dbgfs dir create fail\n");
        return -ENODEV;
    }
    ka_task_mutex_init(&dfx_root->lock);
    dfx_root->dev_num = 0;
    return 0;
}

void tsdrv_dev_dfx_exit(void)
{
    struct tsdrv_proc_dfx *dfx_root = tsdrv_get_dfx_root();

    if (dfx_root->root_dentry != NULL) {
        ka_fs_proc_remove(dfx_root->root_dentry);
        dfx_root->root_dentry = NULL;
    }
    dfx_root->dev_num = 0;
    ka_task_mutex_destroy(&dfx_root->lock);
}
#endif
