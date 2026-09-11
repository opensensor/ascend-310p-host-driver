/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
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
 * Create: 2022-06-22
 */
#include <linux/types.h>

#include "devmm_channel.h"
#include "svm_msg_client.h"
#include "svm_mem_stats.h"
#include "svm_proc_mng.h"

int devmm_chan_report_process_status_d2h(
    struct devmm_svm_process *svm_process, struct devmm_svm_heap *heap, void *msg, u32 *ack_len)
{
    struct devmm_chan_process_status *process_status = (struct devmm_chan_process_status *)msg;

    devmm_drv_debug("rev process_status  (devid=%u, vifd=%u,status=%u, dev_pid=%u, dev_tid=%u)\n",
        process_status->head.dev_id, process_status->head.vfid, process_status->pid_status,
        process_status->pid, process_status->tid);

    devmm_modify_process_status(
        svm_process, process_status->head.dev_id, process_status->head.vfid, process_status->pid_status, true);

    return 0;
}

STATIC int devmm_query_process_status(struct devmm_svm_process *svm_proc,
    struct devmm_chan_process_status *process_status, struct devmm_ioctl_arg *arg)
{
    int ret;

    process_status->head.msg_id = DEVMM_CHAN_QUERY_PROCESS_STATUS_H2D_ID;
    process_status->head.process_id.hostpid = svm_proc->process_id.hostpid;
    process_status->head.process_id.vfid = (u16)arg->head.vfid;
    process_status->head.dev_id = (u16)arg->head.devid;
    process_status->pid_status = arg->data.query_process_status_para.pid_status;

    ret = devmm_chan_msg_send(
        process_status, sizeof(struct devmm_chan_process_status), sizeof(struct devmm_chan_process_status));

    devmm_drv_debug("query process_status. (devid=%u, vifd=%u,status=%u, occur=%d)\n", process_status->head.dev_id,
        process_status->head.vfid, process_status->pid_status, process_status->status_occur);

    return ret;
}

int devmm_ioctl_query_process_status(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    static const bool need_active_chk[STATUS_MAX] = {
        [STATUS_NOMEM] = true,
        [STATUS_SVM_PAGE_FALUT_ERR_OCCUR] = false,
        [STATUS_SVM_PAGE_FALUT_ERR_CNT] = false,
    };
    struct devmm_chan_process_status process_status = {{{0}}};
    processType_t process_type;
    processStatus_t pid_status;
    u32 *result = NULL;
    int ret;

    process_type = arg->data.query_process_status_para.process_type;
    if (process_type != PROCESS_CP1) {
        return DRV_ERROR_INVALID_VALUE;
    }

    pid_status = arg->data.query_process_status_para.pid_status;
    if ((pid_status < STATUS_NOMEM) || (pid_status >= STATUS_MAX)) {
        devmm_drv_err("Pid_status is invalid. (pid_status=%u)\n", pid_status);
        return -EINVAL;
    }

    result = &arg->data.query_process_status_para.status_result;
    *result = 0;

    /* 1.no need to actively update the status from the device side, obtain the status directly from the local. */
    if (need_active_chk[pid_status] == false) {
        *result = devmm_get_process_status(svm_proc, arg->head.devid, arg->head.vfid, pid_status);
        return 0;
    }

    /* 2.the status has not occurred, return not occurred. */
    if (devmm_get_process_status(svm_proc, arg->head.devid, arg->head.vfid, pid_status) == 0) {
        *result = 0;
        return 0;
    }

    /* 3.the status occurs, need to actively update the status from the device side again. */
    ret = devmm_query_process_status(svm_proc, &process_status, arg);
    if (ret != 0) {
        devmm_drv_err("Msg send error! (ret=%d)\n", ret);
        return ret;
    }

    devmm_modify_process_status(svm_proc, process_status.head.dev_id, process_status.head.process_id.vfid,
        process_status.pid_status, process_status.status_occur);
    *result = (u32)process_status.status_occur;

    if ((pid_status == STATUS_NOMEM) && (process_status.status_occur == true)) {
        devmm_dev_mem_stats_log_show(arg->head.logical_devid);
    }
    return 0;
}

