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

#ifndef VTSDRV_COMMON_H
#define VTSDRV_COMMON_H

#include "virtmng_interface.h"
#include "tsdrv_log.h"
#include "tsdrv_kernel_common.h"
#include "tsdrv_ioctl.h"
#include "ka_errno_pub.h"
#include "ka_driver_pub.h"
#include "ka_common_pub.h"
#include "ka_task_pub.h"
#include "ka_fs_pub.h"
enum tsdrv_vpc_cmd {
    CALLBACK_DEV_ALLOC_SQ,
    CALLBACK_DEV_ALLOC_CQ,
    CALLBACK_DEV_FREE_SQ,
    CALLBACK_DEV_FREE_CQ,
    CALLBACK_DEV_MAILBOX_SEND,
    CALLBACK_DEV_MAILBOX_LOGIC_SEND,
    CALLBACK_DEV_SET_DOORBELL,
    LOGIC_CQ_ALLOC,
    LOGIC_CQ_FREE,
    LOGIC_CQ_HEAD_UPDATE,
    SHM_OFFSET_GET,
    DEV_PROC_MAXCMD
};

enum vtsdrv_cmd {
    TSDRV_DEV_READY_NOTIFY,
    TSDRV_DEV_OPEN,
    TSDRV_DEV_RELEASE,
    TSDRV_DEV_IOCTRL,
    TSDRV_DEV_PROC,
    HVTSDRV_DEV_PROC,
    HVTSDRV_DEV_RECYCLE,
    TSDRV_DEV_MAXCMD = 50 // for incompatible problem, if one day add another cmd.
};

#define TSDRV_DEV_INITED 1
#define TSDRV_DEV_UNINITED 0

#define COMP_CODE_SUCCESS 0
#define COMP_CODE_NO_RESPONSE (-1)

/* virtual device status */
#define VTSDRV_DEV_READY 1
#define VTSDRV_DEV_NOTREADY 0

#define VTSDRV_VPC_MSG_MAXLEN 160

struct vtsdrv_dev_ready {
    u32 devid;
    struct tsdrv_id_capacity id_capacity[DEVDRV_MAX_TS_NUM];
};

struct vtsdrv_dev_open {
    ka_pid_t tgid;
    s64 unique_id;
};

struct vtsdrv_dev_release {
    ka_pid_t tgid;
};

struct vtsdrv_dev_ctrl {
    ka_pid_t tgid;
    struct devdrv_ioctl_arg tsdrv_ioctl;
};


struct vtsdrv_vcq_data_para {
    s64 unique_id;
    u32 vcq_id;
    u32 vcq_tail;
    u32 cq_type;
};

struct vtsdrv_rec_status_para {
    int recycle_status;
    u64 unique_id;
};

struct hvtsdrv_dev_proc_msg {
    u32 tsid;
    union {
        struct vtsdrv_vcq_data_para vcq_data_para;
        struct vtsdrv_rec_status_para rec_data_para;
    };
};

struct hvtsdrv_dev_proc {
    ka_pid_t tgid;
    struct hvtsdrv_dev_proc_msg proc_msg;
};

struct cb_physic_sqcq {
    u32 virt_id;

    u32 doorbell_val;

    u32 mailbox_type;
    u32 vpid;
    u32 grpid;
    u32 logic_cqid;
};

struct shm_sq_offset {
    size_t offset;
};

#define PROC_MSG_MAX_LEN 64
struct vtsdrv_dev_proc_msg {
    u32 tsid;
    union {
        struct cb_physic_sqcq cb_sqcq;
        struct shm_sq_offset shm_offset;
        u8 msg[PROC_MSG_MAX_LEN];
    };
};

struct vtsdrv_dev_proc {
    ka_pid_t tgid;
    struct vtsdrv_dev_proc_msg proc_msg;
};

struct vtsdrv_msg {
    enum vtsdrv_cmd cmd;
    u32 sub_cmd;
    s32 complete_code;
    union {
        struct vtsdrv_dev_ready vdev_ready;
        struct vtsdrv_dev_open vdev_open;
        struct vtsdrv_dev_release vdev_release;
        struct vtsdrv_dev_ctrl vdev_ctrl;
        struct vtsdrv_dev_proc vdev_proc;
        struct hvtsdrv_dev_proc hvdev_proc;
        char resv[VTSDRV_VPC_MSG_MAXLEN];
    };
};

#endif
