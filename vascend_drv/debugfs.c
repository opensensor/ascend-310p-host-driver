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
 */

#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/rbtree.h>
#include <linux/fs.h>

#include "dvt.h"
#include "kvmdt.h"
#include "vfio_ops.h"

#define MAX_NAME_LEN 32

STATIC struct kref debugfs_ref;
STATIC DEFINE_MUTEX(debugfs_vascend_lock);
STATIC struct dentry *vascend_debugfs_root = NULL;

STATIC int vdavinci_msix_count_show(struct seq_file *s, void *unused)
{
    int i;
    struct hw_vdavinci *vdavinci = s->private;

    seq_printf(s, "%-16s %-16s\n", "Vector", "Count");
    for (i = 0; i < vdavinci->debugfs.nvec; i++) {
        seq_printf(s, "%-16d %-16llu\n", i, vdavinci->debugfs.msix_count[i]);
    }

    return 0;
}

static int seq_file_msix_count_open(struct inode *inode, struct file *file)
{
    return single_open(file, &vdavinci_msix_count_show, inode->i_private);
}

static const struct file_operations vdavinci_msix_count_fops = {
    .owner = THIS_MODULE,
    .open = seq_file_msix_count_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

/**
 * hw_dvt_debugfs_add_vdavinci - register debugfs entries for a vdavinci
 */
void hw_dvt_debugfs_add_vdavinci(struct hw_vdavinci *vdavinci)
{
    int ret;
    char *name = NULL;

    name = kzalloc(MAX_NAME_LEN, GFP_KERNEL);
    if (!name) {
        ret = -ENOMEM;
        vascend_err(vdavinci_to_dev(vdavinci), "add debugfs failed, "
            "malloc name fialed, vid: %u, ret: %d\n", vdavinci->id, ret);
        return;
    }

    if (vdavinci->dvt->dev_num > 1) {
        ret = snprintf_s(name, MAX_NAME_LEN, MAX_NAME_LEN - 1, "vascend_p%u_%u",
                         vdavinci->dev.dev_index, vdavinci->id);
    } else {
        ret = snprintf_s(name, MAX_NAME_LEN, MAX_NAME_LEN - 1, "vascend%u",
                         vdavinci->id);
    }
    if (ret < 0) {
        vascend_err(vdavinci_to_dev(vdavinci), "add debugfs failed, "
            "vid: %u, ret: %d\n", vdavinci->id, ret);
        kfree(name);
        return;
    }

    vdavinci->debugfs.debugfs = debugfs_create_dir(name, vdavinci->dvt->debugfs_root);

    debugfs_create_u64("notify_count", 0400, vdavinci->debugfs.debugfs,
        &vdavinci->debugfs.notify_count);
    debugfs_create_file("msix_count", 0400, vdavinci->debugfs.debugfs, vdavinci,
        &vdavinci_msix_count_fops);
    kfree(name);
}

/**
 * hw_dvt_debugfs_remove_vdavinci - remove debugfs entries of a vdavinci
 */
void hw_dvt_debugfs_remove_vdavinci(struct hw_vdavinci *vdavinci)
{
    debugfs_remove_recursive(vdavinci->debugfs.debugfs);
    vdavinci->debugfs.debugfs = NULL;
}

STATIC void hw_dvt_debugfs_release(struct kref *ref)
{
    debugfs_remove_recursive(vascend_debugfs_root);
    vascend_debugfs_root = NULL;
}

/**
 * hw_dvt_debugfs_init - register dvt debugfs root entry
 */
void hw_dvt_debugfs_init(struct hw_dvt *dvt)
{
    int ret;
    char *name = NULL;
    struct vdavinci_priv *vdavinci_priv = dvt->vdavinci_priv;
    struct pci_dev *pdev = container_of(vdavinci_priv->dev, struct pci_dev, dev);

    mutex_lock(&debugfs_vascend_lock);
    if (vascend_debugfs_root == NULL) {
        kref_init(&debugfs_ref);
        vascend_debugfs_root = debugfs_create_dir("vascend", NULL);
    } else {
        kref_get(&debugfs_ref);
    }
    mutex_unlock(&debugfs_vascend_lock);

    name = kzalloc(MAX_NAME_LEN, GFP_KERNEL);
    if (!name) {
        ret = -ENOMEM;
        vascend_err(vdavinci_priv->dev, "debugfs init failed, "
                    "malloc name fialed, ret: %d\n", ret);
        goto debugfs_root;
    }

    ret = snprintf_s(name, MAX_NAME_LEN, MAX_NAME_LEN - 1, "vascend_%02x_%02x_%u",
                     pdev->bus->number, PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));
    if (ret < 0) {
        vascend_err(vdavinci_priv->dev, "debugfs init failed, "
                    "snprientf_s fialed, ret: %d\n", ret);
        goto free_name;
    }

    dvt->debugfs_root = debugfs_create_dir(name, vascend_debugfs_root);
    kfree(name);
    return;

free_name:
    kfree(name);
debugfs_root:
    mutex_lock(&debugfs_vascend_lock);
    (void)kref_put(&debugfs_ref, hw_dvt_debugfs_release);
    mutex_unlock(&debugfs_vascend_lock);
}

/**
 * hw_dvt_debugfs_clean - remove debugfs entries
 */
void hw_dvt_debugfs_clean(struct hw_dvt *dvt)
{
    debugfs_remove_recursive(dvt->debugfs_root);
    dvt->debugfs_root = NULL;

    mutex_lock(&debugfs_vascend_lock);
    (void)kref_put(&debugfs_ref, hw_dvt_debugfs_release);
    mutex_unlock(&debugfs_vascend_lock);
}
