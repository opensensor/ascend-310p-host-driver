/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
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
 * Create: 20122-08-13
 */

#ifndef DP_PROC_MNG_CMD_H
#define DP_PROC_MNG_CMD_H

#include "ascend_hal_define.h"
#include "drv_type.h"

#ifdef EMU_ST
#define STATIC
#define INLINE
#else
#define STATIC static
#define INLINE inline
#endif

#define DAVINCI_DP_PROC_MNG_SUB_MODULE_NAME "DP_PROC_MNG"
#define DAVINCI_DP_PROC_MNG_AGENT_SUB_MODULE_NAME "DP_PROC_MNG_AGENT"

#ifdef CFG_SOC_PLATFORM_CLOUD_V2
#define DP_PROC_MNG_MAX_DEVICE_NUM 1124
#define DP_PROC_MNG_MAX_AGENT_DEVICE_NUM 64
#else
#define DP_PROC_MNG_MAX_DEVICE_NUM 64
#define DP_PROC_MNG_MAX_AGENT_DEVICE_NUM 4
#endif

#define DP_PROC_MNG_DEVICE_SIDE_AGENT_NUM DP_PROC_MNG_MAX_DEVICE_NUM
#define DP_PROC_MNG_HOST_SIDE_AGENT_NUM 1
#define DP_PROC_MNG_MAX_AGENT_NUM (DP_PROC_MNG_DEVICE_SIDE_AGENT_NUM + DP_PROC_MNG_HOST_SIDE_AGENT_NUM)
#define DP_PROC_MNG_MAX_VF_NUM 32

#define DP_PROC_MNG_INVALID_DEVICE_PHYID 0xff

#define DP_PROC_MNG_HOST_AGENT_ID DP_PROC_MNG_DEVICE_SIDE_AGENT_NUM


/*
 * user mode: devid means logic_id
 * kernel mode: devid means phyid
 */
struct dp_proc_mng_devid {
    uint32_t logical_devid;
    uint32_t devid;
    uint32_t vfid;
};

struct dp_proc_mng_bind_cgroup_para {
    BIND_CGROUP_TYPE bind_type;
};

struct dp_proc_mng_get_mem_stats {
    u64 mbuff_used_size;
    u64 aicpu_used_size;
    u64 custom_used_size;
    u64 hccp_used_size;
};

struct dp_proc_mng_mem_sharing {
    void *ptr;
    u64 size;
    enum drv_mem_side side;
    accessMember_t accessor;
    uint32_t pg_prot;
    uint32_t enable_flag; /* 0:enable; 1:disable */
};

struct dp_proc_mng_ioctl_arg {
    struct dp_proc_mng_devid head; // 用户态无需感知
    union {
        struct dp_proc_mng_bind_cgroup_para bind_cgroup_para;
        struct dp_proc_mng_get_mem_stats get_mem_stats_para;
        struct dp_proc_mng_mem_sharing mem_sharing_para;
    } data;
};

struct dp_proc_mng_process_id {
    int32_t hostpid;
    uint32_t devid;
    uint32_t vm_id; // 虚拟机
    uint32_t vfid; // 算力分组容器
};

#define DP_PROC_MNG_BIND_CGROUP            _IOW(DP_PROC_MNG_MAGIC, 1, struct dp_proc_mng_ioctl_arg)
#define DP_PROC_MNG_GET_MEM_STATS          _IOWR(DP_PROC_MNG_MAGIC, 2, struct dp_proc_mng_ioctl_arg)
#define DP_PROC_MNG_MEM_SHARING               _IOWR(DP_PROC_MNG_MAGIC, 3, struct dp_proc_mng_ioctl_arg)
#define DP_PROC_MNG_CMD_MAX_CMD            4

#define DP_PROC_MNG_MAGIC                  'G'

#endif
