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
#ifndef SVM_SHMEM_NODE_POD_H
#define SVM_SHMEM_NODE_POD_H

#include "devmm_channel.h"

bool svm_is_sdid_in_local_server(u32 devid, u32 sdid);
int devmm_s2s_msg_sync_send(u32 devid, u32 sdid, void *msg, size_t size);

int devmm_get_remote_share_mem_info_process(u32 devid, struct devmm_ipc_pod_msg_data *msg);
int devmm_put_remote_share_mem_info_process(u32 devid, struct devmm_ipc_pod_msg_data *msg);
int devmm_get_cs_host_chan_target_blk_info_process(u32 devid, struct devmm_ipc_pod_msg_data *msg);

#endif
