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
#ifndef DEVDRV_FUNCTIONAL_CQSQ_API_H
#define DEVDRV_FUNCTIONAL_CQSQ_API_H

#include <linux/types.h>

#include "ka_task_pub.h"
#include "tsdrv_interface.h"

/**
 * IPC Message Command Type
 */
#define DEVDRV_MAILBOX_NOTICE_ACK_IRQ_VALUE 0x0000
#define DEVDRV_MAILBOX_CREATE_CQSQ_CALC 0x0001
#define DEVDRV_MAILBOX_RELEASE_CQSQ_CALC 0x0002
#define DEVDRV_MAILBOX_CREATE_CQSQ_LOG 0x0003
#define DEVDRV_MAILBOX_RELEASE_CQSQ_LOG 0x0004
#define DEVDRV_MAILBOX_CREATE_CQSQ_DEBUG 0x0005
#define DEVDRV_MAILBOX_RELEASE_CQSQ_DEBUG 0x0006
#define DEVDRV_MAILBOX_CREATE_CQSQ_PROFILE 0x0007
#define DEVDRV_MAILBOX_RELEASE_CQSQ_PROFILE 0x0008
#define DEVDRV_MAILBOX_CREATE_CQSQ_BEAT 0x0009
#define DEVDRV_MAILBOX_RELEASE_CQSQ_BEAT 0x000A
#define DEVDRV_MAILBOX_RECYCLE_EVENT_ID 0x000B
#define DEVDRV_MAILBOX_RECYCLE_STREAM_ID 0x000C
#define DEVDRV_MAILBOX_SEND_CONTAINER_TFLOP 0x000D
#define DEVDRV_MAILBOX_CONFIG_P2P_INFO 0x000E
#define DEVDRV_MAILBOX_SEND_RDMA_INFO 0x000F
#define DEVDRV_MAILBOX_RESET_NOTIFY_ID 0x0010
#define DEVDRV_MAILBOX_RECYCLE_PID_ID 0x0012
#define DEVDRV_MAILBOX_FREE_STREAM_ID 0x0013
#define DEVDRV_MAILBOX_SEND_SHARE_MEMORY_INFO 0x0014
#define DEVDRV_MAILBOX_SEND_HWTS_INFO 0x0015
#define DEVDRV_MAILBOX_CREATE_CBCQSQ 0x0018
#define DEVDRV_MAILBOX_RELEASE_CBCQSQ 0x0019
#define DEVDRV_MAILBOX_CREATE_LOGIC_CBCQ 0x001a
#define DEVDRV_MAILBOX_RELEASE_LOGIC_CBCQ 0x001b
#define DEVDRV_MAILBOX_RESET_EVENT_ID 0x0020
#define TSDRV_MBOX_SHM_SQCQ_ALLOC            33U
#define TSDRV_MBOX_SHM_SQCQ_FREE             34U
#define TSDRV_MBOX_LOGIC_CQ_ALLOC            35U
#define TSDRV_MBOX_LOGIC_CQ_FREE             36U
#define TSDRV_MBOX_NOTICE_KERNER_SQCQ        37U
#define TSDRV_MBOX_RELEASE_TOPIC_SQCQ        38U
#define TSDRV_MBOX_RESOURCE_MAPPING_NOTICE   39U
#define DEVDRV_MAILBOX_RECYCLE_CHECK         40U
#define TSDRV_MBOX_ALLOC_RUNTIME_STREAM_ID   41U  /* 40 alloc runtime stream id for bs9sx1a */
#define TSDRV_MBOX_FREE_RUNTIME_STREAM_ID    42U  /* 41 free runtime stream id for bs9sx1a */
#define TSDRV_MBOX_CREATE_KERNEL_SQCQ        43U
#define TSDRV_MBOX_RELEASE_KERNEL_SQCQ       44U
#define TSDRV_MBOX_NOTICE_SSID               45U
#define TSDRV_MBOX_QUERY_SSID                46U
#define TSDRV_MBOX_NOTICE_TS_SQCQ_CREATE     47U
#define TSDRV_MBOX_NOTICE_TS_SQCQ_FREE       48U
#define DEVDRV_MAILBOX_CREATE_CTRL_CQSQ      49U
#define DEVDRV_MAILBOX_RELEASE_CTRL_CQSQ     50U

#define DEVDRV_FUNCTIONAL_BRIEF_CQ 0
#define DEVDRV_FUNCTIONAL_DETAILED_CQ 1
#define DEVDRV_FUNCTIONAL_CALLBACK_HS_CQ 2
#define DEVDRV_FUNCTIONAL_REPORT_HS_CQ 3
#define DEVDRV_MAILBOX_INVALID_INDEX 0xFFFF

#define DEVDRV_FUNCTIONAl_MAX_SQ_SLOT_LEN 128
#define DEVDRV_FUNCTIONAL_MAX_CQ_SLOT_LEN 128

struct devdrv_mailbox_cqsq {
    u16 valid;    /* validity judgement, 0x5a5a is valid */
    u16 cmd_type; /* command type */
    u32 result;   /* TS's process result succ or fail: no error: 0, error: not 0 */

    u64 sq_addr; /* invalid addr: 0x0 */
    u64 cq0_addr;
    u64 cq1_addr;
    u64 cq2_addr;
    u64 cq3_addr;
    u16 sq_index;  /* invalid index: 0xFFFF */
    u16 cq0_index; /* sq's return */
    u16 cq1_index; /* ts's return */
    u16 cq2_index; /* ai cpu's return */
    u16 cq3_index; /* reserved */
    u16 cq_irq;
    u8 plat_type;    /* inform TS, msg is sent from host or device, device: 0 host: 1 */
    u8 cq_slot_size; /* calculation cq's slot size, default: 12 bytes */
};

#define SQCQ_RTS_INFO_LENGTH 5
struct devdrv_normal_cqsq_mailbox {
    u16 valid;    /* validity judgement, 0x5a5a is valid */
    u16 cmd_type; /* command type */
    u32 result;   /* TS's process result succ or fail: no error: 0, error: not 0 */

    u64 sq_addr; /* invalid addr: 0x0 */
    u64 cq0_addr;

    u16 sq_index;  /* invalid index: 0xFFFF */
    u16 cq0_index; /* sq's return */

#if (defined(CFG_SOC_PLATFORM_MINI) && (!defined(CFG_SOC_PLATFORM_MINIV2)) && (!defined(CFG_SOC_PLATFORM_MINIV3)))
    u8 app_flag; /* inform TS, msg is sent from host or device, device: 0 host: 1 */
#else
    u8 app_type : 1;  /* inform TS, msg is sent from host or device, device: 0 host: 1 */
    u8 sw_reg_flag : 1; /* 1: sq saves head and tail (and other dfx info in future) in share memory at the last 2 sqe
                           0: not save */
    u8 fid : 6;       /* 0:hsot, 1~16:virt machine */
#endif
    u8 sq_cq_side : 2;    /* bit 0 sq side, bit 1 cq side. device: 0 host: 1  */
    u8 master_pid_flag : 1;
    u8 rsv : 5;

    u8 sqesize;
    u8 cqesize;  /* calculation cq's slot size, default: 12 bytes */
    u16 cqdepth;
    u16 sqdepth;
    ka_pid_t pid;
    u16 cq_irq;
    u16 ssid;

    uint32_t info[SQCQ_RTS_INFO_LENGTH];
};

struct devdrv_notice_ts_sqcq_mailbox {
    struct devdrv_mailbox_message_header head;
    u32 sq_id;
    u32 cq_id;
    u32 vfid;
    ka_pid_t pid;
    u16 ssid;
    u16 rsv;
    u32 info[SQCQ_RTS_INFO_LENGTH];
};

struct resource_mapping_notice_mailbox_t {
    struct devdrv_mailbox_message_header head;
    volatile u8 vf_id;
    volatile u8 resource_type;   // 0:notify id, 1:event id
    volatile u8 operation_type;  // 0:map, 1:unmap
    volatile u8 reserve0;
    volatile u16 vir_id;
    volatile u16 phy_id;
    volatile u32 host_pid;
    volatile u8 reserve[44];
};

enum devdrv_cqsq_func {
    DEVDRV_CQSQ_HEART_BEAT = 0x0,
    DEVDRV_MAX_CQSQ_FUNC
};

int devdrv_create_functional_sq(u32 devid, u32 tsid, u32 slot_len, u32 *sq_index, u64 *addr);
void devdrv_destroy_functional_sq(u32 devid, u32 tsid, u32 sq_index);
int devdrv_create_functional_cq(u32 devid, u32 tsid, u32 slot_len, u32 cq_type,
    void (*callback)(u32 device_id, u32 tsid, const u8 *cq_slot, u8 *sq_slot), u32 *cq_index, u64 *addr);
void devdrv_destroy_functional_cq(u32 devid, u32 tsid, u32 cq_index);
int devdrv_functional_send_sq(u32 devid, u32 tsid, u32 sq_index, const u8 *buffer, u32 buf_len);
int devdrv_mailbox_send_cqsq(u32 devid, u32 tsid, struct devdrv_mailbox_cqsq *cqsq);
int devdrv_functional_set_cq_func(u32 devid, u32 tsid, u32 cq_index, enum devdrv_cqsq_func function);
int devdrv_functional_set_sq_func(u32 devid, u32 tsid, u32 sq_index, enum devdrv_cqsq_func function);

#endif
