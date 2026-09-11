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

#include "securec.h"

#include "davinci_api.h"

#include "vtsdrv_proc.h"
#include "vtsdrv_sqcq.h"
#include "vtsdrv_init.h"
#include "vtsdrv_shm_sq.h"

#include "devdrv_cb.h"
#include "devdrv_devinit.h"
#include "devdrv_cbctrl.h"
#include "tsdrv_common.h"
#include "tsdrv_log.h"
#include "tsdrv_device.h"
#include "vtsdrv_shm_sq.h"
#include "logic_cq.h"
#include "tsdrv_nvme.h"
#include "vtsdrv_interface.h"

#define VTSDRV_VPC_MSG_SEND_RETRY_TIME 100

static int vtsdrv_get_dev_info(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    arg->devInfo.virt_type = TSDRV_VIRTUAL_TYPE;

    TSDRV_PRINT_INFO("Get dev info succeed. (virt_type=%d)\n", arg->devInfo.virt_type);
    return 0;
}

STATIC int (* const vtsdrv_ioctl_handlers[TSDRV_MAX_CMD])(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg) = {
    [_KA_IOC_NR(TSDRV_CBSQCQ_ALLOC_ID)] = devdrv_ioctl_cbsqcq_alloc,
    [_KA_IOC_NR(TSDRV_CBSQCQ_FREE_ID)] = devdrv_ioctl_cbsqcq_free,
    [_KA_IOC_NR(TSDRV_CBSQCQ_SEND_TASK)] = devdrv_ioctl_cbsq_send,
    [_KA_IOC_NR(TSDRV_REPORT_WAIT)] = vtsdrv_wait_cq_report,
#ifdef CFG_FEATURE_RUNTIME_NO_THREAD
    [_KA_IOC_NR(TSDRV_LOGIC_CQ_ALLOC)] = logic_ioctl_cq_alloc,
    [_KA_IOC_NR(TSDRV_LOGIC_CQ_FREE)] = logic_ioctl_cq_free,
    [_KA_IOC_NR(TSDRV_LOGIC_CQ_WAIT)] = logic_ioctl_cq_wait,
#endif
    [_KA_IOC_NR(TSDRV_GET_DEV_INFO)] = vtsdrv_get_dev_info,
}; //lint !e167

int (* const vtsdrv_vpc_ioctl_handlers[TSDRV_MAX_CMD])(struct tsdrv_ctx *ctx, struct vtsdrv_msg *msg) = {
    [_KA_IOC_NR(TSDRV_SQCQ_ALLOC_ID)] = vtsdrv_sqcq_alloc_id,
    [_KA_IOC_NR(TSDRV_SQCQ_FREE_ID)] = vtsdrv_sqcq_free_id,
    [_KA_IOC_NR(TSDRV_SHM_SQCQ_ALLOC)] = vtsdrv_shm_ioctl_sq_alloc,
    [_KA_IOC_NR(TSDRV_SHM_SQCQ_FREE)] = vtsdrv_shm_ioctl_sq_free,
}; //lint !e167

s32 vtsdrv_vpc_msg_send(u32 devid, struct vtsdrv_msg *msg)
{
    u32 retry_time = VTSDRV_VPC_MSG_SEND_RETRY_TIME;
    struct vmng_tx_msg_proc_info tx_info;
    s32 ret;

    msg->complete_code = COMP_CODE_NO_RESPONSE;

    tx_info.data = msg;
    tx_info.in_data_len = sizeof(struct vtsdrv_msg);
    tx_info.out_data_len = sizeof(struct vtsdrv_msg);
    tx_info.real_out_len = 0;

    do {
        ret = vpc_msg_send(devid, VPC_VM_FID, VMNG_VPC_TYPE_TSDRV, &tx_info, VPC_DEFAULT_TIMEOUT);
        if (ret != -ENOSPC) {
            break;
        }
#ifndef TSDRV_UT
        ka_system_usleep_range(100, 200);  /* 100us ~ 200us */
        retry_time--;
#endif
    } while (retry_time != 0);

    if ((ret != 0) || (msg->complete_code != COMP_CODE_SUCCESS)) {
        if (msg->complete_code == -EOPNOTSUPP) {
            return -EOPNOTSUPP;
        }

        if (msg->sub_cmd != CALLBACK_DEV_MAILBOX_SEND) {
            TSDRV_PRINT_ERR("Vpc msg send failed. (devid=%u; cmd=%u; sub_cmd=%u; ret=%d; err_code=%d)\n", devid, msg->cmd,
                msg->sub_cmd, ret, msg->complete_code);
        }
        return -EFAULT;
    }

    return 0;
}
EXPORT_SYMBOL_UNRELEASE(vtsdrv_vpc_msg_send);

s32 vtsdrv_common_msg_send(u32 devid, struct vtsdrv_msg *msg)
{
    struct vmng_tx_msg_proc_info tx_info;
    s32 ret;

    msg->complete_code = COMP_CODE_NO_RESPONSE;
    tx_info.data = msg;
    tx_info.in_data_len = sizeof(struct vtsdrv_msg);
    tx_info.out_data_len = sizeof(struct vtsdrv_msg);
    tx_info.real_out_len = 0;
    ret = vmnga_common_msg_send(devid, VMNG_MSG_COMMON_TYPE_TSDRV, &tx_info);
    if ((ret != 0) || (msg->complete_code != COMP_CODE_SUCCESS)) {
        TSDRV_PRINT_ERR("common msg send failed. cmd %u,ret = %d,error_code = %d.\n", msg->cmd,
            ret, msg->complete_code);
        return -EFAULT;
    }
    return 0;
}
EXPORT_SYMBOL_UNRELEASE(vtsdrv_common_msg_send);

long vtsdrv_ioctl(ka_file_t *file, u32 cmd, unsigned long arg)
{
    struct tsdrv_ctx *ctx = (struct tsdrv_ctx *)file->private_data;
    void __user *uarg = (void __user *)(uintptr_t)arg;
    struct vtsdrv_msg msg;
    bool flag = false;
    u32 devid;
    s32 ret;

    flag = (_KA_IOC_TYPE(cmd) != TSDRV_ID_MAGIC) || (_KA_IOC_NR(cmd) >= TSDRV_MAX_CMD) || (uarg == NULL) || (ctx == NULL);
    if (flag != 0) {
        TSDRV_PRINT_ERR("Invalid cmd. (cmd=%u)\n", _KA_IOC_NR(cmd));
        return -EINVAL;
    }

    if ((_KA_IOC_NR(cmd)) == (_KA_IOC_NR(TSDRV_CBSQCQ_WAIT_ID))) {
        return devdrv_ioctl_cbcq_wait(ctx, uarg);
    }

    if (_KA_IOC_DIR(cmd) & _KA_IOC_WRITE) {
        if (ka_base_copy_from_user(&msg.vdev_ctrl.tsdrv_ioctl, uarg, sizeof(struct devdrv_ioctl_arg)) != 0) {
            TSDRV_PRINT_ERR("copy from user fail cmd(%u).\n", _KA_IOC_NR(cmd));
            return -ENOMEM;
        }
    }

    devid = tsdrv_get_devid_by_ctx(ctx);

    if (msg.vdev_ctrl.tsdrv_ioctl.tsid >= DEVDRV_MAX_TS_NUM) {
        TSDRV_PRINT_ERR("invalid para, tsid(%u).\n", msg.vdev_ctrl.tsdrv_ioctl.tsid);
        return -EINVAL;
    }

    if (vtsdrv_ioctl_handlers[_KA_IOC_NR(cmd)] != NULL) {
        ret = vtsdrv_ioctl_handlers[_KA_IOC_NR(cmd)](ctx, &msg.vdev_ctrl.tsdrv_ioctl);
        if (ret != 0) {
            return ret;
        }
    } else if (vtsdrv_vpc_ioctl_handlers[_KA_IOC_NR(cmd)] != NULL) {
        ret = vtsdrv_vpc_ioctl_handlers[_KA_IOC_NR(cmd)](ctx, &msg);
        if (ret != 0) {
            return ret;
        }
    } else { /* cmd which just needs vpc proc */
        msg.cmd = TSDRV_DEV_IOCTRL;
        msg.sub_cmd = _KA_IOC_NR(cmd);
        msg.vdev_ctrl.tgid = current->tgid;
        ret = vtsdrv_vpc_msg_send(devid, &msg);
        if (ret != 0) {
            TSDRV_PRINT_ERR("msg send fail cmd(%u).\n", _KA_IOC_NR(cmd));
            goto out;
        }
    }
out:
    if (_KA_IOC_DIR(cmd) & _KA_IOC_READ) {
        if (ka_base_copy_to_user(uarg, &msg.vdev_ctrl.tsdrv_ioctl, sizeof(struct devdrv_ioctl_arg))) {
            TSDRV_PRINT_ERR("copy to user fail cmd(%u).\n", _KA_IOC_NR(cmd));
            return -EFAULT;
        }
    }

    return ret;
}

s32 vtsdrv_open(ka_inode_t *node, ka_file_t *file)
{
    struct vtsdrv_ctrl *vctrl = NULL;
    u32 tsnum = DEVDRV_MAX_TS_NUM;
    struct tsdrv_ctx *ctx = NULL;
    struct vtsdrv_msg msg;
    u32 devid, tsid;
    s32 ret;

    devid = drv_davinci_get_device_id(file);
    if (devid >= TSDRV_MAX_DAVINCI_NUM) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid parameter, device(%u)\n.", devid);
#endif
        return -ENODEV;
    }

    ctx = tsdrv_dev_ctx_get(devid, TSDRV_PM_FID, current->tgid);
    if (ctx == NULL) {
        return -EFAULT;
    }

    vctrl = vtsdrv_get_ctrl(devid);
    ctx->unique_id = ka_base_atomic64_inc_return((ka_atomic64_t *)&vctrl->unique_num);
    ret = vtsdrv_shm_sq_ctx_init(ctx, tsnum);
    if (ret != 0) {
        tsdrv_dev_ctx_put(ctx);
        return -ENODEV;
    }
    msg.cmd = TSDRV_DEV_OPEN;
    msg.vdev_open.tgid = ctx->tgid;
    msg.vdev_open.unique_id = ctx->unique_id;
    ret = vtsdrv_common_msg_send(devid, &msg);
    if (ret != 0) {
#ifndef TSDRV_UT
        vtsdrv_shm_sq_ctx_exit(ctx, tsnum);
        tsdrv_dev_ctx_put(ctx);
        return ret;
#endif
    }

    for (tsid = 0; tsid < DEVDRV_MAX_TS_NUM; tsid++) {
        ka_task_spin_lock_init(&ctx->ts_ctx[tsid].ctx_lock);
        ka_task_init_waitqueue_head(&ctx->ts_ctx[tsid].report_wait);
        ctx->ts_ctx[tsid].wait_queue_inited = VTSDRV_WAIT_QUEUE_INITED;
        ctx->ts_ctx[tsid].cq_report_status = 0;
        ctx->ts_ctx[tsid].vcqid = CQ_UMAX;
        ctx->ts_ctx[tsid].cq_tail_updated = CQ_HEAD_UPDATE_FLAG;
    }
    (void)logic_sqcq_ctx_init(ctx, tsnum);
    callback_ctx_init(ctx, tsnum);
    tsdrv_set_ctx_run(ctx);
    file->private_data = ctx;
    return 0;
}

static int  vtsdrv_wait_recycle_finish(u32 devid, struct hvtsdrv_dev_proc *vdev_proc)
{
    struct tsdrv_ctx *ctx = NULL;
    ka_pid_t tgid = vdev_proc->tgid;

    ctx = tsdrv_dev_proc_ctx_get(devid, TSDRV_PM_FID, tgid);
    if (ctx == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Faild to find tsdrv ctx. devid[%u], tgid[%d].\n", devid, tgid);
        return -EINVAL;
#endif
    }

    if (ctx->unique_id != vdev_proc->proc_msg.rec_data_para.unique_id) {
#ifndef TSDRV_UT
        tsdrv_dev_proc_ctx_put(ctx);
        TSDRV_PRINT_ERR("unique_id is not same. devid[%u], tgid[%d], unique_id[%lld].\n",
            devid, tgid, ctx->unique_id);
        return -EINVAL;
#endif
    }

    ctx->vm_recycle_flag = 1;
    ka_mb();

    if (ka_task_waitqueue_active(&ctx->vm_recycle_work) != 0) {
        TSDRV_PRINT_INFO("recv recycle finish vpc msg. devid[%u], tgid[%d]\n",
            devid, tgid);
        ka_task_wake_up(&ctx->vm_recycle_work);
    }
    tsdrv_set_ctx_release(ctx);
    tsdrv_dev_proc_ctx_put(ctx);

    return 0;
}

#ifdef CFG_FEATURE_VM_GRACEFUL_RELEASE
#define WAIT_TIME 305
void vtsdrv_wait_pm_recycle_exit(u32 devid, ka_pid_t tgid)
{
    struct tsdrv_ctx *ctx = NULL;
    unsigned long timeout;

    ctx = tsdrv_dev_proc_ctx_get(devid, TSDRV_PM_FID, tgid);
    if (unlikely(ctx == NULL)) {
        TSDRV_PRINT_ERR("ctx is NULL, maybe the release completed.\n");
        return;
    }

    timeout = jiffies + (WAIT_TIME * KA_HZ); // recycle resources in 300 s + 5 s
    ka_task_wait_event_interruptible_lock_irq_timeout(ctx->vm_recycle_work,
        ctx->vm_recycle_flag == 1, ctx->ctx_lock, timeout);
    tsdrv_dev_proc_ctx_put(ctx);
}
KA_EXPORT_SYMBOL_GPL(vtsdrv_wait_pm_recycle_exit);
#endif

int vtsdrv_release_prepare(ka_file_t *file_op, unsigned long mode)
{
    struct tsdrv_ctx *ctx = file_op->private_data;
    u32 devid;
    s32 ret;

    if ((mode != NOTIFY_MODE_RELEASE_PREPARE) || (ctx == NULL)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid mode(%lu) or ctx is null.\n", mode);
        return -ENOMEM;
#endif
    }

    devid = tsdrv_get_devid_by_ctx(ctx);
    if (devid >= TSDRV_MAX_DAVINCI_NUM) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid devid=%u\n", devid);
        return -ENODEV;
#endif
    }

    ret = tsdrv_set_ctx_releasing(ctx);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("set ctx releasing fail, devid=%u\n", devid);
        return ret;
#endif
    }
    ka_base_atomic_set(&ctx->status, TSDRV_CTX_RECYCLE);

    TSDRV_PRINT_DEBUG("[devid=%u]tsdrv end release prepare\n", devid);
    return 0;
}

s32 vtsdrv_release(ka_inode_t *node, ka_file_t *file)
{
    struct tsdrv_ctx *ctx = file->private_data;
    u32 tsnum = DEVDRV_MAX_TS_NUM;
    struct vtsdrv_msg msg;
    u32 tsid, devid;
    s32 ret;

    if (ctx == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("ctx is null.\n");
#endif
        return -EINVAL;
    }

    devid = tsdrv_get_devid_by_ctx(ctx);
    if (devid >= TSDRV_MAX_DAVINCI_NUM) {
        TSDRV_PRINT_ERR("devid(%u) invalid.\n", devid);
        return -ENODEV;
    }

    vtsdrv_shm_sq_ctx_exit(ctx, tsnum);
    TSDRV_PRINT_DEBUG("Debug. (devid=%u; tgid=%d)\n", devid, ctx->tgid);

    for (tsid = 0; tsid < DEVDRV_MAX_TS_NUM; tsid++) {
        vtsdrv_proc_remove_all_sqcq(ctx, tsid);
        ka_task_spin_lock_irq(&ctx->ts_ctx[tsid].ctx_lock);
        ctx->ts_ctx[tsid].cq_report_status = 0;
        ctx->ts_ctx[tsid].vcqid = CQ_UMAX;
        ctx->ts_ctx[tsid].cq_tail_updated = CQ_HEAD_UPDATE_FLAG;
        ctx->ts_ctx[tsid].wait_queue_inited = VTSDRV_WAIT_QUEUE_UNINITED;
        ka_task_spin_unlock_irq(&ctx->ts_ctx[tsid].ctx_lock);
    }
    callback_recycle_logic_cq(devid, TSDRV_PM_FID, 0, ctx);
    (void)logic_sqcq_ctx_exit(ctx, tsnum);

    /* notify PM to free resource */
    msg.cmd = TSDRV_DEV_RELEASE;
    msg.vdev_release.tgid = ctx->tgid;
    ret = vtsdrv_common_msg_send(devid, &msg);
    if (ret != 0) {
        TSDRV_PRINT_ERR("devid(%u) notify pm file close failure.\n", devid);
    }
    file->private_data = NULL;
    return 0;
}

static int vtsdrv_mmap_param_check(const ka_file_t *filep, const struct vm_area_struct *vma)
{
    if (filep == NULL) {
        TSDRV_PRINT_ERR("filep check failed\n");
        return -ENODEV;
    }

    if (filep->private_data == NULL) {
        TSDRV_PRINT_ERR("filep->private_data is NULL\n");
        return -EFAULT;
    }

    return 0;
}
#ifndef TSDRV_UT
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 3, 0)
static int vtsdrv_mremap(struct vm_area_struct * area)
{
    return -ENOTSUPP;
}
#endif
#endif

static struct vm_operations_struct vtsdrv_vm_ops = {
#ifndef TSDRV_UT
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 3, 0)
    .mremap = vtsdrv_mremap,
#endif
#endif
};

int vtsdrv_mmap(ka_file_t *filep, struct vm_area_struct *vma)
{
    struct tsdrv_ctx *ctx = NULL;
    u32 devid;
    int err;

    err = vtsdrv_mmap_param_check(filep, vma);
    if (err != 0) {
        TSDRV_PRINT_ERR("mmap param check failed, err = %d\n", err);
        return -EINVAL;
    }

    ctx = filep->private_data;
    devid = tsdrv_get_devid_by_ctx(ctx);
    vma->vm_ops = &vtsdrv_vm_ops;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
    ka_mm_set_vm_flags(vma, KA_VM_LOCKED | KA_VM_DONTEXPAND | KA_VM_PFNMAP| KA_VM_DONTDUMP | KA_VM_DONTCOPY | KA_VM_IO);
    if (!(vma->vm_flags & KA_VM_WRITE)) {
        vm_flags_clear(vma, VM_MAYWRITE);
    }
#else
    vma->vm_flags |= KA_VM_LOCKED | KA_VM_DONTEXPAND | KA_VM_PFNMAP| KA_VM_DONTDUMP | KA_VM_DONTCOPY | KA_VM_IO;
    if (!(vma->vm_flags & KA_VM_WRITE)) {
        vma->vm_flags &= ~VM_MAYWRITE;
    }
#endif
    vma->vm_private_data = ctx;
    TSDRV_PRINT_DEBUG("devid = %u, vma->vm_start = 0x%pK, vma->vm_end = 0x%pK\n",
        devid, (void *)(uintptr_t)vma->vm_start, (void *)(uintptr_t)vma->vm_end);

    return 0;
}

void vtsdrv_com_msg_init(u32 dev_id, u32 fid, s32 status)
{
}

s32 vtsdrv_com_msg_recv(u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info)
{
    return -EPERM;
}

s32 vtsdrv_vpc_init(void)
{
    return 0;
}

STATIC s32 vtsdrv_vpc_msg_para_check(u32 dev_id, struct vmng_rx_msg_proc_info *proc_info)
{
    if ((proc_info == NULL) || (proc_info->real_out_len == NULL) || (proc_info->data == NULL) ||
        (dev_id >= TSDRV_MAX_DAVINCI_NUM)) {
        TSDRV_PRINT_ERR("para error. dev_id %u.\n", dev_id);
        return -EINVAL;
    }

    if ((proc_info->in_data_len != sizeof(struct vtsdrv_msg)) ||
        (proc_info->out_data_len != sizeof(struct vtsdrv_msg))) {
        TSDRV_PRINT_ERR("in_data_len %u or out_data_len %u invalid.\n", proc_info->in_data_len,
            proc_info->out_data_len);
        return -EINVAL;
    }
    return 0;
}

s32 vtsdrv_vpc_msg_recv(u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info)
{
    struct vtsdrv_msg *msg = NULL;
    s32 ret = 0;

    if (vtsdrv_vpc_msg_para_check(dev_id, proc_info) != 0) {
        TSDRV_PRINT_ERR("hvtsdrv dev-%u vpc msg para check failed.\n", dev_id);
        return -EINVAL;
    }

    msg = (struct vtsdrv_msg *)proc_info->data;
    switch (msg->cmd) {
        case HVTSDRV_DEV_PROC:
            ret = vtsdrv_cq_wake_up(dev_id, &msg->hvdev_proc);
            break;
        case HVTSDRV_DEV_RECYCLE:
            ret = vtsdrv_wait_recycle_finish(dev_id, &msg->hvdev_proc);
            break;
        default:
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("hvtsdrv dev-%u vpc msg cmd %d error.\n", dev_id, (int)msg->cmd);
#endif
            return -EINVAL;
    }

    *(proc_info->real_out_len) = sizeof(struct vtsdrv_msg);
    msg->complete_code = ret;

    TSDRV_PRINT_DEBUG("---\n");
    return 0;
}

