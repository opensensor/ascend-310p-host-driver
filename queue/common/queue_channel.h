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

#ifndef QUEUE_CHANNEL_H
#define QUEUE_CHANNEL_H

#include "ka_task_pub.h"
#include "ka_list_pub.h"
#include "ka_common_pub.h"
#include "ka_base_pub.h"
#include "ka_memory_pub.h"

#include "ascend_hal_define.h"
#include "dbl/chip_config.h"
#include "event_sched_inner.h"
#include "queue_dma.h"
#include "queue_ioctl.h"

#define DBL_NUMA_ID_MAX_NUM 64

struct queue_chan_dma_info {
    int hostpid;
    u64 serial_num;
    u64 que_chan_addr;
};

/* h2d */
typedef enum {
    QUEUE_CTRL_MSG = 0,
    QUEUE_DATA_MSG,
    QUEUE_CHAN_MSG_MAX,
} QUEUE_CHAN_MSG_TYPE;

struct queue_numa_nids {
    int nids[DBL_NUMA_ID_MAX_NUM];
    int nid_num;
};

struct queue_chan_head {
    u32 devid;
    int hostpid;
    QUEUE_CHAN_MSG_TYPE msg_type;
    struct sched_published_event_info event_info;
    char msg[EVENT_MAX_MSG_LEN];
};

#define QUEUE_CTRL_MSG_DATA_LEN  (4 * 1024)
struct queue_chan_ctrl_msg_mng {
    struct queue_chan_head head;
    char ctrl_data[QUEUE_CTRL_MSG_DATA_LEN];
    u32 ctrl_data_len;
    u32 host_timestamp;
};

enum que_chan_mem_node_type {
    QUEUE_CHAN_MEM_NODE_VA = 0,
    QUEUE_CHAN_MEM_NODE_DMA,
    QUEUE_CHAN_MEM_NODE_MAX
};

struct queue_chan_mem_node_va {
    u64 va;
    u64 len;
    u64 blks_num;
};

struct queue_chan_mem_node_dma {
    ka_dma_addr_t dma;
    u64 size;
};

struct queue_chan_mem_node {
    enum que_chan_mem_node_type mem_node_type;
    union {
        struct queue_chan_mem_node_va va_node;
        struct queue_chan_mem_node_dma dma_node;
    };
};

struct queue_chan_enque {
    struct queue_chan_head head;
    enum queue_memory_type memory_type;
    u64 size;

    u64 que_chan_addr;
    u64 serial_num;
    u32 qid;
    u32 page_size;

    u64 total_va_len;
    u32 total_va_num;
    u64 total_dma_blk_num;

    u32 max_mem_node_num;

    u32 copyed_va_node_num;
    u32 va_node_num;
    u32 dma_node_num;
    struct queue_chan_mem_node mem_node[];
};

struct queue_chan_iovec {
    u64 va;
    u64 len;
    bool dma_flag;
};

typedef int (*queue_chan_send_t)(void *data, size_t size, void *priv, int time_out);

struct queue_chan_attr {
    QUEUE_CHAN_MSG_TYPE msg_type;
    enum queue_memory_type memory_type;
    u32 devid;
    ka_pid_t host_pid;
    ka_device_t *dev;

    int chan_id;
    u64 serial_num;
    u32 qid;
    u32 remote_page_size;
    int loc_passid;
    bool hccs_vm_flag;

    struct sched_published_event_info event_info;
    char *msg;
    size_t msg_len;

    struct queue_numa_nids nids;

    void *priv; /* Private arg for send function */
    queue_chan_send_t send;
};

struct queue_chan_dma {
    struct queue_dma_list dma_list;
};

struct queue_chan {
    struct queue_chan_attr attr;
    struct queue_chan_enque *enque;

    u32 remote_page_size;

    u32 remote_total_va_num;
    u64 remote_total_va_len;
    u64 remote_total_dma_blk_num;

    u32 remote_va_num;        /* Remote va num that already added to que_chan */
    u64 remote_dma_blk_num;         /* Remote dma blk num that already added to que_chan */
    struct queue_chan_mem_node *remote_mem_node;

    u32 local_total_va_num;
    u64 local_va_len;         /* Local va len that already added to que_chan */
    u32 local_va_num;         /* Local va num that already added to que_chan */
    u64 local_dma_blk_num;          /* Local dma blk num that already added to que_chan */
    struct queue_chan_dma *local_chan_dma;

    ka_semaphore_t tx_complete;

    ka_list_head_t list;
};

struct queue_chan_copy_addr_attr {
    u32 op; /* Enque or Deque. Value is defined as QUEUE_ENQUEUE_FLAG and QUEUE_DEQUEUE_FLAG */

    u64 ctx_addr;
    u64 ctx_len;
    u64 va;
    u64 len;
};

/* HDCDRV_HUGE_PACKET_SEGMENT is (512 * 1024) */
/* HDCDRV_MEM_BLOCK_HEAD_SIZE is 61 */
#define QUEUE_CHAN_MAX_MSG_SIZE (512 * 1024 - 64)

static inline u64 queue_chan_max_msg_size(void)
{
    return (u64)QUEUE_CHAN_MAX_MSG_SIZE;
}

static inline u64 queue_chan_get_enque_size(u64 mem_node_num)
{
    return (u64)sizeof(struct queue_chan_enque) + mem_node_num * sizeof(struct queue_chan_mem_node);
}

struct queue_chan *queue_chan_create(struct queue_chan_attr *attr);
void queue_chan_destroy(struct queue_chan *que_chan);

int queue_chan_dma_create(struct queue_chan *que_chan, u32 local_total_va_num);

int queue_chan_iovec_add(struct queue_chan *que_chan, struct queue_chan_iovec *iovec);
int queue_chan_iovec_get(struct queue_chan *que_chan, struct iovec_info *iovec, u32 num,
    u32 *real_num);
int queue_chan_send(struct queue_chan *que_chan, int time_out);

int queue_chan_mem_node_create(struct queue_chan *que_chan, u32 total_va_num,
    u64 total_va_len, u64 total_dma_blk_num);
int queue_chan_enque_add(struct queue_chan *que_chan, struct queue_chan_enque *enque);
bool queue_chan_enque_add_finished(struct queue_chan *que_chan);

int queue_chan_copy_addr_add(struct queue_chan *que_chan, struct queue_chan_copy_addr_attr *attr);
int queue_chan_copy(struct queue_chan *que_chan, enum devdrv_dma_direction dir);

int queue_chan_get_iovec_size(struct queue_chan *que_chan, enum queue_dma_side side, u64 *size);
int queue_chan_get_iovec_num(struct queue_chan *que_chan, enum queue_dma_side side, u32 *num);

void queue_chan_wake_up(struct queue_chan *que_chan);
int queue_chan_wait(struct queue_chan *que_chan, int timeout);

void *queue_chan_alloc_node(struct queue_chan *que_chan, size_t size);

#endif
