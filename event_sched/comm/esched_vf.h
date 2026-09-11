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

#ifndef ESCHED_VF_H
#define ESCHED_VF_H

#include "ka_base_pub.h"
#include "ka_task_pub.h"

#include "devdrv_manager_comm.h"
#include "virtmng_interface.h"
#include "hvtsdrv_tsagent.h"

#define SCHED_MAX_PROC_PER_VF         1024
#define SCHED_DEFAULT_VF_ID           0   /* On physical machines, processes are all in vf_0 by default */
#define SCHED_DEFAULT_HOST_PID        0

#define SCHED_VF_STATUS_UNCREATED 0
#define SCHED_VF_STATUS_NORMAL    1
#define SCHED_VF_STATUS_DELETING  2

struct esched_vf_info {
    u32 dev_id;
    u32 chip_id;
    u32 vfgid;
    u32 vfid;
    u32 dtype;
    u32 config_sched_cpu_mask;
};

struct sched_vf_stat {
    ka_atomic64_t cur_event_num;
    ka_atomic64_t publish_event_num;
    ka_atomic64_t sched_event_num;
};

struct sched_vf_ctx {
    u32 vfgid; /* hard_sched:equal to pool_id */
    u32 vfid;
    u64 create_timestamp;
    u64 config_sched_cpu_mask;
    u64 sched_cpu_mask;
    u32 que_depth;
    u32 max_use_cpu_num;
    u32 cur_use_cpu_num;
    ka_spinlock_t vf_lock;
    ka_atomic_t status;
    ka_atomic_t proc_num;
    struct sched_vf_stat stat;
};

struct sched_slice_ctx {
    u32 enable_flag;
    u32 num;
    struct sched_vf_ctx vf_ctx[VMNG_VDEV_MAX_PER_PDEV];  /* the first vf_ctx for physical machine scheme */
};

void sched_node_vf_init(u32 devid);
void sched_vf_init(void);
void sched_vf_uninit(void);

#endif
