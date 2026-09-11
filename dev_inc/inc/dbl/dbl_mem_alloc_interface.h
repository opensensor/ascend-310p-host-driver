/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
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
 * Create: 2024-7-15
 */

#ifndef DMS_MEM_ALLOC_INTERFACE_H
#define DMS_MEM_ALLOC_INTERFACE_H

#include <linux/types.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/mman.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#include <linux/nodemask.h>
#ifdef DAVINCI_DEVICE
#include <linux/share_pool.h>
#endif
#include "ka_memory.h"
#include "ascend_hal_define.h"

/*******
    module_type: HAL_MODULE_TYPE_DEV_MANAGER
        KA_SUB_MODULE_TYPE_0: drv_devmng.ko/ drv_devmng_device.ko/ drv_devmng_host.ko/ drv_vdevmng.ko
        KA_SUB_MODULE_TYPE_1: ascend_uda.ko
        KA_SUB_MODULE_TYPE_2: ascend_urd.ko
        KA_SUB_MODULE_TYPE_3: NULL
        KA_SUB_MODULE_TYPE_4: NULL

    module_type: HAL_MODULE_TYPE_DMP
        KA_SUB_MODULE_TYPE_0: drv_ascend_ctl.ko
        KA_SUB_MODULE_TYPE_1: drv_davinci_intf.ko/ drv_davinci_intf_host.ko
        KA_SUB_MODULE_TYPE_2: ascend_udis.ko
        KA_SUB_MODULE_TYPE_3: NULL
        KA_SUB_MODULE_TYPE_4: NULL

    module_type: HAL_MODULE_TYPE_FAULT
        KA_SUB_MODULE_TYPE_0: ascend_dms_dtm.ko/ ascend_dms_smf.ko
        KA_SUB_MODULE_TYPE_1: drv_soft_fault.ko
        KA_SUB_MODULE_TYPE_2: drv_fpdc.ko
        KA_SUB_MODULE_TYPE_3: drv_dfm.ko
        KA_SUB_MODULE_TYPE_4: drv_soc_misc.ko

    module_type: HAL_MODULE_TYPE_UPGRADE
        KA_SUB_MODULE_TYPE_0: drv_upgrade.ko
        KA_SUB_MODULE_TYPE_1: drv_user_cfg.ko
        KA_SUB_MODULE_TYPE_2: drv_pkicms.ko
        KA_SUB_MODULE_TYPE_3: NULL
        KA_SUB_MODULE_TYPE_4: NULL
*/
#if defined(CFG_FEATURE_MEMALLOC_MODULE_TYPE) && defined(CFG_FEATURE_MEMALLOC_SUBMODULE_TYPE)
    #define DMS_MODULE_TYPE CFG_FEATURE_MEMALLOC_MODULE_TYPE
    #define DMS_KA_SUB_MODULE_TYPE CFG_FEATURE_MEMALLOC_SUBMODULE_TYPE
#endif

#define dbl_kmalloc(size, flags) \
    ka_kmalloc(size, flags, ka_get_module_id(DMS_MODULE_TYPE, DMS_KA_SUB_MODULE_TYPE))
#define dbl_kzalloc(size, flags) \
    ka_kzalloc(size, flags, ka_get_module_id(DMS_MODULE_TYPE, DMS_KA_SUB_MODULE_TYPE))
#define dbl_kcalloc(size, flags) \
    ka_kcalloc(size, flags, ka_get_module_id(DMS_MODULE_TYPE, DMS_KA_SUB_MODULE_TYPE))

#define dbl_kfree(addr) \
    ka_kfree(addr, ka_get_module_id(DMS_MODULE_TYPE, DMS_KA_SUB_MODULE_TYPE))

#define dbl_vzalloc(size)  \
    ka_vzalloc(size, ka_get_module_id(DMS_MODULE_TYPE, DMS_KA_SUB_MODULE_TYPE))
#define dbl_vmalloc(size, gfp_mask, prot) \
    __ka_vmalloc(size, gfp_mask, prot, ka_get_module_id(DMS_MODULE_TYPE, DMS_KA_SUB_MODULE_TYPE))

#define dbl_vfree(addr) \
    ka_vfree(addr, ka_get_module_id(DMS_MODULE_TYPE, DMS_KA_SUB_MODULE_TYPE))

#define dbl_get_free_pages(gfp_mask, order) \
    ka_get_free_pages(gfp_mask, order, ka_get_module_id(DMS_MODULE_TYPE, DMS_KA_SUB_MODULE_TYPE))
#define dbl_alloc_pages(gfp_mask, order) \
    ka_alloc_pages(gfp_mask, order, ka_get_module_id(DMS_MODULE_TYPE, DMS_KA_SUB_MODULE_TYPE))
#define dbl_free_pages(addr, order) \
    ka_free_pages(addr, order, ka_get_module_id(DMS_MODULE_TYPE, DMS_KA_SUB_MODULE_TYPE))

#define dbl_mg_sp_alloc_nodemask(size, sp_flags, spg_id, nodemask) \
    ka_mg_sp_alloc_nodemask(size, sp_flags, spg_id, nodemask, ka_get_module_id(DMS_MODULE_TYPE, DMS_KA_SUB_MODULE_TYPE))
#define dbl_mg_sp_alloc(size, sp_flags, spg_id) \
    ka_mg_sp_alloc(size, sp_flags, spg_id, ka_get_module_id(DMS_MODULE_TYPE, DMS_KA_SUB_MODULE_TYPE))
#define dbl_mg_sp_free(addr, id) \
    ka_mg_sp_free(addr, id, module_id)

#define dbl_alloc_pages_exact(size, gfp_mask) \
    ka_alloc_pages_exact(size, gfp_mask, ka_get_module_id(DMS_MODULE_TYPE, DMS_KA_SUB_MODULE_TYPE))
#define dbl_free_pages_exact(virt, size) \
    ka_free_pages_exact(virt, size, ka_get_module_id(DMS_MODULE_TYPE, DMS_KA_SUB_MODULE_TYPE))

#define dbl_alloc_pages_node(nid, gfp_mask, order) \
    ka_alloc_pages_node(nid, gfp_mask, order, ka_get_module_id(DMS_MODULE_TYPE, DMS_KA_SUB_MODULE_TYPE))
#define dbl_free_pages_ex(page, order) \
    __ka_free_pages(page, order, ka_get_module_id(DMS_MODULE_TYPE, DMS_KA_SUB_MODULE_TYPE))

#define dbl_kvzalloc(size, flags) \
    ka_kvzalloc(size, flags, ka_get_module_id(DMS_MODULE_TYPE, DMS_KA_SUB_MODULE_TYPE))
#define dbl_kvmalloc_node(size, flags, node) \
    ka_kvmalloc_node(size, flags, node, ka_get_module_id(DMS_MODULE_TYPE, DMS_KA_SUB_MODULE_TYPE))
#define dbl_kvmalloc(size, flags) \
    ka_kvmalloc(size, flags, ka_get_module_id(DMS_MODULE_TYPE, DMS_KA_SUB_MODULE_TYPE))
#define dbl_kvfree(addr)  \
    ka_kvfree(addr, ka_get_module_id(DMS_MODULE_TYPE, DMS_KA_SUB_MODULE_TYPE))
#endif