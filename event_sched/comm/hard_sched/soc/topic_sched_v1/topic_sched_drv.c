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
 * Create: 2024-03-01
 */
#include "ka_memory_pub.h"
#include "ka_system_pub.h"
#include "securec.h"

#include "esched.h"
#include "esched_drv_adapt.h"
#include "esched_fops.h"
#include "topic_sched_drv.h"

#include "tsdrv_interface.h"
#include "trs_chan.h"
#include "devdrv_interface.h"
#include "hwts_task_info.h"

static u8 sched_topic_types[POLICY_MAX][DST_ENGINE_MAX];

void esched_init_topic_types(void)
{
    u32 i, j;

    for (i = 0; i < POLICY_MAX; i++) {
        for (j = 0; j < DST_ENGINE_MAX; j++) {
            sched_topic_types[i][j] = TOPIC_TYPE_MAX;
        }
    }

    sched_topic_types[ONLY][ACPU_DEVICE] = TOPIC_TYPE_AICPU_DEVICE_ONLY;
    sched_topic_types[ONLY][ACPU_HOST] = TOPIC_TYPE_AICPU_HOST_ONLY;
    sched_topic_types[ONLY][CCPU_DEVICE] = TOPIC_TYPE_CCPU_DEVICE;
    sched_topic_types[ONLY][CCPU_HOST] = TOPIC_TYPE_CCPU_HOST;
    sched_topic_types[ONLY][TS_CPU] = TOPIC_TYPE_TSCPU;

    sched_topic_types[FIRST][ACPU_DEVICE] = TOPIC_TYPE_AICPU_DEVICE_FIRST;
    sched_topic_types[FIRST][ACPU_HOST] = TOPIC_TYPE_AICPU_HOST_FIRST;

#ifdef ESCHED_HOST
    sched_topic_types[ONLY][ACPU_LOCAL] = TOPIC_TYPE_AICPU_HOST_ONLY;
    sched_topic_types[ONLY][CCPU_LOCAL] = TOPIC_TYPE_CCPU_HOST;
#else
    sched_topic_types[ONLY][ACPU_LOCAL] = TOPIC_TYPE_AICPU_DEVICE_ONLY;
    sched_topic_types[ONLY][CCPU_LOCAL] = TOPIC_TYPE_CCPU_DEVICE;
#endif
}

u8 esched_get_topic_type(unsigned int policy, unsigned int dst_engine)
{
    return sched_topic_types[policy][dst_engine];
}

void esched_init_usrdata_flag(struct topic_sched_sqe *sqe)
{
    sqe->usr_pid_flag = 0;
    sqe->devid_flag = 0;
    sqe->tid_flag = 0;
}

#ifndef ESCHED_HOST
static int esched_sqe_user_data_append(struct topic_sched_sqe *sqe, u16 value)
{
    if ((sqe->user_data_len + sizeof(value)) > sizeof(sqe->user_data)) {
        sched_err("Out of len. (topic_id=%u; subtopic_id=%u; pid=%d; user_data_len=%d)\n",
            sqe->topic_id, sqe->subtopic_id, sqe->pid, sqe->user_data_len);
        return -EFAULT;
    }

    if ((sqe->user_data_len % sizeof(value)) != 0) {
#ifndef EMU_ST
        sched_err("User data len not align. (topic_id=%u; subtopic_id=%u; pid=%d; user_data_len=%d)\n",
            sqe->topic_id, sqe->subtopic_id, sqe->pid, sqe->user_data_len);
        return -EFAULT;
#endif
    }

    *(u16 *)&(((u8 *)sqe->user_data)[sqe->user_data_len]) = value;
    sqe->user_data_len += sizeof(value);
    return 0;
}
#endif

int esched_update_sqe_user_data(struct topic_sched_sqe *sqe, struct sched_published_event_info *event_info)
{
#ifndef ESCHED_HOST
    int ret;

    if ((event_info->dst_engine == (u32)TS_CPU) || (event_info->dst_engine == (u32)DVPP_CPU)) {
        return 0;
    }

    if (event_info->dst_devid != SCHED_INVALID_DEVID) {
#ifndef EMU_ST
        ret = esched_sqe_user_data_append(sqe, (u16)event_info->dst_devid);
        if (ret != 0) {
            return ret;
        }
        sqe->devid_flag = 1;
#endif
    }

    if (event_info->tid != SCHED_INVALID_TID) {
        ret = esched_sqe_user_data_append(sqe, (u16)event_info->tid);
        if (ret != 0) {
            return ret;
        }
        sqe->tid_flag = 1;
    }
#endif
    return 0;
}

static int esched_mb_user_data_subtract(struct topic_sched_mailbox *mb, u16 size, u32 *value)
{
    u16 i;

    if (mb->user_data_len < size) {
#ifndef EMU_ST
        sched_err("No more space. (topic_id=%u; subtopic_id=%u; pid=%d; user_data_len=%d; size=%u)\n",
            mb->topic_id, mb->subtopic_id, mb->pid, mb->user_data_len, size);
        return -EFAULT;
#endif
    }

    mb->user_data_len -= size;

    *value = 0;
    for (i = 0; i < size; i++) {
        *value |= (u32)(((u8 *)mb->user_data)[mb->user_data_len + i]) << (i * 8); /* Shift 8 bit per Byte */
    }

    return 0;
}

int esched_restore_mb_user_data(struct topic_sched_mailbox *mb, u32 *dst_devid, u32 *tid, u32 *pid)
{
    int ret;

    if (mb->usr_pid_flag == 1) {
#ifdef ESCHED_HOST
#ifndef EMU_ST
        mb->usr_pid_flag = 0;
        ret = esched_mb_user_data_subtract(mb, (u16)sizeof(u32), pid);
        if (ret != 0) {
            return ret;
        }
#endif
#endif
    }

    if (mb->tid_flag == 1) {
        mb->tid_flag = 0;
        ret = esched_mb_user_data_subtract(mb, (u16)sizeof(u16), tid);
        if (ret != 0) {
#ifndef EMU_ST
            return ret;
#endif
        }
    }

    if (mb->devid_flag == 1) {
#ifndef EMU_ST
        mb->devid_flag = 0;
        ret = esched_mb_user_data_subtract(mb, (u16)sizeof(u16), dst_devid);
        if (ret != 0) {
            return ret;
        }
#endif
    }

    return 0;
}

int esched_drv_map_host_dev_pid(struct sched_proc_ctx *proc_ctx, u32 identity)
{
    devdrv_host_pids_info_t pids_info = {0};

    if (proc_ctx->host_pid != 0) {
#ifndef EMU_ST
        return 0;
#endif
    }

    (void)devdrv_query_process_host_pids_by_pid(proc_ctx->pid, &pids_info);
    if (pids_info.vaild_num == 0) {
#ifdef CFG_FEATURE_NO_BIND_SCHED
        sched_info("Query host pid of process no bind. (pid=%d)\n", proc_ctx->pid);
        return 0;
#else
#ifndef ESCHED_HOST
        sched_warn("Can not query host pid of process. (pid=%d)\n", proc_ctx->pid);
#endif
        return DRV_ERROR_NO_PROCESS;
#endif
    }
    proc_ctx->host_pid = pids_info.host_pids[0];
    proc_ctx->cp_type = pids_info.cp_type[0];

    sched_info("Show details. (pid=%d; dev_id=%u; vfid=%u; cp_type=%d; host_pid=%d)\n",
               proc_ctx->pid, pids_info.chip_id[0], pids_info.vfid[0], (int)(proc_ctx->cp_type), proc_ctx->host_pid);

#ifndef CFG_FEATURE_MIA_MAP_TOPIC_TABLE
    if (identity != 0) {
#ifndef EMU_ST
        return DRV_ERROR_NONE;
#endif
    }
#endif

    if (proc_ctx->cp_type == DEVDRV_PROCESS_DEV_ONLY) {
#ifndef EMU_ST
        return 0;
#endif
    }

    return esched_drv_config_pid(proc_ctx, identity, &pids_info);
}

void esched_drv_unmap_host_dev_pid(struct sched_proc_ctx *proc_ctx, u32 identity)
{
    if ((proc_ctx->host_pid == 0) || (proc_ctx->cp_type == DEVDRV_PROCESS_DEV_ONLY)) {
#ifndef EMU_ST
        return;
#endif
    }

#ifndef CFG_FEATURE_MIA_MAP_TOPIC_TABLE
    if (identity != 0) {
#ifndef EMU_ST
        return;
#endif
    }
#endif

    esched_drv_del_pid(proc_ctx, identity);
}

bool esched_drv_is_used_host_pid(u32 event_id, u32 topic_type)
{
    if (event_id == EVENT_SPLIT_KERNEL) {
#ifndef EMU_ST
        return false;
#endif
    }

    /* For AICPU or DATACPU tasks, [Host CTRLCPU PID] is used.
       For tasks with other types of CPUs, fills the dest_pid directly. */
    if ((topic_type == TOPIC_TYPE_AICPU_DEVICE_ONLY) ||
        (topic_type == TOPIC_TYPE_AICPU_DEVICE_FIRST) ||
        (topic_type == TOPIC_TYPE_AICPU_HOST_ONLY) ||
        (topic_type == TOPIC_TYPE_AICPU_HOST_FIRST) ||
        (topic_type == TOPIC_TYPE_DCPU_DEVICE)) {
        return true;
    }

    return false;
}

int esched_get_real_pid(struct topic_sched_mailbox *mb, u32 devid, u32 pid)
{
#ifndef CFG_FEATURE_MIA_MAP_TOPIC_TABLE
    enum devdrv_process_type cp_type;
    bool is_pid_dst = false;
    u32 dst_pid;
    int real_pid;
    int ret;
    static u32 query_failed_pid = SCHED_INVALID_PID;

    /* The preferred way to passthrough pid is by usr_pid_flag,
       other pid fields in user_data can be normalized here. */
    if (pid != SCHED_INVALID_PID) {
#ifndef EMU_ST
        mb->pid = pid;
        return DRV_ERROR_NONE;
#endif
    }

    if ((esched_is_phy_dev(devid) == true) && (mb->pid != TOPIC_SCHED_ESCHED_COMM_PID)) {
        mb->pid &= MB_PID_MASK;
        return DRV_ERROR_NONE;
    }

    if ((mb->topic_id == EVENT_TS_HWTS_KERNEL) || (mb->topic_id == EVENT_TS_CTRL_MSG)) {
        dst_pid = mb->user_data[TOPIC_SCHED_TS_PID_INDEX];
    } else if (mb->topic_id == EVENT_TS_CALLBACK_MSG) {
        dst_pid = mb->user_data[TOPIC_SCHED_TS_CALLBACK_PID_INDEX];
    } else {
        ka_mm_memcpy_fromio(&dst_pid, &mb->stream_id, sizeof(dst_pid)); /* pid store in stream_id and task_id */
        is_pid_dst = ((dst_pid & PID_MAP_MASK) != 0);
        dst_pid &= MB_PID_MASK;
    }

    if ((esched_drv_is_used_host_pid(mb->topic_id, mb->topic_type) == false) || is_pid_dst) {
        mb->pid = dst_pid;
        return DRV_ERROR_NONE;
    }

    cp_type = (mb->kernel_type == TOPIC_SCHED_CUSTOM_KERNEL_TYPE) ? DEVDRV_PROCESS_CP2 : DEVDRV_PROCESS_CP1;
    ret = devdrv_query_process_by_host_pid_kernel(dst_pid, devid, cp_type, 0, &real_pid);
    if (ret != 0) {
#ifndef EMU_ST
        if (cp_type == DEVDRV_PROCESS_CP2) {
            goto query_fail;
        }
        ret = devdrv_query_process_by_host_pid_kernel(dst_pid, devid, DEVDRV_PROCESS_QS, 0, &real_pid);
        if (ret != 0) {
            goto query_fail;
        }
#endif
    }

    mb->pid = (u32)real_pid;
    if (query_failed_pid == dst_pid) {
#ifndef EMU_ST
        query_failed_pid = SCHED_INVALID_PID;
#endif
    }

    return DRV_ERROR_NONE;

#ifndef EMU_ST
query_fail:
    if (query_failed_pid != dst_pid) {
        if (!esched_log_limited(SCHED_LOG_LIMIT_GET_REAL_PID)) {
            sched_err("Failed to query target pid by pid. (devid=%u; vfid=%u; topic_id=%u; topic_type=%u; "
                "kernel_type=%u; pid=%u; ret=%d)\n",
                devid, mb->vfid + 1U, mb->topic_id, mb->topic_type, mb->kernel_type, dst_pid, ret);
        }
    }
    query_failed_pid = dst_pid;
    return DRV_ERROR_NO_PROCESS;
#endif
#else
    return DRV_ERROR_NONE;
#endif
}
