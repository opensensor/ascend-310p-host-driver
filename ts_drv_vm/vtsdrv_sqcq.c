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

#include "vtsdrv_init.h"
#include "vtsdrv_proc.h"
#include "vtsdrv_sqcq.h"
#include "vtsdrv_cb.h"

#include "tsdrv_device.h"
#include "devdrv_cb.h"
#include "vtsdrv_logic_cq.h"
#include "vtsdrv_shm_sq.h"
#include "devdrv_cqsq.h"
#include "ka_list_pub.h"

struct proc_id_node {
    ka_list_head_t list;
    u32 id;

    unsigned long map_va;
    unsigned long map_len;
};

static struct proc_id_node *vtsdrv_list_get_id(ka_list_head_t *head, u32 id)
{
    struct proc_id_node *node = NULL;

    ka_list_for_each_entry(node, head, list) {
        if (node->id == id) {
            return node;
        }
    }

    return NULL;
}

static int vtsdrv_list_add_id(ka_list_head_t *head, u32 id)
{
    struct proc_id_node *node = NULL;

    node = vtsdrv_list_get_id(head, id);
    if (node != NULL) {
        return 0;
    }

    node = tsdrv_kmalloc(sizeof(struct proc_id_node), KA_GFP_KERNEL);
    if (node == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Mem malloc failed, (id=%u)\n", id);
        return -ENOMEM;
#endif
    }

    node->id = id;
    ka_list_add(&node->list, head);

    return 0;
}

static void vtsdrv_list_del_id(ka_list_head_t *head, u32 id)
{
    struct proc_id_node *node = NULL;

    node = vtsdrv_list_get_id(head, id);
    if (node == NULL) {
        return;
    }

    ka_list_del(&node->list);
    tsdrv_kfree(node);
}

static void vtsdrv_list_clr_id(ka_list_head_t *head)
{
    struct proc_id_node *node = NULL, *tmp = NULL;

    ka_list_for_each_entry_safe(node, tmp, head, list) {
        ka_list_del(&node->list);
        tsdrv_kfree(node);
    }
}

void vtsdrv_proc_remove_all_sqcq(struct tsdrv_ctx *ctx, u32 tsId)
{
    ka_task_mutex_lock(&ctx->mutex_lock);
    vtsdrv_list_clr_id(&ctx->ts_ctx[tsId].id_ctx[TSDRV_SQ_ID].id_list);
    vtsdrv_list_clr_id(&ctx->ts_ctx[tsId].id_ctx[TSDRV_CQ_ID].id_list);
    ka_task_mutex_unlock(&ctx->mutex_lock);
}

int vtsdrv_proc_add_sqcq(struct tsdrv_ctx *ctx, u32 tsId, struct normal_alloc_sqcq_para *para)
{
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    struct vtsdrv_ctrl *tsdrv_ctrl = vtsdrv_get_ctrl(devid);
    int ret;

    if (((para->flag & TSDRV_SQ_REUSE) == 0) && (para->sqId < tsdrv_ctrl->id_capacity[tsId].sq_capacity)) {
        struct proc_id_node *node = NULL;

        ret = vtsdrv_list_add_id(&ctx->ts_ctx[tsId].id_ctx[TSDRV_SQ_ID].id_list, para->sqId);
        if (ret != 0) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("Add sq failed, (sqid=%u)\n", para->sqId);
            return ret;
#endif
        }
        node = vtsdrv_list_get_id(&ctx->ts_ctx[tsId].id_ctx[TSDRV_SQ_ID].id_list, para->sqId);
        node->map_va = para->sqMapVaddr;
        node->map_len = KA_MM_PAGE_ALIGN(para->sqeSize * para->sqeDepth);
    }

    if (((para->flag & TSDRV_CQ_REUSE) == 0) && (para->cqId < tsdrv_ctrl->id_capacity[tsId].cq_capacity)) {
        struct proc_id_node *node = NULL;

        ret = vtsdrv_list_add_id(&ctx->ts_ctx[tsId].id_ctx[TSDRV_CQ_ID].id_list, para->cqId);
        if (ret != 0) {
#ifndef TSDRV_UT
            if (((para->flag & TSDRV_SQ_REUSE) == 0) && (para->sqId < tsdrv_ctrl->id_capacity[tsId].sq_capacity)) {
                vtsdrv_list_del_id(&ctx->ts_ctx[tsId].id_ctx[TSDRV_SQ_ID].id_list, para->sqId);
            }
            TSDRV_PRINT_ERR("Add cq failed, (cqId=%u)\n", para->cqId);
            return ret;
#endif
        }
        node = vtsdrv_list_get_id(&ctx->ts_ctx[tsId].id_ctx[TSDRV_CQ_ID].id_list, para->cqId);
        node->map_va = para->cqMapVaddr;
        node->map_len = KA_MM_PAGE_ALIGN(para->cqeSize * para->cqeDepth);
        ctx->ts_ctx[tsId].vcqid = para->cqId;
    }

    return 0;
}

void vtsdrv_proc_del_sqcq(struct tsdrv_ctx *ctx, u32 tsId, struct normal_free_sqcq_para *para)
{
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    struct vtsdrv_ctrl *tsdrv_ctrl = vtsdrv_get_ctrl(devid);

    if (para->sqId < tsdrv_ctrl->id_capacity[tsId].sq_capacity) {
        vtsdrv_list_del_id(&ctx->ts_ctx[tsId].id_ctx[TSDRV_SQ_ID].id_list, para->sqId);
    }

    if ((para->cqId < tsdrv_ctrl->id_capacity[tsId].cq_capacity) && ((para->flag & TSDRV_CQ_REUSE) == 0)) {
        vtsdrv_list_del_id(&ctx->ts_ctx[tsId].id_ctx[TSDRV_CQ_ID].id_list, para->cqId);
    }
}

#ifndef VM_ST_TEST
/* stub func */
int tsdrv_phy_cq_alloc(struct tsdrv_ctx *ctx, u32 tsid, struct phy_cq_alloc_para *para)
{
    return 0;
}

int tsdrv_phy_cq_free(struct tsdrv_ctx *ctx, u32 tsid, struct phy_cq_free_para *para)
{
    return 0;
}
#endif

phys_addr_t vtsdrv_get_ts_sq_mem(u32 dev_id, u32 ts_id, u32 vsq_id)
{
    struct vtsdrv_ctrl *tsdrv_ctrl = NULL;
    phys_addr_t base_addr;
    phys_addr_t slot_addr;

    tsdrv_ctrl = vtsdrv_get_ctrl(dev_id);
    base_addr = tsdrv_ctrl->sqcq_hwinfo.sqcq_bar_addr;

    /* tsid is not consider */
    slot_addr = base_addr + vsq_id * DEVDRV_SQ_SLOT_SIZE * DEVDRV_MAX_SQ_DEPTH;

    return slot_addr;
}

phys_addr_t vtsdrv_get_ts_cq_mem(u32 dev_id, u32 ts_id, u32 vcq_id)
{
    struct vtsdrv_ctrl *tsdrv_ctrl = NULL;
    phys_addr_t base_addr;
    phys_addr_t slot_addr;
    u32 sq_capacity;

    tsdrv_ctrl = vtsdrv_get_ctrl(dev_id);
    sq_capacity = tsdrv_ctrl->id_capacity[ts_id].sq_capacity;

    base_addr = tsdrv_ctrl->sqcq_hwinfo.sqcq_bar_addr;

    /* tsid is not consider */
    /* callback vcqid is max_vcqid */
    slot_addr = base_addr + sq_capacity * DEVDRV_MAX_SQ_DEPTH * DEVDRV_SQ_SLOT_SIZE +
            vcq_id * KA_MM_PAGE_ALIGN(DEVDRV_MAX_CQ_DEPTH * DEVDRV_CQ_SLOT_SIZE);

    return slot_addr;
}

bool tsdrv_is_sq_belong_to_proc(struct tsdrv_ctx *ctx, u32 tsId, u32 sqId)
{
    struct proc_id_node *node = NULL;

    node = vtsdrv_list_get_id(&ctx->ts_ctx[tsId].id_ctx[TSDRV_SQ_ID].id_list, sqId);
    return (node != NULL);
}

bool tsdrv_is_cq_belong_to_proc(struct tsdrv_ctx *ctx, u32 tsId, u32 cqId)
{
    struct proc_id_node *node = NULL;

    node = vtsdrv_list_get_id(&ctx->ts_ctx[tsId].id_ctx[TSDRV_CQ_ID].id_list, cqId);
    return (node != NULL);
}

int tsdrv_get_vsq_mem_phy_addr(struct tsdrv_ctx *ctx, u32 tsId, u32 sqId, struct tsdrv_phy_addr_get *info)
{
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    struct vtsdrv_ctrl *tsdrv_ctrl = vtsdrv_get_ctrl(devid);

    if (sqId >= tsdrv_ctrl->id_capacity[tsId].sq_capacity) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Input pararmeter is error. (sqId=%u)\n", sqId);
        return -EINVAL;
#endif
    }

    if (!tsdrv_is_sq_belong_to_proc(ctx, tsId, sqId)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Not self sq. (sqId=%u)\n", sqId);
        return -EINVAL;
#endif
    }

    if ((info->offset + info->len) != DEVDRV_SQ_SLOT_SIZE * DEVDRV_MAX_SQ_DEPTH) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Input va len error. (sqId=%u; offset=%u; len=%u)\n", sqId, info->offset, info->len);
        return -EINVAL;
#endif
    }

    info->paddr = vtsdrv_get_ts_sq_mem(devid, tsId, sqId);
    info->paddr_len = DEVDRV_SQ_SLOT_SIZE * DEVDRV_MAX_SQ_DEPTH;
    return 0;
}

int tsdrv_get_sq_mem_phy_addr(struct tsdrv_ctx *ctx, u32 tsId, u32 sqId, u32 sq_type, struct tsdrv_phy_addr_get *info)
{
    if (sq_type == MEM_MAP_SUBTYPE_SHM) {
        return vtsdrv_get_shm_phy_addr(tsdrv_get_devid_by_ctx(ctx), tsId, info);
    } else {
        return tsdrv_get_vsq_mem_phy_addr(ctx, tsId, sqId, info);
    }
}

int tsdrv_get_cq_mem_phy_addr(struct tsdrv_ctx *ctx, u32 tsId, u32 cqId, u32 cq_type, struct tsdrv_phy_addr_get *info)
{
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    struct vtsdrv_ctrl *tsdrv_ctrl = vtsdrv_get_ctrl(devid);

    if (cqId >= tsdrv_ctrl->id_capacity[tsId].cq_capacity) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Input pararmeter is error. (cqid=%u)\n", cqId);
        return -EINVAL;
#endif
    }

    if (!tsdrv_is_cq_belong_to_proc(ctx, tsId, cqId)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Not self cq. (cqid=%u)\n", cqId);
        return -EINVAL;
#endif
    }

    if ((info->offset + info->len) != KA_MM_PAGE_ALIGN(DEVDRV_MAX_CQ_DEPTH * DEVDRV_CQ_SLOT_SIZE)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Input va len error. (cqId=%u; offset=%u; len=%u)\n", cqId, info->offset, info->len);
        return -EINVAL;
#endif
    }

    info->paddr = vtsdrv_get_ts_cq_mem(devid, tsId, cqId);
    info->paddr_len = KA_MM_PAGE_ALIGN(DEVDRV_MAX_CQ_DEPTH * DEVDRV_CQ_SLOT_SIZE);
    return 0;
}

int tsdrv_get_sq_mem_map_prot(struct tsdrv_ctx *ctx, u32 tsId, u32 sq_type, ka_pgprot_t vm_page_prot, ka_pgprot_t *prot)
{
    if (sq_type == MEM_MAP_SUBTYPE_SHM) {
        *prot = vm_page_prot; /* PAGE_READONLY */
    } else {
#ifdef CONFIG_ARM64
        *prot = ka_mm_pgprot_device(vm_page_prot);
#else
        *prot = vm_page_prot;
#endif
    }

    return 0;
}

int tsdrv_get_cq_mem_map_prot(struct tsdrv_ctx *ctx, u32 tsId, struct tsdrv_mem_map_para *map_para,
    ka_pgprot_t vm_page_prot, ka_pgprot_t *prot)
{
#ifdef CONFIG_ARM64
    *prot = ka_mm_pgprot_device(vm_page_prot);
#else
    *prot = vm_page_prot;
#endif

    return 0;
}

static int vtsdrv_normal_sq_mmap(struct tsdrv_ctx *ctx, u32 tsid, u32 sqid, unsigned long va, unsigned long size)
{
    struct tsdrv_mem_map_para map_para;

    map_para.id_type = TSDRV_SQ_ID;
    map_para.sub_type = MEM_MAP_SUBTYPE_NORMAL;
    map_para.id = sqid;
    map_para.va = va;
    map_para.len = size;

    return tsdrv_remap_va(ctx, tsid, &map_para);
}

static int vtsdrv_normal_cq_mmap(struct tsdrv_ctx *ctx, u32 tsid, u32 cqid, unsigned long va, unsigned long size)
{
    struct tsdrv_mem_map_para map_para;

    map_para.id_type = TSDRV_CQ_ID;
    map_para.sub_type = MEM_MAP_SUBTYPE_NORMAL;
    map_para.id = cqid;
    map_para.va = va;
    map_para.len = size;

    return tsdrv_remap_va(ctx, tsid, &map_para);
}

int vtsdrv_sqcq_alloc_id(struct tsdrv_ctx *ctx, struct vtsdrv_msg *msg)
{
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    struct vtsdrv_ctrl *tsdrv_ctrl = vtsdrv_get_ctrl(devid);
    struct tsdrv_mem_unmap_para unmap_para;
    struct normal_free_sqcq_para free_para = {0};
    unsigned long sq_map_va, cq_map_va;
    u32 sqsize, cqsize;
    u32 sqid, cqid;
    u32 flag;
    u32 tsid;
    s32 ret;

    msg->cmd = TSDRV_DEV_IOCTRL;
    msg->sub_cmd = _KA_IOC_NR(TSDRV_SQCQ_ALLOC_ID);
    msg->vdev_ctrl.tgid = current->tgid;
    msg->vdev_ctrl.tsdrv_ioctl.sqcq_alloc_para.res[0] = (u32)KA_MM_PAGE_SIZE;
    ka_task_mutex_lock(&ctx->mutex_lock);
    ret = vtsdrv_vpc_msg_send(devid, msg);
    if (ret != 0) {
        ka_task_mutex_unlock(&ctx->mutex_lock);
        if (ret != -EOPNOTSUPP) {
            TSDRV_PRINT_ERR("vpc msg send failed. (ret=%d)\n", ret);
        }
        return ret;
    }

    tsid = msg->vdev_ctrl.tsdrv_ioctl.tsid;
    sqid = msg->vdev_ctrl.tsdrv_ioctl.sqcq_alloc_para.sqId;
    cqid = msg->vdev_ctrl.tsdrv_ioctl.sqcq_alloc_para.cqId;
    sqsize = msg->vdev_ctrl.tsdrv_ioctl.sqcq_alloc_para.sqeSize * msg->vdev_ctrl.tsdrv_ioctl.sqcq_alloc_para.sqeDepth;
    cqsize = msg->vdev_ctrl.tsdrv_ioctl.sqcq_alloc_para.cqeSize * msg->vdev_ctrl.tsdrv_ioctl.sqcq_alloc_para.cqeDepth;
    sq_map_va = msg->vdev_ctrl.tsdrv_ioctl.sqcq_alloc_para.sqMapVaddr;
    cq_map_va = msg->vdev_ctrl.tsdrv_ioctl.sqcq_alloc_para.cqMapVaddr;
    flag = msg->vdev_ctrl.tsdrv_ioctl.sqcq_alloc_para.flag;

    ret = vtsdrv_proc_add_sqcq(ctx, tsid, &msg->vdev_ctrl.tsdrv_ioctl.sqcq_alloc_para);
    if (ret != 0) {
#ifndef TSDRV_UT
        goto proc_add_sqcq_fail;
#endif
    }

    if ((flag & TSDRV_SQ_REUSE) == 0) {
        ret = vtsdrv_normal_sq_mmap(ctx, tsid, sqid, sq_map_va, sqsize);
        if (ret != 0) {
#ifndef TSDRV_UT
            goto sq_mmap_fail;
#endif
        }
    }
    if (((flag & TSDRV_CQ_REUSE) == 0) && (cqid < tsdrv_ctrl->id_capacity[tsid].cq_capacity)) {
        ret = vtsdrv_normal_cq_mmap(ctx, tsid, cqid, cq_map_va, cqsize);
        if (ret != 0) {
#ifndef TSDRV_UT
            goto cq_mmap_fail;
#endif
        }
    }

    ka_task_mutex_unlock(&ctx->mutex_lock);
    return 0;
#ifndef TSDRV_UT
cq_mmap_fail:
    unmap_para.va = sq_map_va;
    unmap_para.len = sqsize;
    (void)tsdrv_unmap_va(ctx, tsid, &unmap_para);
sq_mmap_fail:
    free_para.cqId = cqid;
    free_para.sqId = sqid;
    free_para.flag = flag;
    vtsdrv_proc_del_sqcq(ctx, tsid, &free_para);
proc_add_sqcq_fail:
    msg->cmd = TSDRV_DEV_IOCTRL;
    msg->sub_cmd = _KA_IOC_NR(TSDRV_SQCQ_FREE_ID);
    msg->vdev_ctrl.tgid = current->tgid;
    (void)vtsdrv_vpc_msg_send(devid, msg);
    ka_task_mutex_unlock(&ctx->mutex_lock);
    return ret;
#endif
}

int vtsdrv_sqcq_free_id(struct tsdrv_ctx *ctx, struct vtsdrv_msg *msg)
{
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    struct proc_id_node *node = NULL;
    struct tsdrv_mem_unmap_para unmap_para;
    u32 tsid = msg->vdev_ctrl.tsdrv_ioctl.tsid;
    s32 ret;

    msg->cmd = TSDRV_DEV_IOCTRL;
    msg->sub_cmd = _KA_IOC_NR(TSDRV_SQCQ_FREE_ID);
    msg->vdev_ctrl.tgid = current->tgid;
    ka_task_mutex_lock(&ctx->mutex_lock);
    ret = vtsdrv_vpc_msg_send(devid, msg);
    if (ret != 0) {
        ka_task_mutex_unlock(&ctx->mutex_lock);
        TSDRV_PRINT_ERR("vpc msg send failed.\n");
        return ret;
    }
    if ((msg->vdev_ctrl.tsdrv_ioctl.sqcq_free_para.flag & TSDRV_SQ_REUSE) == 0) {
        node = vtsdrv_list_get_id(&ctx->ts_ctx[tsid].id_ctx[TSDRV_SQ_ID].id_list,
            msg->vdev_ctrl.tsdrv_ioctl.sqcq_free_para.sqId);
        if (node != NULL) {
            unmap_para.va = node->map_va;
            unmap_para.len = node->map_len;
            (void)tsdrv_unmap_va(ctx, tsid, &unmap_para);
        }
    }
    if ((msg->vdev_ctrl.tsdrv_ioctl.sqcq_free_para.flag & TSDRV_CQ_REUSE) == 0) {
        node = vtsdrv_list_get_id(&ctx->ts_ctx[tsid].id_ctx[TSDRV_CQ_ID].id_list,
            msg->vdev_ctrl.tsdrv_ioctl.sqcq_free_para.cqId);
        if (node != NULL) {
            unmap_para.va = node->map_va;
            unmap_para.len = node->map_len;
            (void)tsdrv_unmap_va(ctx, tsid, &unmap_para);
        }
    }
    vtsdrv_proc_del_sqcq(ctx, msg->vdev_ctrl.tsdrv_ioctl.tsid, &msg->vdev_ctrl.tsdrv_ioctl.sqcq_free_para);
    ka_task_mutex_unlock(&ctx->mutex_lock);

    return 0;
}

#define VTSDRV_WAIT_REPORT_SUCC 1
int vtsdrv_wait_cq_report(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
#ifndef TSDRV_UT
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    int timeout = arg->report_para.timeout;
    unsigned long jiffies_timeout;
    u32 tsid = arg->tsid;
    int ret;

    TSDRV_PRINT_DEBUG("devid(%u), tgid(%u) timeout(%d)(s)\n", devid, ctx->tgid, timeout);
    if ((timeout <= 0) && (timeout != -1)) {
        TSDRV_PRINT_ERR("invalid timeout(%d)(s) devid(%u)\n", timeout, devid);
        return -EINVAL;
    }

    ka_task_spin_lock_irq(&ctx->ts_ctx[tsid].ctx_lock);
    if (ctx->ts_ctx[tsid].cq_report_status == VTSDRV_CQ_HAVE_REPORTS) {
        goto succeed;
    }

    ctx->ts_ctx[tsid].cq_tail_updated = 0;
    if (timeout == -1) {
        wait_event_interruptible_lock_irq(ctx->ts_ctx[tsid].report_wait,
            ctx->ts_ctx[tsid].cq_tail_updated == CQ_HEAD_UPDATE_FLAG, ctx->ts_ctx[tsid].ctx_lock);
        goto succeed;
    } else {
        jiffies_timeout = ka_system_msecs_to_jiffies(timeout);
        ret = ka_task_wait_event_interruptible_lock_irq_timeout(ctx->ts_ctx[tsid].report_wait,
            ctx->ts_ctx[tsid].cq_tail_updated == CQ_HEAD_UPDATE_FLAG, ctx->ts_ctx[tsid].ctx_lock, jiffies_timeout);
        if (ret > 0) {
            goto succeed;
        }
    }
    ka_task_spin_unlock_irq(&ctx->ts_ctx[tsid].ctx_lock);
    TSDRV_PRINT_DEBUG("wait event fail, devid(%u), tgid(%u), ret(%d), timeout(%d %lu)(s)\n",
        devid, ctx->tgid, ret, timeout, jiffies_timeout);
    arg->report_para.timeout = ret;

    return 0;
succeed:
    /* Set timeout to 1 means there is proper cq received */
    arg->report_para.timeout = VTSDRV_WAIT_REPORT_SUCC;
    arg->report_para.cq_tail = ctx->ts_ctx[tsid].cq_tail;
    ctx->ts_ctx[tsid].cq_report_status = 0;
    ka_task_spin_unlock_irq(&ctx->ts_ctx[tsid].ctx_lock);
    TSDRV_PRINT_DEBUG("devid(%u), tgid(%u), vcqid(%u), cq tail(%u)\n", devid, ctx->tgid, ctx->ts_ctx[tsid].vcqid,
        arg->report_para.cq_tail);
#endif
    return 0;
}

int vtsdrv_cq_wake_up(u32 devid, struct hvtsdrv_dev_proc *vdev_proc)
{
    struct vtsdrv_ctrl *vctrl = NULL;
    struct tsdrv_ctx *ctx = NULL;
    u32 tsid = vdev_proc->proc_msg.tsid;
    u32 vcq_id = vdev_proc->proc_msg.vcq_data_para.vcq_id;
    u32 vcq_tail = vdev_proc->proc_msg.vcq_data_para.vcq_tail;
    s64 unique_id = vdev_proc->proc_msg.vcq_data_para.unique_id;
    u32 cq_type = vdev_proc->proc_msg.vcq_data_para.cq_type;

    TSDRV_PRINT_DEBUG("devid(%u), tsid(%u), tgid(%u), vcqid(%u), vcq_tail(%u), type(%u)\n",
        devid, tsid, vdev_proc->tgid, vcq_id, vcq_tail, cq_type);

    if (devid >= TSDRV_MAX_DAVINCI_NUM || tsid >= DEVDRV_MAX_TS_NUM || vcq_tail >= DEVDRV_MAX_CQ_DEPTH) {
        TSDRV_PRINT_ERR("invalid para, devid(%u), tsid(%u), vcqid(%u), vcq_tail(%u)\n", devid, tsid, vcq_id, vcq_tail);
        return -EINVAL;
    }

    vctrl = vtsdrv_get_ctrl(devid);
    if (vcq_id > vctrl->id_capacity[tsid].cq_capacity) {
        TSDRV_PRINT_ERR("invalid para, devid(%u), tsid(%u), vcqid(%u), vcq_tail(%u)\n", devid, tsid, vcq_id, vcq_tail);
        return -EINVAL;
    } else if (vcq_id == vctrl->id_capacity[tsid].cq_capacity) {
        return callback_vcq_handle(devid, vcq_id, vcq_tail);
    }
#ifndef TSDRV_UT
    if (cq_type == LOGIC_SQCQ_TYPE) {
        return logic_cq_vcq_handle(devid, tsid, vcq_id, vcq_tail);
    }

    ctx = tsdrv_dev_proc_ctx_get(devid, TSDRV_PM_FID, vdev_proc->tgid);
    if (ctx == NULL) {
        TSDRV_PRINT_ERR("devid(%u), tgid(%u) find ctx fail.\n", devid, vdev_proc->tgid);
        return -ESRCH;
    }

    ka_task_spin_lock_irq(&ctx->ts_ctx[tsid].ctx_lock);
    if (ctx->ts_ctx[tsid].wait_queue_inited == VTSDRV_WAIT_QUEUE_UNINITED || ctx->ts_ctx[tsid].vcqid != vcq_id ||
        ka_base_atomic64_read((ka_atomic64_t *)&ctx->unique_id) != unique_id) {
        ka_task_spin_unlock_irq(&ctx->ts_ctx[tsid].ctx_lock);
        tsdrv_dev_proc_ctx_put(ctx);
        TSDRV_PRINT_WARN("devid(%u), tgid(%u), unique_id(%lld), vcqid(%u), vcq_id(%u), inited(%u).\n",
            devid, ctx->tgid, unique_id, ctx->ts_ctx[tsid].vcqid, vcq_id, ctx->ts_ctx[tsid].wait_queue_inited);
        return 0;
    }

    if (ctx->ts_ctx[tsid].cq_tail_updated != CQ_HEAD_WAIT_FLAG) {
        ctx->ts_ctx[tsid].cq_tail = vcq_tail;
        ctx->ts_ctx[tsid].cq_report_status = VTSDRV_CQ_HAVE_REPORTS;
        ka_task_spin_unlock_irq(&ctx->ts_ctx[tsid].ctx_lock);
        tsdrv_dev_proc_ctx_put(ctx);
        TSDRV_PRINT_DEBUG("no runtime thread is waiting, devid(%u), tsid(%u), tgid(%u), vcq_tail(%u), updated(%d)\n",
            devid, tsid, vdev_proc->tgid, ctx->ts_ctx[tsid].cq_tail, ctx->ts_ctx[tsid].cq_tail_updated);
        return 0;
    }

    ctx->ts_ctx[tsid].cq_tail = vcq_tail;
    ctx->ts_ctx[tsid].cq_tail_updated = CQ_HEAD_UPDATE_FLAG;
    ka_mb();
    if (ka_task_waitqueue_active(&ctx->ts_ctx[tsid].report_wait)) {
        ka_task_wake_up(&ctx->ts_ctx[tsid].report_wait);
    }
    ka_task_spin_unlock_irq(&ctx->ts_ctx[tsid].ctx_lock);

    TSDRV_PRINT_DEBUG("devid(%u), tsid(%u), tgid(%u), vcq_tail(%u), update(%u)\n",
        devid, tsid, ctx->tgid, ctx->ts_ctx[tsid].cq_tail, ctx->ts_ctx[tsid].cq_tail_updated);

    tsdrv_dev_proc_ctx_put(ctx);
#endif
    return 0;
}
