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

#include "devmm_common.h"
#include "svm_msg_client.h"
#include "devmm_channel.h"
#include "svm_proc_mng.h"
#include "svm_proc_fs.h"
#include "devmm_dev.h"
#include "svm_master_dev_capability.h"

struct devmm_dev_feature_handlers_st g_devmm_dev_feature[DEVMM_MAX_FEATURE_ID] = {
    [PCIE_TH_FEATURE] = {"pcie_th", devmm_dev_capability_support_pcie_th, NULL, false},
    [PCIE_BAR_FEATURE] = {"bar_mem", devmm_dev_capability_support_bar_mem, NULL, true},
    [PCIE_BAR_HUGE_FEATURE] = {"bar_mem_huge", devmm_dev_capability_support_bar_huge_mem, NULL, true},
    [READ_ONLY_FEATURE] = {"read_only", devmm_dev_capability_support_read_only, NULL, false},
    [HOST_RW_DEV_RO_FEATURE] = {"host_rw_dev_ro", devmm_dev_capability_support_host_rw_dev_ro, NULL, false},
    [DVPP_MEM_SIZE] = {"dvpp_mem_size", NULL, devmm_dev_capability_dvpp_mem_size, false},
    [OFFSET_SECURITY] = {"offset_security", devmm_dev_capability_support_offset_security, NULL, false},
    [CONVERT_OFFSET] = {"convert_offset", devmm_dev_capability_convert_support_offset, NULL, false},
    [DEV_MEM_MAP_HOST_FEATURE] = {"dev_mem_map_host", devmm_dev_capability_support_dev_mem_map_host, NULL, false},
    [PCIE_DMA_SVA_FEATURE] = {"pcie_dma_sva", devmm_dev_capability_support_pcie_dma_sva, NULL, false},
    [DOUBLE_PGTABLE_OFFSET] = {"double_pgtable_offset", NULL, devmm_dev_capability_double_pgtable_offset, false},
    [HOST_PIN_PRE_REGISTER_FEATURE] = {"host_pin_pre_register", devmm_dev_capability_support_host_pin_pre_register, NULL, false},
    [HOST_MEM_POOL_FEATURE] = {"host_mem_pool", devmm_dev_capability_support_host_mem_pool, NULL, false},
    [DMA_PREPARE_POOL_FEATURE] = {"dma_prepare_pool", devmm_dev_capability_support_dma_prepare_pool, NULL, false},
    [AIC_REG_MAP] = {"aic_reg_map", devmm_dev_capability_support_aic_reg_map, NULL, true},
    [REMOTE_MMAP] = {"remote_mmap", devmm_dev_capability_support_remote_mmap, NULL, false},
    [SHMEM_REPAIR] = {"shmem_repair", devmm_dev_capability_support_shmem_repair, NULL, false},
    [SHMEM_MAP_EXBUS] = {"shmem_map_exbus", devmm_dev_capability_support_shmem_map_exbus, NULL, true},
    [MEM_HOST_UVA_FEATURE] = {"mem_host_uva", devmm_dev_capability_support_mem_host_uva, NULL, false},
    [MEM_MAP_CAPABILITY_FEATURE] = {"mem_map_capability", devmm_dev_capability_support_mem_map_capability, NULL, false},  
};

bool g_dev_feature_capabilty_disable[SVM_MAX_AGENT_NUM][DEVMM_MAX_FEATURE_ID] = {{false}};

#ifndef EMU_ST
void devmm_dev_feature_capability_disable(u32 devid, u32 feature_id)
{
    g_dev_feature_capabilty_disable[devid][feature_id] = (devmm_device_is_pf(devid) &&
        g_devmm_dev_feature[feature_id].is_support_disable) ? true : false;
}

void devmm_dev_feature_capability_enable(u32 devid, u32 feature_id)
{
    g_dev_feature_capabilty_disable[devid][feature_id] = false;
}
#endif

bool devmm_dev_capability_support_shmem_map_exbus(u32 devid)
{
    return (((devid < DEVMM_MAX_PHY_DEVICE_NUM) && (devmm_is_hccs_connect(devid))) ? true : false) &&
        (g_dev_feature_capabilty_disable[devid][SHMEM_MAP_EXBUS] == false);
}

bool devmm_dev_capability_support_host_pin_pre_register(u32 devid)
{
    return (((devid < DEVMM_MAX_PHY_DEVICE_NUM) && (devmm_is_hccs_connect(devid))) ? true : false) &&
        (g_dev_feature_capabilty_disable[devid][HOST_PIN_PRE_REGISTER_FEATURE] == false);
}

bool devmm_dev_capability_support_host_mem_pool(u32 devid)
{
    return (((devid < DEVMM_MAX_PHY_DEVICE_NUM) && (devmm_is_hccs_connect(devid))) ? true : false) &&
        (g_dev_feature_capabilty_disable[devid][HOST_MEM_POOL_FEATURE] == false);
}

bool devmm_dev_capability_support_dma_prepare_pool(u32 devid)
{
    return (((devid < DEVMM_MAX_PHY_DEVICE_NUM) && (devmm_is_hccs_connect(devid))) ? true : false) &&
        (g_dev_feature_capabilty_disable[devid][DMA_PREPARE_POOL_FEATURE] == false);
}

bool devmm_dev_capability_support_host_rw_dev_ro(u32 devid)
{
    return (devmm_current_is_vdev() == false) &&
        (g_dev_feature_capabilty_disable[devid][HOST_RW_DEV_RO_FEATURE] == false);
}

void devmm_set_dev_mem_size_info(u32 did, struct devmm_chan_exchange_pginfo *info)
{
    /* ddr_size and hbm_size are used for va distrubution, need 2^n size */
    devmm_svm->device_info.ddr_size[did][0] =
        (1ul << (u32)ka_mm_get_order(info->dev_mem[DEVMM_EXCHANGE_DDR_SIZE])) * KA_MM_PAGE_SIZE;
    devmm_svm->device_info.p2p_ddr_size[did][0] = info->dev_mem_p2p[DEVMM_EXCHANGE_DDR_SIZE];
    devmm_svm->device_info.hbm_size[did][0] =
        (1ul << (u32)ka_mm_get_order(info->dev_mem[DEVMM_EXCHANGE_HBM_SIZE])) * KA_MM_PAGE_SIZE;
    devmm_svm->device_info.p2p_hbm_size[did][0] = info->dev_mem_p2p[DEVMM_EXCHANGE_HBM_SIZE];
    ka_base_atomic64_add(devmm_svm->device_info.ddr_size[did][0], &devmm_svm->device_info.total_ddr);
    ka_base_atomic64_add(devmm_svm->device_info.hbm_size[did][0], &devmm_svm->device_info.total_hbm);
    devmm_drv_info("Memory info. (did=%u; "
        "ddr_size=%llu; p2p_ddr_size=%llu; hbm_size=%llu; p2p_hbm_size=%llu; "
        "host_ddr=%llu; total_ddr=%llu; total_hbm=%llu)\n",
        did,
        devmm_svm->device_info.ddr_size[did][0], devmm_svm->device_info.p2p_ddr_size[did][0],
        devmm_svm->device_info.hbm_size[did][0], devmm_svm->device_info.p2p_hbm_size[did][0],
        devmm_svm->device_info.host_ddr,
        (u64)ka_base_atomic64_read(&devmm_svm->device_info.total_ddr), (u64)ka_base_atomic64_read(&devmm_svm->device_info.total_hbm));
}

void devmm_clear_dev_mem_size_info(u32 devid)
{
    ka_base_atomic64_sub(devmm_svm->device_info.ddr_size[devid][0], &devmm_svm->device_info.total_ddr);
    ka_base_atomic64_sub(devmm_svm->device_info.hbm_size[devid][0], &devmm_svm->device_info.total_hbm);
    devmm_svm->device_info.ddr_size[devid][0] = 0;
    devmm_svm->device_info.p2p_ddr_size[devid][0] = 0;
    devmm_svm->device_info.p2p_ddr_hugepage_size[devid][0] = 0;
    devmm_svm->device_info.hbm_size[devid][0] = 0;
    devmm_svm->device_info.p2p_hbm_size[devid][0] = 0;
    devmm_svm->device_info.p2p_hbm_hugepage_size[devid][0] = 0;
    devmm_svm->device_info.ts_ddr_size[devid][0] = 0;
    devmm_svm->device_info.ts_ddr_hugepage_size[devid][0] = 0;
}

#define DEVMM_BAR4_BIT 4
#define DEVMM_BAR4_MASK (1 << DEVMM_BAR4_BIT)
#ifndef EMU_ST
static bool devmm_is_support_wc_scene(u32 devid)
{
    u32 value;
    int ret;

    if (devmm_get_host_run_mode(devid) == DEVMM_HOST_IS_PHYS) {
        return true;
    }

    ret = devdrv_get_bar_wc_flag(devid, &value);
    if (ret != 0) {
        devmm_drv_err("Devdrv_get_bar_wc_flag failed. (devid=%u; ret=%d)\n", devid, ret);
        return false;
    }

    return ((value & DEVMM_BAR4_MASK) == DEVMM_BAR4_MASK) ? true : false;
}
#else
static bool devmm_is_support_wc_scene(u32 devid)
{
    return true;
}
#endif

int devmm_set_dev_capability(const u32 did, const u32 vfid, struct devmm_chan_exchange_pginfo *info)
{
    if (info->device_capability.dvpp_memsize != DEVMM_MAX_HEAP_MEM_FOR_DVPP_16G &&
        info->device_capability.dvpp_memsize != DEVMM_MAX_HEAP_MEM_FOR_DVPP_4G) {
        devmm_drv_err("Synchronize dvpp_memsize failed. (dvpp_memsize=%llu)\n", info->device_capability.dvpp_memsize);
        return -EINVAL;
    }

    devmm_svm->dev_capability[did].svm_offset_num = info->ts_shm_data_num;
    devmm_svm->dev_capability[did].dvpp_memsize = info->device_capability.dvpp_memsize;
    /*
     * device support alloc p2p mem,
     * if host KA_MM_PAGE_SIZE bigger than device KA_MM_PAGE_SIZE, remap continuously bar to user, will out-of-bounds memory access
     * vm can not use write combine page table properties, otherwise vm will hang
     */
    devmm_svm->dev_capability[did].feature_bar_mem =
        ((devmm_svm->device_page_size == devmm_svm->host_page_size) && devmm_is_support_wc_scene(did)) ?
        info->device_capability.feature_bar_mem : 0;
    devmm_svm->dev_capability[did].feature_bar_huge_mem = devmm_is_support_wc_scene(did) ?
        info->device_capability.feature_bar_mem : 0;
    devmm_svm->dev_capability[did].feature_dev_mem_map_host =
        ((devmm_svm->device_page_size == devmm_svm->host_page_size) &&
        (vfid == 0)) ? info->device_capability.feature_dev_mem_map_host : 0;
    devmm_svm->dev_capability[did].feature_phycial_address = info->device_capability.feature_phycial_address;
    devmm_svm->dev_capability[did].feature_pcie_th = info->device_capability.feature_pcie_th;
    devmm_svm->dev_capability[did].feature_dev_read_only = info->device_capability.feature_dev_read_only;
    devmm_svm->dev_capability[did].feature_pcie_dma_support_sva = info->device_capability.feature_pcie_dma_support_sva;
    devmm_svm->dev_capability[did].double_pgtable_offset = info->device_capability.double_pgtable_offset;
    devmm_svm->dev_capability[did].feature_aic_reg_map = true;
    devmm_svm->dev_capability[did].feature_giant_page = info->device_capability.feature_giant_page;
    devmm_svm->dev_capability[did].feature_remote_mmap = info->device_capability.feature_remote_mmap;
    devmm_svm->dev_capability[did].feature_shmem_repair = info->device_capability.feature_shmem_repair;
    devmm_svm->dev_capability[did].feature_host_mem_map_cap = info->device_capability.feature_host_mem_map_cap;
    devmm_svm->dev_capability[did].feature_mem_host_uva = info->device_capability.feature_mem_host_uva;

    devmm_drv_info("Device capability info. (did=%u; vfid=%u; ts_shm_map_bar=%u; ts_shm_data_num=%u; "
        "feature_phycial_address=0x%x; feature_pcie_th=%u; feature_bar_mem=%x; "
        "dvpp_memsize=%llu; svm_offset_num=%u; feature_read_mem=%u; feature_pcie_dma_support_sva=%u; "
        "feature_dev_mem_map_host=%u; feature_bar_huge_mem=%u; "
        "double_pgtable_offset=%llu; feature_giant_page=%u; "
        "feature_remote_mmap=%u; feature_shmem_repair=%u; feature_host_mem_map_cap=%x; feature_mem_host_uva=%u)\n",
        did, vfid, info->ts_shm_support_bar_write, info->ts_shm_data_num,
        devmm_svm->dev_capability[did].feature_phycial_address,
        devmm_svm->dev_capability[did].feature_pcie_th, devmm_svm->dev_capability[did].feature_bar_mem,
        devmm_svm->dev_capability[did].dvpp_memsize, devmm_svm->dev_capability[did].svm_offset_num,
        devmm_svm->dev_capability[did].feature_dev_read_only,
        devmm_svm->dev_capability[did].feature_pcie_dma_support_sva,
        devmm_svm->dev_capability[did].feature_dev_mem_map_host,
        devmm_svm->dev_capability[did].feature_bar_huge_mem,
        devmm_svm->dev_capability[did].double_pgtable_offset,
        devmm_svm->dev_capability[did].feature_giant_page,
        devmm_svm->dev_capability[did].feature_remote_mmap,
        devmm_svm->dev_capability[did].feature_shmem_repair,
        devmm_svm->dev_capability[did].feature_host_mem_map_cap,
        devmm_svm->dev_capability[did].feature_mem_host_uva);
    return 0;
}

void devmm_clear_dev_capability(const u32 did)
{
    devmm_svm->dev_capability[did].svm_offset_num = 0;
    devmm_svm->dev_capability[did].dvpp_memsize = 0;
    devmm_svm->dev_capability[did].feature_phycial_address = 0;
}

static bool devmm_support_host_mem_map_capability(u32 host_mem_map_cap, u32 acc_module_type)
{
    return host_mem_map_cap & (1 << acc_module_type);
}
 
int devmm_ioctl_mem_map_capability(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_map_cap_para *cap_para = &arg->data.mem_map_cap_para;
    struct devmm_devid devids = arg->head;
    u32 host_mem_map_cap = devmm_svm->dev_capability[devids.devid].feature_host_mem_map_cap;
    u32 acc_module_type = cap_para->acc_module_type;
 
    if (acc_module_type >= DRV_ACC_MODULE_TYPE_MAX) {
        devmm_drv_err("Accelerator chip type is invalid. (acc_module_type=%u)\n", acc_module_type);
        return -EINVAL;
    }
    cap_para->mem_map_capability = devmm_support_host_mem_map_capability(host_mem_map_cap, acc_module_type);
 
    return 0;
}