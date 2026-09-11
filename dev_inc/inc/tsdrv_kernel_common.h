/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2023. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Description:
 * Author: huawei
 * Create: 2021-4-13
 */
#ifndef TSDRV_USER_COMMON_H
#define TSDRV_USER_COMMON_H

#include "tsdrv_interface.h"

#ifdef CFG_MANAGER_HOST_ENV
#ifdef CFG_SOC_PLATFORM_CLOUD_V2
#define TSDRV_MAX_DAVINCI_NUM 1124
#else
#define TSDRV_MAX_DAVINCI_NUM 64
#endif

#else
#ifdef CFG_SOC_PLATFORM_CLOUD_V2
#define TSDRV_MAX_DAVINCI_NUM 64
#else
#define TSDRV_MAX_DAVINCI_NUM 4
#endif
#endif

#ifdef CFG_SOC_PLATFORM_CLOUD
#define DEVDRV_MAX_MODEL_ID  2048
#else
#define DEVDRV_MAX_MODEL_ID  1024
#endif

#define DEVDRV_MAX_CMO_ID   (64 * 1024)

#define IPC_EVENT_MASK 0x8000
#define IPC_EVENT_TYPE 1

#ifdef CFG_SOC_PLATFORM_MDC_V51
#define DEVDRV_MAX_IPC_EVENT_ID 0
#endif

#define DEVDRV_MIN_HW_EVENT_ID 0
#ifdef CFG_FEATURE_IPC_NOTIFY_WITH_EVENTID
#define NOTIFY_ID_TYPE IPC_EVENT_TYPE
#else
#define NOTIFY_ID_TYPE 0
#endif
#ifdef CFG_SOC_MDC_V51_LITE
#define DEVDRV_MAX_HW_EVENT_ID  (512 - 1)
#define DEVDRV_MAX_SW_EVENT_ID  (512)

#else
#define DEVDRV_MAX_HW_EVENT_ID  (1024 - 1)

#if defined(CFG_SOC_PLATFORM_CLOUD) || defined(CFG_SOC_PLATFORM_MINIV3)
#define DEVDRV_MAX_SW_EVENT_ID  (64 * 1024)
#else
#define DEVDRV_MAX_SW_EVENT_ID  (1024)
#endif
#endif
#define DEVDRV_SQ_SLOT_SIZE             (64)
#if defined(CFG_SOC_PLATFORM_CLOUD_V2) || defined(CFG_SOC_PLATFORM_MINIV3)
#define DEVDRV_CQ_SLOT_SIZE             (16)
#else
#define DEVDRV_CQ_SLOT_SIZE             (12)
#endif
#define DEVDRV_MAX_CQE_SIZE             32

#define DEVDRV_MAX_CQ_SLOT_SIZE         (128)

#if defined(CFG_SOC_PLATFORM_MINI) && !defined(CFG_SOC_PLATFORM_MINIV2) && !defined(CFG_SOC_PLATFORM_MINIV3)
#define DEVDRV_MAX_SQ_DEPTH             (1024)
#define DEVDRV_MAX_CQ_DEPTH             (1024)
#define DEVDRV_MAX_SQ_NUM               (512 - 1)
#define DEVDRV_MAX_CQ_NUM               (352 - 1)
#define DEVDRV_DB_SPACE_SIZE            (1024 * 4096)
#else
#ifdef CFG_SOC_PLATFORM_CLOUD_V2
#ifdef CFG_MANAGER_HOST_ENV
#define DEVDRV_MAX_SQ_DEPTH             (65535ULL)
#define DEVDRV_MAX_CQ_DEPTH             (65535ULL)
#else
#define DEVDRV_MAX_SQ_DEPTH             (1024)
#define DEVDRV_MAX_CQ_DEPTH             (1024)
#endif
#define DEVDRV_MAX_SQ_NUM               (2048ULL)
#define DEVDRV_MAX_CQ_NUM               DEVDRV_MAX_SQ_NUM
#define DEVDRV_DB_SPACE_SIZE            (2048 * 65536)
#elif defined(CFG_SOC_PLATFORM_MINIV3)
#define DEVDRV_MAX_SQ_DEPTH             (2048)
#define DEVDRV_MAX_CQ_DEPTH             (1024)
#define DEVDRV_MAX_SQ_NUM               (512)
#define DEVDRV_MAX_CQ_NUM               DEVDRV_MAX_SQ_NUM
#define DEVDRV_DB_SPACE_SIZE            (512 * 65536)
#elif defined(CFG_SOC_MDC_V51_LITE)
#define DEVDRV_MAX_SQ_DEPTH             (1024)
#define DEVDRV_MAX_CQ_DEPTH             (1024)
#define DEVDRV_MAX_SQ_NUM               (48)
#define DEVDRV_MAX_CQ_NUM               (58)
#else
#define DEVDRV_MAX_SQ_DEPTH             (1024)
#define DEVDRV_MAX_CQ_DEPTH             (1024)
#define DEVDRV_MAX_SQ_NUM               (512)
#define DEVDRV_MAX_CQ_NUM               (352)
#define DEVDRV_DB_SPACE_SIZE            (1024 * 4096)
#endif
#endif

#ifdef CFG_SOC_PLATFORM_CLOUD_V2
#define DEVDRV_MAX_STREAM_ID DEVDRV_MAX_SQ_NUM
#define DEVDRV_MAX_NOTIFY_ID 8192
#elif defined(CFG_SOC_PLATFORM_CLOUD)
#define DEVDRV_MAX_STREAM_ID 2048
#define DEVDRV_MAX_NOTIFY_ID 1024
#elif defined(CFG_SOC_PLATFORM_MINIV3)
#define DEVDRV_MAX_STREAM_ID DEVDRV_MAX_SQ_NUM
#define DEVDRV_MAX_NOTIFY_ID 2048
#elif defined(CFG_SOC_MDC_V51_LITE)
#define DEVDRV_MAX_STREAM_ID 128
#define DEVDRV_MAX_NOTIFY_ID 512
#else
#define DEVDRV_MAX_STREAM_ID 1024
#define DEVDRV_MAX_NOTIFY_ID 1024
#endif

#ifdef CFG_SOC_PLATFORM_MINIV3
#define DEVDRV_MIN_SQ_DEPTH             (2048)
#else
#define DEVDRV_MIN_SQ_DEPTH             (1024)
#endif
#define DEVDRV_MIN_CQ_DEPTH             (1024)

#define DEVDRV_MAX_CQ_NUM_PER_PROCESS   2

#define DEVDRV_FUNCTIONAL_SQ_FIRST_INDEX    (496)
#define DEVDRV_FUNCTIONAL_CQ_FIRST_INDEX    (500)
#define DEDVRV_DEV_PROCESS_HANG             (0x0F000FFF)

/* Lite : Mini */
#define DEVDRV_MAX_FUNCTIONAL_SQ_NUM        (4)
#define DEVDRV_MAX_FUNCTIONAL_CQ_NUM        (10)

#define TSDRV_CQ_REPORT_SIZE   64U

#define DEVDRV_CB_SQ_SLOT_MAX_NUM 1024
#define DEVDRV_CB_CQ_SLOT_MAX_NUM 1024

#define DEVDRV_CB_SQ_MAX_NUM 1024
#define DEVDRV_CB_CQ_MAX_NUM 1024

#define DEVDRV_CB_SQCQ_MAX_SIZE 64
#define DEVDRV_CB_SQCQ_MIN_SIZE 32

#define  DEVDRV_CB_MAX_REPORT_NUM   32

#define TSDRV_MAX_SHM_SQE_SIZE      64U
#define TSDRV_MIN_SHM_SQE_SIZE      64U

#define TSDRV_MAX_SHM_SQE_DEPTH     1024U
#define TSDRV_MIN_SHM_SQE_DEPTH     1024U

#define TSDRV_MAX_SHM_CQE_SIZE      16U
#define TSDRV_MIN_SHM_CQE_SIZE      16U

#define TSDRV_MAX_SHM_CQE_DEPTH     1024U
#define TSDRV_MIN_SHM_CQE_DEPTH     1024U

#ifdef CFG_FEATURE_STARS
#define TSDRV_MIN_LOGIC_CQE_SIZE    32U
#define TSDRV_MAX_LOGIC_CQE_SIZE    32U
#else
#define TSDRV_MIN_LOGIC_CQE_SIZE    16U
#define TSDRV_MAX_LOGIC_CQE_SIZE    16U
#endif

#ifdef CFG_SOC_PLATFORM_CLOUD_V2
#define TSDRV_MAX_LOGIC_CQE_DEPTH   4096U
#define TSDRV_MIN_LOGIC_CQE_DEPTH   4096U
#elif defined(CFG_SOC_PLATFORM_MDC_V51)
#define TSDRV_MAX_LOGIC_CQE_DEPTH   4096U
#define TSDRV_MIN_LOGIC_CQE_DEPTH   1024U
#define TSDRV_PHY_CQE_DEPTH         1024U
#else
#define TSDRV_MAX_LOGIC_CQE_DEPTH   1024U
#define TSDRV_MIN_LOGIC_CQE_DEPTH   1024U
#endif

#define SQCQ_RTS_INFO_LENGTH 5
#define SQCQ_RESV_LENGTH 8

#define TSDRV_USERMAP_SQ_REG_SIZE   (4 * 1024)

/**
 * When modifying the macro definition,
 * ensure that the value is less than or equal to 4094 (< = 4094).
 */
#if (defined CFG_SOC_PLATFORM_CLOUD)
#define TSDRV_MAX_LOGIC_CQ_NUM      4094U
#else
#define TSDRV_MAX_LOGIC_CQ_NUM      2048U
#endif

#define TSDRV_MAX_LOGIC_CQ_REPORT_NUM   32U

#if (defined CFG_SOC_PLATFORM_MDC) || (defined CFG_SOC_PLATFORM_MDC_V51)
#ifdef CFG_SOC_MDC_V51_LITE
#define DEVDRV_MAX_TS_NUM                   (1)
#else
#define DEVDRV_MAX_TS_NUM                   (2)
#endif
#else
#define DEVDRV_MAX_TS_NUM                   (1)
#endif

enum devdrv_ts_status {
    TS_WORK = 0x0,
    TS_SUSPEND,
    TS_DOWN,
    TS_INITING,
    TS_BOOTING,
    TS_FAIL_TO_SUSPEND,
    TS_FW_VERIFY_FAIL,
    TS_MAX_STATUS
};

#define TSDRV_CQ_REUSE      0x00000001
#define TSDRV_SQ_REUSE      0x00000002
#define TSDRV_REMOTE_FLAG   0x00000020

#define DEVDRV_PHASE_STATE_0 0
#define DEVDRV_PHASE_STATE_1 1

typedef enum sqcq_alloc_status {
    SQCQ_INACTIVE = 0,
    SQCQ_ACTIVE
}  drvSqCqAllocType_t;

enum phy_sqcq_type {
    NORMAL_SQCQ_TYPE,
    CALLBACK_SQCQ_TYPE,
    LOGIC_SQCQ_TYPE,
    SHM_SQCQ_TYPE,
    DFX_SQCQ_TYPE,
    TS_SQCQ_TYPE,
    KERNEL_SQCQ_TYPE,
    CTRL_SQCQ_TYPE
};

struct devdrv_ts_sq_info {
    enum phy_sqcq_type type;
    pid_t tgid;
    u32 head;
    u32 tail;
    u32 depth;
    u32 credit;
    u32 index;
    int uio_fd;

    u8 *uio_addr;
    int uio_size;

    unsigned long sqDbVaddr;
    unsigned long sqHeadVaddr;
    unsigned long sqTailVaddr;
    drvSqCqAllocType_t alloc_status;
    u64 send_count;

    void *sq_sub;
    u32 bind_cqid;
};

struct devdrv_ts_cq_info {
    enum phy_sqcq_type type;
    pid_t tgid;
    u32 vfid;

    u32 head;
    u32 tail;
    u32 depth;
    u32 release_head;  /* runtime read cq head value */
    volatile u32 count_report;
    u32 index;
    u32 phase;
    u32 int_flag;

    int uio_fd;

    u8 *uio_addr;
    int uio_size;

    drvSqCqAllocType_t alloc_status;
    u64 receive_count;

    void *cq_sub;
    u8 slot_size;
    u32 bind_logic_cqid;
};

struct tsdrv_sw_rx_reg { /* size <= sqeSize */
    u16 sq_head;
};

enum tsdrv_host_flag {
    TSDRV_PHYSICAL_TYPE = 0,
    TSDRV_VIRTUAL_TYPE,
    TSDRV_CONTAINER_TYPE,
    TSDRV_MAX_VM_TYPE
};

typedef enum {
    QUERY_IDS_CAPACITY = 0,
    QUERY_ID_CONFIG_INFO
} id_query_opt;

struct tsdrv_id_capacity {
    u32 stream_capacity;
    u32 event_capacity;
    u32 notify_capacity;
    u32 model_capacity;
    u32 sq_capacity;
    u32 cq_capacity;
    u32 cmo_capacity;
};

struct tsdrv_id_config_info {
    enum tsdrv_id_type id_type;
    u32 capacity;
    u32 avail_num;
    u32 cur_num;
};

struct tsdrv_id_query_para {
    id_query_opt opt;
    union {
        struct tsdrv_id_capacity id_capacity;
        struct tsdrv_id_config_info id_config_info;
    };
};

#endif
