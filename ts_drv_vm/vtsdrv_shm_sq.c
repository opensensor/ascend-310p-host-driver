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
#ifdef CFG_FEATURE_RUNTIME_NO_THREAD

#include "ascend_hal_define.h"
#include "devdrv_devinit.h"
#include "tsdrv_log.h"
#include "vtsdrv_init.h"
#include "vtsdrv_proc.h"
#include "shm_sqcq.h"
#include "vtsdrv_shm_sq.h"
#include "ka_ioctl_pub.h"

int vtsdrv_shm_sq_ctx_init(struct tsdrv_ctx *ctx, u32 tsnum)
{
    u32 tsid;

    for (tsid = 0; tsid < tsnum; tsid++) {
        struct shm_sqcq_ts_ctx *shm_ctx = NULL;

        shm_ctx = tsdrv_kzalloc(sizeof(struct shm_sqcq_ts_ctx), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
        if (shm_ctx == NULL) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("Kzalloc shm ctx fail\n");
            goto err_shm_ctx_alloc;
#endif
        }
        ctx->ts_ctx[tsid].shm_sqcq_ctx = shm_ctx;
        ka_task_mutex_init(&shm_ctx->lock);
    }
    return 0;
#ifndef TSDRV_UT
err_shm_ctx_alloc:
    for (tsid = 0; tsid < tsnum; tsid++) {
        if (ctx->ts_ctx[tsid].shm_sqcq_ctx != NULL) {
            struct shm_sqcq_ts_ctx *shm_ctx = NULL;

            shm_ctx = (struct shm_sqcq_ts_ctx *)ctx->ts_ctx[tsid].shm_sqcq_ctx;
            ka_task_mutex_destroy(&shm_ctx->lock);
            tsdrv_kfree(ctx->ts_ctx[tsid].shm_sqcq_ctx);
            ctx->ts_ctx[tsid].shm_sqcq_ctx = NULL;
        }
    }
    return -ENODEV;
#endif
}

void vtsdrv_shm_sq_ctx_exit(struct tsdrv_ctx *ctx, u32 tsnum)
{
    u32 tsid;

    for (tsid = 0; tsid < tsnum; tsid++) {
        struct shm_sqcq_ts_ctx *shm_ctx = NULL;

        shm_ctx = (struct shm_sqcq_ts_ctx *)ctx->ts_ctx[tsid].shm_sqcq_ctx;
        if (shm_ctx != NULL) {
            ka_task_mutex_destroy(&shm_ctx->lock);
            tsdrv_kfree(shm_ctx);
            ctx->ts_ctx[tsid].shm_sqcq_ctx = NULL;
        }
    }
}

phys_addr_t vtsdrv_get_bar_addr(u32 devid)
{
#ifndef TSDRV_UT
    struct vtsdrv_ctrl *tsdrv_ctrl = NULL;

    tsdrv_ctrl = vtsdrv_get_ctrl(devid);
    if (tsdrv_ctrl == NULL) {
        TSDRV_PRINT_ERR("Get tsdrv ctrl fail. (devid=%u)\n", devid);
        return 0;
    }
    return tsdrv_ctrl->sqcq_hwinfo.sqcq_bar_addr;
#endif
}

static bool vtsdrv_shm_alloc_allowed(struct shm_sqcq_ts_ctx *shm_ctx, struct vtsdrv_msg *msg)
{
    if (shm_ctx->shm_sqcq_num != 0) {
        TSDRV_PRINT_INFO("Shm sqcq already allocated, (shm_sqcq_num=%u)\n", shm_ctx->shm_sqcq_num);
        return false;
    }

    return true;
}

int vtsdrv_get_shm_phy_addr(u32 devid, u32 tsid, struct tsdrv_phy_addr_get *info)
{
    struct vtsdrv_ctrl *tsdrv_ctrl = NULL;
    struct vtsdrv_msg msg = {0};
    int ret;

    tsdrv_ctrl = vtsdrv_get_ctrl(devid);
    if (tsdrv_ctrl == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Get vtsdrv_ctrl failed. (devid=%u)\n", devid);
        return -ENODEV;
#endif
    }
    msg.cmd = TSDRV_DEV_PROC;
    msg.sub_cmd = SHM_OFFSET_GET;
    msg.vdev_ctrl.tgid = current->tgid;
    msg.vdev_proc.proc_msg.tsid = tsid;
    ret = vtsdrv_vpc_msg_send(devid, &msg);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Shm memory offset vpc msg send failed. (devid=%u)\n", devid);
        return ret;
#endif
    }

    info->paddr = tsdrv_ctrl->sqcq_hwinfo.sqcq_bar_addr + msg.vdev_proc.proc_msg.shm_offset.offset;
    info->paddr_len = info->len;

    return 0;
}

int vtsdrv_shm_ioctl_sq_alloc(struct tsdrv_ctx *ctx, struct vtsdrv_msg *msg)
{
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    u32 tsid = msg->vdev_ctrl.tsdrv_ioctl.tsid;
    struct shm_sqcq_ts_ctx *shm_ctx = NULL;
    struct tsdrv_mem_map_para map_para;
    int ret;

    msg->cmd = TSDRV_DEV_IOCTRL;
    msg->sub_cmd = _KA_IOC_NR(TSDRV_SHM_SQCQ_ALLOC);
    msg->vdev_ctrl.tgid = current->tgid;

    shm_ctx = (struct shm_sqcq_ts_ctx *)ctx->ts_ctx[tsid].shm_sqcq_ctx;
    if (shm_ctx == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Shm ctx is NULL\n");
        return -ENOMEM;
#endif
    }

    ka_task_mutex_lock(&shm_ctx->lock);
    if (vtsdrv_shm_alloc_allowed(shm_ctx, msg) != true) {
#ifndef TSDRV_UT
        ka_task_mutex_unlock(&shm_ctx->lock);
        return -EFAULT;
#endif
    }
    ret = vtsdrv_vpc_msg_send(devid, msg);
    if (ret != 0) {
#ifndef TSDRV_UT
        ka_task_mutex_unlock(&shm_ctx->lock);
        TSDRV_PRINT_ERR("Shm sqcq alloc vpc msg send failed. (devid=%u)\n", devid);
        return ret;
#endif
    }

    map_para.id = msg->vdev_ctrl.tsdrv_ioctl.shm_sqcq_alloc_para.sq_id;
    map_para.id_type = TSDRV_SQ_ID;
    map_para.sub_type = MEM_MAP_SUBTYPE_SHM;
    map_para.va = msg->vdev_ctrl.tsdrv_ioctl.shm_sqcq_alloc_para.sqMapVaddr;
    map_para.len = msg->vdev_ctrl.tsdrv_ioctl.shm_sqcq_alloc_para.sq_depth *
        msg->vdev_ctrl.tsdrv_ioctl.shm_sqcq_alloc_para.sq_size;
    ret = tsdrv_remap_va(ctx, tsid, &map_para);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Map shm sq fail.\n");
        return -ENODEV;
#endif
    }
    shm_ctx->sq_map_va = map_para.va;
    shm_ctx->sq_map_len = map_para.len;
    shm_ctx->shm_sqcq_num++;
    ka_task_mutex_unlock(&shm_ctx->lock);

    msg->vdev_ctrl.tsdrv_ioctl.shm_sqcq_alloc_para.rdonly = TSRRV_FLAG_SQ_RDONLY;
    return 0;
}

int vtsdrv_shm_ioctl_sq_free(struct tsdrv_ctx *ctx, struct vtsdrv_msg *msg)
{
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    u32 tsid = msg->vdev_ctrl.tsdrv_ioctl.tsid;
    struct shm_sqcq_ts_ctx *shm_ctx = NULL;
    struct tsdrv_mem_unmap_para unmap_para;
    int ret;

    msg->cmd = TSDRV_DEV_IOCTRL;
    msg->sub_cmd = _KA_IOC_NR(TSDRV_SHM_SQCQ_FREE);
    msg->vdev_ctrl.tgid = current->tgid;

    shm_ctx = (struct shm_sqcq_ts_ctx *)ctx->ts_ctx[tsid].shm_sqcq_ctx;
    if (shm_ctx == NULL) {
        TSDRV_PRINT_ERR("Shm ctx is NULL\n");
        return -ENOMEM;
    }
    ka_task_mutex_lock(&shm_ctx->lock);
    unmap_para.va = shm_ctx->sq_map_va;
    unmap_para.len = shm_ctx->sq_map_len;
    ret = tsdrv_unmap_va(ctx, tsid, &unmap_para);
    if (ret != 0) {
        ka_task_mutex_unlock(&shm_ctx->lock);
        TSDRV_PRINT_ERR("UNmap shm sq fail.\n");
        return -ENODEV;
    }
    ret = vtsdrv_vpc_msg_send(devid, msg);
    if (ret != 0) {
        ka_task_mutex_unlock(&shm_ctx->lock);
        TSDRV_PRINT_ERR("vpc msg send failed.\n");
        return ret;
    }
    shm_ctx->shm_sqcq_num--;
    ka_task_mutex_unlock(&shm_ctx->lock);

    return 0;
}
#endif

