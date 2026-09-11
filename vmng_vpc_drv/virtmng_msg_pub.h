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

#ifndef VIRTMNG_MSG_DEF_H
#define VIRTMNG_MSG_DEF_H
#include "virtmng_resource.h"
#include "virtmng_interface.h"
#include "virtmng_msg_common.h"

#include "ka_list_pub.h"
#include "ka_task_pub.h"

/* config */
#define VMNG_MSG_CHAN_NUM_MAX VMNG_IRQ_NUM_FOR_MSG

/* msg device status */
#define VMNG_MSG_DEV_ALIVE      0   /* msg dev is normal */
#define VMNG_MSG_DEV_REMOVE     1   /* remote vpc is removed */
#define VMNG_MSG_DEV_DEAD       2   /* heartbeat lost or npu offline */

/* message status */
#define VMNG_MSG_CHAN_STATUS_DISABLE 0x0
#define VMNG_MSG_CHAN_STATUS_IDLE 0x1
#define VMNG_MSG_CHAN_STATUS_USED 0x2

/* message cluster status */
#define VMNG_MSG_CLUSTER_STATUS_DISABLE 0x0
#define VMNG_MSG_CLUSTER_STATUS_INIT 0x1
#define VMNG_MSG_CLUSTER_STATUS_ENABLE 0x2

#define VMNG_MSG_SQ_STATUS_IDLE 0x0
#define VMNG_MSG_SQ_STATUS_PREPARE 0x112
#define VMNG_MSG_SQ_STATUS_ENTER_PROC 0x667
#define VMNG_MSG_SQ_STATUS_NO_PROC 0x223
#define VMNG_MSG_SQ_STATUS_PROC_RET_FAILED 0x334
#define VMNG_MSG_SQ_STATUS_PROC_DATA_ERROR 0x445
#define VMNG_MSG_SQ_STATUS_PROC_SUCCESS 0x556

/* message queue */
#define VMNG_MSG_QUEUE_BASE 0x1000
#define VMNG_MSG_QUEUE_SQ_SIZE 0x21000
#define VMNG_MSG_SQ_TXRX 2

#define VMNG_BLK_STATUS 0x221
#define VMNG_NO_BLK_STATUS 0x112

// schedule time & process time dfx
#define VMNG_MSG_WORK_SCHE_TIME 10
#define VMNG_MSG_PROCESS_TIME 2000

#define VMNG_MSG_TX_FINISH_WAIT_CNT 10

struct vmng_msg_desc {
    u32 status;
    u32 in_data_len;
    u32 out_data_len;
    u32 real_out_len;
    u32 opcode_d1; /* cluster msg, first degree index. */
    u32 opcode_d2; /* operation in cluster, second degree index. */
    char data[];
};

#define VMNG_MSG_SQ_DATA_MAX_SIZE (VMNG_MSG_QUEUE_SQ_SIZE - sizeof(struct vmng_msg_desc))

/* module that need block msg */
enum vmng_msg_block_type {
    VMNG_MSG_BLOCK_TYPE_HDC = 0,
    VMNG_MSG_BLOCK_TYPE_MAX,
};

/* three chan type : admin, common, vpc */
enum vmng_msg_chan_type {
    VMNG_MSG_CHAN_TYPE_ADMIN = 0x0,
    VMNG_MSG_CHAN_TYPE_COMMON,
    VMNG_MSG_CHAN_TYPE_VPC = VMNG_MSG_CHAN_TYPE_COMMON + VMNG_VPC_TYPE_MAX,
    VMNG_MSG_CHAN_TYPE_BLOCK = VMNG_MSG_CHAN_TYPE_VPC + VMNG_MSG_BLOCK_TYPE_MAX,
    VMNG_MSG_CHAN_TYPE_MAX // fixed value
};
#define VMNG_VPC_TO_MSG_CHAN_OFFSET 0x2
#define VMNG_BLK_TO_MSG_CHAN_OFFSET (VMNG_MSG_CHAN_TYPE_VPC + 1)

struct vmng_msg_chan_rx_proc_info {
    void *data;
    u32 in_data_len;
    u32 out_data_len;
    u32 *real_out_len;
    u32 opcode_d1;
    u32 opcode_d2;
};

struct vmng_msg_chan_tx {
    enum vmng_msg_chan_type chan_type;
    u32 status;
    u32 chan_id;
    u32 trig_time;
    void *msg_cluster;
    void *msg_dev;
    void *sq_tx;
    void (*send_irq_to_remote)(void *msg_dev, u32 vector);
    ka_wait_queue_head_t tx_block_wq;
    u32 tx_block_status;
    ka_mutex_t mutex;
    u32 tx_send_irq; /* from msg_dev->db_base or msix_base */
    u32 tx_finish_irq;
    u32 sq_size;
};

struct vmng_msg_chan_rx {
    enum vmng_msg_chan_type chan_type;
    u32 status;
    u32 chan_id;
    u32 trig_time;
    void *msg_cluster;
    void *msg_dev;
    void *sq_rx;
    void *sq_rx_safe_data;
    ka_workqueue_struct_t *rx_wq;
    ka_work_struct_t rx_work; /* rx */
    int (*rx_proc)(void *msg_chan, struct vmng_msg_chan_rx_proc_info *proc_info);
    void (*send_int)(void *msg_chan);
    u32 rx_recv_irq;
    u32 resp_int_id;
    u32 sq_size;
    u64 stamp; // record schedule work & callback func consume time
};

struct vmng_msg_chan_res {
    u32 tx_base;
    u32 tx_num;
    u32 rx_base;
    u32 rx_num;
};

struct vmng_msg_chan_irqs {
    u32 *tx_send_irq;
    u32 *tx_finish_irq;
    u32 *rx_recv_irq;
    u32 *rx_resp_irq;
};

struct vmng_msg_ops {
    void (*send_irq_to_remote)(void *msg_dev, u32 vector);
    int (*tx_irq_init)(struct vmng_msg_chan_tx *msg_chan);
    int (*tx_irq_uninit)(struct vmng_msg_chan_tx *msg_chan);
    int (*rx_irq_init)(struct vmng_msg_chan_rx *msg_chan);
    int (*rx_irq_uninit)(struct vmng_msg_chan_rx *msg_chan);
};

typedef int (*vmng_msg_rx_recv_proc)(void *msg_chan_in, struct vmng_msg_chan_rx_proc_info *proc_info);
typedef void (*vmng_msg_tx_finsih_proc)(unsigned long data);

struct vmng_msg_proc {
    vmng_msg_rx_recv_proc rx_recv_proc;
    vmng_msg_tx_finsih_proc tx_finish_proc;
};

struct vmng_msg_cluster {
    enum vmng_msg_chan_type chan_type;
    u32 dev_id;
    u32 fid;
    u32 status;
    void *msg_dev;
    struct vmng_msg_proc msg_proc;
    ka_mutex_t mutex;             /* tx */
    ka_semaphore_t cluster_sema;
    ka_wait_queue_head_t tx_alloc_wq;  /* tx */
    struct vmng_stack *alloc_stack; /* tx */
    ka_wait_queue_head_t tx_data_wq;   /* tx */
    struct vmng_msg_chan_rx *msg_chan_rx_beg;
    struct vmng_msg_chan_tx *msg_chan_tx_beg;
    struct vmng_msg_chan_res res;
};

enum vmng_msg_dev_type {
    VMNG_MSG_DEV_UNDEFINE = 0,
    VMNG_MSG_DEV_NORMAL,
    VMNG_MSG_DEV_SRIOV
};

struct vmng_msg_dev {
    void *unit;
    ka_workqueue_struct_t *work_queue;
    void __iomem *db_base; /* db base from zero, so db_base + db_irq_base *4 is the first doorbell for msg */
    void __iomem *mem_base;
    struct vmng_msg_ops ops;
    struct vmng_msg_cluster msg_cluster[VMNG_MSG_CHAN_TYPE_MAX];
    struct vmng_msg_chan_tx *admin_tx;
    struct vmng_msg_chan_rx *admin_rx;
    struct vmng_msg_chan_tx msg_chan_tx[VMNG_MSG_CHAN_NUM_MAX];
    struct vmng_msg_chan_rx msg_chan_rx[VMNG_MSG_CHAN_NUM_MAX];
    struct vmng_msg_common common_msg;
    struct vmng_vpc_client vpc_clients[VMNG_VPC_TYPE_MAX];
    u32 dev_id;
    u32 fid;
    u32 chan_num;
    u32 msix_irq_base;
    u32 db_irq_base;
    u32 admin_db_id;
    enum vmng_msg_dev_type msg_dev_type;
    u32 status;
    ka_list_head_t list;
};

bool vmng_is_vpc_chan(enum vmng_msg_chan_type chan_type);
bool vmng_is_blk_chan(enum vmng_msg_chan_type chan_type);

enum vmng_msg_block_type vmng_vpc_to_blk_type(enum vmng_vpc_type vpc_type);
enum vmng_vpc_type vmng_blk_to_vpc_type(enum vmng_msg_block_type blk_type);

enum vmng_msg_chan_type vmng_vpc_type_to_msg_chan_type(enum vmng_vpc_type vpc_type);
enum vmng_vpc_type vmng_msg_chan_type_to_vpc_type(enum vmng_msg_chan_type chan_type);

enum vmng_msg_chan_type vmng_block_type_to_msg_chan_type(enum vmng_msg_block_type block_type);
enum vmng_msg_block_type vmng_msg_chan_type_to_block_type(enum vmng_msg_chan_type chan_type);

int vmng_msg_chan_tx_info_para_check(const struct vmng_tx_msg_proc_info *tx_info);

void vmng_msg_tx_finish_task(unsigned long data);
void vmng_msg_rx_msg_task(ka_work_struct_t *p_work);
void vmng_msg_push_rx_queue_work(struct vmng_msg_chan_rx *msg_chan);

int vmng_msg_fill_desc(const struct vmng_tx_msg_proc_info *tx_info, u32 opcode_d1, u32 opcode_d2,
    struct vmng_msg_chan_tx *msg_chan, u32 **p_sq_status);
int vmng_sync_msg_send(struct vmng_msg_cluster *msg_cluster, struct vmng_tx_msg_proc_info *tx_info, u32 opcode_d1,
    u32 opcode_d2, u32 timeout);
int vmng_sync_msg_wait_undesire(const u32 *status, int status_init, int status_enter_proc, u32 cycle, u32 len);
int vmng_msg_reply_data(struct vmng_msg_chan_tx *msg_chan, void *data, u32 out_data_len, u32 *real_out_data_len);

int vmng_alloc_local_msg_cluster(struct vmng_msg_dev *msg_dev, enum vmng_msg_chan_type chan_type,
    const struct vmng_msg_chan_res *res, struct vmng_msg_chan_irqs *int_irq_ary, struct vmng_msg_proc *msg_proc);
void vmng_free_msg_cluster(struct vmng_msg_dev *msg_dev, enum vmng_msg_chan_type chan_type);

int vmng_msg_chan_init(u32 side, struct vmng_msg_dev *msg_dev);
void vmng_free_msg_dev(struct vmng_msg_dev *msg_dev);

void vmng_msg_dfx_resq_time(u32 dev_id, u32 fid, struct vmng_msg_chan_rx *msg_chan, const char *errstr, u32 timeout);
#endif
