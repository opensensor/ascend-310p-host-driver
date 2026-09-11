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

#ifndef ESCHED_MSG_DEF_H
#define ESCHED_MSG_DEF_H

#include "event_sched_inner.h"
#include "esched.h"

typedef enum esched_msg_type {
    ESCHED_MSG_TYPE_ADD_HOST_PID,
    ESCHED_MSG_TYPE_DEL_HOST_PID,
    ESCHED_MSG_TYPE_ADD_POOL,
    ESCHED_MSG_TYPE_GET_CPU_MBID,
    ESCHED_MSG_TYPE_ADD_MB,
    ESCHED_MSG_TYPE_CONF_INTR,
    ESCHED_MSG_TYPE_REMOTE_SUBMIT,
    ESCHED_MSG_TYPE_REMOTE_QUERY_GID,
    ESCHED_MSG_TYPE_REMOTE_QUERY_TRACE,
    ESCHED_MSG_TYPE_MAX_NUM
} ESCHED_MSG_TYPE;

struct esched_ctrl_msg_cfg_host_pid {
    int vfid;
    int host_ctrl_pid;
    int pid_type;
    int pid;
};

struct esched_ctrl_msg_cfg_pool {
    u32 cpu_type;
};

struct esched_ctrl_msg_get_cpu_mbid {
    u32 cpu_type;
    u32 mb_id;
    u32 wait_mb_id;
};

struct esched_ctrl_msg_cfg_mb {
    u32 vf_id;
};

struct esched_ctrl_msg_intr {
    u32 irq;
};

struct esched_remote_submit_msg {
    int pid;
    struct sched_published_event_info event_info;
    char msg[SCHED_MAX_EVENT_MSG_LEN];
};

struct esched_remote_query_gid_msg {
    int pid;
    char grp_name[EVENT_MAX_GRP_NAME_LEN];
    u32 gid;
};
struct esched_query_trace_msg {
    u32 pid;
    u32 gid;
    u32 tid;
    struct sched_sync_event_trace sched_trace;
};

struct esched_ctrl_msg {
    ESCHED_MSG_TYPE type;
    int error_code;
    union {
        struct esched_ctrl_msg_cfg_host_pid host_pid_msg;
        struct esched_ctrl_msg_cfg_pool pool_msg;
        struct esched_ctrl_msg_get_cpu_mbid mbid_msg;
        struct esched_ctrl_msg_cfg_mb mb_msg;
        struct esched_ctrl_msg_intr intr_msg;
        struct esched_remote_submit_msg submit_msg;
        struct esched_remote_query_gid_msg query_gid_msg;
        struct esched_query_trace_msg trace_msg;
    };
};

#endif
