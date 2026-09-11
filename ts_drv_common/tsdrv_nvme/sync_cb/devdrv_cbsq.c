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
#include <linux/io.h>
#include <linux/vmalloc.h>
#include "devdrv_cbsqcq_common.h"
#include "devdrv_cbsq.h"
#include "devdrv_manager_common.h"
#include "tsdrv_log.h"
#include "tsdrv_device.h"
#include "ka_memory_pub.h"

STATIC struct devdrv_cbsq_data *g_dev_cbsq[TSDRV_MAX_DAVINCI_NUM] = { NULL };

STATIC void devdrv_cbsq_res_init(u32 devid, u32 tsid, struct devdrv_cbsq_data *cbsq_data)
{
    u32 cbsqid;

    TSDRV_PRINT_INFO("cbsq res init start: devid=%u tsid=%u cbsq_num=%u\n", devid, tsid,
        cbsq_data->cbsq_num[tsid]);
    ka_task_mutex_lock(&cbsq_data->sq_mutex[tsid]);
    for (cbsqid = 0; cbsqid < cbsq_data->cbsq_num[tsid]; cbsqid++) {
        cbsq_data->cbsq[tsid][cbsqid].devid = devid;
        cbsq_data->cbsq[tsid][cbsqid].tsid = tsid;
        cbsq_data->cbsq[tsid][cbsqid].tgid = 0;
        cbsq_data->cbsq[tsid][cbsqid].pair_cqid = DEVDRV_CB_CQ_MAX_NUM;
        cbsq_data->cbsq[tsid][cbsqid].sqid = cbsqid;
        ka_list_add_tail(&cbsq_data->cbsq[tsid][cbsqid].sq_list, &cbsq_data->sq_avail_head[tsid]);
        cbsq_data->cbsq[tsid][cbsqid].paddr = 0;
        cbsq_data->cbsq[tsid][cbsqid].vaddr = NULL;
        cbsq_data->cbsq[tsid][cbsqid].size = 0;
        cbsq_data->cbsq[tsid][cbsqid].depth = 0;
        cbsq_data->cbsq[tsid][cbsqid].ctx = NULL;
        ka_base_atomic_set(&cbsq_data->cbsq[tsid][cbsqid].allocated, CBSQCQ_UNALLOC);
    }
    ka_task_mutex_unlock(&cbsq_data->sq_mutex[tsid]);
}

STATIC void devdrv_cbsq_res_destroy(u32 devid, u32 tsid, struct devdrv_cbsq_data *cbsq_data)
{
    u32 cbsqid;

    ka_task_mutex_lock(&cbsq_data->sq_mutex[tsid]);
    for (cbsqid = 0; cbsqid < cbsq_data->cbsq_num[tsid]; cbsqid++) {
        cbsq_data->cbsq[tsid][cbsqid].devid = 0;
        cbsq_data->cbsq[tsid][cbsqid].tsid = 0;
        cbsq_data->cbsq[tsid][cbsqid].sqid = 0;
        ka_list_del(&cbsq_data->cbsq[tsid][cbsqid].sq_list);
        cbsq_data->cbsq[tsid][cbsqid].paddr = 0;
        cbsq_data->cbsq[tsid][cbsqid].vaddr = NULL;
        cbsq_data->cbsq[tsid][cbsqid].size = 0;
        cbsq_data->cbsq[tsid][cbsqid].depth = 0;
        ka_base_atomic_set(&cbsq_data->cbsq[tsid][cbsqid].allocated, CBSQCQ_UNALLOC);
    }
    ka_task_mutex_unlock(&cbsq_data->sq_mutex[tsid]);
}

STATIC int devdrv_cbsq_ioremap_mem(u32 devid, u32 tsid, struct devdrv_cbsq_init_para *sq_init,
    struct devdrv_cbsq_data *cbsq_data)
{
    cbsq_data->vaddr[tsid] = ka_mm_ioremap(sq_init->paddr[tsid], sq_init->p_size[tsid]);
    if (cbsq_data->vaddr[tsid] == NULL) {
        TSDRV_PRINT_ERR("ka_mm_ioremap fail, devid(%u) tsid(%u)\n", devid, tsid);
        return -ENOMEM;
    }
    cbsq_data->paddr[tsid] = sq_init->paddr[tsid];
    cbsq_data->p_size[tsid] = sq_init->p_size[tsid];

    return 0;
}

STATIC void devdrv_cbsq_iounmap_mem(u32 devid, u32 tsid, struct devdrv_cbsq_data *cbsq_data)
{
    if (cbsq_data->vaddr[tsid] != NULL) {
        ka_mm_iounmap(cbsq_data->vaddr[tsid]);
        cbsq_data->vaddr[tsid] = NULL;
    }
    cbsq_data->paddr[tsid] = 0;
    cbsq_data->p_size[tsid] = 0;
}

STATIC int devdrv_cbsq_mem_init(u32 devid, u32 tsid, struct devdrv_cbsq_init_para *sq_init,
    struct devdrv_cbsq_data *cbsq_data)
{
    int ret;

    switch (sq_init->mem_type[tsid]) {
        case RESERVED_MEM:
            ret = devdrv_cbsq_ioremap_mem(devid, tsid, sq_init, cbsq_data);
            break;
        case ALLOCATE_MEM:
#ifndef TSDRV_UT
            TSDRV_PRINT_WARN("allocated mem is not supported yet, devid=%u tsid=%u\n", devid, tsid);
#endif
            ret = -EFAULT;
            break;
        default:
            ret = -ENOMEM;
            break;
    }
    return ret;
}

STATIC void devdrv_cbsq_mem_exit(u32 devid, u32 tsid, struct devdrv_cbsq_data *cbsq_data)
{
    switch (cbsq_data->mem_type[tsid]) {
        case RESERVED_MEM:
            devdrv_cbsq_iounmap_mem(devid, tsid, cbsq_data);
            break;
        case ALLOCATE_MEM:
#ifndef TSDRV_UT
            TSDRV_PRINT_WARN("allocated mem is not supported yet, devid=%u tsid=%u\n", devid, tsid);
#endif
            break;
        default:
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("Unknown mem_type(%d), devid(%u) tsid(%u)\n", (int)cbsq_data->mem_type[tsid],
                devid, tsid);
#endif
            break;
    }
}

STATIC int devdrv_cbsq_data_init(u32 devid, u32 tsid, struct devdrv_cbsq_init_para *sq_init)
{
    struct devdrv_cbsq_data *cbsq_data = devdrv_cbsq_data_get(devid);
    int ret;

    if (cbsq_data == NULL) {
        TSDRV_PRINT_ERR("cbsq_data is NULL, devid=%u tsid=%u\n", devid, tsid);
        return -EINVAL;
    }
    ret = devdrv_cbsq_mem_init(devid, tsid, sq_init, cbsq_data);
    if (ret != 0) {
        TSDRV_PRINT_ERR("cbsq mem init fail, devid(%u)\n", devid);
        return -ENOMEM;
    }
    cbsq_data->cbsq_num[tsid] = sq_init->cbsq_num[tsid];
    cbsq_data->mem_type[tsid] = sq_init->mem_type[tsid];
    cbsq_data->cbsq_max_depth[tsid] = sq_init->depth[tsid];
    cbsq_data->cbsq_max_size[tsid] = sq_init->slot_size[tsid];

    ka_task_mutex_init(&cbsq_data->sq_mutex[tsid]);
    KA_INIT_LIST_HEAD(&cbsq_data->sq_avail_head[tsid]);
    KA_INIT_LIST_HEAD(&cbsq_data->sq_alloc_head[tsid]);
    KA_INIT_LIST_HEAD(&cbsq_data->sq_abandon_head[tsid]);

    devdrv_cbsq_res_init(devid, tsid, cbsq_data);

    return 0;
}

STATIC void devdrv_cbsq_data_exit(u32 devid, u32 tsid)
{
    struct devdrv_cbsq_data *cbsq_data = devdrv_cbsq_data_get(devid);

    if (cbsq_data == NULL) {
        return;
    }

    devdrv_cbsq_res_destroy(devid, tsid, cbsq_data);
    devdrv_cbsq_mem_exit(devid, tsid, cbsq_data);

    cbsq_data->mem_type[tsid] = ALLOCATE_MEM;
    cbsq_data->cbsq_max_depth[tsid] = 0;
    cbsq_data->cbsq_max_size[tsid] = 0;
    ka_task_mutex_destroy(&cbsq_data->sq_mutex[tsid]);
}

struct devdrv_cbsq_data *devdrv_cbsq_data_get(u32 devid)
{
    return g_dev_cbsq[devid];
}

static inline void devdrv_cbsq_iomem_zero_slot(void __iomem *vaddr)
{
    ka_mm_memset_io(vaddr, 0, DEVDRV_CB_SQCQ_MAX_SIZE);
}

STATIC int devdrv_cbsq_mem_alloc(u32 devid, u32 tsid, struct devdrv_cbsq_data *cbsq_data,
    struct devdrv_cbsq *cbsq)
{
    struct tsdrv_sq_hwinfo *sq_hwinfo = NULL;

    switch (cbsq_data->mem_type[tsid]) {
        case RESERVED_MEM:
            sq_hwinfo = tsdrv_get_ts_sq_hwinfo(devid, tsid);
            cbsq->paddr = sq_hwinfo->paddr + DEVDRV_MAX_SQ_NUM * DEVDRV_SQ_SLOT_SIZE * DEVDRV_MAX_SQ_DEPTH +
                cbsq->sqid * DEVDRV_CB_SQCQ_MAX_SIZE * cbsq->depth;
            cbsq->vaddr = cbsq_data->vaddr[tsid] + (u64)cbsq->sqid * DEVDRV_CB_SQCQ_MAX_SIZE * cbsq->depth;
            if (cbsq->vaddr == NULL) {
                TSDRV_PRINT_ERR("sq slot alloc fail, devid(%u) tsid(%u) sqid(%u)\n", devid, tsid, cbsq->sqid);
                return -ENOMEM;
            }

            devdrv_cbsq_iomem_zero_slot(cbsq->vaddr);
            break;
        default:
            TSDRV_PRINT_ERR("Unknown mem_type=%d, devid=%u tsid=%u\n", (int)cbsq_data->mem_type[tsid], devid, tsid);
            return -ENODEV;
    }

    return 0;
}

STATIC void devdrv_cbsq_mem_free(u32 devid, u32 tsid, struct devdrv_cbsq_data *cbsq_data, struct devdrv_cbsq *cbsq)
{
    cbsq->paddr = 0;
    cbsq->vaddr = NULL;
}

STATIC void devdrv_cbsq_db_write(u32 devid, u32 tsid)
{
    struct tsdrv_db_hwinfo_t *db_hwinfo = tsdrv_get_db_hwinfo_t(devid, tsid, TSDRV_SQ_DB);
    void __iomem *doorbell = NULL;
    static int val = 0;

    doorbell = db_hwinfo->dbs + (unsigned long)SYNC_CB_DB_INDEX * db_hwinfo->db_stride;
    val++;
    ka_mm_writel(val, doorbell);
}

struct devdrv_cb_sq_msg {
    uint16_t phase : 1;
    uint16_t SOP : 1;
    uint16_t MOP : 1;
    uint16_t EOP : 1;
    uint16_t reserved1 : 12;
    uint16_t streamID;
    uint16_t taskID;
    uint16_t CQ_id;
    uint16_t CQ_tail;
    uint16_t sequenceId;
    uint32_t reserved2;
};

STATIC int devdrv_cbsq_slot_copy(u32 devid, u32 tsid, struct callback_send_sq_para *sq_para)
{
    struct devdrv_cbsq_data *cbsq_data = devdrv_cbsq_data_get(devid);
    struct devdrv_cb_sq_msg *cbsq_msg = NULL;
    struct devdrv_cbsq *cbsq = NULL;

    cbsq = &cbsq_data->cbsq[tsid][sq_para->sq_id];
    switch (cbsq_data->mem_type[tsid]) {
        case RESERVED_MEM:
            cbsq_msg = (struct devdrv_cb_sq_msg *)sq_para->sq_task;
            cbsq_msg->phase = !cbsq->phase;
            ka_mm_memcpy_toio(cbsq->vaddr, cbsq_msg, TSDRV_SQ_TASK_SIZE);

            cbsq_msg->phase = cbsq->phase;
            ka_mm_memcpy_toio(cbsq->vaddr, cbsq_msg, ka_offsetof(struct devdrv_cb_sq_msg, streamID));

            cbsq->phase = !cbsq->phase;
            break;
        default:
            return -ENODEV;
    }
    return 0;
}

STATIC int devdrv_cbsq_msg_send_param_chk(u32 devid, u32 tsid, struct callback_send_sq_para *sq_para)
{
    struct devdrv_cbsq_data *cbsq_data = devdrv_cbsq_data_get(devid);
    enum cbsqcq_alloc_flag alloc_flag;
    struct devdrv_cbsq *cbsq = NULL;
    struct tsdrv_ctx *ctx = NULL;

    if (sq_para->sq_id >= SYNC_CB_MAX_SQID_NUM) {
        TSDRV_PRINT_ERR("invalid sqid=%u, devid=%u tsid=%u\n", sq_para->sq_id, devid, tsid);
        return -ENOMEM;
    }
    cbsq = &cbsq_data->cbsq[tsid][sq_para->sq_id];
    alloc_flag = ka_base_atomic_read(&cbsq->allocated);

    ctx = tsdrv_dev_proc_ctx_get(devid, TSDRV_PM_FID, current->tgid);
    if (ctx == NULL) {
        TSDRV_PRINT_ERR("invalid ctx, devid=%u tsid=%u tgid=%d\n", devid, tsid, current->tgid);
        return -EINVAL;
    }

    if ((alloc_flag != CBSQCQ_ALLOC) || (cbsq->ctx != ctx)) {
        tsdrv_dev_proc_ctx_put(ctx);
        TSDRV_PRINT_ERR("invalid sq state(%d), or sq(%u) doesn't belong to this ctx, devid=%u tsid=%u\n",
            (int)alloc_flag, sq_para->sq_id, devid, tsid);
        return -ENODEV;
    }
    tsdrv_dev_proc_ctx_put(ctx);

    return 0;
}

int devdrv_cbsq_msg_send(u32 devid, u32 tsid, struct callback_send_sq_para *sq_para)
{
    int err;

    err = devdrv_cbsq_msg_send_param_chk(devid, tsid, sq_para);
    if (err != 0) {
        return err;
    }
    err = devdrv_cbsq_slot_copy(devid, tsid, sq_para);
    if (err != 0) {
        return err;
    }
    devdrv_cbsq_db_write(devid, tsid);
    return 0;
}

STATIC int devdrv_cbsq_alloc_param_chk(u32 devid, u32 tsid, struct devdrv_cbsq_alloc_para *cbsq_alloc)
{
    if (cbsq_alloc->size < SYNC_CB_MIN_SQE_SIZE || cbsq_alloc->size > SYNC_CB_MAX_SQE_SIZE) {
        TSDRV_PRINT_ERR("invalid sq size=%u, devid=%u tsid=%u\n", cbsq_alloc->size, devid, tsid);
        return -ENODEV;
    }
    if (cbsq_alloc->depth < SYNC_CB_MIN_SQE_DEPTH || cbsq_alloc->depth > SYNC_CB_MAX_SQE_DEPTH) {
        TSDRV_PRINT_ERR("invalid sq size=%u, devid=%u tsid=%u\n", cbsq_alloc->size, devid, tsid);
        return -EFAULT;
    }
    return 0;
}

int devdrv_cbsq_alloc(u32 devid, u32 tsid, struct devdrv_cbsq_alloc_para *cbsq_alloc)
{
    struct devdrv_cbsq_data *cbsq_data = NULL;
    struct devdrv_cbsq *cbsq = NULL;
    struct tsdrv_ctx *ctx = NULL;
    int ret;

    ret = devdrv_cbsq_alloc_param_chk(devid, tsid, cbsq_alloc);
    if (ret != 0) {
        return ret;
    }
    ctx = (struct tsdrv_ctx *)cbsq_alloc->ctx;
    cbsq_data = devdrv_cbsq_data_get(devid);
    if (cbsq_data == NULL) {
        TSDRV_PRINT_ERR("get cbsq data fail, devid=%u tsid=%u\n", devid, tsid);
        return -ENOMEM;
    }
    ka_task_mutex_lock(&cbsq_data->sq_mutex[tsid]);
    DRV_CHECK_EXP_ACT(ka_list_empty_careful(&cbsq_data->sq_avail_head[tsid]) != 0, goto err_list_check, "sq list empty ");
    cbsq = ka_list_first_entry(&cbsq_data->sq_avail_head[tsid], struct devdrv_cbsq, sq_list);
    DRV_CHECK_EXP_ACT(cbsq->devid != devid || cbsq->tsid != tsid, goto err_devid_tsid_check,
        "cbsq->devid(%u) cbsq->tsid(%u) ", cbsq->devid, cbsq->tsid);

    cbsq->phase = 1;
    cbsq->size = cbsq_alloc->size;
    cbsq->depth = cbsq_alloc->depth;
    ret = devdrv_cbsq_mem_alloc(devid, tsid, cbsq_data, cbsq);
    DRV_CHECK_EXP_ACT(ret != 0, goto err_mem_alloc, "cbsq mem alloc fail, ret(%d) ", ret);

    cbsq_data->cbsq_num[tsid]--;
    cbsq->tgid = ctx->tgid;
    cbsq->ctx = ctx;
    ka_list_del(&cbsq->sq_list);
    ka_list_add(&cbsq->sq_list, &cbsq_data->sq_alloc_head[tsid]);

    ka_task_mutex_unlock(&cbsq_data->sq_mutex[tsid]);

    ka_base_atomic_set(&cbsq->allocated, CBSQCQ_ALLOC);

    cbsq_alloc->sqid = cbsq->sqid;
    cbsq_alloc->paddr = cbsq->paddr;
    cbsq_alloc->vaddr = cbsq->vaddr;

    return 0;
err_mem_alloc:
err_devid_tsid_check:
err_list_check:
    ka_task_mutex_unlock(&cbsq_data->sq_mutex[tsid]);
    TSDRV_PRINT_ERR("cbsq alloc fail, devid(%u) tsid(%u) tgid(%d) cbsq_num=%u\n", devid, tsid, ctx->tgid,
        cbsq_data->cbsq_num[tsid]);
    return -EINVAL;
}

int devdrv_cbsq_recycle(u32 devid, u32 tsid, struct devdrv_cbsq_alloc_para *cbsq_alloc,
    enum cbsqcq_alloc_flag flag)
{
    struct devdrv_cbsq_data *cbsq_data = NULL;
    enum cbsqcq_alloc_flag alloc_flag;
    struct devdrv_cbsq *cbsq = NULL;
    u32 sqid;

    sqid = cbsq_alloc->sqid;
    DRV_CHECK_EXP_ACT(sqid >= SYNC_CB_MAX_SQID_NUM, return -ENODEV, "invalid sqid=%u=, devid=%u tsid=%u\n",
        sqid, devid, tsid);
    DRV_CHECK_EXP_ACT(flag >= CBSQCQ_CMDMAX, return -ENODEV, "invalid sqid(%u), devid(%u) tsid(%u) flag(%d)\n",
        sqid, devid, tsid, (int)flag);

    cbsq_data = devdrv_cbsq_data_get(devid);
    cbsq = &cbsq_data->cbsq[tsid][sqid];

    ka_task_mutex_lock(&cbsq_data->sq_mutex[tsid]);
    alloc_flag = ka_base_atomic_read(&cbsq->allocated);
    if (alloc_flag != CBSQCQ_ALLOC) {
#ifndef TSDRV_UT
        ka_task_mutex_unlock(&cbsq_data->sq_mutex[tsid]);
        TSDRV_PRINT_ERR("invalid state, sqid(%u) allocated(%d)\n", sqid, (int)alloc_flag);
        return -ENODEV;
#endif
    }
    cbsq->tgid = 0;
    cbsq->pair_cqid = DEVDRV_CB_CQ_MAX_NUM;
    cbsq->ctx = NULL;
    cbsq->phase = 1;
    devdrv_cbsq_mem_free(devid, tsid, cbsq_data, cbsq);
    ka_list_del(&cbsq->sq_list);
    if (flag == CBSQCQ_UNALLOC) {
        /* free preparation is done, add to available head to make it allocatable */
        ka_list_add(&cbsq->sq_list, &cbsq_data->sq_avail_head[tsid]);
        cbsq_data->cbsq_num[tsid]++;
    } else {
        /* abandon preparation is done, add to abandon head to make it abandoned */
        ka_list_add(&cbsq->sq_list, &cbsq_data->sq_abandon_head[tsid]);
    }
    ka_base_atomic_set(&cbsq->allocated, flag);
    ka_task_mutex_unlock(&cbsq_data->sq_mutex[tsid]);

    return 0;
}

int devdrv_cbsq_init(u32 devid, u32 tsid, struct devdrv_cbsq_init_para *sq_init)
{
    int ret;

    ret = devdrv_cbsq_data_init(devid, tsid, sq_init);
    if (ret != 0) {
        TSDRV_PRINT_ERR("cbsq data init fail, devid=%u ret=%d\n", devid, ret);
        return -EFAULT;
    }
    return 0;
}

void devdrv_cbsq_exit(u32 devid, u32 tsid)
{
    devdrv_cbsq_data_exit(devid, tsid);
}

int devdrv_cbsq_setup(u32 devid)
{
    if (devid >= TSDRV_MAX_DAVINCI_NUM) {
        TSDRV_PRINT_ERR("devid is invalid. (devid=%u)\n", devid);
        return -EINVAL;
    }

    g_dev_cbsq[devid] = (struct devdrv_cbsq_data *)tsdrv_kzalloc(sizeof(struct devdrv_cbsq_data), KA_GFP_KERNEL);
    if (g_dev_cbsq[devid] == NULL) {
        TSDRV_PRINT_ERR("callback sq alloc failed. (devid=%u; size=%lu)\n",
            devid, sizeof(struct devdrv_cbsq_data));
        return -ENOMEM;
    }

    return 0;
}

void devdrv_cbsq_cleanup(u32 devid)
{
    if (devid >= TSDRV_MAX_DAVINCI_NUM) {
        TSDRV_PRINT_ERR("devid is invalid. (devid=%u)\n", devid);
        return;
    }

    if (g_dev_cbsq[devid] != NULL) {
        tsdrv_kfree(g_dev_cbsq[devid]);
        g_dev_cbsq[devid] = NULL;
    }
}

