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

#ifndef DEVDRV_CB_H
#define DEVDRV_CB_H

#include "devdrv_common.h"
#include "tsdrv_device.h"
#include "devdrv_cqsq.h"

#define CALLBACK_UNINIT 0
#define CALLBACK_INIT 1
#define CALLBACK_MBOX_SENDED 1
#define CALLBACK_MBOX_SQCQ_ALLOC 0
#define CALLBACK_MBOX_SQCQ_FREE 1

#if ((defined CFG_SOC_PLATFORM_MINI) && (!defined CFG_SOC_PLATFORM_MINIV2))
# define CALLBACK_MAX_CQ_NUM 0
#elif (defined CFG_SOC_PLATFORM_MDC_V51)
/**
 * The MDC has two TS. Each TS uses 4 MB memory (defined as CQ_RESERVE_MEM_SIZE).
 * Each CQ occupies 16 KB memory (defined as CQ_RESERVE_MEM_CQ_OFFSET).
 * Therefore, each TS supports a maximum of 256 physical CQs.
 *
 * Currently, a maximum of 352 physical CQs (defined as DEVDRV_MAX_CQ_NUM) are supported.
 * According to the TS, the CQ numbered 351 is a callback CQ. Therefore, the macro is set
 * to 96(352-256), indicating that the first 256 sets are used by the normal CQ,
 * and the second 96 sets are used by the callback. (Actually, only 351 is used by the MDC.)
 */
# define CALLBACK_MAX_CQ_NUM 96
#elif ((defined CFG_SOC_PLATFORM_MINIV3) || (defined CFG_SOC_PLATFORM_CLOUD_V2))
# define CALLBACK_MAX_CQ_NUM 0
#else
# define CALLBACK_MAX_CQ_NUM 18  /* pm:350~351, vm:334~349 */
#endif

#define CALLBACK_PHYSICAL_CQ_NUM 2
#define CALLBACK_HOSTCQ_INDEX 350
#define CALLBACK_DEVICECQ_INDEX 351

#define CALLBACK_CQ_IS_FREE      0
#define CALLBACK_CQ_IN_USE       1
#define CALLBACK_CQ_COPY_REPORT  2

/* cqid 334 to 350 are reserved for the callback feature.
 * 350--fid0,
 * 349--fid1,
 * ...
 */
#define GET_CALLBACK_CQ_ID(x) (CALLBACK_HOSTCQ_INDEX - (x))

#define DEVDRV_MIN_CBCQ_SIZE 32U
#define DEVDRV_MAX_CBCQ_SIZE 64U
#define DEVDRV_MAX_CBCQ_DEPTH 1024U

#define SOC_PLATFORM_MINIV2 1
#define SOC_NORMAL_BOARD 0
#define CALLBACK_RESERVED_MEM_ADDR (0x2C981000 - 0x20000) /* MINIV2 callback reserved mem */

struct callback_phy_sqcq_info {
    u8 phase;
    u32 index;
    u32 virtid;
    u32 head;
    u32 tail;
    u32 size; // slot size
    u32 depth; // slot depth
    u32 in_cnt; // All counts are only used to assist in problem location, allowing flipping
    u32 out_cnt;
    u32 drop_cnt;
    phys_addr_t paddr;  // slot base paddr
    void *vaddr;        // slot base vaddr
    void *vvaddr;       // vsq slot base vaddr
    ka_task_spinlock_t lock;
};
struct callback_phy_sqcq {
    u32 devid;
    u32 fid;
    u32 tsid;
    u32 init;
    u32 send_flag;
    u32 plat_type;
    u32 board_type;
    void __iomem *doorbell_vaddr;
    struct callback_phy_sqcq_info cb_sq;
    struct callback_phy_sqcq_info cb_cq;
};

struct callback_logic_cq_sub {
    u8 useflag;
    u32 cqid;
    u32 head;
    u32 tail;
    u32 size; // cq slot size
    u32 depth; // cq depth
    u32 in_cnt;
    u32 out_cnt;
    u32 vpid;
    u32 grpid; // indicats thread group id, 0~127
    ka_list_head_t cq_list;
    void *vaddr; // cbsq slot base vaddr
    void *ctx; // process context
};

struct callback_logic_cq {
    u32 init;
    u32 devid;
    u32 fid;
    u32 tsid;
    u32 plat_type;
    ka_task_spinlock_t lock;
    ka_list_head_t cq_free_list;
    ka_list_head_t cq_alloc_list;
    struct callback_logic_cq_sub sub_cq[DEVDRV_CB_CQ_MAX_NUM];
};

struct callback_info {
    ka_mutex_t mutex_t;
    struct callback_logic_cq logic_cq;
    struct callback_phy_sqcq phy_sqcq;
};

struct cbcq_report {
    u16 phase : 1;
    u16 SOP : 1;    /* start of packet, indicates this is the first 32bit return payload */
    u16 MOP : 1;    /* middle of packet, indicates the payload is a continuation of previous task return payload */
    u16 EOP : 1;    /* end of packet, indicates this is the last 32bit return payload.
                        SOP & EOP can appear in the same packet, MOP & EOP can also appear on the same packet. */
    u16 cq_id : 12; /* logic cq id */
    u16 stream_id;
    u16 task_id;
    u16 sq_id; /* physical sq id */
    u16 sq_head; /* physical sq head */
    u16 sequence_id; /* for match */
    u8 is_block;
    u8 reserved[3];
    u64 host_func_cb_ptr;
    u64 fn_data_ptr;
};

#pragma pack(1)
struct tsdrv_cb_task {
    uint16_t cqid;
    uint8_t is_block;
    uint8_t reserved1[3];
    uint64_t host_func_cb_ptr;
    uint64_t fn_data_ptr;
};

struct tsdrv_normal_task {
    uint32_t pid;
    uint16_t grp_id;
    uint16_t tid;
    uint16_t subevent_id;
    uint8_t event_id;
    uint8_t msg_len;
    uint8_t msg[8];
    uint16_t phy_devid;
};

typedef struct tsdrv_cpu_task {
    uint8_t phase : 1;
    uint8_t reserved0 : 7;
    uint8_t task_type; /* 0:cb task; 1:normal task */
    uint16_t sqid;
    uint16_t sq_head;
    uint16_t stream_id;
    uint16_t task_id;
    union {
        struct tsdrv_cb_task cb_task;
        struct tsdrv_normal_task normal_task;
    } task;
} tsdrv_cpu_task_t;
#pragma pack()

struct callback_sqcq_mbox_free {
    u32 sq_index;
    u32 cq_index;
    u8 plat_type;
    u8 reserved[3]; /* reserved */
};

struct callback_sqcq_mbox_alloc {
    u64 sq_addr;
    u64 cq_addr;
    u32 sq_index;   /* sq index */
    u32 cq_index;
    u16 sqe_size;
    u16 cqe_size;
    u16 sq_depth;   /* sq depth */
    u16 cq_depth;
    u8 plat_type;
    u8 reserved[3]; /* reserved */
    u32 cq_irq;
};

struct callback_cqsq_mbox {
    u16 valid;     /* validity judgement, 0x5a5a is valid */
    u16 cmd_type;  /* command type */
    u32 result;    /* TS's process result succ or fail: no error: 0, error: not 0 */
    union {
        struct callback_sqcq_mbox_alloc alloc;
        struct callback_sqcq_mbox_free free;
    };
};

struct callback_logic_cq_mbox {
    u16 valid;     /* validity judgement, 0x5a5a is valid */
    u16 cmd_type;  /* command type */
    u32 result;    /* TS's process result succ or fail: no error: 0, error: not 0 */
    u32 vpid;
    u32 grpid;
    u32 logic_cqid;
    u32 phy_cqid;
    u32 cq_irq;
    u32 phy_sqid;
    u8 plat_type;
    u8 reserved[3]; /* reserved */
};

void *callback_kvmalloc(size_t size);
void callback_kvfree(const void *addr);
struct callback_info *callback_cb_info_get(u32 devid, u32 fid, u32 tsid);
struct callback_logic_cq *callback_logic_cq_get(u32 devid, u32 fid, u32 tsid);
struct callback_phy_sqcq *callback_physic_sqcq_get(u32 devid, u32 fid, u32 tsid);
s32 callback_cq_match(u32 fid, u32 cqid);
void task_dispatch_handler(u32 devid, u32 fid, u32 tsid);
void task_dispatch_handler_ex(u32 devid, u32 fid, u32 tsid, u32 tail_valid, u32 vcq_tail);
void callback_cq_proc(u32 devid, u32 fid, u32 tsid, u32 cqid);
s32 tsdrv_callback_init(struct tsdrv_device *tsdrv_dev, u32 fid, u32 tsid);
void callback_exit(struct tsdrv_device *tsdrv_dev, u32 fid, u32 tsid);
void callback_recycle_logic_cq(u32 devid, u32 fid, u32 tsid, struct tsdrv_ctx *ctx);
int callback_ctx_init(struct tsdrv_ctx *ctx, u32 tsnum);
void callback_ctx_exit(struct tsdrv_ctx *ctx, u32 tsnum);
int tsdrv_get_cb_cq_mem_phy_addr(u32 devId, u32 tsId, u32 cqId, struct tsdrv_phy_addr_get *info);

bool callback_queue_full(u32 head, u32 tail, u32 depth);
void callback_free_one_cq(struct callback_logic_cq *logic_cq,
    struct callback_phy_sqcq *phy_sqcq, struct callback_logic_cq_sub *sub_cq);
void callback_free_logic_cq(struct callback_logic_cq_sub *sub_cq);
s32 callback_alloc_logic_cq_mbox(struct callback_phy_sqcq *sqcq,
    struct callback_logic_cq_sub *sub_cq);
void callback_info_print(u32 devid, u32 fid, u32 tsid);
int callback_dev_init(u32 devid, u32 fid, u32 tsnum);
void callback_dev_exit(u32 devid, u32 fid, u32 tsnum);
void callback_dev_exit_for_container(u32 devid, u32 fid, u32 tsnum);
s32 callback_physic_sqcq_init(struct tsdrv_device *tsdrv_dev, u32 fid, u32 tsid);
void callback_physic_sqcq_exit(struct tsdrv_device *tsdrv_dev, u32 fid, u32 tsid);
void callback_g_info_exit_all_fids(u32 devid, u32 tsnum);
tsdrv_cpu_task_t *tsdrv_get_cpu_task(struct callback_phy_sqcq_info *phy_cq);

#endif
