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
#include <linux/types.h>
#include <linux/proc_fs.h>

#include "devmm_common.h"
#include "svm_proc_mng.h"
#include "svm_mem_stats.h"
#include "svm_proc_fs.h"
#include "svm_master_feature_proc_fs.h"

#ifdef CONFIG_PROC_FS
STATIC int devmm_dev_sum_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, devmm_dev_mem_stats_procfs_show, ka_base_pde_data(inode));
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const ka_procfs_ops_t devmm_dev_sum_ops = {
    .proc_open    = devmm_dev_sum_open,
    .proc_read    = ka_fs_seq_read,
    .proc_lseek   = ka_fs_seq_lseek,
    .proc_release = ka_fs_single_release,
};
#else
static const ka_file_operations_t devmm_dev_sum_ops = {
    .owner = KA_THIS_MODULE,
    .open    = devmm_dev_sum_open,
    .read    = ka_fs_seq_read,
    .llseek  = ka_fs_seq_lseek,
    .release = ka_fs_single_release,
};
#endif

struct devmm_dev_procfs_entry {
    ka_proc_dir_entry_t *dev_entry;
    ka_semaphore_t sem;
}; 

static struct devmm_dev_procfs_entry devmm_dev_entry[SVM_MAX_AGENT_NUM];

void devmm_dev_proc_fs_init(void)
{
    u32 logic_id;

    for (logic_id = 0; logic_id < SVM_MAX_AGENT_NUM; logic_id++) {
        ka_task_sema_init(&devmm_dev_entry[logic_id].sem, 1);
        devmm_dev_entry[logic_id].dev_entry = NULL;
    }
}

void devmm_dev_proc_fs_create(u32 logic_id)
{
    ka_proc_dir_entry_t *top_entry = NULL;
    ka_proc_dir_entry_t *dev_entry = NULL;
    char name[DEVMM_PROC_FS_NAME_LEN] = {0};
    int ret;

    top_entry = devmm_get_top_entry();
    if (top_entry == NULL) {
        return;
    }

    if (devmm_dev_entry[logic_id].dev_entry != NULL) {
        return;
    }

    ret = sprintf_s(name, (unsigned long)DEVMM_PROC_FS_NAME_LEN, "dev%u", logic_id);
    if (ret <= 0) {
        devmm_drv_debug("Sprintf_s fail. (ret=%d)\n", ret);
        return;
    }

    ka_task_down(&devmm_dev_entry[logic_id].sem);
    if (devmm_dev_entry[logic_id].dev_entry == NULL) {
        dev_entry = ka_fs_proc_mkdir((const char *)name, top_entry);
        if (dev_entry != NULL) {
            ka_fs_proc_create_data("summary", DEVMM_PROC_FS_MODE, dev_entry, &devmm_dev_sum_ops, (void *)(uintptr_t)logic_id);
            devmm_dev_entry[logic_id].dev_entry = dev_entry;
            devmm_dev_feature_proc_fs_create(dev_entry, logic_id);
        }
    }
    ka_task_up(&devmm_dev_entry[logic_id].sem);
}

void devmm_dev_proc_fs_uninit(void)
{
    u32 logic_id;

    for (logic_id = 0; logic_id < SVM_MAX_AGENT_NUM; logic_id++) {
        ka_task_down(&devmm_dev_entry[logic_id].sem);
        if (devmm_dev_entry[logic_id].dev_entry != NULL) {
            devmm_dev_feature_proc_fs_destroy(logic_id);
            ka_fs_proc_remove(devmm_dev_entry[logic_id].dev_entry);
            devmm_dev_entry[logic_id].dev_entry = NULL;
        }
        ka_task_up(&devmm_dev_entry[logic_id].sem);
    }
}

static void devmm_info_show_ts_share_memory(ka_seq_file_t *seq, struct devmm_svm_dev *svm_dev)
{
    u32 i;

    ka_fs_seq_printf(seq, "Svm statistics ts share memory start\r\n");
    for (i = 0; i < DEVMM_MAX_DEVICE_NUM; i++) {
        bool vf_printf;
        u32 j;

        if (svm_dev->pa_info[i].total_block_num == 0) {
            continue;
        }
        ka_fs_seq_printf(seq, "Ts share mem info.(devid=%d, total_block_num=%u, "
            "total_data_num=%u, free_index=%u, recycle_index=%u)\r\n",
            i, svm_dev->pa_info[i].total_block_num,
            svm_dev->pa_info[i].total_data_num,
            svm_dev->pa_info[i].free_index,
            svm_dev->pa_info[i].recycle_index);

        for (j = 0, vf_printf = true; j < DEVMM_MAX_VF_NUM; j++) {
            if (svm_dev->pa_info[i].core_num[j] == 0) {
                continue;
            }
            if (vf_printf == true) {
                vf_printf = false;
                ka_fs_seq_printf(seq, "Vf info:\r\n"
                    "vf_id       core_num   total_core_num       data_total        "
                    "data_free    convert_total     convert_free\r\n");
            }
            ka_fs_seq_printf(seq, "%03u %16u %16u %16u %16u %16llu %16llu\r\n",
                j, svm_dev->pa_info[i].core_num[j],
                svm_dev->pa_info[i].total_core_num[j],
                svm_dev->pa_info[i].vdev_total_data_num[j],
                svm_dev->pa_info[i].vdev_free_data_num[j],
                svm_dev->pa_info[i].vdev_total_convert_len[j],
                svm_dev->pa_info[i].vdev_free_convert_len[j]);
        }
    }
    ka_fs_seq_printf(seq, "Svm statistics ts share memory end\r\n");
}

int devmm_info_show(ka_seq_file_t *seq, void *offset)
{
    struct devmm_svm_dev *svm_dev = (struct devmm_svm_dev *)seq->private;

    devmm_info_show_ts_share_memory(seq, svm_dev);

    return 0;
}

#endif
