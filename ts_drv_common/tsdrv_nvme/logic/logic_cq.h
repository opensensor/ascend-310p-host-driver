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
#ifndef LOGIC_CQ_H
#define LOGIC_CQ_H

#include "securec.h"

#include "devdrv_cqsq.h"
#include "tsdrv_device.h"
#include "tsdrv_ioctl.h"
#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_memory_pub.h"
#define LOGIC_SQCQ_UNINIT   0
#define LOGIC_SQCQ_INIT     1

#define LOGIC_CQ_IS_FREE 0
#define LOGIC_CQ_IN_USE 1
#define LOGIC_CQ_ABANDONED 3

#define TS_BASE_SPEC_LOGIC_CQ_ID_MAX_NUM           (128)
#define TS_PREMIUM_SPEC_LOGIC_CQ_ID_MAX_NUM        (256)
#define TS_ULTIMATE_SPEC_LOGIC_CQ_ID_MAX_NUM       (512)

struct logic_cq_alloc_para {
    u32 cq_id;      // output
    u32 cq_size;    // input
    u32 cq_depth;   // input
    u32 type;
    int thread_bind_irq;
};

struct logic_cq_free_para {
    u32 cq_id;
};

struct logic_cq_sub {
    u8 useflag;
    int thread_bind_irq;
    u32 cqid;
    u32 head;
    u32 tail;
    u32 size; // cq slot size
    u32 depth; // cq depth
    u32 in_cnt;
    u32 out_cnt;
    u32 vpid;
    u32 type;
    ka_list_head_t cq_list;
    void *vaddr; // cbsq slot base vaddr
    void *ctx; // process context
    ka_mutex_t mutex;
};

struct logic_cq_info {
    u32 init;
    u32 logic_cq_num;
    ka_task_spinlock_t lock;
    ka_list_head_t cq_free_list;
    struct logic_cq_sub sub_cq[TSDRV_MAX_LOGIC_CQ_NUM];
};

#define TSDRV_INVALID_PHY_CQID 0xFFFFU
#define LOGIC_CQ_PHY_CQ_UNINIT  0U
#define LOGIC_CQ_PHY_CQ_INIT    1U
#define LOGIC_CQ_CLEANED_CQID   (TSDRV_MAX_LOGIC_CQ_NUM + 1)

struct logic_sqcq_phy_cq {
    u32 inited;
    u8 phase;
    u16 cq_irq;
    u32 index;
    u32 head;
    u32 tail;
    u32 size; // slot size
    u32 depth; // slot depth
    u32 in_cnt; // All counts are only used to assist in problem location, allowing flipping
    u32 out_cnt;
    u32 drop_cnt;
    phys_addr_t paddr;  // slot base paddr
    void *vaddr;        // slot base vaddr
};

struct logic_sqcq_ts_ctx {
    ka_pid_t pid;
    ka_pid_t tgid;
    ka_mutex_t lock;
    u32 logic_cq_num;
    u32 ctrl_logic_cq_num;
    u32 phy_cq_addr_set_once_flag;
    struct logic_sqcq_phy_cq phy_cq;
    ka_list_head_t logic_cq_list;
    ka_atomic_t wakeup_num[TSDRV_MAX_LOGIC_CQ_NUM];
    ka_atomic_t wait_thread_num[TSDRV_MAX_LOGIC_CQ_NUM];
    ka_wait_queue_head_t logic_cq_wait[TSDRV_MAX_LOGIC_CQ_NUM];
};

struct logic_sqcq {
    ka_mutex_t mutex_t;
    struct logic_cq_info logic_cq;
};

typedef struct tag_ts_logic_cq_report_msg {
    volatile uint16_t phase      : 1;
    volatile uint16_t SOP        : 1; /* start of packet, indicates this is the first 32bit return payload */
    volatile uint16_t MOP        : 1; /* middle of packet, indicates the payload is a continuation of previous task
                                      return payload */
    volatile uint16_t EOP        : 1; /* end of packet, indicates this is the last 32bit return payload. SOP & EOP
                                      can appear in the same packet, MOP & EOP can also appear on the same packet. */
    volatile uint16_t logic_cq_id  : 12;
    volatile uint16_t stream_id ;
    volatile uint16_t task_id;
    volatile uint8_t error_type;
    volatile uint8_t match_flag; /* ts set notice drv thread recv must match stream id and task id (sync task set) */
    volatile uint32_t error_code;
    volatile uint32_t reserved1;
} ts_logic_cq_report_msg_t;

typedef struct tag_logic_cq_report_msg {
    volatile uint16_t stream_id;
    volatile uint16_t task_id;
    volatile uint32_t errorCode;
    volatile uint8_t errorType;
    volatile uint8_t sqeType;
    volatile uint16_t sqId;
    volatile uint16_t sqHead;
    volatile uint16_t match_flag : 1;
    volatile uint16_t reserved0 : 15;
    volatile uint64_t timeStamp;
    volatile uint64_t reserved1;
} logic_cq_report_msg_t;

#define LOGIC_CQE_SIZE sizeof(struct tag_ts_logic_cq_report_msg)
#define LOGIC_CQE_MAX_SIZE 64
#define LOGIC_CQ_DEPTH 1024

/* create logic cq mbox msg struct */
typedef struct tag_ts_create_logic_cq {
    uint64_t phy_cq_addr;
    uint16_t cqe_size;
    uint16_t cq_depth;
    uint32_t vpid;
    uint16_t logic_cqid;
    uint16_t phy_cqid;
    uint16_t cq_irq;
    uint8_t app_flag;
    uint8_t thread_bind_irq_flag;
    uint8_t vfid;
    uint8_t rsv[3];
    uint32_t info[SQCQ_RTS_INFO_LENGTH];
} ts_create_logic_cq_t;

/* relese logic cq mbox msg struct */
typedef struct tag_ts_release_logic_cq {
    uint32_t vpid;
    uint16_t logic_cqid;
    uint16_t phy_cqid;
    uint8_t vfid;
    uint8_t rsv[3];
} ts_release_logic_cq_t;

struct logic_cq_update_head {
    uint32_t logic_cq_head;
};

struct logic_cqsq_mbox_msg {
    u16 valid;     /* validity judgement, 0x5a5a is valid */
    u16 cmd_type;  /* command type */
    u32 result;    /* TS's process result succ or fail: no error: 0, error: not 0 */
    union {
        struct tag_ts_create_logic_cq alloc;
        struct tag_ts_release_logic_cq free;
        struct logic_cq_update_head cq_head_update;
    };
};

u32 logic_cq_num_get(u32 devid, u32 fid, u32 tsid);

struct logic_cq_info *logic_cq_info_get(u32 devid, u32 fid, u32 tsid);
int logic_cq_dispatch(u32 devid, u32 fid, u32 tsid, u32 phy_cqid, struct logic_cq_info *logic_cq, struct tag_ts_logic_cq_report_msg *report);

int logic_ioctl_cq_alloc(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
int logic_ioctl_cq_wait(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
int logic_ioctl_cq_free(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);

int logic_sqcq_dev_init(u32 devid, u32 fid, u32 tsnum);
void logic_sqcq_dev_exit(u32 devid, u32 fid, u32 tsnum);

int logic_sqcq_ctx_init(struct tsdrv_ctx *ctx, u32 tsnum);
void logic_sqcq_ctx_exit(struct tsdrv_ctx *ctx, u32 tsnum);

int tsdrv_fill_logic_cqe(u32 devid, u32 fid, u32 tsid, u32 logic_cqid, struct logic_cq_info *logic_cq, logic_cq_report_msg_t *logic_cqe);
#ifdef CFG_SOC_PLATFORM_MDC_V51
void tsdrv_set_logic_cq_id_max_num(struct devdrv_info *dev_info);
#endif
int logic_cq_is_belong_to_proc(u32 devid, u32 fid, u32 tsid, u32 logic_cqid,
    struct logic_sqcq_ts_ctx *logic_ctx);

void logic_cq_send_event(struct tsdrv_ts_resource *ts_res, u32 stream_id);

#endif /* LOGIC_CQ_H */
