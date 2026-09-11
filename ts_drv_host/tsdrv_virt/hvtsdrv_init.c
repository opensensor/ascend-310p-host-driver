/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2023. All rights reserved.
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
#include "hvtsdrv_init.h"
#ifdef CFG_FEATURE_VFIO
#include <linux/vmalloc.h>
#include <linux/bitmap.h>

#include "ka_list_pub.h"
#include "dbl/uda.h"
#include "soc_res.h"

#include "securec.h"
#include "tsdrv_log.h"
#include "tsdrv_device.h"
#include "tsdrv_sync.h"
#include "tsdrv_nvme.h"
#include "devdrv_cb.h"
#include "devdrv_cqsq.h"

#include "devdrv_manager_common.h"
#include "devdrv_interface.h"
#include "virtmng_interface.h"
#include "davinci_interface.h"
#include "davinci_api.h"

#include "hvtsdrv_proc.h"
#include "hvtsdrv_tsagent.h"
#include "hvtsdrv_id.h"
#include "hvtsdrv_cqsq.h"
#include "hvtsdrv_cb.h"
#include "tsdrv_sqcq_timestamp.h"

struct vmng_common_msg_client hvtsdrv_common_msg_client = {
    .type = VMNG_MSG_COMMON_TYPE_TSDRV,
    .init = NULL,
    .common_msg_recv = hvtsdrv_com_msg_recv,
};

struct vmng_vpc_client hvtsdrv_vpc_client = {
    .vpc_type = VMNG_VPC_TYPE_TSDRV,
    .init = NULL,
    .msg_recv = hvtsdrv_vpc_msg_recv,
};

bool tsdrv_pm_ctx_is_finished(u32 devid)
{
    u32 ctx_num = tsdrv_dev_get_ctx_num(devid, TSDRV_PM_FID);
    if (ctx_num == 0) {
        return true;
    }
    return false;
}

STATIC int hvtsdrv_init_vsqvcq_mem(u32 devid, u32 fid, u32 tsid, u64 *sqcq_vaddr,
    enum tsdrv_host_flag host_flag)
{
    struct tsdrv_ts_resource *ts_resource = NULL;
    u64 size = 0;
    u64 tmp_size;
    int ret;

    ts_resource = tsdrv_get_ts_resoruce(devid, fid, tsid);

    /* callback vcq use the VM_INFO_MEM */
    tmp_size = DEVDRV_VM_SQ_SLOT_SIZE * ts_resource->id_res[TSDRV_SQ_ID].id_capacity +
        DEVDRV_VM_CQ_SLOT_SIZE * ts_resource->id_res[TSDRV_CQ_ID].id_capacity +
        DEVDRV_VM_INFO_MEM_SIZE;
    tmp_size = ka_base_round_up(tmp_size, KA_MM_PAGE_SIZE) + KA_MM_PAGE_SIZE;
    if (host_flag == TSDRV_VIRTUAL_TYPE) {
        /* current only support one ts */
        ret = vmngh_get_virtual_addr_info(devid, fid, VMNG_GET_ADDR_TYPE_TSDRV, sqcq_vaddr, &size);
        if (ret != 0) {
            TSDRV_PRINT_ERR("devid(%u %u), get addr fail ret(%d).\n", devid, fid, ret);
            return ret;
        }

        if (size < tmp_size) {
            TSDRV_PRINT_ERR("devid(%u %u), tsid(%d), size(%#llx) capacity(%u %u)\n", devid, fid, tsid,
                size, ts_resource->id_res[TSDRV_SQ_ID].id_capacity, ts_resource->id_res[TSDRV_CQ_ID].id_capacity);
            return -EFAULT;
        }
        TSDRV_PRINT_DEBUG("size(0x%llx)\n", size);
    } else {
        *sqcq_vaddr = (u64)(uintptr_t)tsdrv_vzalloc(tmp_size);
        if (*sqcq_vaddr == 0) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("vzalloc sqcq vaddr is failed.\n");
            return -EFAULT;
#endif
        }
        TSDRV_PRINT_DEBUG("size(0x%llx)\n", tmp_size);
    }

    return 0;
}

STATIC void hvtsdrv_uninit_vsqvcq_mem(u32 devid, u32 fid, u32 tsid, enum tsdrv_host_flag host_flag)
{
    struct tsdrv_ts_resource *ts_resource = NULL;
    u64 sqcq_vaddr = 0;

    ts_resource = tsdrv_get_ts_resoruce(devid, fid, tsid);
    if (host_flag == TSDRV_CONTAINER_TYPE) {
        sqcq_vaddr = ts_resource->unalign_addr;
        if (sqcq_vaddr != 0) {
            tsdrv_vfree((void *)(uintptr_t)sqcq_vaddr);
            ts_resource->unalign_addr = 0;
            ts_resource->mem_info[DEVDRV_SQ_MEM].virt_addr = 0;
        }
    }
#ifdef TSDRV_UT
    if (host_flag == TSDRV_VIRTUAL_TYPE) {
        if (ts_resource->unalign_addr != 0) {
            tsdrv_vfree((void *)(uintptr_t)ts_resource->unalign_addr);
        }
    }
#endif
}

STATIC int hvtsdrv_sq_mem_init(u32 devid, u32 fid, u32 tsid, enum tsdrv_host_flag host_flag)
{
    struct tsdrv_ts_resource *ts_resource = tsdrv_get_ts_resoruce(devid, fid, tsid);
    u64 sqcq_vaddr = 0;
    int ret;

    ret = hvtsdrv_init_vsqvcq_mem(devid, fid, tsid, &sqcq_vaddr, host_flag);
    if (ret != 0) {
        TSDRV_PRINT_ERR("Failed to init vsqvcq memory. (devid=%u; fid=%u; tsid=%u)\n", devid, fid, tsid);
        return ret;
    }

    ts_resource->unalign_addr = sqcq_vaddr;
    ts_resource->mem_info[DEVDRV_SQ_MEM].virt_addr = ka_base_round_up(sqcq_vaddr, KA_MM_PAGE_SIZE);
    ts_resource->mem_info[DEVDRV_SQ_MEM].size = (size_t)DEVDRV_SQ_SLOT_SIZE * DEVDRV_MAX_SQ_DEPTH *
        ts_resource->id_res[TSDRV_SQ_ID].id_capacity;
    ts_resource->mem_info[DEVDRV_CQ_MEM].virt_addr = ts_resource->mem_info[DEVDRV_SQ_MEM].virt_addr +
        ts_resource->mem_info[DEVDRV_SQ_MEM].size;
    ts_resource->mem_info[DEVDRV_CQ_MEM].size = (size_t)DEVDRV_MAX_CQ_DEPTH * DEVDRV_CQ_SLOT_SIZE *
        ts_resource->id_res[TSDRV_CQ_ID].id_capacity + DEVDRV_MAX_CBCQ_SIZE * DEVDRV_MAX_CBCQ_DEPTH;

    TSDRV_PRINT_DEBUG("devid(%u %u), tsid(%u), sq_mem_size(0x%lx), cq_mem_size(0x%lx)\n", devid, fid, tsid,
        ts_resource->mem_info[DEVDRV_SQ_MEM].size, ts_resource->mem_info[DEVDRV_CQ_MEM].size);

    return 0;
}

void hvtsdrv_sq_mem_uninit(u32 devid, u32 fid, u32 tsid, enum tsdrv_host_flag host_flag)
{
    hvtsdrv_uninit_vsqvcq_mem(devid, fid, tsid, host_flag);
}

STATIC void hvtsdrv_shm_destroy(u32 devid, u32 fid, enum tsdrv_host_flag host_flag)
{
    struct tsdrv_ts_resource *ts_resource = NULL;
    unsigned long addr, flags;
    u32 order, i;
    size_t size;

    for (i = 0; i < tsdrv_get_dev_tsnum(devid); i++) {
        ts_resource = tsdrv_get_ts_resoruce(devid, fid, i);
        size = ts_resource->mem_info[DEVDRV_VSQ_INFO_MEM].size + ts_resource->mem_info[DEVDRV_VCQ_INFO_MEM].size;
        addr = (unsigned long)ts_resource->mem_info[DEVDRV_VSQ_INFO_MEM].virt_addr;
        order = ka_mm_get_order(size);
        if (addr != 0) {
            ka_task_spin_lock_irqsave(&ts_resource->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);
            ts_resource->mem_info[DEVDRV_VCQ_INFO_MEM].virt_addr = 0;
            ts_resource->mem_info[DEVDRV_VSQ_INFO_MEM].virt_addr = 0;
            ka_task_spin_unlock_irqrestore(&ts_resource->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock, flags);

            ka_mm_free_pages(addr, order);
        }
        if (host_flag == TSDRV_CONTAINER_TYPE) {
            hvtsdrv_sq_mem_uninit(devid, fid, i, host_flag);
        }
    }
}

int hvtsdrv_shm_init(u32 devid, u32 fid, enum tsdrv_host_flag host_flag)
{
    gfp_t gfp_flags = KA_GFP_KERNEL | __KA_GFP_COMP | __KA_GFP_ZERO;
    struct tsdrv_ts_resource *pm_ts_resource = NULL;
    struct tsdrv_ts_resource *ts_resource = NULL;
    u64 vsq_info_size, vcq_info_size;
    char *tmp = NULL;
    int ret;
    u32 i;

    for (i = 0; i < tsdrv_get_dev_tsnum(devid); i++) {
        if (host_flag == TSDRV_CONTAINER_TYPE) {
            ret = hvtsdrv_sq_mem_init(devid, fid, i, host_flag);
            if (ret != 0) {
#ifndef TSDRV_UT
                TSDRV_PRINT_ERR("Failed to init sq memory. (devid=%u; fid=%u; tsid=%u)\n", devid, fid, i);
                goto shm_init_err;
#endif
            }
        }

        pm_ts_resource = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, i);
        ts_resource = tsdrv_get_ts_resoruce(devid, fid, i);

        vsq_info_size = ts_resource->id_res[TSDRV_SQ_ID].id_capacity * sizeof(struct tsdrv_vsq_info);
        /* callback vcq_info must consume one mem */
        vcq_info_size = (ts_resource->id_res[TSDRV_CQ_ID].id_capacity + 1) * sizeof(struct tsdrv_vcq_info);
        tmp = (char *)(uintptr_t)__get_free_pages(gfp_flags, ka_mm_get_order(vsq_info_size + vcq_info_size));
        if (tmp == NULL) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("alloc mem fail, devid(%u %u) tsid(%u)\n", devid, fid, i);
            goto shm_init_err;
#endif
        }

        ka_task_spin_lock_init(&ts_resource->mem_info[DEVDRV_VCQ_INFO_MEM].spinlock);

        ts_resource->mem_info[DEVDRV_INFO_MEM].virt_addr = pm_ts_resource->mem_info[DEVDRV_INFO_MEM].virt_addr;
        ts_resource->mem_info[DEVDRV_INFO_MEM].size = DEVDRV_VM_INFO_MEM_SIZE;

        ts_resource->mem_info[DEVDRV_VSQ_INFO_MEM].virt_addr = (phys_addr_t)(uintptr_t)tmp;
        ts_resource->mem_info[DEVDRV_VSQ_INFO_MEM].size = vsq_info_size;
        ts_resource->mem_info[DEVDRV_VCQ_INFO_MEM].virt_addr = (phys_addr_t)(uintptr_t)tmp + vsq_info_size;
        ts_resource->mem_info[DEVDRV_VCQ_INFO_MEM].size = vcq_info_size;

        TSDRV_PRINT_DEBUG("devid(%u %u), tsid(%u), vsq_info_size(0x%llx), vcq_info_size(0x%llx)\n",
            devid, fid, i, vsq_info_size, vcq_info_size);
    }

    return 0;
#ifndef TSDRV_UT
shm_init_err:
    hvtsdrv_shm_destroy(devid, fid, host_flag);
    return -EINVAL;
#endif
}

int hvtsdrv_dev_logic_cq_init(u32 dev_id, u32 fid)
{
    struct tsdrv_dev_resource *dev_res = tsdrv_get_dev_resource(dev_id, fid);
    u32 i, tsnum;

    if (dev_res == NULL) {
        TSDRV_PRINT_ERR("Failed to get device resource. (devid=%u; fid=%u).\n", dev_id, fid);
        return -EINVAL;
    }

    tsnum = tsdrv_get_dev_tsnum(dev_id);
    for (i = 0; i < tsnum; i++) {
        dev_res->ts_resource[i].logic_phy_cq = 0;
        dev_res->ts_resource[i].logic_phy_cq_flag = -1;
    }

    TSDRV_PRINT_INFO("Init logic cq success. (devid=%u; fid=%u)\n", dev_id, fid);
    return 0;
}

void hvtsdrv_dev_logic_cq_uninit(u32 dev_id, u32 fid)
{
    struct tsdrv_dev_resource *dev_res = tsdrv_get_dev_resource(dev_id, fid);
    u32 i, tsnum;

    if (dev_res == NULL) {
        return;
    }

    tsnum = tsdrv_get_dev_tsnum(dev_id);
    for (i = 0; i < tsnum; i++) {
        if (dev_res->ts_resource[i].logic_phy_cq_flag >= 0) {
            tsdrv_kernel_free_cq(dev_id, fid, i, dev_res->ts_resource[i].logic_phy_cq);
        }
    }
    TSDRV_PRINT_INFO("Uninit logic cq success. (devid=%u; fid=%u)\n", dev_id, fid);
}

int hvtsdrv_resource_init(u32 dev_id, u32 fid, enum TSDRV_CAPACITY tsdrv_capacity,
    enum tsdrv_host_flag host_flag)
{
    struct tsdrv_device *ts_dev = tsdrv_get_dev(dev_id);
    u32 tsnum = tsdrv_get_dev_tsnum(dev_id);
    int ret;

    ret = tsdrv_dev_register(dev_id, fid, tsdrv_capacity);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("hvtsdrv dev-%u fid-%u register fail, ret-%d.\n", dev_id, fid, ret);
        return ret;
#endif
    }

    tsdrv_dev_ctx_init_per_fid(ts_dev->dev_ctx, fid);

    ret = hvtsdrv_shm_init(dev_id, fid, host_flag);
    if (ret != 0) {
#ifndef TSDRV_UT
        goto dev_reg_err;
#endif
    }

    ret = hvtsdrv_cqsq_init(dev_id, fid);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("hvtsdrv dev-%u fid-%u id cqsq init fail, ret-%d.\n", dev_id, fid, ret);
        goto cqsq_init_err;
#endif
    }

    ret = tsdrv_ids_init(dev_id, fid);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("hvtsdrv dev-%u fid-%u id resource init fail, ret-%d.\n", dev_id, fid, ret);
        goto id_init_err;
#endif
    }

    ret = tsdrv_nvme_init(dev_id, fid, tsnum);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Nvme init fail. (devid=%u; fid=%u; tsnum=%u)\n", dev_id, fid, tsnum);
        goto err_nvme_init;
#endif
    }

    ret = tsdrv_sqcq_dfx_dev_init(dev_id, fid, tsnum);
    if (ret != 0) {
#ifndef TSDRV_UT
        goto err_sqcq_dfx_dev_init;
#endif
    }

    return 0;

#ifndef TSDRV_UT
err_sqcq_dfx_dev_init:
    tsdrv_nvme_exit(dev_id, fid, tsnum);
err_nvme_init:
    tsdrv_ids_uninit(dev_id, fid);
id_init_err:
    hvtsdrv_cqsq_uninit(dev_id, fid);
cqsq_init_err:
    hvtsdrv_shm_destroy(dev_id, fid, host_flag);
dev_reg_err:
    tsdrv_dev_unregister(dev_id, fid);
    return ret;
#endif
}

void hvtsdrv_resource_uninit(u32 dev_id, u32 fid, enum tsdrv_host_flag host_flag)
{
    u32 tsnum = tsdrv_get_dev_tsnum(dev_id);

    tsdrv_sqcq_dfx_dev_exit(dev_id, fid, tsnum);
    tsdrv_ids_uninit(dev_id, fid);
    /* must be after the tsdrv_ids_uninit interface */
    hvtsdrv_cqsq_uninit(dev_id, fid);
    tsdrv_nvme_exit(dev_id, fid, tsnum);
    hvtsdrv_shm_destroy(dev_id, fid, host_flag);
    tsdrv_dev_unregister(dev_id, fid);
}

#define TSDRV_MAX_THREAD_NAME 20
int hvtsdrv_dev_recycle_wq_init(u32 dev_id, u32 fid)
{
    struct tsdrv_dev_resource *dev_res = tsdrv_get_dev_resource(dev_id, fid);
    char thread_name[TSDRV_MAX_THREAD_NAME] = {0};
    int ret;

    if (dev_res == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("get dev resource failed, devid(%u) fid(%u).\n", dev_id, fid);
        return -EINVAL;
#endif
    }

    ret = snprintf_s(thread_name, TSDRV_MAX_THREAD_NAME, TSDRV_MAX_THREAD_NAME - 1, "vm_recycle%u-%u", dev_id, fid);
    if (ret < 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("thread_name copy fail, dev_id(%u) fid(%u) ret(%d)\n", dev_id, fid, ret);
        return -EINVAL;
#endif
    }

    ka_task_mutex_lock(&dev_res->dev_res_lock);
    if (dev_res->vm_res_wq == NULL) {
        dev_res->vm_res_wq = ka_task_create_singlethread_workqueue(thread_name);
        if (dev_res->vm_res_wq == NULL) {
#ifndef TSDRV_UT
            ka_task_mutex_unlock(&dev_res->dev_res_lock);
            TSDRV_PRINT_ERR("vm recycle workqueue create fail, dev_id(%u), fid(%u)\n", dev_id, fid);
            return -EINVAL;
#endif
        }
        KA_TASK_INIT_WORK(&dev_res->vm_res_recycle_work, hvtsdrv_vm_res_recycle_work);
    }
    ka_task_mutex_unlock(&dev_res->dev_res_lock);
    TSDRV_PRINT_DEBUG("dev_id(%u) fid(%u)\n", dev_id, fid);

    return 0;
}

void hvtsdrv_dev_recycle_wq_uninit(u32 dev_id, u32 fid)
{
    struct tsdrv_dev_resource *dev_res = tsdrv_get_dev_resource(dev_id, fid);

    if (dev_res == NULL) {
        return;
    }
    if (dev_res->vm_res_wq != NULL) {
        ka_task_destroy_workqueue(dev_res->vm_res_wq);
        dev_res->vm_res_wq = NULL;
    }
}

int hvtsdrv_phy_cbcqsq_init(u32 devid, u32 tsid)
{
    struct tsdrv_device *tsdrv_dev = tsdrv_get_dev(devid);
    int ret;

    ret = callback_physic_sqcq_init(tsdrv_dev, TSDRV_PM_FID, tsid);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Failed to reinit callback physqcq. (devid=%u; ret=%d)\n", devid, ret);
        return ret;
#endif
    }
    ret = tsdrv_msg_phy_cbcqsq_init(devid, tsid);
    if (ret != 0) {
        TSDRV_PRINT_ERR("Failed to reinit callback physqcq offline. (devid=%u; ret=%d)\n", devid, ret);
        callback_physic_sqcq_exit(tsdrv_dev, TSDRV_PM_FID, tsid);
    }

    return ret;
}

int hvtsdrv_phy_cbcqsq_exit(u32 devid, u32 tsid)
{
    struct tsdrv_device *tsdrv_dev = NULL;
    int ret;

    /* callback phy cqsq of fid0 needs to be recycled first */
    tsdrv_dev = tsdrv_get_dev(devid);
    callback_physic_sqcq_exit(tsdrv_dev, TSDRV_PM_FID, 0);
    /* cqsq used by callback when offline needs to be recycled first */
    ret = tsdrv_msg_phy_cbcqsq_exit(devid, 0);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Failed to recycle cbcqsq from offline. (devid=%u; host_flag=%d; ret=%d)\n",
            devid, tsdrv_get_host_flag(devid), ret);
#endif
    }
    return ret;
}

STATIC int hvtsdrv_get_shm_mem(u32 devid, u32 fid, u32 tsid, size_t bar_size, struct vmngh_map_info *map_info)
{
    struct tsdrv_ts_resource *ts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
    struct devdrv_ts_sq_info *sq_info = NULL;
    struct devdrv_sq_sub_info *sq_sub = NULL;
    struct vmngh_bar_map *bar_map = NULL;
    size_t off_base;
    int ret, sqid, i, j;

    bar_map = tsdrv_kmalloc(sizeof(struct vmngh_bar_map) * TSRDV_SHM_SQ_NUM, KA_GFP_KERNEL);
    if (bar_map == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Failed to kmalloc map_info. (devid=%u; fid=%u)\n", devid, fid);
        return -ENOMEM;
#endif
    }

    off_base = bar_size - DEVDRV_SQ_SLOT_SIZE * DEVDRV_MAX_STREAM_ID;
    for (i = 0; i < TSRDV_SHM_SQ_NUM; i++) {
        sqid = tsdrv_kernel_alloc_sq(devid, fid, tsid, DEVDRV_SQ_SLOT_SIZE, DEVDRV_MAX_SQ_DEPTH, SHM_SQCQ_TYPE);
        if (sqid < 0) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("Failed to alloc sq. (devid=%u; fid=%u; tsid=%u)\n", devid, fid, tsid);
            ret = -EFAULT;
            goto err_alloc_sq;
#endif
        }

        sq_info = tsdrv_get_sq_info(devid, fid, tsid, sqid);
        if (sq_info == NULL) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("Failed to get sq_info. (devid=%u; fid=%u; tsid=%u; sqid=%d)\n", devid, fid, tsid, sqid);
            ret = -EINVAL;
            i++;
            goto err_alloc_sq;
#endif
        }

        sq_sub = sq_info->sq_sub;
        bar_map[i].paddr = sq_sub->bar_addr;
        bar_map[i].size = sq_sub->size * sq_sub->depth;
        bar_map[i].offset = off_base + sq_sub->size * sq_sub->depth * i;
        ts_res->shm_phy_sq[i] = sqid;

        TSDRV_PRINT_INFO("The success. (devid=%u; fid=%u; tsid=%u; sqid=%u)\n", devid, fid, tsid, sqid);
    }
    ts_res->shm_map_addr = bar_map;
    ts_res->shm_offset = off_base;
    map_info->map_info = bar_map;
    map_info->num = TSRDV_SHM_SQ_NUM;

    return 0;
#ifndef TSDRV_UT
err_alloc_sq:
    for (j = 0; j < i; j++) {
        tsdrv_kernel_free_sq(devid, fid, tsid, ts_res->shm_phy_sq[j]);
    }
    tsdrv_kfree(bar_map);
    return ret;
#endif
}

STATIC void hvtsdrv_put_shm_mem(u32 devid, u32 fid, u32 tsid)
{
    struct tsdrv_ts_resource *ts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
    int i;

    if (ts_res->shm_map_addr != NULL) {
        tsdrv_kfree(ts_res->shm_map_addr);
        ts_res->shm_map_addr = NULL;
    }

    for (i = 0; i < TSRDV_SHM_SQ_NUM; i++) {
        tsdrv_kernel_free_sq(devid, fid, tsid, ts_res->shm_phy_sq[i]);
    }
}

STATIC int hvtsdrv_get_map_info(struct vmngh_client_instance *instance, struct vmngh_map_info *map_info)
{
    struct tsdrv_device *tsdrv_dev = NULL;
    u32 devid, fid, tsid, chip_type, i;
    int ret;

    if (hvtsdrv_instance_check(instance) != 0 || (map_info == NULL)) {
        TSDRV_PRINT_ERR("Failed to check the parameters.\n");
        return -EINVAL;
    }
    devid = instance->dev_ctrl->dev_id;
    fid = instance->dev_ctrl->vfid;

    ret = hvtsdrv_phy_cbcqsq_exit(devid, 0);
    if (ret != 0) {
        TSDRV_PRINT_ERR("Failed to exit callback sqcq. (devid=%u)\n", devid);
        return ret;
    }

    tsdrv_dev = tsdrv_get_dev(devid);
    ka_base_atomic_set(&tsdrv_dev->host_flag, TSDRV_VIRTUAL_TYPE);
    TSDRV_PRINT_DEBUG("Start to get map information. (devid=%u; fid=%u)\n", devid, fid);

    ret = hvtsdrv_resource_init(devid, fid, (enum TSDRV_CAPACITY)instance->dev_ctrl->dtype, TSDRV_VIRTUAL_TYPE);
    if (ret != 0) {
#ifndef TSDRV_UT
        hvtsdrv_phy_cbcqsq_init(devid, 0);
        TSDRV_PRINT_ERR("Failed to initialize resource. (ret=%d; capacity=%u)\n", ret, instance->dev_ctrl->dtype);
        return ret;
#endif
    }

    chip_type = devdrv_get_dev_chip_type(devid);
    if (chip_type == HISI_MINI_V2) {
        for (tsid = 0; tsid < DEVDRV_MAX_TS_NUM; tsid++) {
            ret = hvtsdrv_get_shm_mem(devid, fid, tsid, instance->dev_ctrl->bar4_size, map_info);
            if (ret != 0) {
#ifndef TSDRV_UT
                TSDRV_PRINT_ERR("Failed to get share memory.(devid=%u; fid=%u; tsid=%u)\n", devid, fid, tsid);
                goto get_vm_shm_fail;
#endif
            }
        }
    }

    TSDRV_PRINT_INFO("Get map information success. (devid=%u; fid=%u; bar4_size=%llu)\n",
        devid, fid, instance->dev_ctrl->bar4_size);
    return 0;
#ifndef TSDRV_UT
get_vm_shm_fail:
    hvtsdrv_phy_cbcqsq_init(devid, 0);
    if (chip_type == HISI_MINI_V2) {
        for (i = 0; i < tsid; i++) {
            hvtsdrv_put_shm_mem(devid, fid, i);
        }
    }
    hvtsdrv_resource_uninit(devid, fid, TSDRV_VIRTUAL_TYPE);
    return ret;
#endif
}

int hvtsdrv_put_map_info(struct vmngh_client_instance *instance)
{
    u32 devid, fid, tsid, chip_type, fid_num;
    int ret;

    if (instance == NULL || instance->dev_ctrl == NULL) {
        TSDRV_PRINT_ERR("Instance is invalid.\n");
        return -EINVAL;
    }

    devid = instance->dev_ctrl->dev_id;
    fid = instance->dev_ctrl->vfid;

    if (devid >= TSDRV_MAX_DAVINCI_NUM || fid >= TSDRV_MAX_FID_NUM || fid == TSDRV_PM_FID) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Devid or fid is invalid. (devid=%u; fid=%u)\n", devid, fid);
        return -EINVAL;
#endif
    }

    chip_type = devdrv_get_dev_chip_type(devid);
    if (chip_type == HISI_MINI_V2) {
        for (tsid = 0; tsid < DEVDRV_MAX_TS_NUM; tsid++) {
            hvtsdrv_put_shm_mem(devid, fid, tsid);
        }
    }
    hvtsdrv_resource_uninit(devid, fid, TSDRV_VIRTUAL_TYPE);

    fid_num = tsdrv_get_fid_num(devid);
    if (fid_num == 1) {
        ret = hvtsdrv_phy_cbcqsq_init(devid, 0);
        if (ret != 0) {
            TSDRV_PRINT_ERR("Failed to init pf cbsqcq. (devid=%u; ret=%d)\n", devid, ret);
            return ret;
        }
    }
    TSDRV_PRINT_INFO("Free map information success. (devid=%u; fid=%u; fid_num=%u)\n", devid, fid, fid_num);

    return 0;
}

static s32 hvtsdrv_init_instance(u32 dev_id, u32 fid)
{
    u32 tsid = 0;
    s32 ret;

    ret = hvtsdrv_sq_mem_init(dev_id, fid, tsid, TSDRV_VIRTUAL_TYPE);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Failed to init shm. (devid=%u, fid=%u)\n", dev_id, fid);
        return ret;
#endif
    }

    ret = vmngh_register_common_msg_client(dev_id, fid, &hvtsdrv_common_msg_client);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("hvtsdrv dev-%u fid-%u common msg client register failed, ret-%d.\n", dev_id, fid, ret);
        goto register_common_fail;
#endif
    }

    ret = vmngh_vpc_register_client_safety(dev_id, fid, &hvtsdrv_vpc_client);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("hvtsdrv dev-%u fid-%u vpc client register failed, ret-%d.\n", dev_id, fid, ret);
        goto register_vpc_fail;
#endif
    }

    ret = hvtsdrv_dev_logic_cq_init(dev_id, fid);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("hvtsdrv dev-%u fid-%u id logic cq init fail, ret-%d.\n", dev_id, fid, ret);
        goto logic_cq_init_err;
#endif
    }

    tsdrv_set_dev_status(dev_id, fid, TSDRV_DEV_ACTIVE, TSDRV_VIRTUAL_TYPE);

#ifndef TSDRV_UT
    ret = tsdrv_notice_tsagent_dev_create(dev_id, fid);
    if (ret != 0) {
        goto tsagent_ops_fail;
    }
#endif

    ret = hvtsdrv_dev_recycle_wq_init(dev_id, fid);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("dev-%u fid-%u wq init failed, ret-%d.\n", dev_id, fid, ret);
        goto recycle_wq_fail;
#endif
    }

    hvtsdrv_cb_mutex_init(dev_id, fid);

    TSDRV_PRINT_INFO("dev-%u fid-%u host_flag-%d init success.\n", dev_id, fid, tsdrv_get_host_flag(dev_id));
    return 0;
#ifndef TSDRV_UT
recycle_wq_fail:
    tsdrv_notice_tsagent_dev_destroy(dev_id, fid);
tsagent_ops_fail:
    hvtsdrv_dev_logic_cq_uninit(dev_id, fid);
logic_cq_init_err:
    vmngh_vpc_unregister_client(dev_id, fid, &hvtsdrv_vpc_client);
register_vpc_fail:
    vmngh_unregister_common_msg_client(dev_id, fid, &hvtsdrv_common_msg_client);
register_common_fail:
    hvtsdrv_sq_mem_uninit(dev_id, fid, tsid, TSDRV_VIRTUAL_TYPE);
    return ret;
#endif
}

int hvtsdrv_uninit_instance(u32 dev_id, u32 fid)
{
    struct tsdrv_dev_resource *dev_res = NULL;
    enum tsdrv_dev_status status;

    TSDRV_PRINT_INFO("Start hvtsdrv uninit instance.\n");

    dev_res = tsdrv_get_dev_resource(dev_id, fid);
    if (dev_res == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("get dev resource failed, devid(%u) fid(%u).\n", dev_id, fid);
        return -EINVAL;
#endif
    }

    status = (enum tsdrv_dev_status)ka_base_atomic_cmpxchg(&dev_res->status, TSDRV_DEV_ACTIVE,
        TSDRV_DEV_REBOOTING);
    if (status != TSDRV_DEV_ACTIVE) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("hvtsdrv dev-%u fid-%u status-%u not active.\n", dev_id, fid, status);
        return -EINVAL;
#endif
    }

    vmngh_vpc_unregister_client(dev_id, fid, &hvtsdrv_vpc_client);
    vmngh_unregister_common_msg_client(dev_id, fid, &hvtsdrv_common_msg_client);

    hvtsdrv_destroy_cb_sqcq(dev_id, fid);
    hvtsdrv_cb_mutex_destroy(dev_id, fid);

    ka_task_mutex_lock(&dev_res->dev_res_lock);
    if (dev_res->vm_res_wq != NULL) {
        (void)ka_task_queue_work(dev_res->vm_res_wq, &dev_res->vm_res_recycle_work);
    }
    ka_task_mutex_unlock(&dev_res->dev_res_lock);

    return 0;
}

void hvtsdrv_uninit_device(u32 dev_id)
{
    u32 fid;

    for (fid = 0; fid < TSDRV_MAX_FID_NUM; fid++) {
        if (tsdrv_is_pm_fid(fid)) {
            continue;
        }
        hvtsdrv_dev_recycle_wq_uninit(dev_id, fid);
    }

    TSDRV_PRINT_INFO("devid %u uninit success\n", dev_id);
    return;
}
KA_EXPORT_SYMBOL_GPL(hvtsdrv_uninit_device);

static int hvtsdrv_get_aicore_num(u32 udevid, u32 *aicore_num)
{
    u64 bitmap;
    u32 unit_per_bit;
    int ret;

    ret = soc_resmng_dev_get_mia_res(udevid, MIA_AC_AIC, &bitmap, &unit_per_bit);
    if (ret != 0) {
        TSDRV_PRINT_ERR("Get aicore bitmap failed. (udevid=%u; ret=%d)\n", udevid, ret);
        return ret;
    }
    *aicore_num = (u32)ka_base_bitmap_weight((const unsigned long *)&bitmap, 64); /* 64 u64 bitnum */

    return 0;
}

#define HVTSDRV_MIA_NOTIFIER "tsdrv_mia"
static int hvtsdrv_mia_dev_notifier_func(u32 udevid, enum uda_notified_action action)
{
    struct uda_mia_dev_para mia_para;
    u32 fid;
    int ret;

    ret = uda_udevid_to_mia_devid(udevid, &mia_para);
    if (ret != 0) {
        TSDRV_PRINT_ERR("Invalid para. (udevid=%u; action=%d)\n", udevid, action);
        return ret;
    }
    fid = mia_para.sub_devid + 1;

    if (vmng_get_device_split_mode(mia_para.phy_devid) != VMNG_CONTAINER_SPLIT_MODE) {
        devdrv_drv_info("Not container split. (udevid=%u; action=%d)\n", udevid, action);
        return 0;
    }

    if (action == UDA_INIT) {
        u32 aicore_num;
        ret = hvtsdrv_get_aicore_num(udevid, &aicore_num);
        if (ret == 0) {
            u64 num = (u64)aicore_num;
            u32 dtype = ka_base_find_first_bit((const unsigned long *)&num, 32); /* 32 u32 bitnum */
            ret = hvtsdrv_init_mia_dev_instance(mia_para.phy_devid, fid, dtype);
        }
    } else if (action == UDA_UNINIT) {
        ret = hvtsdrv_uninit_mia_dev_instance(mia_para.phy_devid, fid);
    }

    devdrv_drv_info("notifier action. (udevid=%u; action=%d; ret=%d)\n", udevid, action, ret);

    return ret;
}

static int hvtsdrv_mia_dev_agent_notifier_func(u32 udevid, enum uda_notified_action action)
{
    struct uda_mia_dev_para mia_para;
    u32 fid;
    int ret;

    ret = uda_udevid_to_mia_devid(udevid, &mia_para);
    if (ret != 0) {
        TSDRV_PRINT_ERR("Invalid para. (udevid=%u; action=%d)\n", udevid, action);
        return ret;
    }
    fid = mia_para.sub_devid + 1;

    if (action == UDA_INIT) {
        ret = hvtsdrv_init_instance(mia_para.phy_devid, fid);
    } else if (action == UDA_UNINIT) {
        ret = hvtsdrv_uninit_instance(mia_para.phy_devid, fid);
    }

    devdrv_drv_info("notifier action. (udevid=%u; action=%d; ret=%d)\n", udevid, action, ret);

    return ret;
}

int hvtsdrv_notifier_register(void)
{
    struct uda_dev_type type;
    int ret;

    uda_davinci_near_virtual_entity_type_pack(&type);
    ret = uda_notifier_register(HVTSDRV_MIA_NOTIFIER, &type, UDA_PRI2, hvtsdrv_mia_dev_notifier_func);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Register mia dev notifier failed. (ret=%d)\n", ret);
        return ret;
#endif
    }

    uda_davinci_near_virtual_agent_type_pack(&type);
    ret = uda_notifier_register(HVTSDRV_MIA_NOTIFIER, &type, UDA_PRI2, hvtsdrv_mia_dev_agent_notifier_func);
    if (ret != 0) {
#ifndef TSDRV_UT
        uda_davinci_near_virtual_entity_type_pack(&type);
        (void)uda_notifier_unregister(HVTSDRV_MIA_NOTIFIER, &type);
        TSDRV_PRINT_ERR("Register mia dev agent notifier failed. (ret=%d)\n", ret);
        return ret;
#endif
    }

    return 0;
}

static void hvtsdrv_notifier_unregister(void)
{
    struct uda_dev_type type;

    uda_davinci_near_virtual_entity_type_pack(&type);
    (void)uda_notifier_unregister(HVTSDRV_MIA_NOTIFIER, &type);
    uda_davinci_near_virtual_agent_type_pack(&type);
    (void)uda_notifier_unregister(HVTSDRV_MIA_NOTIFIER, &type);
}

struct vmngh_vascend_client hvtsdrv_vascend_client = {
    .type = VMNG_CLIENT_TYPE_TSDRV,
    .get_map_info = hvtsdrv_get_map_info,
    .put_map_info = hvtsdrv_put_map_info,
};

void hvtsdrv_init(void)
{
    int ret;
    ret = vmngh_register_vascend_client(&hvtsdrv_vascend_client);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("hvtsdrv register vmnh vascend client failed, ret = %d\n", ret);
#endif
        return;
    }
    ret = hvtsdrv_notifier_register();
    if (ret != 0) {
        TSDRV_PRINT_ERR("hvtsdrv register vmnh client failed, ret = %d\n", ret);
#ifndef TSDRV_UT
        vmngh_unregister_vascend_client(&hvtsdrv_vascend_client);
#endif
        return;
    }

    TSDRV_PRINT_INFO("hvtsdrv register vmnh client success\n");
    return;
}
KA_EXPORT_SYMBOL_GPL(hvtsdrv_init);

void hvtsdrv_uninit(void)
{
    int ret;

    hvtsdrv_notifier_unregister();

    ret = vmngh_unregister_vascend_client(&hvtsdrv_vascend_client);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("hvtsdrv unregister vmnh vascend client failed, ret = %d\n", ret);
#endif
        return;
    }
    TSDRV_PRINT_INFO("hvtsdrv unregister vmnh client success\n");
    return;
}
KA_EXPORT_SYMBOL_GPL(hvtsdrv_uninit);

#else

void hvtsdrv_init(void)
{
    return;
}
KA_EXPORT_SYMBOL_GPL(hvtsdrv_init);

void hvtsdrv_uninit(void)
{
    return;
}
KA_EXPORT_SYMBOL_GPL(hvtsdrv_uninit);

void hvtsdrv_uninit_device(u32 dev_id)
{
    (void)dev_id;
    return;
}
KA_EXPORT_SYMBOL_GPL(hvtsdrv_uninit_device);

#endif
