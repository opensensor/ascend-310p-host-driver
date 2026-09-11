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

#ifndef DRV_TSDRV_INTERFACE_H
#define DRV_TSDRV_INTERFACE_H

#include <linux/types.h>
#ifndef AOS_LLVM_BUILD
#include <linux/uio_driver.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if defined (CFG_SOC_PLATFORM_CLOUD_V2)
#define TASKID_SHARE_MEM_START_ADDR 0x104D5FDC00 /* 0x104D500000 …… task shm(1k) + aicpu config(8k) */
#define TASKID_SHARE_MEM_BLOCK_NUM    16
#elif defined (CFG_SOC_PLATFORM_MINIV3)
#define TASKID_SHARE_MEM_START_ADDR 0x2223dc00 // (0x22240000(base) - 9k
#define TASKID_SHARE_MEM_BLOCK_NUM    15
#else
#define TASKID_SHARE_MEM_START_ADDR 0x9fcc00
#define TASKID_SHARE_MEM_BLOCK_NUM    15
#endif
#define TASKID_SHARE_MEM_SIZE         1024  // 1k
#define TASKID_SHARE_MEM_MAGIC        0xABCD

enum tsdrv_id_type {
    TSDRV_STREAM_ID,
    TSDRV_NOTIFY_ID,
    TSDRV_MODEL_ID,
    TSDRV_EVENT_SW_ID, /* event alloc/free/inquiry res_num should use SW_ID */
    TSDRV_EVENT_HW_ID,
#ifdef CFG_SOC_PLATFORM_MDC_V51
    TSDRV_IPC_EVENT_ID,
#endif
    TSDRV_SQ_ID,
    TSDRV_CQ_ID,
    TSDRV_CMO_ID,
    TSDRV_MAX_ID
};

struct taskid_share_mem {
    u32 magic;
    u32 count;
    u64 addr[TASKID_SHARE_MEM_BLOCK_NUM];
};

struct devdrv_mailbox_message_header {
    u16 valid;    /* validity judgement, 0x5a5a is valid */
    u16 cmd_type; /* identify command or operation */
    u32 result;   /* TS's process result succ or fail: no error: 0, error: not 0 */
};

struct tsdrv_mbox_data {
    const void *msg;
    u32 msg_len;    /* max len is 64 byte */
    void *out_data;
    u32 out_len;
};

struct tsdrv_id_inst {
    u32 devid;
    u32 fid;
    u32 tsid;
};

static inline void tsdrv_pack_id_inst(u32 devid, u32 fid, u32 tsid, struct tsdrv_id_inst *id_inst)
{
    id_inst->devid = devid;
    id_inst->fid = fid;
    id_inst->tsid = tsid;
}

int tsdrv_mailbox_send_sync(u32 devid, u32 tsid, struct tsdrv_mbox_data *data);

#define TSDRV_MAX_SQ_DEPTH (64 * 1024)
#define TSDRV_MAX_CQ_DEPTH (64 * 1024)

void tsdrv_set_chan_complete_handle(void *handle,
    void(*cq_report_handle)(void *report, u32 report_count));

int tsdrv_submit_task(void *handle, const void *sqe, u32 timeout);
void *tsdrv_task_submit_chan_create(u32 devid, u32 vfid, u32 tsid, int type,
    u32 sq_depth, u32 cq_depth);
void *tsdrv_create_task_topic_sched_submit_chan(u32 devid, u32 vfid, u32 tsid,
    int type, u32 sq_depth, u32 cq_depth, u32 pool_id, u32 pri);

void tsdrv_destroy_task_submit_chan(void *handle, int type);

u32 tsdrv_cdqm_get_instance_by_cdqid(u32 devid, u32 tsid, u32 cdq_id);

int tsdrv_cdqm_get_name_by_cdqid(u32 devid, u32 tsid, u32 cdq_id, char *name, int buf_len);

int tsdrv_cdqm_set_topic_id(u32 devid, u32 topic_id);

int tsdrv_cdqm_query_que_num(u32 devid, u32 tsid);

int tsdrv_pm_extend_set(u32 module, int (*suspend)(u32 devid), int (*resume)(u32 devid));
void tsdrv_pm_extend_exit(u32 module);

struct tsdrv_pm_extend {
    int (*suspend)(u32 devid);
    int (*resume)(u32 devid);
};

int devdrv_wakeup_cce_context_status(pid_t pid, u32 devid, u32 status);

int tsdrv_device_pre_hotreset(u32 devid);
int tsdrv_device_cancel_hotreset(u32 devid);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __DRV_TSDRV_COMM_H */
