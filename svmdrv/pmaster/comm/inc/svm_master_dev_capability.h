/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2020. All rights reserved.
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

#ifndef SVM_MASTER_DEV_CAPABILITY_H
#define SVM_MASTER_DEV_CAPABILITY_H

#include <linux/types.h>
#include "devmm_proc_info.h"
#include "devmm_channel.h"

enum {
    PCIE_TH_FEATURE = 0,
    PCIE_BAR_FEATURE,
    PCIE_BAR_HUGE_FEATURE,
    READ_ONLY_FEATURE,
    HOST_RW_DEV_RO_FEATURE,
    DVPP_MEM_SIZE,
    OFFSET_SECURITY,                /* Addr translate securely */
    CONVERT_OFFSET,                 /* Convert dma_addr to device ts share mem */
    DEV_MEM_MAP_HOST_FEATURE,
    PCIE_DMA_SVA_FEATURE,
    DOUBLE_PGTABLE_OFFSET,
    HOST_PIN_PRE_REGISTER_FEATURE,  /* User_space synchronous copy performance improvement.*/
    HOST_MEM_POOL_FEATURE,
    DMA_PREPARE_POOL_FEATURE,       /* Kernel_space asynchronous copy performance improvement.*/
    AIC_REG_MAP,
    REMOTE_MMAP,
    SHMEM_REPAIR,
    SHMEM_MAP_EXBUS,
    MEM_HOST_UVA_FEATURE,
    MEM_MAP_CAPABILITY_FEATURE,
    DEVMM_MAX_FEATURE_ID
};

#define DEVMM_MAX_FEATURE_NAME_LEN 50
struct devmm_dev_feature_handlers_st {
    char feature_name[DEVMM_MAX_FEATURE_NAME_LEN];
    bool (*feature_capability_get_handlers)(u32 devid);
    u64 (*feature_capability_value_get_handlers)(u32 devid);
    bool is_support_disable;
};

extern struct devmm_dev_feature_handlers_st g_devmm_dev_feature[DEVMM_MAX_FEATURE_ID];
extern bool g_dev_feature_capabilty_disable[SVM_MAX_AGENT_NUM][DEVMM_MAX_FEATURE_ID];

void devmm_dev_feature_capability_disable(u32 devid, u32 feature_id);
void devmm_dev_feature_capability_enable(u32 devid, u32 feature_id);

void devmm_set_dev_mem_size_info(u32 did, struct devmm_chan_exchange_pginfo *info);
void devmm_clear_dev_mem_size_info(u32 devid);
int devmm_set_dev_capability(const u32 did, const u32 vfid, struct devmm_chan_exchange_pginfo *info);
void devmm_clear_dev_capability(const u32 did);

bool devmm_dev_capability_support_host_pin_pre_register(u32 devid);
bool devmm_dev_capability_support_host_mem_pool(u32 devid);
bool devmm_dev_capability_support_dma_prepare_pool(u32 devid);
bool devmm_dev_capability_support_host_rw_dev_ro(u32 devid);
bool devmm_dev_capability_support_shmem_map_exbus(u32 devid);

int devmm_ioctl_mem_map_capability(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);

static inline bool devmm_dev_capability_support_dev_mem_map_host(u32 did)
{
    return devmm_svm->dev_capability[did].feature_dev_mem_map_host &&
        (g_dev_feature_capabilty_disable[did][DEV_MEM_MAP_HOST_FEATURE] == false);
}

static inline bool devmm_dev_capability_support_pcie_th(u32 did)
{
    return devmm_svm->dev_capability[did].feature_pcie_th &&
        (g_dev_feature_capabilty_disable[did][PCIE_TH_FEATURE] == false);
}

static inline bool devmm_dev_capability_support_bar_mem(u32 did)
{
    return devmm_svm->dev_capability[did].feature_bar_mem &&
        (g_dev_feature_capabilty_disable[did][PCIE_BAR_FEATURE] == false);
}

static inline bool devmm_dev_capability_support_bar_huge_mem(u32 did)
{
    return devmm_svm->dev_capability[did].feature_bar_huge_mem &&
        (g_dev_feature_capabilty_disable[did][PCIE_BAR_HUGE_FEATURE] == false);
}

static inline bool devmm_dev_capability_support_giant_page(u32 did)
{
    return devmm_svm->dev_capability[did].feature_giant_page;
}

static inline bool devmm_dev_capability_support_remote_mmap(u32 did)
{
    return devmm_svm->dev_capability[did].feature_remote_mmap;
}

static inline bool devmm_dev_capability_support_read_only(u32 did)
{
    return devmm_svm->dev_capability[did].feature_dev_read_only &&
        (g_dev_feature_capabilty_disable[did][READ_ONLY_FEATURE] == false);
}

static inline u64 devmm_dev_capability_dvpp_mem_size(u32 did)
{
    return g_dev_feature_capabilty_disable[did][DVPP_MEM_SIZE] ? 0 : devmm_svm->dev_capability[did].dvpp_memsize;
}

static inline bool devmm_dev_capability_support_offset_security(u32 did)
{
    return ((devmm_svm->dev_capability[did].feature_phycial_address & DEVMM_SUPPORT_OFFSET_SECURITY_MASK) ==
        DEVMM_SUPPORT_OFFSET_SECURITY_MASK) && (g_dev_feature_capabilty_disable[did][OFFSET_SECURITY] == false);
}

static inline bool devmm_dev_capability_convert_support_offset(u32 did)
{
    return ((devmm_svm->dev_capability[did].feature_phycial_address & DEVMM_CONVERT_SUPPORT_OFFSET_MASK) ==
        DEVMM_CONVERT_SUPPORT_OFFSET_MASK) && (g_dev_feature_capabilty_disable[did][CONVERT_OFFSET] == false);
}

static inline u64 devmm_dev_capability_double_pgtable_offset(u32 did)
{
    return g_dev_feature_capabilty_disable[did][DOUBLE_PGTABLE_OFFSET] ? 0 : devmm_svm->dev_capability[did].double_pgtable_offset;
}

static inline bool devmm_dev_capability_support_pcie_dma_sva(u32 dev_id)
{
    return devmm_svm->dev_capability[dev_id].feature_pcie_dma_support_sva &&
        (g_dev_feature_capabilty_disable[dev_id][PCIE_DMA_SVA_FEATURE] == false);
}

static inline bool devmm_dev_capability_support_aic_reg_map(u32 dev_id)
{
    return devmm_svm->dev_capability[dev_id].feature_aic_reg_map &&
        (g_dev_feature_capabilty_disable[dev_id][AIC_REG_MAP] == false);
}

static inline bool devmm_dev_capability_support_shmem_repair(u32 dev_id)
{
    return devmm_svm->dev_capability[dev_id].feature_shmem_repair &&
        (g_dev_feature_capabilty_disable[dev_id][SHMEM_REPAIR] == false);
}

static inline bool devmm_dev_capability_support_mem_host_uva(u32 dev_id)
{
    return devmm_svm->dev_capability[dev_id].feature_mem_host_uva &&
        (g_dev_feature_capabilty_disable[dev_id][MEM_HOST_UVA_FEATURE] == false);
}

static inline bool devmm_dev_capability_support_mem_map_capability(u32 dev_id)
{
    return devmm_svm->dev_capability[dev_id].feature_host_mem_map_cap &&
        (g_dev_feature_capabilty_disable[dev_id][MEM_MAP_CAPABILITY_FEATURE] == false);
}

#endif /* SVM_MASTER_DEV_CAPABILITY_H__ */
