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

#ifndef __DAVINCI_INTF_INIT_H__
#define __DAVINCI_INTF_INIT_H__

#include "ka_task_pub.h"
#include "ka_list_pub.h"
#include "ka_base_pub.h"
#include "ka_fs_pub.h"

#define DAVINIC_NONE_ROOT_ACCESS    (0640)

#ifndef ASSERT_RET
#define ASSERT_RET(a, ret) {                      \
        if (!(a)) {        \
            return (ret);  \
        }                  \
    }
#endif

#define DAVINIC_NOT_INIT_BY_OPENCMD    (-1)
#define DAVINIC_FREE_LIST_NOT_INIT    (-1)

#define DAVINIC_INTF_INVAILD_DEVICE_ID    (0xffffffffU)

#define DAVINIC_NOT_CONFIRM_USER_ID    (0xffffffffU)
#define DAVINIC_ROOT_USER_ID           (0x0)
/* max time 60s */
#define DAVINIC_CONFIRM_MAX_TIME    (6000)

#define DAVINIC_CONFIRM_EACH_TIME    (10)

#define DAVINIC_CONFIRM_WARN_MASK    (1000)

#define DAVINIC_FREE_WAIT_MAX_TIME    (3600000)
#define DAVINIC_FREE_WAIT_EACH_TIME    (1)

#define DAVINIC_CONFIRM_USER_NUM    (3)

#ifdef CFG_FEATURE_SRIOV
#define MAX_DEVICE_COUNT 1124
#else
#define MAX_DEVICE_COUNT 64
#endif

#define DAVINIC_UNINIT_FILE    "uninit"

#define DAVINIC_FREE_IN_PARALLEL            (1)
#define DAVINIC_FREE_IN_ORDER               (2)
#define DAVINCI_INTF_FULL_DEV_NAME          "/dev/davinci_manager"
#define DAVINCI_INTF_NPU_DEV_CUST_NAME      "npu_device_cust"

#define MODULE_NAME_MAX_LEN 384

struct davinci_intf_file_stru {
    pid_t owner_pid;
    char module_name[DAVINIC_MODULE_NAME_MAX];
    ka_inode_t *inode;
    ka_file_t *file_op;
    ka_list_head_t list;
    int valid;
    unsigned int seq;
    unsigned int open_time;
};

struct davinci_intf_process_stru;

struct davinci_intf_free_list_stru {
    pid_t owner_pid;
    struct davinci_intf_process_stru *owner_proc;
    ka_list_head_t list;
    ka_work_struct_t release_work;
    ka_atomic_t current_count;
    unsigned int current_free_index;
    int all_flag;
};

struct davinci_intf_process_stru {
    void *owner_cb;
    pid_t owner_pid;
    ka_list_head_t list;
    ka_list_head_t file_list;
    ka_mutex_t res_lock;
    ka_atomic_t work_count;
    unsigned int  status;
    u64 start_time;
    struct davinci_intf_free_list_stru *free_list;
};

struct davinci_intf_process_module_stru {
    char module_name[DAVINIC_MODULE_NAME_MAX];
    ka_list_head_t list;
};

struct davinci_intf_sub_module_stru {
    int valid;
    int module_type;
    char module_name[DAVINIC_MODULE_NAME_MAX];
    ka_file_operations_t ops;
    struct notifier_operations notifier;
    ka_list_head_t list;
    int free_type;
    unsigned int open_module_max; /* one process can open max count module, 0 means no limit */
};

struct davinci_intf_stru {
    ka_atomic_t count;
    ka_rw_semaphore_t cb_sem;
    ka_cdev_t cdev;
    ka_device_t *device;
    ka_list_head_t process_list;
    ka_list_head_t module_list;
    unsigned int device_status[MAX_DEVICE_COUNT];
    cpumask_var_t cpumask;
};

struct davinci_intf_free_file_stru {
    pid_t owner_pid;
    struct davinci_intf_free_list_stru *owner_list;
    char module_name[DAVINIC_MODULE_NAME_MAX];
    struct davinci_intf_private_stru *file_private;
    ka_list_head_t list;
    unsigned int free_type;
    unsigned int free_index;
};

struct davinci_intf_private_stru {
    char module_name[DAVINIC_MODULE_NAME_MAX];
    unsigned int device_id;
    pid_t owner_pid;
    int close_flag;
    ka_atomic_t work_count;
    int release_status;
    ka_mutex_t fmutex;
    ka_file_operations_t fops;
    struct notifier_operations notifier;
    struct davinci_intf_stru *device_cb;
    ka_file_t priv_filep;
    unsigned int free_type;
    struct davinci_intf_file_stru *file_stru_node;
    struct davinci_intf_free_list_stru *free_list;
    u64 start_time;
};

void release_file_free_list(struct davinci_intf_free_list_stru *file_free_list);
void drv_ascend_release_work(struct davinci_intf_free_list_stru *free_list);
void drv_intf_trans_free_list_nodes(struct davinci_intf_process_stru *proc,
    ka_file_t *file, unsigned int free_index);
void free_uninit_file_pos(struct davinci_intf_process_stru *proc, ka_file_t *file);
void drv_ascend_free_file_node(ka_file_t *file);
int drv_ascend_add_release_list_all(struct davinci_intf_process_stru *proc, ka_file_t *file);

#endif
