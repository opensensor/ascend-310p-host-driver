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

#ifndef TSDRV_IOCTL_H
#define TSDRV_IOCTL_H

#include "tsdrv_kernel_common.h"
#include "ka_ioctl_pub.h"

struct devdrv_id_para {
    u32 res_id;
    u32 flag;
    u64 value;
};

struct devdrv_event_para {
    u32 event_id;
    u64 value;
};

struct devdrv_rts_stream_para {
    u32 stream_id;
    u32 priority;
    u32 vf_id;
};

struct devdrv_report_para {
    int timeout;
    u32 cq_tail;
};

struct normal_alloc_sqcq_para {
    uint32_t tsId;
    uint32_t sqeSize;
    uint32_t cqeSize;
    uint32_t sqeDepth;
    uint32_t cqeDepth;
    int IsSqDbMap;
    int IsHwtsSqMap;
    int isCtrlType;
    unsigned long sqDbVaddr;
    unsigned long sqMapVaddr;
    unsigned long cqMapVaddr;
    unsigned long sqHeadRegVaddr;
    unsigned long sqTailRegVaddr;
    uint32_t grpId;
    uint32_t flag;
    uint32_t sqId;
    uint32_t cqId;
    uint32_t info[SQCQ_RTS_INFO_LENGTH];
    uint32_t res[SQCQ_RESV_LENGTH];
};

struct normal_free_sqcq_para {
    uint32_t tsId;
    int isCtrlType;
    uint32_t flag;
    uint32_t sqId;
    uint32_t cqId;
    uint32_t res[SQCQ_RESV_LENGTH];
};

struct tsdrv_sqcq_data_para {
    u32 id;
    u32 val;
};

struct tsdrv_normal_sq_send_para {
    u32 sq_id;
    u32 sq_tail;
    u32 sqe_num;
};

struct callback_alloc_sqcq_para {
    u32 sq_size;
    u32 cq_size;
    u32 sq_depth;
    u32 cq_depth;
    u32 sq_id;
    u32 cq_id;
    u32 grp_id;

    unsigned long sqMapVaddr;
    unsigned long cqMapVaddr;
};

struct callback_free_sqcq_para {
    u32 sq_id;
    u32 cq_id;
};

#define TSDRV_SQ_TASK_SIZE     64U
struct callback_send_sq_para {
    u32 sq_id;
    u8 sq_task[TSDRV_SQ_TASK_SIZE];
};

#define DEVDRV_CB_BITMAP_SIZE 16
struct callback_wait_cq_para {
    u32 tsid;
    u32 gid;
    int timeout;
    u64 bitmap_buf[DEVDRV_CB_BITMAP_SIZE];
    u32 bitmap_size;
    u32 report_cnt;
    u32 report_len;
    u8 *report_buf;
};

struct shm_sqcq_alloc_para {
    u32 sq_size;            // input
    u32 cq_size;            // input
    u32 sq_depth;           // input
    u32 cq_depth;           // input
    unsigned long sqMapVaddr;    // input

    u32 sq_id;          // output
    u32 cq_id;          // output
    u32 rdonly;         // output
    u32 info[SQCQ_RTS_INFO_LENGTH]; // input
};

struct shm_sqcq_free_para {
    u32 sq_id;      // input
    u32 cq_id;      // input
};

struct logic_sqcq_alloc_para {
    u32 cq_size;    // input
    u32 cq_depth;   // input
    u32 cq_id;       // output logic cq id
    u32 flag;
    u32 info[SQCQ_RTS_INFO_LENGTH];
};

struct logic_sqcq_free_para {
    u32 cq_id;      // output
};

struct logic_cq_wait_cq_para {
    u32 tsid;       // input
    u32 cqid;       // input
    int timeout;    // input
    u32 version;    /* input irq wait verion 0, recv version 1 */
    u32 stream_id;  /* input version 1 match stream id */
    u32 task_id;    /* input version 1 match task id */
    u32 cqe_num; /* input report_buf cqe num */
    u32 report_cqe_num; /* output report_buf cqe num */
    u8 *report_buf; // input
};

struct tsdrv_sqcq_set_para {
    u32 sq_id;
    u32 prop;
    u32 value;
};

#define MEM_MAP_SUBTYPE_NORMAL 0
#define MEM_MAP_SUBTYPE_CB 1
#define MEM_MAP_SUBTYPE_SHM 2

struct tsdrv_mem_map_para {
    u32 id_type; /* enum tsdrv_id_type */
    u32 sub_type; /* normal, cb, shm */
    u32 id;
    u32 addr_side;
    unsigned long va;
    unsigned long len;
};

struct tsdrv_mem_unmap_para {
    unsigned long va;
    unsigned long len;
};

enum tsdrv_sqcq_set_prop {
    TSDRV_SQCQ_SET_PROP_STATUS = 0x0,
    TSDRV_SQCQ_SET_PROP_SQ_HEAD,
    TSDRV_SQCQ_SQ_DISABLE_TO_ENABLE,
    TSDRV_SQCQ_SET_PROP_MAX
};

enum tsdrv_logic_cq_set_prop {
    TSDRV_STREAM_BIND_LOGIC_CQ = 0x0,
    TSDRV_STREAM_UNBIND_LOGIC_CQ,
    TSDRV_STREAM_ENABLE_EVENT,
    TSDRV_LOGIC_CQ_SET_PROP_MAX
};

enum tsdrv_sqcq_query_prop {
    TSDRV_SQCQ_QUERY_PROP_SQ_STATUS = 0x0,
    TSDRV_SQCQ_QUERY_PROP_SQ_HEAD,
    TSDRV_SQCQ_QUERY_PROP_SQ_TAIL,
    TSDRV_SQCQ_QUERY_PROP_MAX
};

enum tsdrv_ioctl_result {
    ID_NO_EXHAUSTED = 0x0,
    ID_IS_EXHAUSTED
};

struct tsdrv_dev_info {
    int chip_type;
    int virt_type;  /* 0: physical 1: vm 2: container */
};

struct tsdrv_res_config_para {
    u32 prop;
    u32 stream_id;
    u32 logic_cqid;
    u32 grp_id;
    u32 event_id;
};

struct tsdrv_sqcq_query_para {
    u32 prop;
    u32 sqid;
    u32 value;
};

struct devdrv_ioctl_arg {
    u32 tsid;
    u32 result;
    union {
        struct devdrv_id_para id_para;
        struct devdrv_event_para event_para;
        struct devdrv_rts_stream_para rts_stream_para;
        struct devdrv_report_para report_para;
        struct callback_alloc_sqcq_para cq_alloc_para;
        struct callback_free_sqcq_para cq_free_para;
        struct callback_send_sq_para cb_sq_para;
        struct normal_alloc_sqcq_para sqcq_alloc_para;
        struct normal_free_sqcq_para sqcq_free_para;
        struct tsdrv_sqcq_data_para sqcq_data_para;
        struct tsdrv_normal_sq_send_para normal_sq_send_para;
        struct shm_sqcq_alloc_para shm_sqcq_alloc_para;
        struct shm_sqcq_free_para shm_sqcq_free_para;
        struct logic_sqcq_alloc_para logic_sqcq_alloc_para;
        struct logic_sqcq_free_para logic_sqcq_free_para;
        struct logic_cq_wait_cq_para logic_cq_wait;
        struct tsdrv_id_query_para id_query_para;
        struct tsdrv_sqcq_set_para sqcq_set;
        struct tsdrv_mem_map_para mem_map;
        struct tsdrv_mem_unmap_para mem_unmap;
        struct tsdrv_res_config_para res_config_para;
        struct tsdrv_sqcq_query_para sqcq_query_para;
        struct tsdrv_dev_info devInfo;
    };
};

#define TSDRV_ID_MAGIC 'D'
#define TSDRV_ALLOC_STREAM_ID       _KA_IOWR(TSDRV_ID_MAGIC, 1, struct devdrv_ioctl_arg)
#define TSDRV_FREE_STREAM_ID        _KA_IOW(TSDRV_ID_MAGIC, 2, struct devdrv_ioctl_arg)
#define TSDRV_ALLOC_EVENT_ID        _KA_IOWR(TSDRV_ID_MAGIC, 3, struct devdrv_ioctl_arg)
#define TSDRV_FREE_EVENT_ID         _KA_IOW(TSDRV_ID_MAGIC, 4, struct devdrv_ioctl_arg)
#define TSDRV_REPORT_WAIT           _KA_IOWR(TSDRV_ID_MAGIC, 5, struct devdrv_ioctl_arg)
#define TSDRV_ALLOC_MODEL_ID        _KA_IOWR(TSDRV_ID_MAGIC, 6, struct devdrv_ioctl_arg)
#define TSDRV_FREE_MODEL_ID         _KA_IOW(TSDRV_ID_MAGIC, 7, struct devdrv_ioctl_arg)
#define TSDRV_ALLOC_NOTIFY_ID       _KA_IOWR(TSDRV_ID_MAGIC, 8, struct devdrv_ioctl_arg)
#define TSDRV_FREE_NOTIFY_ID        _KA_IOW(TSDRV_ID_MAGIC, 9, struct devdrv_ioctl_arg)
#define TSDRV_ALLOC_CMO_ID          _KA_IOWR(TSDRV_ID_MAGIC, 10, struct devdrv_ioctl_arg)
#define TSDRV_FREE_CMO_ID           _KA_IOW(TSDRV_ID_MAGIC, 11, struct devdrv_ioctl_arg)
#define TSDRV_GET_DEV_INFO          _KA_IOWR(TSDRV_ID_MAGIC, 12, struct devdrv_ioctl_arg)

#define TSDRV_ALLOC_IPC_EVENT_ID    _KA_IOWR(TSDRV_ID_MAGIC, 16, struct devdrv_ioctl_arg)
#define TSDRV_FREE_IPC_EVENT_ID     _KA_IOW(TSDRV_ID_MAGIC,  17, struct devdrv_ioctl_arg)

#define TSDRV_CBSQCQ_ALLOC_ID       _KA_IOWR(TSDRV_ID_MAGIC, 20, struct devdrv_ioctl_arg)
#define TSDRV_CBSQCQ_FREE_ID        _KA_IOWR(TSDRV_ID_MAGIC, 21, struct devdrv_ioctl_arg)
#define TSDRV_CBSQCQ_WAIT_ID        _KA_IOWR(TSDRV_ID_MAGIC, 22, struct devdrv_ioctl_arg)
#define TSDRV_CBSQCQ_SEND_TASK      _KA_IOWR(TSDRV_ID_MAGIC, 23, struct devdrv_ioctl_arg)
#define TSDRV_SQCQ_ALLOC_ID         _KA_IOWR(TSDRV_ID_MAGIC, 24, struct devdrv_ioctl_arg)
#define TSDRV_SQCQ_FREE_ID          _KA_IOW(TSDRV_ID_MAGIC, 25, struct devdrv_ioctl_arg)
#define TSDRV_SQ_MSG_SEND           _KA_IOWR(TSDRV_ID_MAGIC, 26, struct devdrv_ioctl_arg)
#define TSDRV_CQ_REPORT_RELEASE     _KA_IOW(TSDRV_ID_MAGIC, 27, struct devdrv_ioctl_arg)
#define TSDRV_GET_SQ_HEAD           _KA_IOWR(TSDRV_ID_MAGIC, 28, struct devdrv_ioctl_arg)
#define TSDRV_SHM_SQCQ_ALLOC        _KA_IOWR(TSDRV_ID_MAGIC, 29, struct devdrv_ioctl_arg)
#define TSDRV_SHM_SQCQ_FREE         _KA_IOW(TSDRV_ID_MAGIC, 30, struct devdrv_ioctl_arg)
#define TSDRV_LOGIC_CQ_ALLOC        _KA_IOWR(TSDRV_ID_MAGIC, 31, struct devdrv_ioctl_arg)
#define TSDRV_LOGIC_CQ_FREE         _KA_IOW(TSDRV_ID_MAGIC, 32, struct devdrv_ioctl_arg)
#define TSDRV_LOGIC_CQ_WAIT         _KA_IOWR(TSDRV_ID_MAGIC, 33, struct devdrv_ioctl_arg)
#define TSDRV_ID_INFO_QUERY         _KA_IOWR(TSDRV_ID_MAGIC, 34, struct devdrv_ioctl_arg)
#define TSDRV_SQCQ_SET              _KA_IOW(TSDRV_ID_MAGIC, 35, struct devdrv_ioctl_arg)
#define TSDRV_ENABLE_STREAM_ID      _KA_IOW(TSDRV_ID_MAGIC, 36, struct devdrv_ioctl_arg)
#define TSDRV_DISABLE_STREAM_ID     _KA_IOW(TSDRV_ID_MAGIC, 37, struct devdrv_ioctl_arg)
#define TSDRV_GET_SSID              _KA_IOWR(TSDRV_ID_MAGIC, 38, struct devdrv_ioctl_arg)
#define TSDRV_CDQM_COMMAND          _KA_IOWR(TSDRV_ID_MAGIC, 39, struct devdrv_ioctl_arg)
#define TSDRV_RES_CONFIG          _KA_IOW(TSDRV_ID_MAGIC, 40, struct devdrv_ioctl_arg)
#define TSDRV_SQCQ_QUERY            _KA_IOWR(TSDRV_ID_MAGIC, 41, struct devdrv_ioctl_arg)

#define TSDRV_MAX_CMD 42


#endif
