/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2021. All rights reserved.
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

#ifndef __DEVDRV_MSG_H_
#define __DEVDRV_MSG_H_

#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/mutex.h>
#include <linux/jiffies.h>

#include "devdrv_dma.h"
#include "devdrv_pci.h"
#include "devdrv_common_msg.h"
#include "devdrv_interface.h"
#include "devdrv_msg_def.h"
#ifdef CFG_FEATURE_AGENT_S2S
#include "devdrv_s2s_msg.h"
#endif

#define DEVDRV_SUCCESS 0x5a
#define DEVDRV_FAILED 0xa5

/* sync or async msg type */
#define DEVDRV_MSG_SYNC (0 << 0)
#define DEVDRV_MSG_ASYNC (1 << 0)

#define DEVDRV_MSG_CHAN_IRQ_NUM 2
#define DEVDRV_MSG_CHAN_CNT_3559 2

#define DEVDRV_MSG_CHAN_NUM_FOR_NON_HDC 1
#define DEVDRV_DEV_HDC_LITE_MSG_CHAN_CNT_MAX 17

#define DEVDRV_MSG_MAGIC 0x5a6b

#define DEVDRV_STR_NAME_LEN     32

typedef union {
    /* Define the struct bits */
    struct {
        u32 dev_id : 16;    /* [15..0] */
        u32 chan_id : 16;   /* [31..16] */
        u32 magic : 16;     /* [47..32] */
        u32 reserved : 16;  /* [63..48] */
    } bits;

    /* Define an u64 member */
    u64 value;
} DEVDRV_MSG_HANDLE;

struct devdrv_msg_queue_info {
    u32 depth;
    u32 desc_size;
    dma_addr_t dma_handle; /* host alloc msg queue dma addr */
    void *desc_h;
    void *desc_d;
    u32 slave_mem_offset; /* host & slave reserve mem offset */
    u32 head_h; /* host alloc msg queue head */
    u32 tail_h;
    u32 head_d; /* host & slave reserve msg queue head */
    u32 tail_d;
    s32 irq_vector;
    void *base_reserve_h; /* host reserve msg queue virt addr */
    dma_addr_t dma_reserve_h; /* host reserve msg queue dma addr */
    dma_addr_t dma_reserve_d; /* device reserve msg queue dma addr */
};

struct devdrv_msg_chan_stat {
    u64 tx_total_cnt;
    u64 tx_success_cnt;
    u64 tx_no_callback;
    u64 tx_len_check_err;
    u64 tx_reply_len_check_err;
    u64 tx_status_abnormal_err;
    u64 tx_irq_timeout_err;
    u64 tx_timeout_err;
    u64 tx_process_err;
    u64 tx_invalid_para_err;
    u64 rx_total_cnt;
    u64 rx_success_cnt;
    u64 rx_para_err;
    u64 rx_work_max_time;
    u64 rx_work_delay_cnt;
};

struct devdrv_msg_chan_sched_status {
    u64 schedule_in;
    u64 schedule_in_last;
    int no_schedule_cnt;
    atomic_t state;
};

struct devdrv_msg_chan {
    struct devdrv_msg_dev *msg_dev;
    u32 chan_id;
    u64 seq_num;
    u32 status; /* if this channel is used */
    void *priv; /* inited by vnic */
    u32 flag;   /* sync or async:bit0 */
    struct devdrv_msg_queue_info sq_info;
    struct devdrv_msg_queue_info cq_info;
    void __iomem *io_base; /* the address of SQ tail db */
    enum devdrv_msg_client_type msg_type;
    int irq_rx_msg_notify;
    int irq_tx_finish_notity;
    void (*rx_msg_notify)(void *msg_chan);
    void (*tx_finish_notify)(void *msg_chan);
    int (*rx_msg_process)(void *msg_chan, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len);
    int rx_work_flag;
    struct work_struct rx_work;
    struct mutex mutex; /* tx mutex */
    struct mutex rx_mutex; /* rx mutex */
    u64 stamp;
    struct devdrv_msg_chan_stat chan_stat;
    enum msg_queue_type queue_type;
    struct devdrv_msg_chan_sched_status sched_status;
};

struct devdrv_msg_slave_mem {
    u32 offset;
    u32 len;
};

struct devdrv_msg_slave_mem_node {
    struct devdrv_msg_slave_mem mem;
    struct list_head list;
};

struct devdrv_non_trans_msg_send_data_para {
    void *data;
    u32 in_data_len;
    u32 out_data_len;
    u32 *real_out_len;
};

struct devdrv_msg_dev {
    struct devdrv_pci_ctrl *pci_ctrl;
    struct device *dev;
    void __iomem *db_io_base;   /* the base addr of doorbell */
    void __iomem *ctrl_io_base; /* the base addr of nvme ctrl reg */
    void __iomem *reserve_mem_base; /* device reserve mem base, access by ATU */
    void __iomem *local_reserve_mem_base; /* host reserve mem base */
    u32 chan_cnt;
    u32 func_id;
    struct devdrv_common_msg common_msg;
    struct mutex mutex;
    struct devdrv_msg_chan *admin_msg_chan;
    void *agent_smmu_chan;

    struct workqueue_struct *work_queue[DEVDRV_MAX_MSG_CHAN_NUM];

    struct devdrv_msg_slave_mem slave_mem;
    struct list_head slave_mem_list; /* for realloc msg chan */
#ifdef CFG_FEATURE_AGENT_S2S
    struct devdrv_s2s_msg_chan s2s_chan[DEVDRV_S2S_SUPPORT_MAX_CHAN_NUM];
    struct devdrv_s2s_non_trans_ctrl s2s_non_trans;
#endif
    /* msg_chan must be the last element */
    struct devdrv_msg_chan *msg_chan;
};

struct devdrv_msg_chan_info {
    enum devdrv_msg_client_type msg_type;
    enum msg_queue_type queue_type;
    u32 level;
    u32 queue_depth;
    /* param for trans */
    u32 sq_desc_size;
    u32 cq_desc_size;
    void (*rx_msg_notify)(void *msg_chan);
    void (*tx_finish_notify)(void *msg_chan);
    /* param for non-trans */
    u32 flag; /* sync or async:bit0 */
    int (*rx_msg_process)(void *msg_chan, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len);
};

struct devdrv_msg_chan *devdrv_alloc_trans_queue(void *priv, struct devdrv_trans_msg_chan_info *chan_info);
int devdrv_free_trans_queue(struct devdrv_msg_chan *msg_chan);

struct devdrv_msg_chan *devdrv_alloc_non_trans_queue(void *priv, struct devdrv_non_trans_msg_chan_info *chan_info);
int devdrv_free_non_trans_queue(struct devdrv_msg_chan *msg_chan);

int devdrv_msg_init(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_msg_exit(struct devdrv_pci_ctrl *pci_ctrl);

int devdrv_sync_non_trans_msg_send(struct devdrv_msg_chan *msg_chan, void *data, u32 in_data_len, u32 out_data_len,
                                   u32 *real_out_len, enum devdrv_common_msg_type msg_type);

int devdrv_get_general_interrupt_db_info(struct devdrv_msg_dev *msg_dev, u32 *db_start, u32 *db_num);
int devdrv_notify_dev_online(struct devdrv_msg_dev *msg_dev, u32 devid, u32 status);
void devdrv_set_device_status(struct devdrv_pci_ctrl *pci_ctrl, u32 status);

int devdrv_admin_msg_chan_send(struct devdrv_msg_dev *msg_dev, enum devdrv_admin_msg_opcode opcode, const void *cmd,
                               size_t size, void *reply, size_t reply_size);
int devdrv_get_rx_atu_info(struct devdrv_pci_ctrl *pci_ctrl, u32 bar_num);
void devdrv_msg_chan_guard_work_sched(struct devdrv_pci_ctrl *pci_ctrl);
struct devdrv_msg_chan *devdrv_get_msg_chan(const void *chan_handle);
struct devdrv_msg_chan *devdrv_find_msg_chan(const void *chan_handle);
void devdrv_put_msg_chan(const struct devdrv_msg_chan *chan);
void *devdrv_generate_msg_handle(const struct devdrv_msg_chan *chan);
void devdrv_free_msg_queue_res(struct devdrv_msg_chan *msg_chan);
int devdrv_init_admin_msg_chan(struct devdrv_msg_dev *msg_dev);
int devdrv_msg_chan_init(struct devdrv_msg_dev *msg_dev, int chan_start, int chan_end, int irq_base);
#endif
