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

#ifndef QUEUE_IOCTL_H
#define QUEUE_IOCTL_H

#include "ascend_hal_define.h"
#include "event_sched_inner.h"
#ifndef __KERNEL__
#include <sys/ioctl.h>
#define _KA_IOWR _IOWR
#define _KA_IOW  _IOW
#else
#include "ka_ioctl_pub.h"
#endif

#define DAVINCI_QUEUE_SUB_MODULE_NAME "QUEUE"
#define QUEUE_DEV_FULL_NAME   "/dev/hi-queue-manage"

#define QUEUE_ENQUEUE_FLAG 0
#define QUEUE_DEQUEUE_FLAG 1

enum queue_memory_type {
    QUEUE_BUFF,
    QUEUE_BARE_BUFF,
    QUEUE_TYPE_MAX
};

enum queue_host_common_op {
    QUEUE_INIT,
    QUEUE_UNINIT,
    QUEUE_OP_MAX
};

struct queue_ioctl_host_common_op {
    unsigned int devid;
    unsigned int op_flag;
};

struct queue_ioctl_enqueue {
    struct sched_published_event_info event_info;
    unsigned int devid;
    unsigned int qid;
    unsigned int type;
    int time_out;
    unsigned int iovec_count;
    struct buff_iovec *vector;
};

struct queue_init_para {
    unsigned int devid;
    int hostpid;
};

struct queue_ioctl_ctrl_msg_send {
    struct sched_published_event_info event_info;
    unsigned int devid;
    void *ctrl_data_addr;
    unsigned int ctrl_data_len;
    unsigned int host_timestamp;
};

struct queue_ctrl_data_search {
    unsigned long long serial_num;
    unsigned int ctrl_data_len;
    unsigned int host_timestamp;
};

struct queue_ioctl_ctrl_msg_recv {
    struct queue_ctrl_data_search search_flag;
    void *ctrl_data_addr;
};

struct queue_ioctl_copy {
    unsigned int op_flag;
    unsigned int devid;
    unsigned int qid;
    unsigned int type;
    int hostpid;
    void *ctx_addr;
    unsigned long long ctx_addr_len;
    void *addr;
    unsigned long long addr_len;
    unsigned int copy_flag;
    unsigned int count;
    struct iovec_info *ptr;
};

union queue_req_arg {
    struct queue_ioctl_copy copy_arg;
};

struct queue_ioctl_reply {
    unsigned int devid;
    unsigned int qid;
    int hostpid;
};

struct queue_ioctl_iovec_num {
    unsigned int devid;
    unsigned int qid;
    int hostpid;
    unsigned int num;
};

/* user agent and kernel use */
#define MSG_MAX_LEN 64
struct queue_mcast_para {
    unsigned int gid : 8;
    unsigned int mcast_flag : 1;
    unsigned int rsv : 1;
    unsigned int event_sn : 22;
};

struct queue_common_para {
    unsigned int devid : 16;
    unsigned int qid : 16;
    unsigned int host_timestamp;
    struct queue_mcast_para mcast_para;
};

struct queue_event_msg_head {
    struct event_sync_msg sync;
    struct queue_common_para comm;
};

#define QUEUE_IOC_MAGIC                       'Q'
#define QUEUE_HOST_COMMON_OP_CMD              _KA_IOW(QUEUE_IOC_MAGIC, 1, struct queue_ioctl_host_common_op)
#define QUEUE_ENQUEUE_CMD                     _KA_IOW(QUEUE_IOC_MAGIC, 2, struct queue_ioctl_enqueue)
#define QUEUE_COPY_CMD                        _KA_IOWR(QUEUE_IOC_MAGIC, 3, struct queue_ioctl_copy)
#define QUEUE_REPLY_CLIENT_CMD                _KA_IOW(QUEUE_IOC_MAGIC, 4, struct queue_ioctl_reply)
#define QUEUE_SESSION_INIT                    _KA_IOW(QUEUE_IOC_MAGIC, 5, struct queue_init_para)
#define QUEUE_CTRL_MSG_SEND_CMD               _KA_IOW(QUEUE_IOC_MAGIC, 6, struct queue_ioctl_ctrl_msg_send)
#define QUEUE_CTRL_MSG_RECV_CMD               _KA_IOWR(QUEUE_IOC_MAGIC, 7, struct queue_ioctl_ctrl_msg_recv)
#define QUEUE_GET_IOVEC_NUM_CMD               _KA_IOWR(QUEUE_IOC_MAGIC, 8, struct queue_ioctl_iovec_num)

#define QUEUE_CMD_MAX                         9

#endif
