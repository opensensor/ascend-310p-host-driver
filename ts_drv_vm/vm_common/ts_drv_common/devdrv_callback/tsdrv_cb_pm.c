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

#include <securec.h>
#include <linux/slab.h>
#include <linux/io.h>

#include "tsdrv_cb_pm.h"
#include "tsdrv_sync.h"
#include "kernel_cgroup_mem_adapt.h"
#include "devdrv_id.h"
#include "devdrv_cqsq.h"
#include "devdrv_manager_comm.h"
#include "ka_memory_pub.h"

int callback_get_physic_sq(struct callback_phy_sqcq *cb_sqcq)
{
    struct tsdrv_device *tsdrv_dev = tsdrv_get_dev(cb_sqcq->devid);
    struct tsdrv_ts_resource *pm_ts_res = NULL;
    struct devdrv_ts_sq_info *sq_info = NULL;
    struct devdrv_sq_sub_info *sq_sub = NULL;
    struct tsdrv_id_info *sq_id_info = NULL;
    struct tsdrv_ts_resource *ts_res = NULL;
    phys_addr_t info_mem_addr;
    phys_addr_t paddr;
    unsigned long size;

    size = (unsigned long)DEVDRV_SQ_SLOT_SIZE * DEVDRV_MAX_SQ_DEPTH;
    ts_res = &tsdrv_dev->dev_res[cb_sqcq->fid].ts_resource[cb_sqcq->tsid];
    ka_task_spin_lock(&ts_res->id_res[TSDRV_SQ_ID].spinlock);
    if (ts_res->id_res[TSDRV_SQ_ID].id_available_num == 0) {
        ka_task_spin_unlock(&ts_res->id_res[TSDRV_SQ_ID].spinlock);
        (void)tsdrv_msg_alloc_sync_sq(cb_sqcq->devid, ts_res->tsid);
        ka_task_spin_lock(&ts_res->id_res[TSDRV_SQ_ID].spinlock);
        if (ts_res->id_res[TSDRV_SQ_ID].id_available_num == 0) {
            ka_task_spin_unlock(&ts_res->id_res[TSDRV_SQ_ID].spinlock);
            TSDRV_PRINT_ERR("cbsq get failed.\n");
            return -EPERM;
        }
    }

    sq_id_info = ka_list_first_entry(&ts_res->id_res[TSDRV_SQ_ID].id_available_list, struct tsdrv_id_info, list);
    ka_list_del(&sq_id_info->list);
    ts_res->id_res[TSDRV_SQ_ID].id_available_num--;
    ka_task_spin_unlock(&ts_res->id_res[TSDRV_SQ_ID].spinlock);

    cb_sqcq->cb_sq.index = sq_id_info->phy_id;
    cb_sqcq->cb_sq.virtid = sq_id_info->virt_id;

    info_mem_addr = ts_res->mem_info[DEVDRV_INFO_MEM].virt_addr;

    sq_info = devdrv_calc_sq_info(info_mem_addr, sq_id_info->phy_id);
    sq_info->alloc_status = SQCQ_ACTIVE;
    sq_sub = (struct devdrv_sq_sub_info *)sq_info->sq_sub;

    pm_ts_res = &tsdrv_dev->dev_res[TSDRV_PM_FID].ts_resource[cb_sqcq->tsid];
    sq_sub->vaddr = pm_ts_res->mem_info[DEVDRV_SQ_MEM].virt_addr + ((phys_addr_t)(unsigned)sq_id_info->phy_id * size);

    paddr = pm_ts_res->mem_info[DEVDRV_SQ_MEM].phy_addr + ((phys_addr_t)(unsigned)sq_id_info->phy_id * size);
    /* The TS focuses only on the physical address but not on the BAR address */
    cb_sqcq->cb_sq.paddr = paddr;
    if (cb_sqcq->plat_type == DEVDRV_MANAGER_DEVICE_ENV) {
        cb_sqcq->cb_sq.vaddr = (void *)ka_mm_ioremap_wc(paddr, size);
    } else {
        paddr = pm_ts_res->mem_info[DEVDRV_SQ_MEM].bar_addr + ((phys_addr_t)(unsigned)sq_id_info->phy_id * size);
        cb_sqcq->cb_sq.vaddr = (void *)ka_mm_ioremap(paddr, size);
        cb_sqcq->cb_sq.vvaddr = (void *)(uintptr_t)(ts_res->mem_info[DEVDRV_SQ_MEM].virt_addr +
            ((phys_addr_t)(unsigned)sq_id_info->virt_id * size));
    }
    if (cb_sqcq->cb_sq.vaddr == NULL) {
        callback_free_physic_sq(cb_sqcq);
        TSDRV_PRINT_ERR("cbsq ioremap failed\n");
        return -ENOMEM;
    }
    TSDRV_PRINT_DEBUG("Callback phy sq=%u\n", sq_info->index);

    return 0;
}

STATIC s32 callback_device_alloc_cq(struct callback_phy_sqcq *cb_sqcq)
{
    phys_addr_t cb_cq_phys_addr;
    void *cb_cq_vaddr = NULL;
    struct page *page = NULL;
    size_t size;
    s32 ret;

    size = DEVDRV_MAX_CBCQ_SIZE * DEVDRV_MAX_CBCQ_DEPTH;
    if (cb_sqcq->board_type == SOC_PLATFORM_MINIV2) {
        cb_cq_phys_addr = CALLBACK_RESERVED_MEM_ADDR + (cb_sqcq->tsid * size) +
            (cb_sqcq->devid * CHIP_BASEADDR_PA_OFFSET);
        cb_cq_vaddr = (void *)ka_mm_ioremap_wc(cb_cq_phys_addr, size);
        if (cb_cq_vaddr == NULL) {
            TSDRV_PRINT_ERR("ioremap cq addr failed\n");
            return -ENOMEM;
        }

        ret = memset_s(cb_cq_vaddr, size, 0, size);
        if (ret != 0) {
            TSDRV_PRINT_ERR("memset cq memory failed.\n");
            ka_mm_iounmap(cb_cq_vaddr);
            return -EFAULT;
        }
    } else {
        page = ka_alloc_pages_node_ex(devdrv_manager_devid_to_nid(cb_sqcq->devid, (u32)DEVDRV_TS_NODE_DDR_MEM),
            __KA_GFP_ZERO | __KA_GFP_THISNODE | KA_GFP_HIGHUSER_MOVABLE, (u32)ka_mm_get_order(size));
        if (page == NULL) {
            TSDRV_PRINT_ERR("callback cq malloc fail.\n");
            return -ENOMEM;
        }
        cb_cq_vaddr = ka_mm_page_to_virt(page);
        cb_cq_phys_addr = ka_mm_virt_to_phys(cb_cq_vaddr);
    }

    ka_task_spin_lock_bh(&cb_sqcq->cb_cq.lock);
    cb_sqcq->cb_cq.paddr = cb_cq_phys_addr;
    cb_sqcq->cb_cq.vaddr = cb_cq_vaddr;
    cb_sqcq->cb_cq.index = CALLBACK_DEVICECQ_INDEX;
    ka_task_spin_unlock_bh(&cb_sqcq->cb_cq.lock);
    return 0;
}

STATIC s32 callback_host_alloc_cq(struct callback_phy_sqcq *cb_sqcq)
{
    struct tsdrv_device *tsdrv_dev = tsdrv_get_dev(cb_sqcq->devid);
    ka_device_t *dev = tsdrv_get_dev_p(tsdrv_dev->devid);
    void *cb_cq_vaddr = NULL;
    phys_addr_t cb_cq_paddr;
    size_t size;

    size = DEVDRV_MAX_CBCQ_SIZE * DEVDRV_MAX_CBCQ_DEPTH;
    cb_cq_vaddr = tsdrv_kzalloc(size, KA_GFP_ATOMIC);
    if (cb_cq_vaddr == NULL) {
        TSDRV_PRINT_ERR("callback cq malloc fail.\n");
        return -ENOMEM;
    }

    cb_cq_paddr = ka_mm_dma_map_single(dev, cb_cq_vaddr, size, KA_DMA_FROM_DEVICE);
    if (ka_mm_dma_mapping_error(dev, cb_cq_paddr) != 0) {
        tsdrv_kfree(cb_cq_vaddr);
        TSDRV_PRINT_ERR("callback cq dma map fail\n");
        return -ENOMEM;
    }

    ka_task_spin_lock_bh(&cb_sqcq->cb_cq.lock);
    cb_sqcq->cb_cq.index = GET_CALLBACK_CQ_ID(cb_sqcq->fid);
    cb_sqcq->cb_cq.vaddr = cb_cq_vaddr;
    cb_sqcq->cb_cq.paddr = cb_cq_paddr;
    ka_task_spin_unlock_bh(&cb_sqcq->cb_cq.lock);

    return 0;
}

int callback_get_physic_cq(struct callback_phy_sqcq *cb_sqcq)
{
    struct tsdrv_device *tsdrv_dev = tsdrv_get_dev(cb_sqcq->devid);
    struct devdrv_ts_cq_info *cq_info = NULL;
    struct tsdrv_ts_resource *ts_res = NULL;
    phys_addr_t info_mem_addr;
    int ret;

    ts_res = &tsdrv_dev->dev_res[cb_sqcq->fid].ts_resource[cb_sqcq->tsid];
    info_mem_addr = ts_res->mem_info[DEVDRV_INFO_MEM].virt_addr;

    if (cb_sqcq->plat_type == DEVDRV_MANAGER_DEVICE_ENV) {
        ret = callback_device_alloc_cq(cb_sqcq);
        cq_info = devdrv_calc_cq_info(info_mem_addr, CALLBACK_DEVICECQ_INDEX);
    } else {
        ret = callback_host_alloc_cq(cb_sqcq);
        cq_info = devdrv_calc_cq_info(info_mem_addr, GET_CALLBACK_CQ_ID(cb_sqcq->fid));
    }

    if (ret != 0) {
        TSDRV_PRINT_ERR("get cq fail ret=%d.\n", ret);
        return -EPERM;
    }
    cq_info->head = 0;
    cq_info->tail = 0;
    cq_info->vfid = cb_sqcq->fid;
    cq_info->alloc_status = SQCQ_ACTIVE;
    cq_info->phase = DEVDRV_PHASE_STATE_1;
    TSDRV_PRINT_DEBUG("Callback phy cq=%u\n", cq_info->index);

    return 0;
}

void callback_free_physic_sq(struct callback_phy_sqcq *cb_sqcq)
{
    struct tsdrv_device *tsdrv_dev = tsdrv_get_dev(cb_sqcq->devid);
    struct devdrv_ts_sq_info *sq_info = NULL;
    struct tsdrv_ts_resource *ts_res = NULL;
    struct tsdrv_id_info *sq_id_info = NULL;
    phys_addr_t info_mem_addr;

    ts_res = &tsdrv_dev->dev_res[cb_sqcq->fid].ts_resource[cb_sqcq->tsid];
    sq_id_info = devdrv_find_one_id(&ts_res->id_res[TSDRV_SQ_ID], cb_sqcq->cb_sq.index);
    if (sq_id_info == NULL) {
        TSDRV_PRINT_ERR("sq_id(%u) is invalid.\n", cb_sqcq->cb_sq.index);
        return;
    }

    info_mem_addr = ts_res->mem_info[DEVDRV_INFO_MEM].virt_addr;
    sq_info = devdrv_calc_sq_info(info_mem_addr, sq_id_info->phy_id);
    sq_info->alloc_status = SQCQ_INACTIVE;
    sq_info->head = 0;
    sq_info->tail = 0;

    ka_task_spin_lock(&ts_res->id_res[TSDRV_SQ_ID].spinlock);
    ka_list_add(&sq_id_info->list, &ts_res->id_res[TSDRV_SQ_ID].id_available_list);
    ts_res->id_res[TSDRV_SQ_ID].id_available_num++;
    ka_task_spin_unlock(&ts_res->id_res[TSDRV_SQ_ID].spinlock);

    return;
}

void callback_destory_cq(struct callback_phy_sqcq *cb_sqcq)
{
    struct tsdrv_device *tsdrv_dev = tsdrv_get_dev(cb_sqcq->devid);
    size_t size = DEVDRV_MAX_CBCQ_SIZE * DEVDRV_MAX_CBCQ_DEPTH;
    ka_device_t *dev = tsdrv_get_dev_p(tsdrv_dev->devid);
    struct tsdrv_ts_resource *ts_res = NULL;
    struct devdrv_ts_cq_info *cq_info = NULL;
    phys_addr_t info_mem_addr;
    void *cb_cq_vaddr = NULL;
    phys_addr_t cb_cq_paddr;

    ts_res = &tsdrv_dev->dev_res[cb_sqcq->fid].ts_resource[cb_sqcq->tsid];
    info_mem_addr = ts_res->mem_info[DEVDRV_INFO_MEM].virt_addr;

    ka_task_spin_lock_bh(&cb_sqcq->cb_cq.lock);
    cb_cq_vaddr = cb_sqcq->cb_cq.vaddr;
    cb_cq_paddr = cb_sqcq->cb_cq.paddr;
    cb_sqcq->cb_cq.vaddr = NULL;
    cb_sqcq->cb_cq.paddr = 0;

    if (cb_sqcq->plat_type == DEVDRV_MANAGER_DEVICE_ENV) {
        cq_info = devdrv_calc_cq_info(info_mem_addr, CALLBACK_DEVICECQ_INDEX);
    } else {
        cq_info = devdrv_calc_cq_info(info_mem_addr, GET_CALLBACK_CQ_ID(cb_sqcq->fid));
    }
    cq_info->alloc_status = SQCQ_INACTIVE;
    cq_info->head = 0;
    cq_info->tail = 0;
    ka_task_spin_unlock_bh(&cb_sqcq->cb_cq.lock);

    if (cb_sqcq->plat_type == DEVDRV_MANAGER_DEVICE_ENV) {
        if (cb_cq_vaddr != NULL) {
            if (cb_sqcq->board_type == SOC_PLATFORM_MINIV2) {
                ka_mm_iounmap(cb_cq_vaddr);
            } else {
                ka_free_pages_ex((unsigned long)(uintptr_t)cb_cq_vaddr, (u32)ka_mm_get_order(size));
            }
        }
    } else {
        if (cb_cq_paddr != 0) {
            ka_mm_dma_unmap_single(dev, cb_cq_paddr, size, KA_DMA_FROM_DEVICE);
        }
        if (cb_cq_vaddr != NULL) {
            tsdrv_kfree(cb_cq_vaddr);
        }
    }
    return;
}

s32 callback_mbox_send(u32 type, struct callback_phy_sqcq *cb_sqcq)
{
    struct tsdrv_cq_hwinfo *cq_hwinfo = tsdrv_get_cq_hwinfo(cb_sqcq->devid, cb_sqcq->tsid);
    struct tsdrv_mbox_data data = {0};
    struct callback_cqsq_mbox cbcqsq;
    u16 irq_id;
    s32 ret;

    /* add message header */
    cbcqsq.valid = DEVDRV_MAILBOX_MESSAGE_VALID;
    switch (type) {
        case CALLBACK_MBOX_SQCQ_ALLOC: {
            cbcqsq.cmd_type = DEVDRV_MAILBOX_CREATE_CBCQSQ;
            cbcqsq.alloc.sq_addr = cb_sqcq->cb_sq.paddr;
            cbcqsq.alloc.cq_addr = cb_sqcq->cb_cq.paddr;
            cbcqsq.alloc.sq_index = cb_sqcq->cb_sq.index;
            cbcqsq.alloc.cq_index = cb_sqcq->cb_cq.index;
            cbcqsq.alloc.sqe_size = cb_sqcq->cb_sq.size;
            cbcqsq.alloc.cqe_size = cb_sqcq->cb_cq.size;
            cbcqsq.alloc.sq_depth = cb_sqcq->cb_sq.depth;
            cbcqsq.alloc.cq_depth = cb_sqcq->cb_cq.depth;
            cbcqsq.alloc.plat_type = cb_sqcq->plat_type;
            devdrv_calc_cq_irq_id(cb_sqcq->cb_cq.index, cq_hwinfo->cq_irq_num, DEVDRV_MAX_CQ_NUM, &irq_id);
            cbcqsq.alloc.cq_irq = cq_hwinfo->cq_irq[irq_id];
            TSDRV_PRINT_DEBUG("alloc cbsqcq mailbox: cmd_type(0x%x), sq_index(%u), sqe_size(%u), "
                "sq_depth(%u), cq_index(%u), cqe_size(%u), cq_depth(%u)\n", (u32)cbcqsq.cmd_type,
                cbcqsq.alloc.sq_index, (u32)cbcqsq.alloc.sqe_size, (u32)cbcqsq.alloc.sq_depth,
                cbcqsq.alloc.cq_index, (u32)cbcqsq.alloc.cqe_size, (u32)cbcqsq.alloc.cq_depth);
            break;
        }
        case CALLBACK_MBOX_SQCQ_FREE: {
            cbcqsq.cmd_type = DEVDRV_MAILBOX_RELEASE_CBCQSQ;
            cbcqsq.free.sq_index = cb_sqcq->cb_sq.index;
            cbcqsq.free.cq_index = cb_sqcq->cb_cq.index;
            cbcqsq.free.plat_type = cb_sqcq->plat_type;
            TSDRV_PRINT_DEBUG("free cbsqcq mailbox: sqid(%u) cqid(%u)\n", cbcqsq.free.sq_index, cbcqsq.free.cq_index);
            break;
        }
        default: {
            TSDRV_PRINT_ERR("Unknown mbox type(%u)\n", type);
            return -EINVAL;
        }
    }
    cbcqsq.result = 0;

    data.msg = &cbcqsq;
    data.msg_len = sizeof(struct callback_cqsq_mbox);
    ret = tsdrv_mailbox_send_sync(cb_sqcq->devid, cb_sqcq->tsid, &data);
    if (ret != 0) {
        TSDRV_PRINT_ERR("mailbox send fail(%d)\n", ret);
    }

    return ret;
}

s32 callback_mbox_logic_cq(u32 cmd, struct callback_phy_sqcq *phy_sqcq,
    struct callback_logic_cq_sub *sub_cq)
{
    struct callback_logic_cq_mbox cq;
    struct tsdrv_mbox_data data = {0};
    struct tsdrv_cq_hwinfo *cq_hwinfo = tsdrv_get_cq_hwinfo(phy_sqcq->devid, phy_sqcq->tsid);
    s32 ret;
    u16 irq_id;

    if ((cmd != DEVDRV_MAILBOX_CREATE_LOGIC_CBCQ) && (cmd != DEVDRV_MAILBOX_RELEASE_LOGIC_CBCQ)) {
        TSDRV_PRINT_ERR("Invalid mbox cmd(%u)\n", cmd);
        return -EINVAL;
    }

    devdrv_calc_cq_irq_id(phy_sqcq->cb_cq.index, cq_hwinfo->cq_irq_num, DEVDRV_MAX_CQ_NUM, &irq_id);
    cq.valid = DEVDRV_MAILBOX_MESSAGE_VALID;
    cq.cmd_type = cmd;
    cq.vpid = sub_cq->vpid;
    cq.grpid = sub_cq->grpid;
    cq.logic_cqid = sub_cq->cqid;
    cq.phy_cqid = phy_sqcq->cb_cq.index;
    cq.phy_sqid = phy_sqcq->cb_sq.index;
    cq.plat_type = phy_sqcq->plat_type;
    cq.cq_irq = cq_hwinfo->cq_irq[irq_id];
    cq.result = 0;

    data.msg = &cq;
    data.msg_len = sizeof(struct callback_logic_cq_mbox);
    ret = tsdrv_mailbox_send_sync(phy_sqcq->devid, phy_sqcq->tsid, &data);
    if (ret != 0) {
        TSDRV_PRINT_ERR("mailbox send fail(%d)\n", ret);
    }

    return ret;
}

int callback_set_doorbell(struct callback_phy_sqcq *cb_sqcq, u32 idx, u32 val)
{
    u8 *base_addr = cb_sqcq->doorbell_vaddr;
    u32 *doorbell = NULL;
    u8 *addr = NULL;

    addr = base_addr + ((unsigned long)idx * DEVDRV_TS_DOORBELL_STRIDE);
    doorbell = (u32 *)addr;
    ka_mm_writel(val, (void __iomem *)doorbell);
    return 0;
}

void callback_func_lock(struct callback_info *cb_info)
{
    ka_task_spin_lock_bh(&cb_info->phy_sqcq.cb_sq.lock);
}

void callback_func_unlock(struct callback_info *cb_info)
{
    ka_task_spin_unlock_bh(&cb_info->phy_sqcq.cb_sq.lock);
}

void callback_irq_lock(struct callback_phy_sqcq_info *phy_sq)
{
}

void callback_irq_unlock(struct callback_phy_sqcq_info *phy_sq)
{
}

