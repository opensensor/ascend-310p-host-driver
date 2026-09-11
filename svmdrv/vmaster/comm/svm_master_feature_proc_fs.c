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

#include <linux/types.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>

#include "devmm_proc_info.h"
#include "svm_master_dev_capability.h"
#include "svm_master_feature_proc_fs.h"
#include "ka_fs_pub.h"

#define DEVMM_DEV_FEATURE_PROC_FS_NAME_LEN  32U
#define DEVMM_DEV_FEATURE_PROC_FS_MODE      0644
#define DEVMM_SEQ_PRIVATE_DATA_OFFSET       32U

static ka_proc_dir_entry_t *g_devmm_dev_feature_entry[SVM_MAX_AGENT_NUM] = {NULL};

#ifndef EMU_ST
static int devmm_dev_feature_capability_show(ka_seq_file_t *seq, void *offset)
{
    u32 devid = (u32)((u64)(uintptr_t)seq->private >> DEVMM_SEQ_PRIVATE_DATA_OFFSET);
    u32 feature_id = (u32)(uintptr_t)seq->private;

    if (g_devmm_dev_feature[feature_id].feature_capability_get_handlers != NULL) {
        ka_fs_seq_printf(seq, "%d\n", g_devmm_dev_feature[feature_id].feature_capability_get_handlers(devid));
    } else {
        ka_fs_seq_printf(seq, "%llu\n", g_devmm_dev_feature[feature_id].feature_capability_value_get_handlers(devid));
    }
    return 0;
}

static int devmm_dev_feature_ops_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, devmm_dev_feature_capability_show, ka_base_pde_data(inode));
}

static ssize_t devmm_dev_feature_ops_write(ka_file_t *filp, const char __ka_user *ubuf, size_t count, loff_t *ppos)
{
    u32 devid = (u32)((u64)(uintptr_t)(((ka_seq_file_t *)ka_fs_get_file_private_data(filp))->private) >> DEVMM_SEQ_PRIVATE_DATA_OFFSET);
    u32 feature_id = (u32)(uintptr_t)(((ka_seq_file_t *)ka_fs_get_file_private_data(filp))->private);
    char ch[2] = {0}; /* 2 bytes long */
    bool user_input;
    int ret;

    if ((ppos == NULL) || (*ppos != 0) || (count != sizeof(ch)) || (ubuf == NULL)) {
        return -EINVAL;
    }

    if (ka_base_copy_from_user(ch, ubuf, count)) {
        return -ENOMEM;
    }

    ch[count - 1] = '\0';
    ret = ka_base_kstrtobool(ch, &user_input);
    if (ret != 0) {
        return ret;
    }

    if (user_input) {
        devmm_dev_feature_capability_enable(devid, feature_id);
    } else {
        devmm_dev_feature_capability_disable(devid, feature_id);
    }
    return (ssize_t)count;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const ka_procfs_ops_t devmm_dev_feature_ops = {
    .proc_open    = devmm_dev_feature_ops_open,
    .proc_read    = ka_fs_seq_read,
    .proc_write   = devmm_dev_feature_ops_write,
    .proc_lseek   = ka_fs_seq_lseek,
    .proc_release = ka_fs_single_release,
};
#else
static const ka_file_operations_t devmm_dev_feature_ops = {
    .owner = KA_THIS_MODULE,
    .open    = devmm_dev_feature_ops_open,
    .read    = ka_fs_seq_read,
    .write   = devmm_dev_feature_ops_write,
    .llseek  = ka_fs_seq_lseek,
    .release = ka_fs_single_release,
};
#endif
#endif

void devmm_dev_feature_proc_fs_create(ka_proc_dir_entry_t  *dev_entry, u32 logic_id)
{
#ifndef EMU_ST
    char name[DEVMM_DEV_FEATURE_PROC_FS_NAME_LEN] = {0};
    ka_proc_dir_entry_t *feature_entry = NULL;
    u32 devid, vfid, i;
    int ret;

    ret = sprintf_s(name, (unsigned long)DEVMM_DEV_FEATURE_PROC_FS_NAME_LEN, "feature");
    if (ret <= 0) {
        devmm_drv_debug("Sprintf_s fail. (ret=%d)\n", ret);
        return;
    }

    ret = devmm_container_vir_to_phs_devid(logic_id, &devid, &vfid);
    if (ret != 0) {
        return;
    }

    if (g_devmm_dev_feature_entry[devid] == NULL) {
        feature_entry = ka_fs_proc_mkdir((const char *)name, dev_entry);
        if (feature_entry != NULL) {
            for (i = 0; i < DEVMM_MAX_FEATURE_ID; ++i) {
                u64 data = ((u64)devid << DEVMM_SEQ_PRIVATE_DATA_OFFSET) | (u64)i;
                ka_fs_proc_create_data(g_devmm_dev_feature[i].feature_name, DEVMM_DEV_FEATURE_PROC_FS_MODE, feature_entry,
                    &devmm_dev_feature_ops, (void *)(uintptr_t)data);
            }
            g_devmm_dev_feature_entry[devid] = feature_entry;
        }
    }
#endif
}

void devmm_dev_feature_proc_fs_destroy(u32 logic_id)
{
#ifndef EMU_ST
    u32 devid, vfid;
    int ret;

    ret = devmm_container_vir_to_phs_devid(logic_id, &devid, &vfid);
    if (ret != 0) {
        return;
    }

    if (g_devmm_dev_feature_entry[devid] != NULL) {
        ka_fs_proc_remove(g_devmm_dev_feature_entry[devid]);
        g_devmm_dev_feature_entry[devid] = NULL;
    }
#endif
}
