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
* Create: 2022-11-15
*/
#ifndef __DMS_MEM_INFO_H__
#define __DMS_MEM_INFO_H__

#include <linux/types.h>
#include <linux/errno.h>

#include "dms_define.h"
#include "drv_type.h"

#define DMS_MEM_NAME "DMS_MEM_NAME"

#define MEM_SERVICE 0
#define MEM_SYSTEM  1
#define MEM_MEDIUM  2

typedef int (*devmng_get_vdev_hbm_info)(u32, u32, u32, u64 *, u64 *);
typedef int (*devmng_get_vdev_ddr_info)(u32, u32, u32, u64 *, u64 *);

struct mem_info {
    u64 total_size;
    u64 use_size;
    u64 free_size;
    u64 hpage_2M_num;
    u64 hpage_1G_num;
    u32 mem_util;
};

struct mem_hpage_1G {
    u64 huge_2M_total;
    u64 huge_2M_free;
    u64 huge_1G_total;
    u64 huge_1G_free;
};

struct base_mem_info {
    u64 total_size;
    u64 free_size;
};

struct udis_mem_info {
    struct base_mem_info sys_mem_info;
    struct base_mem_info service_mem_info;
    struct base_mem_info medium_mem_info;
};

struct cgroup_mem_info {
    unsigned long long limit_in_bytes;       /**< maximum number of used memory */
    unsigned long long max_usage_in_bytes;   /**< maximum memory used in history */
    unsigned long long usage_in_bytes;       /**< current memory usage */
};

int mem_get_dev_ddr_info(u32 dev_id, struct mem_info *info);
int mem_get_cgroup_info(u32 dev_id, u32 vfid, struct cgroup_mem_info *cg_info);
int mem_get_tsnode_cdm(u32 dev_id, u32 vfid, u64 *total_size, u64 *free_size);
int mem_get_cgroup_and_cdm_memory(u32 dev_id, u32 vfid, u64 *total_size, u64 *free_size);

int mem_get_dev_hbm_info(u32 dev_id, u32 type, struct mem_info *info);
int mem_get_vdev_hbm_info(u32 dev_id, u32 vfid, struct mem_info *info);
int mem_get_hbm_node_info(u32 dev_id, u32 type, struct mem_info *info);
int mem_get_hbm_medium_info(u32 dev_id, struct mem_info *hbm_info);
int mem_get_hbm_info(u32 dev_id, u32 vfid, u32 type, struct mem_info *info);
int mem_get_dev_sliceable_memory(u32 dev_id, struct mem_info *info);

int dms_mem_init(void);
void dms_mem_uninit(void);
int dms_drv_get_ai_ddr_info(void *feature, char *in, u32 in_len, char *out, u32 out_len);
int dms_get_ddr_mem_info(void *feature, char *in, u32 in_len, char *out, u32 out_len);
int dms_get_hbm_mem_info(void *feature, char *in, u32 in_len, char *out, u32 out_len);
int dms_get_cgroup_mem_info(void *feature, char *in, u32 in_len, char *out, u32 out_len);
int dms_get_service_mem_info(void *feature, char *in, u32 in_len, char *out, u32 out_len);
int dms_get_system_mem_info(void *feature, char *in, u32 in_len, char *out, u32 out_len);

#ifdef CFG_FEATURE_VFIO_SOC
int mem_get_container_mem_info(u32 dev_id, struct mem_info *info);
#endif

#endif
