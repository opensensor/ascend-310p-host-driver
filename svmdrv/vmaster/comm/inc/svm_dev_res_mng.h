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
#ifndef SVM_DEV_RES_MNG_H
#define SVM_DEV_RES_MNG_H

#include "devmm_common.h"

#ifndef DEVMM_UT
#define DEVMM_MSG_INIT_SEND_WAITTIME 1000
#else
#define DEVMM_MSG_INIT_SEND_WAITTIME 0
#endif

#define DEVMM_HASH_LIST_NUM_SHIFT 10
#define DEVMM_HASH_LIST_NUM (1 << DEVMM_HASH_LIST_NUM_SHIFT)  /* 1024 */

struct devmm_dev_msg_client {
    ka_device_t *dev;
    void *msg_chan;
};

struct devmm_ipc_mem_node_info {
    KA_DECLARE_HASHTABLE(node_htable, DEVMM_HASH_LIST_NUM_SHIFT);
    ka_rwlock_t rwlock;
    ka_mutex_t mutex;
};

struct devmm_task_dev_res_info {
    ka_list_head_t head;
    ka_rw_semaphore_t rw_sem;
};

struct devmm_share_phy_addr_agent_blk_mng {
    ka_rw_semaphore_t rw_sem;
    ka_rb_root_t rbtree;
};

struct devmm_dev_res_mng {
    struct svm_id_inst id_inst;

    ka_kref_t ref;

    struct devmm_ipc_mem_node_info ipc_mem_node_info;
    struct devmm_dev_msg_client dev_msg_client;

    struct devmm_task_dev_res_info task_dev_res_info;
    struct devmm_share_phy_addr_agent_blk_mng share_agent_blk_mng;
    bool is_mdev_vm;
    void *dev;
};

int devmm_dev_res_mng_create(struct svm_id_inst *id_inst, ka_device_t *dev);
void devmm_dev_res_mng_destroy(struct svm_id_inst *id_inst);
struct devmm_dev_res_mng *devmm_dev_res_mng_get(struct svm_id_inst *id_inst);
void devmm_dev_res_mng_put(struct devmm_dev_res_mng *res_mng);
void devmm_dev_res_mng_destroy_all(void);

void devmm_init_task_dev_res_info(struct devmm_task_dev_res_info *info);
bool devmm_is_mdev_vm(u32 devid, u32 vfid);
#endif /* SVM_DEV_RES_MNG_H */
