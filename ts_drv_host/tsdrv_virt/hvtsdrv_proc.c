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
#include "tsdrv_log.h"
#include "tsdrv_device.h"
#include "devdrv_devinit.h"
#include "devdrv_cb.h"
#include "devdrv_cbctrl.h"
#include "tsdrv_get_ssid.h"
#include "davinci_interface.h"
#include "davinci_api.h"
#include "devdrv_manager_container.h"
#include "devdrv_manager.h"
#include "tsdrv_vsq.h"

#include "shm_sqcq.h"
#include "logic_cq.h"
#include "hvtsdrv_proc.h"
#include "hvtsdrv_id.h"
#include "hvtsdrv_cqsq.h"
#include "hvtsdrv_cb.h"
#include "hvtsdrv_logic_cq.h"
#include "hvtsdrv_shm_sqcq.h"
#include "ka_task_pub.h"
#include "ka_ioctl_pub.h"
#include "ka_list_pub.h"

static int (* const hvtsdrv_vpc_proc_handlers[DEV_PROC_MAXCMD])(u32 devid, u32 tsid, u32 fid, ka_pid_t tgid, u8 *msg) = {
    [CALLBACK_DEV_ALLOC_SQ] = hvtsdrv_generate_cb_sq,
    [CALLBACK_DEV_ALLOC_CQ] = hvtsdrv_generate_cb_cq,
    [CALLBACK_DEV_MAILBOX_SEND] = hvtsdrv_cb_mailbox_to_ts,
    [CALLBACK_DEV_MAILBOX_LOGIC_SEND] = hvtsdrv_cb_mailbox_logic_to_ts,
    [CALLBACK_DEV_SET_DOORBELL] = hvtsdrv_cb_set_doorbell,
    [LOGIC_CQ_ALLOC] = hvtsdrv_logic_cq_alloc,
    [LOGIC_CQ_FREE] = hvtsdrv_logic_cq_free,
    [LOGIC_CQ_HEAD_UPDATE] = hvtsdrv_logic_cq_head_update,
    [SHM_OFFSET_GET] = hvtsdrv_shm_offset_get,
};

int hvtsdrv_logic_ioctl_cq_alloc(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    return logic_ioctl_cq_alloc(ctx, arg);
}

int hvtsdrv_logic_ioctl_cq_free(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    return logic_ioctl_cq_free(ctx, arg);
}

int hvtsdrv_logic_ioctl_cq_wait(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    return logic_ioctl_cq_wait(ctx, arg);
}

int hvtsdrv_get_dev_info(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    arg->devInfo.virt_type = TSDRV_CONTAINER_TYPE;

    TSDRV_PRINT_INFO("Get dev info succeed. (virt_type=%d)\n", arg->devInfo.virt_type);
    return 0;
}

STATIC int (* const hvtsdrv_ioctl_proc_handlers[TSDRV_MAX_CMD])(struct tsdrv_ctx *ctx,
    struct devdrv_ioctl_arg *arg) = {
        [_KA_IOC_NR(TSDRV_ALLOC_NOTIFY_ID)] = tsdrv_notify_id_alloc,
        [_KA_IOC_NR(TSDRV_FREE_NOTIFY_ID)] = tsdrv_notify_id_free,
        [_KA_IOC_NR(TSDRV_ID_INFO_QUERY)] = hvtsdrv_id_info_query,
        [_KA_IOC_NR(TSDRV_SQCQ_ALLOC_ID)] = hvtsdrv_sqcq_alloc,
        [_KA_IOC_NR(TSDRV_SQCQ_FREE_ID)] = hvtsdrv_sqcq_free,
        [_KA_IOC_NR(TSDRV_SQ_MSG_SEND)] = tsdrv_vsq_msg_send,
        [_KA_IOC_NR(TSDRV_CQ_REPORT_RELEASE)] = hvtsdrv_cq_report_release,
        [_KA_IOC_NR(TSDRV_GET_SQ_HEAD)] = hvtsdrv_get_sq_head,
        [_KA_IOC_NR(TSDRV_REPORT_WAIT)] = hvtsdrv_wait_cq_report,
        [_KA_IOC_NR(TSDRV_CBSQCQ_ALLOC_ID)] = hvtsdrv_ioctl_cbsqcq_alloc,
        [_KA_IOC_NR(TSDRV_CBSQCQ_FREE_ID)] = hvtsdrv_ioctl_cbsqcq_free,
        [_KA_IOC_NR(TSDRV_CBSQCQ_SEND_TASK)] = hvtsdrv_ioctl_cbsq_send,
        [_KA_IOC_NR(TSDRV_SHM_SQCQ_ALLOC)] = hvtsdrv_shm_sqcq_alloc,
        [_KA_IOC_NR(TSDRV_SHM_SQCQ_FREE)] = hvtsdrv_shm_sqcq_free,
        [_KA_IOC_NR(TSDRV_GET_SSID)] = hvtsdrv_ioctl_get_ssid,
        [_KA_IOC_NR(TSDRV_LOGIC_CQ_ALLOC)] = hvtsdrv_logic_ioctl_cq_alloc,
        [_KA_IOC_NR(TSDRV_LOGIC_CQ_FREE)] = hvtsdrv_logic_ioctl_cq_free,
        [_KA_IOC_NR(TSDRV_LOGIC_CQ_WAIT)] = hvtsdrv_logic_ioctl_cq_wait,
        [_KA_IOC_NR(TSDRV_GET_DEV_INFO)] = hvtsdrv_get_dev_info,
};

STATIC s32 hvtsdrv_rx_msg_para_check(u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info)
{
    if ((proc_info == NULL) || (proc_info->real_out_len == NULL) || (proc_info->data == NULL) ||
        (dev_id >= TSDRV_MAX_DAVINCI_NUM) || (fid >= TSDRV_MAX_FID_NUM) || (fid == TSDRV_PM_FID)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("para error. dev_id %u, fid %u.\n", dev_id, fid);
        return -EINVAL;
#endif
    }

    if ((proc_info->in_data_len != sizeof(struct vtsdrv_msg)) ||
        (proc_info->out_data_len != sizeof(struct vtsdrv_msg))) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("in_len %u or out_len %u invalid.\n", proc_info->in_data_len, proc_info->out_data_len);
        return -EINVAL;
#endif
    }

    return 0;
}

static s32 hvtsdrv_device_ready(u32 dev_id, u32 fid, struct vtsdrv_msg *msg)
{
    struct tsdrv_ts_resource *ts_res = NULL;
    enum tsdrv_dev_status status;
    u32 tsid, ts_num;

    status = tsdrv_get_dev_status(dev_id, fid);
    if (status != TSDRV_DEV_ACTIVE) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid status, devid(%u), fid(%u), status(%u)\n", dev_id, fid, status);
        return -EEXIST;
#endif
    }

    ts_num = tsdrv_get_dev_tsnum(dev_id);
    for (tsid = 0; tsid < ts_num; tsid++) {
        ts_res = tsdrv_get_ts_resoruce(dev_id, fid, tsid);
        msg->vdev_ready.id_capacity[tsid].sq_capacity = ts_res->id_res[TSDRV_SQ_ID].id_capacity;
        msg->vdev_ready.id_capacity[tsid].cq_capacity = ts_res->id_res[TSDRV_CQ_ID].id_capacity;
    }

    return 0;
}

STATIC s32 hvtsdrv_open_device(u32 devid, u32 fid, ka_pid_t tgid, s64 unique_id)
{
    enum tsdrv_dev_status status;
    struct tsdrv_ctx *ctx = NULL;
    int ret;

    status = tsdrv_get_dev_status(devid, fid);
    if (status != TSDRV_DEV_ACTIVE) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("hvtsdrv open dev-%u fid-%u fail, invalid status(%d)\n", devid, fid, status);
        return -EEXIST;
#endif
    }

    ctx = tsdrv_dev_ctx_get(devid, fid, tgid);
    if (ctx == NULL) {
#ifndef TSDRV_UT
        return -EFAULT;
#endif
    }

    ret = shm_sqcq_ctx_init(ctx, tsdrv_get_dev_tsnum(devid));
    if (ret != 0) {
#ifndef TSDRV_UT
        tsdrv_dev_ctx_put(ctx);
        return -ENODEV;
#endif
    }

    TSDRV_PRINT_DEBUG("devid(%u), fid(%u), tgid(%u)\n", devid, fid, tgid);
    ctx->unique_id = unique_id;

    tsdrv_set_ctx_normal_type(ctx);
    tsdrv_set_ctx_run(ctx);

    return 0;
}

static s32 hvtsdrv_close_device(u32 dev_id, u32 fid, ka_pid_t tgid, struct tsdrv_ctx *ctx)
{
    enum tsdrv_dev_status status;

    status = tsdrv_get_dev_status(dev_id, fid);
    if (status != TSDRV_DEV_ACTIVE) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("hvtsdrv close dev-%u fid-%u fail, invalid status(%d)\n", dev_id, fid, status);
        return -EEXIST;
#endif
    }

    /* The container scene has a pre-recycling operation, so this operation is not required */
    if (tsdrv_is_in_vm(dev_id) == true) {
        tsdrv_dev_proc_ctx_ref_disable(ctx);
        if (tsdrv_dev_proc_ctx_ref_check(ctx) == false) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("dev-%u fid-%u tgid-%d ctx is running.\n", dev_id, fid, tgid);
            return -EBUSY;
#endif
        }

        if (tsdrv_set_ctx_releasing(ctx) != 0) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("dev-%u fid-%u set ctx releasing fail, tgid=%d\n", dev_id, fid, tgid);
            return -EINVAL;
#endif
        }
        tsdrv_dev_set_ctx_recycle(ctx);
    }
    TSDRV_PRINT_DEBUG("devid(%u), fid(%u), tgid(%u)\n", dev_id, fid, tgid);

    return tsdrv_ctx_ids_recycle(dev_id, fid, ctx);
}

static s32 hvtsdrv_vpc_handlers(u32 dev_id, u32 fid, struct vtsdrv_msg *msg)
{
    if (msg->sub_cmd >= DEV_PROC_MAXCMD) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("dev-%u fid-%u vpc msg cmd(%u %u) error.\n", dev_id, fid, msg->cmd, msg->sub_cmd);
        return -EINVAL;
#endif
    }

    if (msg->vdev_proc.proc_msg.tsid >= tsdrv_get_dev_tsnum(dev_id)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("The tsid is invaild. (devid=%u; fid=%u; tsid=%u)\n", dev_id, fid,
            msg->vdev_proc.proc_msg.tsid);
        return -EINVAL;
#endif
    }

    if (hvtsdrv_vpc_proc_handlers[msg->sub_cmd] == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_WARN("invalid cmd(%u)\n", msg->sub_cmd);
        return -EINVAL;
#endif
    }
    TSDRV_PRINT_DEBUG("dev-%u fid-%u vpc msg cmd(%u %u).\n", dev_id, fid, msg->cmd, msg->sub_cmd);

    return hvtsdrv_vpc_proc_handlers[msg->sub_cmd](dev_id,
        msg->vdev_proc.proc_msg.tsid, fid, msg->vdev_proc.tgid, msg->vdev_proc.proc_msg.msg);
}

static s32 hvtsdrv_ioctl_handlers(u32 dev_id, u32 fid, ka_pid_t tgid, u32 cmd, struct devdrv_ioctl_arg *arg)
{
    enum tsdrv_dev_status status;
    struct tsdrv_ctx *ctx = NULL;
    u32 tsid;
    int ret;

    status = tsdrv_get_dev_status(dev_id, fid);
    if (status != TSDRV_DEV_ACTIVE) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("hvtsdrv dev-%u fid-%u, invalid status(%d).\n", dev_id, fid, status);
        return -EEXIST;
#endif
    }

    if (cmd >= TSDRV_MAX_CMD) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("hvtsdrv dev-%u fid-%u ioctl cmd(%u) out of range.\n", dev_id, fid, cmd);
        return -EINVAL;
#endif
    }

    tsid = arg->tsid;
    if (tsid >= tsdrv_get_dev_tsnum(dev_id)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("dev-%u fid-%u tsid-%u invalid.\n", dev_id, fid, tsid);
        return -EINVAL;
#endif
    }

    TSDRV_PRINT_DEBUG("dev(%u), fid(%u), cmd(%u).\n", dev_id, fid, cmd);

    ctx = tsdrv_dev_proc_ctx_get(dev_id, fid, tgid);
    if (ctx == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Find ctx fail. (devid=%u; fid=%u; tgid=%d)\n", dev_id, fid, tgid);
        return -ESRCH;
#endif
    }

    switch (cmd) {
        case _KA_IOC_NR(TSDRV_ALLOC_EVENT_ID):
            ret = tsdrv_res_id_alloc(ctx, TSDRV_EVENT_SW_ID, arg);
            break;
        case _KA_IOC_NR(TSDRV_FREE_EVENT_ID):
            ret = tsdrv_res_id_free(ctx, TSDRV_EVENT_SW_ID, arg);
            break;
        case _KA_IOC_NR(TSDRV_ALLOC_MODEL_ID):
            ret = tsdrv_res_id_alloc(ctx, TSDRV_MODEL_ID, arg);
            break;
        case _KA_IOC_NR(TSDRV_FREE_MODEL_ID):
            ret = tsdrv_res_id_free(ctx, TSDRV_MODEL_ID, arg);
            break;
        case _KA_IOC_NR(TSDRV_ALLOC_STREAM_ID):
            ret = tsdrv_res_id_alloc(ctx, TSDRV_STREAM_ID, arg);
            break;
        case _KA_IOC_NR(TSDRV_FREE_STREAM_ID):
            ret = tsdrv_res_id_free(ctx, TSDRV_STREAM_ID, arg);
            break;
        case _KA_IOC_NR(TSDRV_ALLOC_NOTIFY_ID):
        case _KA_IOC_NR(TSDRV_FREE_NOTIFY_ID):
        case _KA_IOC_NR(TSDRV_ID_INFO_QUERY):
        case _KA_IOC_NR(TSDRV_SQCQ_ALLOC_ID):
        case _KA_IOC_NR(TSDRV_SQCQ_FREE_ID):
        case _KA_IOC_NR(TSDRV_SQ_MSG_SEND):
        case _KA_IOC_NR(TSDRV_CQ_REPORT_RELEASE):
        case _KA_IOC_NR(TSDRV_GET_SQ_HEAD):
        case _KA_IOC_NR(TSDRV_REPORT_WAIT):
        case _KA_IOC_NR(TSDRV_CBSQCQ_ALLOC_ID):
        case _KA_IOC_NR(TSDRV_CBSQCQ_FREE_ID):
        case _KA_IOC_NR(TSDRV_CBSQCQ_SEND_TASK):
        case _KA_IOC_NR(TSDRV_SHM_SQCQ_ALLOC):
        case _KA_IOC_NR(TSDRV_SHM_SQCQ_FREE):
        case _KA_IOC_NR(TSDRV_GET_SSID):
        case _KA_IOC_NR(TSDRV_LOGIC_CQ_ALLOC):
        case _KA_IOC_NR(TSDRV_LOGIC_CQ_FREE):
        case _KA_IOC_NR(TSDRV_LOGIC_CQ_WAIT):
        case _KA_IOC_NR(TSDRV_GET_DEV_INFO):
            ret = hvtsdrv_ioctl_proc_handlers[cmd](ctx, arg);
            break;
        default:
            TSDRV_PRINT_ERR("devid(%u), fid(%u), tgid(%u), cmd(%u) is not support.\n", dev_id, fid, ctx->tgid, cmd);
            ret = -EINVAL;
    }

    if ((ret != 0) && (ret != -EOPNOTSUPP)) {
        TSDRV_PRINT_ERR("devid(%u), fid(%u), tgid(%u), cmd(%u) handle failed.\n", dev_id, fid, ctx->tgid, cmd);
    }

    tsdrv_dev_proc_ctx_put(ctx);

    return ret;
}

s32 hvtsdrv_com_msg_recv(u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info)
{
    struct vtsdrv_msg *msg = NULL;
    struct tsdrv_ctx *ctx = NULL;
    s32 ret = 0;

    if (hvtsdrv_rx_msg_para_check(dev_id, fid, proc_info) != 0) {
        TSDRV_PRINT_ERR("hvtsdrv dev-%u fid-%u common msg para check failed.\n", dev_id, fid);
        return -EINVAL;
    }

    msg = (struct vtsdrv_msg *)proc_info->data;
    switch (msg->cmd) {
        case TSDRV_DEV_READY_NOTIFY:
            ret = hvtsdrv_device_ready(dev_id, fid, msg);
            break;
        case TSDRV_DEV_OPEN:
            ret = hvtsdrv_open_device(dev_id, fid, msg->vdev_open.tgid, msg->vdev_open.unique_id);
            break;
        case TSDRV_DEV_RELEASE:
            ctx = tsdrv_dev_proc_ctx_get(dev_id, fid, msg->vdev_release.tgid);
            if (ctx == NULL) {
#ifndef TSDRV_UT
                TSDRV_PRINT_ERR("dev-%u fid-%u find ctx fail.\n", dev_id, fid);
                return -EINVAL;
#endif
            }
            tsdrv_dev_proc_ctx_put(ctx);
            ret = hvtsdrv_close_device(dev_id, fid, msg->vdev_release.tgid, ctx);
            break;
        default:
            TSDRV_PRINT_ERR("hvtsdrv dev-%u fid-%u common msg cmd %u invalid.\n", dev_id, fid, msg->cmd);
            return -EINVAL;
    }

    *(proc_info->real_out_len) = sizeof(struct vtsdrv_msg);
    msg->complete_code = ret;

    return 0;
}

bool hvtsdrv_is_vpc_not_support_cmd(struct vtsdrv_msg *msg)
{
    return ((msg->sub_cmd == _KA_IOC_NR(TSDRV_REPORT_WAIT)) || (msg->sub_cmd == _KA_IOC_NR(TSDRV_CBSQCQ_ALLOC_ID)) ||
        (msg->sub_cmd == _KA_IOC_NR(TSDRV_CBSQCQ_FREE_ID)) || (msg->sub_cmd == _KA_IOC_NR(TSDRV_CBSQCQ_SEND_TASK)) ||
        (msg->sub_cmd == _KA_IOC_NR(TSDRV_LOGIC_CQ_ALLOC)) || (msg->sub_cmd == _KA_IOC_NR(TSDRV_LOGIC_CQ_FREE)) ||
        (msg->sub_cmd == _KA_IOC_NR(TSDRV_LOGIC_CQ_WAIT)));
}

s32 hvtsdrv_vpc_msg_recv(u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info)
{
    struct vtsdrv_msg *msg = NULL;
    s32 ret = 0;

    if (hvtsdrv_rx_msg_para_check(dev_id, fid, proc_info) != 0) {
        TSDRV_PRINT_ERR("hvtsdrv dev-%u fid-%u vpc msg para check failed.\n", dev_id, fid);
        return -EINVAL;
    }
    msg = (struct vtsdrv_msg *)proc_info->data;

    switch (msg->cmd) {
        case TSDRV_DEV_IOCTRL:
            if (hvtsdrv_is_vpc_not_support_cmd(msg)) {
                    TSDRV_PRINT_WARN("dev-%u fid-%u cmd-%d invalid.\n", dev_id, fid, msg->sub_cmd);
                    return -EINVAL;
            }
            ret = hvtsdrv_ioctl_handlers(dev_id, fid, msg->vdev_ctrl.tgid, msg->sub_cmd, &msg->vdev_ctrl.tsdrv_ioctl);
            break;
        case TSDRV_DEV_PROC:
            ret = hvtsdrv_vpc_handlers(dev_id, fid, msg);
            break;
        default:
            TSDRV_PRINT_ERR("hvtsdrv dev-%u fid-%u vpc msg cmd %u error.\n", dev_id, fid, msg->cmd);
            return -EINVAL;
    }

    *(proc_info->real_out_len) = sizeof(struct vtsdrv_msg);
    msg->complete_code = ret;

    return 0;
}

s32 hvtsdrv_vpc_msg_send(u32 devid, u32 fid, struct vtsdrv_msg *msg)
{
#ifdef CFG_FEATURE_VFIO
    struct vmng_tx_msg_proc_info tx_info;
    s32 ret;

    msg->complete_code = COMP_CODE_NO_RESPONSE;

    tx_info.data = msg;
    tx_info.in_data_len = sizeof(struct vtsdrv_msg);
    tx_info.out_data_len = sizeof(struct vtsdrv_msg);
    tx_info.real_out_len = 0;
    ret = vmngh_vpc_msg_send(devid, fid, VMNG_VPC_TYPE_TSDRV, &tx_info, VPC_DEFAULT_TIMEOUT);
    if ((ret != 0) || (msg->complete_code != COMP_CODE_SUCCESS)) {
        TSDRV_PRINT_ERR("vpc msg send failed. cmd(%u), sub_cmd(%u), ret(%d), error_code(%d).\n", msg->cmd, msg->sub_cmd,
            ret, msg->complete_code);
        return -EFAULT;
    }
    return 0;
#else
    return 0;
#endif
}
EXPORT_SYMBOL_UNRELEASE(hvtsdrv_vpc_msg_send);

int hvtsdrv_open(ka_inode_t *inode, ka_file_t *filep)
{
    u32 vdevid = drv_davinci_get_device_id(filep);
    struct tsdrv_ctx *ctx = NULL;
    enum tsdrv_dev_status status;
    u32 devid = 0;
    u32 fid = 0;
    int ret;

    ret = devdrv_manager_container_check_devid_in_container_ns(vdevid, current);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("check device in container failed. dev_id(%u)\n", vdevid);
        return -ENODEV;
#endif
    }

    ret = devmng_get_vdavinci_info(vdevid, &devid, &fid);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid para, vdevid(%u), devid(%u), fid(%u)\n", vdevid, devid, fid);
        return -EFAULT;
#endif
    }

    TSDRV_PRINT_DEBUG("vdevid(%u), devid(%u), fid(%u), tgid(%u)\n", vdevid, devid, fid, current->tgid);

    if (devid >= TSDRV_MAX_DAVINCI_NUM || fid >= TSDRV_MAX_FID_NUM || fid == TSDRV_PM_FID) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid para, vdevid(%u), devid(%u), fid(%u)\n", vdevid, devid, fid);
        return -EFAULT;
#endif
    }

    status = tsdrv_get_dev_status(devid, fid);
    if (status != TSDRV_DEV_ACTIVE) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("hvtsdrv open dev-%u fid-%u fail, invalid status(%d)\n", devid, fid, status);
        return -EEXIST;
#endif
    }

    ctx = tsdrv_dev_ctx_get(devid, fid, current->tgid);
    if (ctx == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Failed to tsdrv_dev_ctx_get. (devid=%u; fid=%u; tgid=%u)\n",
            devid, fid, current->tgid);
        return -EFAULT;
#endif
    }

    ret = shm_sqcq_ctx_init(ctx, tsdrv_get_dev_tsnum(devid));
    if (ret != 0) {
#ifndef TSDRV_UT
        tsdrv_dev_ctx_put(ctx);
        TSDRV_PRINT_ERR("Failed to shm_sqcq_ctx_init. (devid=%u; fid=%u; tgid=%u)\n",
            devid, fid, current->tgid);
        return -ENODEV;
#endif
    }

    ret = logic_sqcq_ctx_init(ctx, tsdrv_get_dev_tsnum(devid));
    if (ret != 0) {
#ifndef TSDRV_UT
        tsdrv_dev_ctx_put(ctx);
        shm_sqcq_ctx_exit(ctx, tsdrv_get_dev_tsnum(devid));
        TSDRV_PRINT_ERR("Failed to init logic ctx. (devid=%u; fid=%u; tgid=%u)\n",
            devid, fid, current->tgid);
        return ret;
#endif
    }

    ret = callback_ctx_init(ctx, tsdrv_get_dev_tsnum(devid));
    if (ret != 0) {
#ifndef TSDRV_UT
        tsdrv_dev_ctx_put(ctx);
        shm_sqcq_ctx_exit(ctx, tsdrv_get_dev_tsnum(devid));
        logic_sqcq_ctx_exit(ctx, tsdrv_get_dev_tsnum(devid));
        TSDRV_PRINT_ERR("Failed to callback_ctx_init. (devid=%u; fid=%u; tgid=%u)\n",
            devid, fid, current->tgid);
        return ret;
#endif
    }

    tsdrv_set_ctx_normal_type(ctx);
    ctx->unique_id = 0;
    tsdrv_set_ctx_run(ctx);
    filep->private_data = ctx;

    return 0;
}

int hvtsdrv_release(ka_inode_t *inode, ka_file_t *filep)
{
    struct tsdrv_ctx *ctx = filep->private_data;
    u32 devid, fid;

    if (ctx == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("ctx is NULL\n");
        return -EINVAL;
#endif
    }

    devid = tsdrv_get_devid_by_ctx(ctx);
    fid = tsdrv_get_fid_by_ctx(ctx);

    TSDRV_PRINT_DEBUG("devid(%u), fid(%u), tgid(%u)\n", devid, fid, ctx->tgid);

    logic_sqcq_ctx_exit(ctx, tsdrv_get_dev_tsnum(devid));
    return hvtsdrv_close_device(devid, fid, ctx->tgid, ctx);
}

int hvtsdrv_mmap(ka_file_t *filep, struct vm_area_struct *vma)
{
    return devdrv_mmap(filep, vma);
}

long hvtsdrv_ioctl(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    struct tsdrv_ctx *ctx = (struct tsdrv_ctx *)filep->private_data;
    void __user *uarg = (void __user *)(uintptr_t)arg;
    struct devdrv_ioctl_arg *ioctl_arg = NULL;
    u32 devid, fid;
    int ret;

    ret = (_KA_IOC_TYPE(cmd) != TSDRV_ID_MAGIC) || (_KA_IOC_NR(cmd) >= TSDRV_MAX_CMD) ||
        (uarg == NULL) || (ctx == NULL);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid param, cmd_type=%c cmd_nr=%u, arg=0x%pK, ctx=0x%pK\n",
            _KA_IOC_TYPE(cmd), _KA_IOC_NR(cmd), (void *)arg, (void *)(u64)(uintptr_t)ctx);
        return -EINVAL;
#endif
    }

    if (tsdrv_ctx_is_run(ctx) != true) {
        TSDRV_PRINT_ERR("invalid ctx state, tgid=%d\n", ctx->tgid);
        return -ENODEV;
    }
    if ((_KA_IOC_NR(cmd)) == (_KA_IOC_NR(TSDRV_CBSQCQ_WAIT_ID))) {
        return devdrv_ioctl_cbcq_wait(ctx, uarg);
    }

    ioctl_arg = tsdrv_kzalloc(sizeof(struct devdrv_ioctl_arg), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (ioctl_arg == NULL) {
        TSDRV_PRINT_ERR("kmalloc ioctl_arg fail.\n");
        return -EINVAL;
    }

    if (_KA_IOC_DIR(cmd) & _KA_IOC_WRITE) {
        if (copy_from_user_safe(ioctl_arg, uarg, sizeof(struct devdrv_ioctl_arg)) != 0) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("copy from user fail cmd(%u)\n", _KA_IOC_NR(cmd));
            ret = -ENOMEM;
            goto out;
#endif
        }
    }

    devid = tsdrv_get_devid_by_ctx(ctx);
    fid = tsdrv_get_fid_by_ctx(ctx);
    ret = hvtsdrv_ioctl_handlers(devid, fid, ctx->tgid, _KA_IOC_NR(cmd), ioctl_arg);

    if (_KA_IOC_DIR(cmd) & _KA_IOC_READ) {
        if (copy_to_user_safe(uarg, ioctl_arg, sizeof(struct devdrv_ioctl_arg)) != 0) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("copy to user fail cmd(%u)\n", _KA_IOC_NR(cmd));
            ret = -EFAULT;
            goto out;
#endif
        }
    }

out:
    tsdrv_kfree(ioctl_arg);
    ioctl_arg = NULL;

    return ret;
}

STATIC int hvtsdrv_davinci_open(ka_inode_t *inode, ka_file_t *filep)
{
    return 0;
}

ka_file_operations_t hvtsdrv_vdavinci_fops = {
    ka_fs_init_f_owner(KA_THIS_MODULE)
    ka_fs_init_f_open(hvtsdrv_davinci_open)
};

ka_file_operations_t *hvtsdrv_get_vdavinci_fop(void)
{
    return &hvtsdrv_vdavinci_fops;
}

