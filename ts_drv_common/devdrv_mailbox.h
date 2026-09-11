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
#ifndef DEVDRV_MAILBOX_H
#define DEVDRV_MAILBOX_H

#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_memory_pub.h"
#include "ka_system_pub.h"
#include "ka_list_pub.h"
#include "kernel_version_adapt.h"
#include "tsdrv_interface.h"

#define DEVDRV_MAILBOX_PAYLOAD_LENGTH 64
#define DEVDRV_MAILBOX_SEND_SRAM (SOC_TS_SRAM_REG - SOC_TS_BASE) /* (0x1F0200000) */
#define DEVDRV_MAILBOX_RECEIVE_SRAM (DEVDRV_MAILBOX_SEND_SRAM + 64)

#define DEVDRV_MAILBOX_SEND_ONLINE_IRQ 0

#define DEVDRV_MAILBOX_READ_DONE_IRQ 327

#define DEVDRV_MAILBOX_SRAM 0
#define DEVDRV_MAILBOX_IPC 1

#define DEVDRV_MAILBOX_SYNC 0
#define DEVDRV_MAILBOX_ASYNC 1

#define DEVDRV_MAILBOX_FREE 0
#define DEVDRV_MAILBOX_BUSY 1

#define DEVDRV_MAILBOX_NO_FEEDBACK 0
#define DEVDRV_MAILBOX_NEED_FEEDBACK 1

#define DEVDRV_MAILBOX_SYNC_MESSAGE 1
#define DEVDRV_MAILBOX_CONTEXT_ASYNC_MESSAGE 2
#define DEVDRV_MAILBOX_KERNEL_ASYNC_MESSAGE 3

#define DEVDRV_MAILBOX_VALID_MESSAGE 0
#define DEVDRV_MAILBOX_RECYCLE_MESSAGE 1

#define DEVDRV_MAILBOX_DOWN_TRY_TIMES 5
#define DEVDRV_MAILBOX_MESSAGE_VALID 0x5A5A

struct devdrv_mailbox_sending_queue {
    ka_task_spinlock_t spinlock;
    volatile int status; /* mailbox busy or free */
    int mailbox_type;    /* mailbox communication method: SPI+SRAM or IPC */
    ka_workqueue_struct_t *wq;
    ka_list_head_t list_header;
};

struct devdrv_mailbox_waiting_queue {
    ka_task_spinlock_t spinlock;
    int mailbox_type; /* mailbox communication method: SPI+SRAM or IPC */
    ka_workqueue_struct_t *wq;
    ka_list_head_t list_header;
};

struct devdrv_mailbox_message {
    u8 message_payload[DEVDRV_MAILBOX_PAYLOAD_LENGTH];
    int message_length;
    ka_semaphore_t wait; /* for synchronizing */
    int feedback;          /* need feedback or no need */
    int feedback_num;      /* number of expect ackdata */
    u8 *feedback_buffer;   /* buffer to save feedback data, must alloc memory when message inited */
    int feedback_count;    /* counter of received feedback */
    int process_result;    /* result of processing mailbox by TS */
    u32 hwts_sqid;         /* in mc2 feature, ts will come back hwts sqid to tsdrv for calc sq head/tail reg addr */
    int sync_type;         /* sync or nonsync */
    int cmd_type;          /* related to task */
    int message_index;     /* message index when there is several message within one task */
    int message_pid;       /* who am i(process who send this message) */
    ka_work_struct_t send_work;
    ka_work_struct_t wait_work;
    ka_list_head_t send_list_node;
    ka_list_head_t wait_list_node;
    void (*callback)(void *data);
    struct devdrv_mailbox *mailbox;
    struct tsdrv_ts_ctx *ctx;
    struct tsdrv_ts_resource *ts_resource;
    struct devdrv_info *dev_info;

    int abandon; /* init with zero value, set 1 mean that this message should be abandoned */

    int message_type; /* use for enter different message process branch, user donot need to set this para */
    /*
     * sync_message
     * context_async_message
     * kernel_async_message
     */
    u8 is_sent;
};

#define TSDRV_CONT_TIMEOUT_CNT      3
#define TSDRV_DFX_MAX_TIMES      3

struct devdrv_mailbox {
    u8 __iomem *send_sram;
    u8 __iomem *receive_sram;

    struct devdrv_mailbox_message message;

    int ack_irq;
    int data_ack_irq;
    volatile int working;

    ka_atomic_t status;  /* mailbox busy or free */
    volatile int mailbox_type;    /* mailbox communication method: SPI+SRAM or IPC */
    ka_atomic_t timeout;
    u32 dfx_times;

    ka_mutex_t mlock;
#ifndef TSDRV_KERNEL_UT
#ifndef TSDRV_UT
    // mailbox dfx status
    u64 send;      /* record mailbox send time */
    u64 down_timeout;   /* record mailbox down_timeout time */
    u64 irq_time;  /* record mailbox irq receive time */
#endif
#endif
};

struct devdrv_mailbox_notice_ack_irq {
    struct devdrv_mailbox_message_header head;
    u32 ack_irq;
};

#ifndef CFG_TRS_REFACTOR_FEATURE
#define MAX_RDMA_INFO_LEN 56
struct rdma_info {
    struct devdrv_mailbox_message_header header;
    u8 buf[MAX_RDMA_INFO_LEN];
};
#endif

/* inform stream id to ts recycle resource */
#define MAX_INFORM_STREAM_INFO 6
struct devdrv_stream_ts_info {
    u32 stream_cnt;
    int stream[MAX_INFORM_STREAM_INFO];
    u32 status[MAX_INFORM_STREAM_INFO];
    u8 plat_type;
    u8 reserved[3];
};

struct devdrv_mailbox_stream_info {
    struct devdrv_mailbox_message_header header;
    struct devdrv_stream_ts_info stream_info;
};

struct hwts_addr_info {
    struct devdrv_mailbox_message_header header;
    u8 local_devid;
    u8 host_devid;
};

int tsdrv_mailbox_kernel_sync_no_feedback(u32 devid, u32 tsid, const u8 *buf, u32 len, int *result);
int devdrv_mailbox_kernel_sync_no_feedback(struct devdrv_mailbox *mailbox, const u8 *buf, u32 len, int *result);
ka_irqreturn_t devdrv_mailbox_feedback_irq(int irq, void *data);
int devdrv_send_rdmainfo_to_ts(u32 dev_id, const u8 *buf, u32 len, int *result);
int devdrv_send_hwts_addr_to_ts(u32 dev_id, u32 tsid, u32 local_devid, u32 host_id);

int tsdrv_mbox_init(u32 devid, u32 tsnum);
void tsdrv_mbox_exit(u32 devid, u32 tsnum);
int tsdrv_mailbox_send_sync(u32 devid, u32 tsid, struct tsdrv_mbox_data *data);


#endif /* DEVDRV_MAILBOX_H */
