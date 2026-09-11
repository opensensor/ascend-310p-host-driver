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

#include "devdrv_common.h"
#include "tsdrv_device.h"

int devdrv_shm_init(u32 devid, u32 fid,  u32 tsid)
{
#ifndef AOS_LLVM_BUILD
    gfp_t gfp_flags = KA_GFP_KERNEL | __KA_GFP_ZERO | __KA_GFP_COMP;
    u32 order;
#endif
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct tsdrv_sq_hwinfo *sq_hwinfo = NULL;
    phys_addr_t db_paddr, sq_paddr;
    enum tsdrv_env_type env_type;
    void *sq_vaddr = NULL;
    size_t db_size;
    char *tmp = NULL;

    ts_resource = tsdrv_get_ts_resoruce(devid, fid, tsid);

    env_type = tsdrv_get_env_type();
    sq_hwinfo = tsdrv_get_ts_sq_hwinfo(devid, tsid);
    if (env_type == TSDRV_ENV_OFFLINE) {
        sq_paddr = sq_hwinfo->paddr;
    } else {
        sq_paddr = sq_hwinfo->bar_addr;
    }
    sq_vaddr = (void *)ka_mm_ioremap(sq_paddr, DEVDRV_RESERVE_MEM_SIZE);
    if (sq_vaddr == NULL) {
        TSDRV_PRINT_ERR("iomem fail, devid=%u fid=%u tsid=%u\n", devid, fid, tsid);
        return -ENOMEM;
    }
    ts_resource->mem_info[DEVDRV_SQ_MEM].bar_addr = sq_hwinfo->bar_addr;
    ts_resource->mem_info[DEVDRV_SQ_MEM].phy_addr = sq_hwinfo->paddr;
    ts_resource->mem_info[DEVDRV_SQ_MEM].virt_addr = (phys_addr_t)(uintptr_t)sq_vaddr;
    ts_resource->mem_info[DEVDRV_SQ_MEM].size = DEVDRV_RESERVE_MEM_SIZE;
    TSDRV_PRINT_DEBUG("sq paddr (0x%pK), size(0x%lx)", (void *)(uintptr_t)sq_hwinfo->paddr, sq_hwinfo->size);
#ifndef AOS_LLVM_BUILD
    order = DEVDRV_MAX_INFO_ORDER;
    tmp = (char *)(uintptr_t)__get_free_pages(gfp_flags, order);
#else
    tmp = tsdrv_kmalloc(DEVDRV_MAX_INFO_SIZE, KA_GFP_USER);
#endif
    if (tmp == NULL) {
        ka_mm_iounmap(sq_vaddr);
        ts_resource->mem_info[DEVDRV_SQ_MEM].virt_addr = 0;
        TSDRV_PRINT_ERR("alloc mem fail, devid=%u fid=%u tsid=%u\n", devid, fid, tsid);
        return -ENOMEM;
    }
    ts_resource->mem_info[DEVDRV_INFO_MEM].phy_addr = ka_mm_virt_to_phys(tmp);
    ts_resource->mem_info[DEVDRV_INFO_MEM].virt_addr = (phys_addr_t)(uintptr_t)tmp;
    ts_resource->mem_info[DEVDRV_INFO_MEM].size = DEVDRV_VM_INFO_MEM_SIZE;

    tsdrv_get_db_phy_info(devid, tsid, &db_paddr, &db_size);
    TSDRV_PRINT_DEBUG("db paddr(0x%pK), db size(0x%lx)", (void *)(uintptr_t)db_paddr, db_size);
    ts_resource->mem_info[DEVDRV_DOORBELL_MEM].phy_addr = db_paddr;
    ts_resource->mem_info[DEVDRV_DOORBELL_MEM].size = db_size;

    return 0;
}

void devdrv_shm_destroy(u32 devid, u32 fid,  u32 tsid)
{
    struct tsdrv_ts_resource *ts_resource = NULL;
    unsigned long addr;
#ifndef AOS_LLVM_BUILD
    u32 order;
#endif
    ts_resource = tsdrv_get_ts_resoruce(devid, fid, tsid);
    if (ts_resource == NULL) {
        TSDRV_PRINT_ERR("devid(%u),devdrv_info is null.\n", devid);
        return;
    }
#ifndef AOS_LLVM_BUILD
    order = DEVDRV_MAX_INFO_ORDER;
#endif
    addr = (unsigned long)ts_resource->mem_info[DEVDRV_INFO_MEM].virt_addr;
    if (addr != 0) {
#ifndef AOS_LLVM_BUILD
        ka_mm_free_pages(addr, order);
#else
        tsdrv_kfree(addr);
#endif
        ts_resource->mem_info[DEVDRV_INFO_MEM].virt_addr = 0;
    }

    if (ts_resource->mem_info[DEVDRV_SQ_MEM].virt_addr != 0) {
        ka_mm_iounmap((void *)(uintptr_t)ts_resource->mem_info[DEVDRV_SQ_MEM].virt_addr);
        ts_resource->mem_info[DEVDRV_SQ_MEM].virt_addr = 0;
    }
}

