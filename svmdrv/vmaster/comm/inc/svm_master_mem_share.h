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
 * Create: 2023-10-11
 */
#ifndef SVM_MASTER_MEM_SHARE_H
#define SVM_MASTER_MEM_SHARE_H

#include "svm_cmd.h"
#include "devmm_proc_info.h"
#include "svm_dev_res_mng.h"

struct devmm_shid_map_node_info {
    int id;
    u32 devid;

    int share_id;
    u32 share_devid;
    u32 share_sdid;
};

struct devmm_share_id_map_node {
    ka_rb_node_t proc_node;
    ka_kref_t ref;

    struct devmm_shid_map_node_info shid_map_node_info;
    u32 blk_type;
    int hostpid;
};

int devmm_ioctl_mem_export(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
int devmm_ioctl_mem_import(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
int devmm_ioctl_mem_set_pid(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
int devmm_ioctl_mem_set_attr(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
int devmm_ioctl_mem_get_attr(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
int devmm_ioctl_mem_get_info(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);

void devmm_share_id_map_node_put(struct devmm_share_id_map_node *map_node);
struct devmm_share_id_map_node *devmm_share_id_map_node_get(struct devmm_svm_process *svm_proc, u32 devid, int id);
void devmm_share_id_map_node_destroy(struct devmm_svm_process *svm_proc,
    u32 devid, struct devmm_share_id_map_node *map_node);
void devmm_share_id_map_node_destroy_by_devid(struct devmm_svm_process *svm_proc, u32 devid, bool need_pid_erase);
void devmm_share_id_map_node_destroy_all(struct devmm_svm_process *svm_proc);

void devmm_share_agent_blk_destroy_all(struct devmm_share_phy_addr_agent_blk_mng *blk_mng);
int devmm_share_agent_blk_put_with_share_id(u32 share_devid, int share_id, int hostpid,
    u32 devid, bool need_pid_erase, u32 dst_sdid);
int devmm_put_remote_share_mem_info(u32 devid, u32 share_id, u32 share_sdid, u32 share_devid);

int devmm_chan_target_blk_query_pa_process(struct devmm_svm_process *svm_proc,
    struct devmm_svm_heap *heap, void *msg, u32 *ack_len);

#endif
