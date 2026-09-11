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
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#ifdef AOS_LLVM_BUILD
#include <linux/irqflags.h>
#endif
#include "devdrv_cbsqcq_common.h"
#include "devdrv_cbcq.h"
#include "devdrv_recycle.h"
#include "devdrv_manager_common.h"
#include "devdrv_interface.h"
#include "tsdrv_osal_mm.h"
#include "tsdrv_log.h"
#include "tsdrv_device.h"
#include "tsdrv_drvops.h"
#include "devdrv_common.h"
#include "tsdrv_osal_intr.h"
#include "ka_task_pub.h"
#include "ka_list_pub.h"
#include "ka_memory_pub.h"
#include "ka_system_pub.h"

#define DEVDRV_CBCQ_MAGIC 0x123456U
#define DEVDRV_CBCQ_INVALID_IRQ 0xFFFFFFFF

STATIC struct devdrv_cbcq_data *g_dev_cbcq[TSDRV_MAX_DAVINCI_NUM] = { NULL };

STATIC void devdrv_cbcq_res_init(struct devdrv_cbcq_data *cbcq_data, u32 tsid)
{
    u32 devid = cbcq_data->devid;
    u32 cbcqid;

    ka_task_spin_lock_bh(&cbcq_data->lock[tsid]);
    for (cbcqid = 0; cbcqid < cbcq_data->cbcq_num[tsid]; cbcqid++) {
        cbcq_data->cbcq[tsid][cbcqid].devid = devid;
        cbcq_data->cbcq[tsid][cbcqid].tsid = tsid;
        cbcq_data->cbcq[tsid][cbcqid].tgid = 0;
        cbcq_data->cbcq[tsid][cbcqid].pair_sqid = SYNC_CB_MAX_SQID_NUM;
        cbcq_data->cbcq[tsid][cbcqid].cqid = cbcqid;
        ka_list_add_tail(&cbcq_data->cbcq[tsid][cbcqid].cq_list, &cbcq_data->cq_avail_head[tsid]);
        cbcq_data->cbcq[tsid][cbcqid].paddr = 0;
        cbcq_data->cbcq[tsid][cbcqid].vaddr = NULL;
        cbcq_data->cbcq[tsid][cbcqid].size = 0;
        cbcq_data->cbcq[tsid][cbcqid].depth = 0;
        cbcq_data->cbcq[tsid][cbcqid].allocated = CBSQCQ_UNALLOC;
        cbcq_data->cbcq[tsid][cbcqid].phase = 1;
        cbcq_data->cbcq[tsid][cbcqid].data = CQREPORT_NO_DATA;
        cbcq_data->cbcq[tsid][cbcqid].gid = DEVDRV_CBCQ_MAGIC;
        ka_task_spin_lock_init(&cbcq_data->cbcq[tsid][cbcqid].lock);
    }
    ka_task_spin_unlock_bh(&cbcq_data->lock[tsid]);
}

STATIC void devdrv_cbcq_res_destroy(struct devdrv_cbcq_data *cbcq_data, u32 tsid)
{
    u32 cbcqid;

    ka_task_spin_lock_bh(&cbcq_data->lock[tsid]);
    for (cbcqid = 0; cbcqid < cbcq_data->cbcq_num[tsid]; cbcqid++) {
        cbcq_data->cbcq[tsid][cbcqid].tsid = 0;
        cbcq_data->cbcq[tsid][cbcqid].cqid = 0;
        ka_list_del(&cbcq_data->cbcq[tsid][cbcqid].cq_list);
        cbcq_data->cbcq[tsid][cbcqid].paddr = 0;
        cbcq_data->cbcq[tsid][cbcqid].vaddr = NULL;
        cbcq_data->cbcq[tsid][cbcqid].size = 0;
        cbcq_data->cbcq[tsid][cbcqid].depth = 0;
        cbcq_data->cbcq[tsid][cbcqid].allocated = CBSQCQ_UNALLOC;
        cbcq_data->cbcq[tsid][cbcqid].phase = 0;
    }
    ka_task_spin_unlock_bh(&cbcq_data->lock[tsid]);
}

struct devdrv_cbcq_data *devdrv_cbcq_data_get(u32 devid)
{
    return g_dev_cbcq[devid];
}

STATIC int devdrv_cbcq_queue_mem_init_paddr(struct devdrv_cbcq_data *cbcq_data, struct devdrv_cbcq *cbcq)
{
    size_t size = ka_base_round_up(cbcq->size * cbcq->depth, KA_MM_PAGE_SIZE);
    ka_device_t *dev = tsdrv_get_dev_p(cbcq_data->devid);
    enum tsdrv_env_type env_type = tsdrv_get_env_type();
    switch (env_type) {
        case TSDRV_ENV_ONLINE:
            cbcq->paddr = ka_mm_dma_map_single(dev, cbcq->vaddr, size, KA_DMA_FROM_DEVICE);
            if (ka_mm_dma_mapping_error(dev, cbcq->paddr)) {
                TSDRV_PRINT_ERR("dma map err\n");
                return -ENOMEM;
            }
            break;
        case TSDRV_ENV_OFFLINE:
            cbcq->paddr = ka_mm_virt_to_phys(cbcq->vaddr);
            break;
    }
    return 0;
}

STATIC void devdrv_cbcq_queue_mem_exit_paddr(struct devdrv_cbcq_data *cbcq_data, struct devdrv_cbcq *cbcq)
{
    size_t size = ka_base_round_up(cbcq->size * cbcq->depth, KA_MM_PAGE_SIZE);
    ka_device_t *dev = tsdrv_get_dev_p(cbcq_data->devid);
    enum tsdrv_env_type env_type = tsdrv_get_env_type();
    if (env_type == TSDRV_ENV_ONLINE) {
        ka_mm_dma_unmap_single(dev, cbcq->paddr, size, KA_DMA_FROM_DEVICE);
    }
    cbcq->paddr = 0;
}

int tsdrv_get_cb_cq_mem_phy_addr(u32 devId, u32 tsId, u32 cqId, struct tsdrv_phy_addr_get *info)
{
    struct devdrv_cbcq_data *cbcq_data = devdrv_cbcq_data_get(devId);
    struct devdrv_cbcq *cbcq = NULL;

    if (cqId >= SYNC_CB_MAX_CQID_NUM) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Input pararmeter is error. (cqid=%u)\n", cqId);
        return -EINVAL;
#endif
    }

    cbcq = &cbcq_data->cbcq[tsId][cqId];
    if ((cbcq->tgid != current->tgid) || (cbcq->allocated == 0)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Not self cq. (cqid=%u)\n", cqId);
        return -EINVAL;
#endif
    }

    if ((info->offset + info->len) != KA_MM_PAGE_ALIGN(cbcq->size * cbcq->depth)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Para is error. (offset=%u; len=%u; cq_size=%lu)\n",
            info->offset, info->len, KA_MM_PAGE_ALIGN(cbcq->size * cbcq->depth));
        return -EINVAL;
#endif
    }

    info->paddr = ka_mm_virt_to_phys(cbcq->vaddr);
    info->paddr_len = KA_MM_PAGE_ALIGN(cbcq->size * cbcq->depth);
    return 0;
}

STATIC int devdrv_cbcq_queue_mem_alloc(struct devdrv_cbcq *cbcq)
{
    size_t size = cbcq->size * cbcq->depth;
    enum tsdrv_env_type env_type = tsdrv_get_env_type();

    switch (env_type) {
        case TSDRV_ENV_ONLINE:
            cbcq->vaddr = tsdrv_alloc_pages_exact(size, KA_GFP_KERNEL);
            break;
        case TSDRV_ENV_OFFLINE:
            cbcq->vaddr = tsdrv_alloc_pages_exact_nid(
                devdrv_manager_devid_to_nid(cbcq->devid, (u32)DEVDRV_TS_NODE_DDR_MEM),
                size, __KA_GFP_THISNODE | KA_GFP_HIGHUSER_MOVABLE);
            break;
    }

    if (cbcq->vaddr == NULL) {
        TSDRV_PRINT_ERR("cbcq->vaddr is NULL\n");
        return -ENOMEM;
    }
    return 0;
}

STATIC void devdrv_cbcq_queue_mem_free(struct devdrv_cbcq *cbcq)
{
    if (cbcq->vaddr != NULL) {
        tsdrv_free_pages_exact(cbcq->vaddr, cbcq->size * cbcq->depth);
        cbcq->vaddr = NULL;
        cbcq->map_va = 0;
    }
}

STATIC int devdrv_cbcq_setup_queue_mem(struct devdrv_cbcq_data *cbcq_data, u32 tsid, struct devdrv_cbcq *cbcq)
{
    int ret;

    ret = devdrv_cbcq_queue_mem_alloc(cbcq);
    DRV_CHECK_EXP_ACT(ret != 0, return -ENOMEM, "queue mem alloc fail, ret(%d)\n", ret);

    ret = devdrv_cbcq_queue_mem_init_paddr(cbcq_data, cbcq);
    DRV_CHECK_EXP_ACT(ret != 0, goto err_init_paddr, "queue mem init paddr fail, ret(%d)\n", ret);

    return 0;
err_init_paddr:
    devdrv_cbcq_queue_mem_free(cbcq);
    return -ENOMEM;
}

STATIC void devdrv_cbcq_free_queue_mem(u32 tsid, struct devdrv_cbcq_data *cbcq_data, struct devdrv_cbcq *cbcq)
{
    DRV_CHECK_PTR(cbcq, return, "cbcq is NULL\n");
    DRV_CHECK_PTR(cbcq->ctx, return, "cbcq->ctx is NULL\n");

    devdrv_cbcq_queue_mem_exit_paddr(cbcq_data, cbcq);
    devdrv_cbcq_queue_mem_free(cbcq);
}

STATIC int devdrv_cbcq_setup_queue(struct devdrv_cbcq_data *cbcq_data, u32 tsid, struct devdrv_cbcq *cbcq)
{
    int ret;

    switch (cbcq_data->mem_type[tsid]) {
        case ALLOCATE_MEM:
            ret = devdrv_cbcq_setup_queue_mem(cbcq_data, tsid, cbcq);
            break;
        default:
            TSDRV_PRINT_ERR("Unknown mem_type(%u), devid(%u) tsid(%d)\n", cbcq_data->devid, tsid,
                (int)cbcq_data->mem_type[tsid]);
            ret = -ENODEV;
            break;
    }
    return ret;
}

STATIC void devdrv_cbcq_free_queue(struct devdrv_cbcq_data *cbcq_data, u32 tsid, struct devdrv_cbcq *cbcq)
{
    switch (cbcq_data->mem_type[tsid]) {
        case ALLOCATE_MEM:
            devdrv_cbcq_free_queue_mem(tsid, cbcq_data, cbcq);
            break;
        default:
            break;
    }
}

STATIC int devdrv_cbcq_alloc_chk(u32 devid, u32 tsid, struct devdrv_cbcq *cbcq)
{
    if (cbcq->allocated == CBSQCQ_ALLOC) {
        return -EINVAL;
    }
    if (cbcq->devid != devid || cbcq->tsid != tsid) {
        return -EFAULT;
    }
    if (cbcq->gid != DEVDRV_CBCQ_MAGIC) {
        return -ENODEV;
    }
    if (cbcq->ctx != NULL) {
        return -ENOMEM;
    }
    return 0;
}

STATIC struct devdrv_cbcq *tsdrv_cbcq_get(struct devdrv_cbcq_data *cbcq_data, u32 tsid,
    struct devdrv_cbcq_alloc_para *cbcq_alloc)
{
    struct devdrv_cbcq *cbcq = NULL;
    struct tsdrv_ctx *ctx = NULL;
    u32 devid = cbcq_data->devid;
    int ret;

    ctx = (struct tsdrv_ctx *)cbcq_alloc->ctx;
    ka_task_spin_lock_bh(&cbcq_data->lock[tsid]);
    if (ka_list_empty_careful(&cbcq_data->cq_avail_head[tsid])) {
        ka_task_spin_unlock_bh(&cbcq_data->lock[tsid]);
        TSDRV_PRINT_ERR("cq available head empty, devid=%u tsid=%u\n", devid, tsid);
        return NULL;
    }
    cbcq = ka_list_first_entry(&cbcq_data->cq_avail_head[tsid], struct devdrv_cbcq, cq_list);
    ka_task_spin_lock_bh(&cbcq->lock);
    ret = devdrv_cbcq_alloc_chk(devid, tsid, cbcq);
    if (ret != 0) {
#ifndef TSDRV_UT
        ka_task_spin_unlock_bh(&cbcq->lock);
        ka_task_spin_unlock_bh(&cbcq_data->lock[tsid]);
        TSDRV_PRINT_ERR("Invalid para. (devid=%u; tsid=%u; cqid=%u; allocted=%u; gid=0x%x;)\n",
            cbcq->devid, cbcq->tsid, cbcq->cqid, cbcq->allocated, cbcq->gid);
        return NULL;
#endif
    }
    cbcq->depth = cbcq_alloc->depth;
    cbcq->size = cbcq_alloc->size;
    cbcq->ctx = cbcq_alloc->ctx;
    cbcq->gid = cbcq_alloc->gid;
    cbcq->data = CQREPORT_NO_DATA;
    cbcq->phase = 1;
    cbcq->tgid = ctx->tgid;
    ka_task_spin_unlock_bh(&cbcq->lock);
    ka_list_del(&cbcq->cq_list);
    ka_task_spin_unlock_bh(&cbcq_data->lock[tsid]);

    return cbcq;
}

#ifndef TSDRV_UT
STATIC void tsdrv_cbcq_put(struct devdrv_cbcq_data *cbcq_data, u32 tsid, struct devdrv_cbcq *cbcq)
{
    ka_task_spin_lock_bh(&cbcq_data->lock[tsid]);
    ka_task_spin_lock_bh(&cbcq->lock);
    cbcq->allocated = CBSQCQ_UNALLOC;
    cbcq->depth = 0;
    cbcq->size = 0;
    cbcq->ctx = NULL;
    cbcq->gid = DEVDRV_CBCQ_MAGIC;
    cbcq->phase = 0;
    ka_task_spin_unlock_bh(&cbcq->lock);
    list_add(&cbcq->cq_list, &cbcq_data->cq_avail_head[tsid]);
    ka_task_spin_unlock_bh(&cbcq_data->lock[tsid]);
}
#endif

STATIC int tsdrv_cbcq_init(u32 devid, u32 tsid, struct devdrv_cbcq_data *cbcq_data, struct devdrv_cbcq *cbcq)
{
    // cbcq prepare finished, add cbcq tp alocate list to start check data in tasklet
    ka_task_spin_lock_bh(&cbcq_data->lock[tsid]);
    cbcq->allocated = CBSQCQ_ALLOC;
    list_add(&cbcq->cq_list, &cbcq_data->cq_alloc_head[tsid]);
    ka_task_spin_unlock_bh(&cbcq_data->lock[tsid]);

    return 0;
}

STATIC int devdrv_cbcq_alloc_param_chk(u32 devid, u32 tsid, struct devdrv_cbcq_alloc_para *cbcq_alloc)
{
    if (cbcq_alloc->size < SYNC_CB_MIN_CQE_SIZE || cbcq_alloc->size > SYNC_CB_MAX_CQE_SIZE) {
        TSDRV_PRINT_ERR("invalid cqsize=%u, devid=%u tsid=%u\n", cbcq_alloc->size, devid, tsid);
        return -ENODEV;
    }

    if (cbcq_alloc->depth < SYNC_CB_MIN_CQE_DEPTH || cbcq_alloc->depth > SYNC_CB_MAX_CQE_DEPTH) {
        TSDRV_PRINT_ERR("invalid cqdepth=%u, devid=%u tsid=%u\n", cbcq_alloc->depth, devid, tsid);
        return -EFAULT;
    }

    if (cbcq_alloc->gid >= DEVDRV_CBCQ_MAX_GID) {
        TSDRV_PRINT_ERR("invalid gid=%u, devid=%u tsid=%u\n", cbcq_alloc->gid, devid, tsid);
        return -ENOMEM;
    }
    return 0;
}

int devdrv_cbcq_alloc(u32 devid, u32 tsid, struct devdrv_cbcq_alloc_para *cbcq_alloc)
{
    struct devdrv_cbcq_data *cbcq_data = NULL;
    struct devdrv_cbcq *cbcq = NULL;
    int ret;

    ret = devdrv_cbcq_alloc_param_chk(devid, tsid, cbcq_alloc);
    if (ret != 0) {
        return ret;
    }
    cbcq_data = devdrv_cbcq_data_get(devid);
    if (cbcq_data == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("get cbsq data fail, devid=%u tsid=%u\n", devid, tsid);
        return -ENODEV;
#endif
    }
    cbcq = tsdrv_cbcq_get(cbcq_data, tsid, cbcq_alloc);
    if (cbcq == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("cbcq get fail, devid=%u tsid=%u\n", devid, tsid);
        return -EFAULT;
#endif
    }
    ret = devdrv_cbcq_setup_queue(cbcq_data, tsid, cbcq);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("int cbcq mem init fail, devid=%u tsid=%u\n", devid, tsid);
        goto err_mem_alloc;
#endif
    }
    ret = tsdrv_cbcq_init(devid, tsid, cbcq_data, cbcq);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("cbcq init fail, devid=%u tsid=%u\n", devid, tsid);
        goto err_cbcq_init;
#endif
    }
    cbcq->map_va = cbcq_alloc->cqMapVaddr;
    cbcq_alloc->cqid = cbcq->cqid;
    cbcq_alloc->paddr = cbcq->paddr;
    cbcq_alloc->vaddr = cbcq->vaddr;
    return 0;
#ifndef TSDRV_UT
err_cbcq_init:
    devdrv_cbcq_free_queue(cbcq_data, tsid, cbcq);
err_mem_alloc:
    tsdrv_cbcq_put(cbcq_data, tsid, cbcq);
    return -ENOMEM;
#endif
}

static inline void devdrv_cbcq_wakeup_ctx(struct tsdrv_ctx *ctx, const struct devdrv_cbcq *cbcq)
{
    struct callback_ctx *cb_ctx = (struct callback_ctx *)ctx->ts_ctx[cbcq->tsid].sync_cb_sqcq_ctx;

    cb_ctx->cbcq_wait_flag[cbcq->gid] = 1;
    ka_mb();
    if (ka_task_waitqueue_active(&cb_ctx->cbcq_wait[cbcq->gid])) {
        ka_task_wake_up(&cb_ctx->cbcq_wait[cbcq->gid]);
    }
}

STATIC void devdrv_cbcq_wake_wait_event(u32 tsid, struct devdrv_cbcq_data *cbcq_data,
    struct devdrv_cbcq *cbcq)
{
    struct devdrv_cbcq *cq = NULL;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;
    if (ka_list_empty_careful(&cbcq_data->cq_alloc_head[tsid])) {
        devdrv_cbcq_wakeup_ctx(cbcq->ctx, cbcq);
        return;
    }
    /* find the cb cq with legal data */
    ka_list_for_each_safe(pos, n, &cbcq_data->cq_alloc_head[tsid]) {
        cq = ka_list_entry(pos, struct devdrv_cbcq, cq_list);
        if ((cq->gid == cbcq->gid) && (cq->tgid == cbcq->tgid)) {
            return;
        }
    }
    /* can not find, wake up ioctl cbcq wait */
    devdrv_cbcq_wakeup_ctx(cbcq->ctx, cbcq);
    return;
}

STATIC int devdrv_cbcq_recycle_param_chk(u32 devid, u32 tsid, struct devdrv_cbcq_alloc_para *cbcq_alloc,
    enum cbsqcq_alloc_flag flag)
{
    if (cbcq_alloc->cqid >= SYNC_CB_MAX_CQID_NUM) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid cqid=%u, devid=%u tsid=%u\n", cbcq_alloc->cqid, devid, tsid);
        return -ENOMEM;
#endif
    }
    if (flag != CBSQCQ_UNALLOC && flag != CBSQCQ_ABANDON) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid flag=%d, devid=%u tsid=%u\n", (int)flag, devid, tsid);
        return -EFAULT;
#endif
    }
    return 0;
}

int devdrv_cbcq_recycle(u32 devid, u32 tsid, struct devdrv_cbcq_alloc_para *cbcq_alloc,
    enum cbsqcq_alloc_flag flag)
{
    struct devdrv_cbcq_data *cbcq_data = NULL;
    struct devdrv_cbcq *cbcq = NULL;
    u32 alloc_flag;
    int err;

    err = devdrv_cbcq_recycle_param_chk(devid, tsid, cbcq_alloc, flag);
    if (err != 0) {
        return err;
    }
    cbcq_data = devdrv_cbcq_data_get(devid);
    cbcq = &cbcq_data->cbcq[tsid][cbcq_alloc->cqid];
    /* remove cbcq from alloc list head so tasklet won't check the cbcq data */
    ka_task_spin_lock_bh(&cbcq_data->lock[tsid]);
    alloc_flag = cbcq->allocated;
    if (alloc_flag != CBSQCQ_ALLOC) {
        ka_task_spin_unlock_bh(&cbcq_data->lock[tsid]);
        TSDRV_PRINT_ERR("cqid(%u) wrong status(%u), devid(%u) tsid(%u)\n", cbcq_alloc->cqid, alloc_flag, devid, tsid);
        return -ENODEV;
    }
    cbcq->allocated = (u32)flag;

    ka_list_del(&cbcq->cq_list);
    devdrv_cbcq_wake_wait_event(tsid, cbcq_data, cbcq);
    ka_task_spin_unlock_bh(&cbcq_data->lock[tsid]);

    /* free cq mem and pte */
    devdrv_cbcq_free_queue(cbcq_data, tsid, cbcq);

    ka_task_spin_lock_bh(&cbcq_data->lock[tsid]);
    ka_task_spin_lock_bh(&cbcq->lock);
    cbcq->tgid = 0;
    cbcq->pair_sqid = SYNC_CB_MAX_SQID_NUM;
    cbcq->data = CQREPORT_NO_DATA;
    cbcq->gid = DEVDRV_CBCQ_MAGIC;
    cbcq->ctx = NULL;
    cbcq->depth = 0;
    cbcq->size = 0;
    cbcq->phase = 1;
    ka_task_spin_unlock_bh(&cbcq->lock);
    if (flag == CBSQCQ_UNALLOC) {
        /* free preparation is done, add to available head to make it allocatable */
        ka_list_add(&cbcq->cq_list, &cbcq_data->cq_avail_head[tsid]);
    } else {
        /* abandon preparation is done, add to abandon head to make it abandoned */
        ka_list_add(&cbcq->cq_list, &cbcq_data->cq_abandon_head[tsid]);
    }
    ka_task_spin_unlock_bh(&cbcq_data->lock[tsid]);
    return 0;
}

STATIC ka_irqreturn_t devdrv_cbcq_irq_handler(int irq, void *data)
{
    struct devdrv_cbcq_irq_data *irq_data = (struct devdrv_cbcq_irq_data *)(uintptr_t)data;
    unsigned long flags;

    ka_task_local_irq_save(flags);
    ka_system_tasklet_schedule(&irq_data->tasklet_data);
    ka_task_local_irq_restore(flags);
    return KA_IRQ_HANDLED;
}

STATIC void devdrv_cb_dma_sync(u32 devid, phys_addr_t paddr, size_t size)
{
    enum tsdrv_env_type env_type = tsdrv_get_env_type();
    if (env_type == TSDRV_ENV_ONLINE) {
        ka_device_t *pci_dev = tsdrv_get_dev_p(devid);
        ka_mm_dma_sync_single_for_cpu(pci_dev, paddr, size, KA_DMA_FROM_DEVICE);
    }
}

static inline struct devdrv_cbcq_data *trans_irq_data_to_cq_data(struct devdrv_cbcq_irq_data *irq_data)
{
    u32 tsid = irq_data->tsid;

    return ka_container_of(irq_data, struct devdrv_cbcq_data, irq_data[tsid]);
}

STATIC void devdrv_cbcq_tasklet_handler(unsigned long data)
{
#ifndef TSDRV_UT
    struct devdrv_cbcq_irq_data *irq_data = (struct devdrv_cbcq_irq_data *)(uintptr_t)data;
    struct devdrv_cbcq_data *cbcq_data = NULL;
    ka_list_head_t *pos = NULL, *n = NULL;
    u32 tsid = irq_data->tsid;
    u32 offset;

    cbcq_data = trans_irq_data_to_cq_data(irq_data);
    ka_task_spin_lock_bh(&cbcq_data->lock[tsid]);
    if (ka_list_empty_careful(&cbcq_data->cq_alloc_head[tsid])) {
        ka_task_spin_unlock_bh(&cbcq_data->lock[tsid]);
        return;
    }

    ka_list_empty_careful(pos, n, &cbcq_data->cq_alloc_head[tsid]) {
        struct devdrv_cbcq *cbcq = ka_list_entry(pos, struct devdrv_cbcq, cq_list);
        struct cbcq_report *report = (struct cbcq_report *)cbcq->vaddr;

        ka_task_spin_lock_bh(&cbcq->lock);
        if (report == NULL || cbcq->ctx == NULL) {
            ka_task_spin_unlock_bh(&cbcq->lock);
            continue;
        }
        if (cbcq->allocated != CBSQCQ_ALLOC) {
            ka_task_spin_unlock_bh(&cbcq->lock);
            continue;
        }
        if (cbcq->data == CQREPORT_HAS_DATA) {
            ka_task_spin_unlock_bh(&cbcq->lock);
            devdrv_cbcq_wakeup_ctx((struct tsdrv_ctx *)cbcq->ctx, cbcq);
            continue;
        }

        offset = (u32)cbcq->size * 2; // slot size 32 * 2 = cacheline
        devdrv_cb_dma_sync(cbcq_data->devid, cbcq->paddr + cbcq->cqid * offset, offset);
        if (report->phase == cbcq->phase) {
            cbcq->data = CQREPORT_HAS_DATA;
            // reverse the phase
            cbcq->phase = !cbcq->phase;
            ka_wmb();
            ka_task_spin_unlock_bh(&cbcq->lock);
            devdrv_cbcq_wakeup_ctx((struct tsdrv_ctx *)cbcq->ctx, cbcq);
            continue;
        }
        ka_task_spin_unlock_bh(&cbcq->lock);
    }
    ka_task_spin_unlock_bh(&cbcq_data->lock[tsid]);
#endif
}

STATIC u32 devdrv_cbcq_data_scan(u32 devid, u32 tsid, struct devdrv_report_wait_para *cbcq_wait,
    struct tsdrv_ctx *ctx)
{
#ifndef TSDRV_UT
    struct callback_ctx *cb_ctx = (struct callback_ctx *)ctx->ts_ctx[tsid].sync_cb_sqcq_ctx;
    struct devdrv_cbcq_data *cbcq_data = devdrv_cbcq_data_get(devid);
    ka_list_head_t *pos = NULL, *n = NULL;
    u32 cqnum = 0;
    u32 offset;
    u32 index;

    ka_task_spin_lock_bh(&cbcq_data->lock[tsid]);
    cb_ctx->cbcq_wait_flag[cbcq_wait->gid] = 0;
    if (ka_list_empty_careful(&cbcq_data->cq_alloc_head[tsid])) {
        goto err_alloc_list_chk;
    }

    ka_list_empty_careful(pos, n, &cbcq_data->cq_alloc_head[tsid]) {
        struct devdrv_cbcq *cbcq = ka_list_entry(pos, struct devdrv_cbcq, cq_list);
        if (cbcq->tgid != ctx->tgid || cbcq->gid != cbcq_wait->gid) {
            continue;
        }
        ka_task_spin_lock_bh(&cbcq->lock);
        if (cbcq->data == CQREPORT_HAS_DATA) {
            index = cbcq->cqid >> 6; /* right 6 bits mean divided by 64 */
            offset = cbcq->cqid & (BITS_PER_LONG_LONG - 1);
            cbcq_wait->cbcq_bitmap[index] |= 0x01ULL << offset;
            cbcq->data = CQREPORT_NO_DATA;
            cqnum++;
        }
        ka_task_spin_unlock_bh(&cbcq->lock);
    }
    ka_task_spin_unlock_bh(&cbcq_data->lock[tsid]);
    return cqnum;
err_alloc_list_chk:
    ka_task_spin_unlock_bh(&cbcq_data->lock[tsid]);
    TSDRV_PRINT_WARN("alloc list empty, devid(%u) tsid(%u) grpid(%u)\n", devid, tsid, cbcq_wait->gid);
#endif
    return 0;
}

STATIC int devdrv_cbcq_wait_event(u32 devid, u32 tsid, struct devdrv_report_wait_para *cbcq_wait,
    struct tsdrv_ctx *ctx)
{
    struct callback_ctx *cb_ctx = (struct callback_ctx *)ctx->ts_ctx[tsid].sync_cb_sqcq_ctx;
    unsigned long timeout;
    long ret;
    u32 gid;

    if (cb_ctx == NULL) {
        TSDRV_PRINT_ERR("cb ctx is NULL, devid=%u tsid=%u\n", devid, tsid);
        return -ENODEV;
    }
    gid = cbcq_wait->gid;
    /* wait event without timeout */
    if (cbcq_wait->timeout == -1) {
        ret = (long)ka_task_wait_event_interruptible(cb_ctx->cbcq_wait[gid], cb_ctx->cbcq_wait_flag[gid] == 1);
        if (ret == -ERESTARTSYS) {
            TSDRV_PRINT_WARN("wait event interrupted\n");
        }
        return ret;
    }
    /* wait event with timeout */
    timeout = ka_system_msecs_to_jiffies((u32)cbcq_wait->timeout);
    ret = ka_task_wait_event_interruptible_timeout(cb_ctx->cbcq_wait[gid], cb_ctx->cbcq_wait_flag[gid] == 1, timeout);
    if (ret == 0) {
        /* wait event timeout */
        ret = -ETIMEDOUT;
    } else if (ret == -ERESTARTSYS) {
        /* wait event is interrupted */
        TSDRV_PRINT_WARN("wait event interrupted\n");
        ret = -ERESTARTSYS;
    } else if (ret > 0) {
        /* wait event is awakened */
        ret = 0;
    }
    return ret;
}

int devdrv_cbcq_report_wait(u32 devid, u32 tsid, struct devdrv_report_wait_para *cbcq_wait, struct tsdrv_ctx *ctx)
{
    u32 cqnum;
    int ret;

    ret = devdrv_cbcq_wait_event(devid, tsid, cbcq_wait, ctx);
    if (ret == 0) {
#ifndef TSDRV_UT
        cqnum = devdrv_cbcq_data_scan(devid, tsid, cbcq_wait, ctx);
        if (cqnum != 0) {
            return 0;
        }
        TSDRV_PRINT_WARN("devid=%u tgid=%d pid=%d cqnum=%u gid=%u\n", devid, ctx->tgid, ctx->pid, cqnum,
            cbcq_wait->gid);
        return -ETIMEDOUT;
#endif
    }
    return ret;
}

u32 devdrv_cbcq_get_irq(u32 devid, u32 tsid)
{
    if (g_dev_cbcq[devid] == NULL) {
        return DEVDRV_CBCQ_INVALID_IRQ;
    }

    return g_dev_cbcq[devid]->irq[tsid];
}

STATIC int devdrv_cbcq_irq_init(struct devdrv_cbcq_data *cbcq_data, u32 tsid)
{
    u32 devid = cbcq_data->devid;
    int err;
#ifdef CFG_SOC_PLATFORM_HELPER
    u32 first_ccpu, last_ccpu;
#endif

    err = tsdrv_request_irq(devid, cbcq_data->irq_request[tsid], devdrv_cbcq_irq_handler,  &cbcq_data->irq_data[tsid],
        "devdrv_cbcq");
    if (err != 0) {
        TSDRV_PRINT_ERR("request irq fail, devid=%u tsid=%u\n", devid, tsid);
        return -EFAULT;
    }
#ifdef CFG_SOC_PLATFORM_HELPER
    err = tsdrv_get_cpu_index_range(devid, &first_ccpu, &last_ccpu);
    if (err != 0) {
        TSDRV_PRINT_ERR("Failed to get ctrl cpu range. (devid=%u; ret=%d)\n", devid, err);
        (void)tsdrv_unrequest_irq(devid, cbcq_data->irq_request[tsid], &cbcq_data->irq_data[tsid]);
        return -EINVAL;
    }
    tsdrv_irq_set_affinity_hint(devid, cbcq_data->irq_request[tsid], first_ccpu, last_ccpu);
#endif
    ka_system_tasklet_init(&cbcq_data->irq_data[tsid].tasklet_data, devdrv_cbcq_tasklet_handler,
        (unsigned long)(uintptr_t)&cbcq_data->irq_data[tsid]);
    return 0;
}

STATIC void devdrv_cbcq_irq_exit(struct devdrv_cbcq_data *cbcq_data, u32 tsid)
{
    ka_system_tasklet_kill(&cbcq_data->irq_data[tsid].tasklet_data);
    if (cbcq_data->irq_request[tsid] != 0) {
        (void)tsdrv_unrequest_irq(cbcq_data->devid, cbcq_data->irq_request[tsid], &cbcq_data->irq_data[tsid]);
    }
}

void callback_cq_ctx_init(struct callback_ctx *cb_ctx)
{
    u32 gid;

    for (gid = 0; gid < SYNC_CB_CQ_MAX_GID; gid++) {
        cb_ctx->cbcq_wait_flag[gid] = 0;
        ka_task_init_waitqueue_head(&cb_ctx->cbcq_wait[gid]);
    }
}

void callback_cq_ctx_exit(struct callback_ctx *cb_ctx)
{
}

STATIC int devdrv_cbcq_data_init(struct devdrv_cbcq_data *cbcq_data, u32 tsid,
    struct devdrv_cbcq_init_para *cq_init)
{
    struct tsdrv_cq_hwinfo *cq_hwinfo = NULL;
    u32 devid = cbcq_data->devid;

    cq_hwinfo = tsdrv_get_cq_hwinfo(devid, tsid);
    cbcq_data->irq_data[tsid].tsid = tsid;
    cbcq_data->cbcq_num[tsid] = cq_init->cbcq_num[tsid];
    cbcq_data->irq_num[tsid] = DEVDRV_MAX_CBCQ_IRQ_NUM;
    cbcq_data->irq[tsid] = cq_hwinfo->cq_irq[cq_hwinfo->cq_irq_num - 1];
    cbcq_data->irq_request[tsid] = cq_hwinfo->cq_request_irq[cq_hwinfo->cq_irq_num - 1];
    cbcq_data->mem_type[tsid] = cq_init->mem_type[tsid];
    cbcq_data->cbcq_max_depth[tsid] = SYNC_CB_MAX_CQE_DEPTH;
    cbcq_data->cbcq_max_size[tsid] = SYNC_CB_MAX_CQE_SIZE;
    ka_task_mutex_init(&cbcq_data->cq_mutex[tsid]);
    ka_task_spin_lock_init(&cbcq_data->lock[tsid]);
    KA_INIT_LIST_HEAD(&cbcq_data->cq_avail_head[tsid]);
    KA_INIT_LIST_HEAD(&cbcq_data->cq_alloc_head[tsid]);
    KA_INIT_LIST_HEAD(&cbcq_data->cq_abandon_head[tsid]);
    return 0;
}

STATIC void devdrv_cbcq_data_exit(struct devdrv_cbcq_data *cbcq_data, u32 tsid)
{
    ka_task_mutex_destroy(&cbcq_data->cq_mutex[tsid]);
}

int devdrv_cbcq_init(u32 devid, u32 tsid, struct devdrv_cbcq_init_para *cq_init)
{
    struct devdrv_cbcq_data *cbcq_data = devdrv_cbcq_data_get(devid);
    int err;

    err = devdrv_cbcq_data_init(cbcq_data, tsid, cq_init);
    if (err != 0) {
        return err;
    }
    err = devdrv_cbcq_irq_init(cbcq_data, tsid);
    if (err != 0) {
        goto err_irq_init;
    }
    devdrv_cbcq_res_init(cbcq_data, tsid);
    return 0;
err_irq_init:
    devdrv_cbcq_data_exit(cbcq_data, tsid);
    return -ENODEV;
}

void devdrv_cbcq_exit(u32 devid, u32 tsid)
{
    struct devdrv_cbcq_data *cbcq_data = devdrv_cbcq_data_get(devid);

    if (cbcq_data == NULL) {
        return;
    }

    devdrv_cbcq_irq_exit(cbcq_data, tsid);
    devdrv_cbcq_res_destroy(cbcq_data, tsid);
    devdrv_cbcq_data_exit(cbcq_data, tsid);
}

int devdrv_cbcq_setup(u32 devid)
{
    if (devid >= TSDRV_MAX_DAVINCI_NUM) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("devid is invalid. (devid=%u)\n", devid);
        return -EINVAL;
#endif
    }
    g_dev_cbcq[devid] = (struct devdrv_cbcq_data *)tsdrv_kzalloc(sizeof(struct devdrv_cbcq_data), KA_GFP_KERNEL);
    if (g_dev_cbcq[devid] == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("callback cq alloc failed. (devid=%u; size=%lu)\n",
            devid, sizeof(struct devdrv_cbcq_data));
        return -ENOMEM;
#endif
    }
    g_dev_cbcq[devid]->devid = devid;
    return 0;
}

void devdrv_cbcq_cleanup(u32 devid)
{
    if (devid >= TSDRV_MAX_DAVINCI_NUM) {
        TSDRV_PRINT_ERR("devid is invalid. (devid=%u)\n", devid);
        return;
    }
    if (g_dev_cbcq[devid] != NULL) {
        tsdrv_kfree(g_dev_cbcq[devid]);
        g_dev_cbcq[devid] = NULL;
    }
}
