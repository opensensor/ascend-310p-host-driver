/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: mb_struct.h
 *
 * Create: 2023-01-29
 */
#ifndef MB_STRUCT_H
#define MB_STRUCT_H
#include "tsch_defines.h"
#if ((defined STARS_CTRL_CPU) && (!defined ENABLE_VERIFICATION_PATCH))
#include "ascend_hal_define.h"
#endif
#include "mb_struct_callback.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define TS_MAILBOX_SIZE                           (64U)

#define TS_DRV_MAILBOX_VALID_VALUE                (0x5a5a)
#define TS_DRV_MAILBOX_RCV_OK_VALUE               (0x5b5b)
#define TS_DRV_MAILBOX_RCV_NA_VALUE               (0x5c5c)
#define TS_DRV_MAILBOX_INVALID_VALUE              (0x0)

#define RECYCLE_EVENT_COUNT                       (25)
#define APP_EXIT_COUNT_MAX                        (12)
#define MB_CTRL_MSG_LENGTH                        (44U)

typedef enum {
    TS_DRV_MAILBOX = 0X01,
    TS_AICPU_MAILBOX = 0X02,
    TS_INVALID_MAILBOX,
} ts_mailbox_type_t;

/* ts_notify_operation_type_t */
enum {
    TS_NOTIFY_TYPE_CREATE = 0X01,
    TS_NOTIFY_TYPE_DELETE = 0X02,
    TS_NOTIFY_TYPE_INVALID,
};

/* ts_mailbox_notify_type */
enum {
    TS_MAILBOX_NOTIFY_TYPE_NOTIFY = 0,
    TS_MAILBOX_NOTIFY_TYPE_EVENT = 1,
    TS_MAILBOX_NOTIFY_TYPE_INVALID
};

typedef enum {
    OP_APP_EXIT = 0x0U,
    OP_APP_ABORT,
    OP_STREAM_ABORT,
    APP_OP_INVALID
} ts_app_op_type_t;

/* ts_drv_mail_box_cmd_type_t */
enum {
    NOTICE_ACK_IRQ = 0x0000,             /* 0 notice mailbox ack irq */
    CREATE_TASK_CMD_SQCQ = 0X0001,       /* 1 create calculation cqsq */
    RELEASE_TASK_CMD_SQCQ,               /* 2 may release sq only, in this situation, cq_index = 0xFFFF. */
    CREATE_LOG_CMD_SQCQ,                 /* 3 create log cqsq */
    RELEASE_LOG_CMD_SQCQ,                /* 4 release log cqsq */
    CREATE_DEBUG_CMD_SQCQ,               /* 5 create debug cqsq */
    RELEASE_DEBUG_CMD_SQCQ,              /* 6 release debug cqsq */
    CREATE_PROFILE_CMD_SQCQ,             /* 7 create profile cqsq */
    RELEASE_PROFILE_CMD_SQCQ,            /* 8 release profile cqsq */
    CREATE_HEART_BEAT_SQCQ,              /* 9 for heart beat */
    RELEASE_HEART_BEAT_SQCQ,             /* 10 for heart beat */
    RECYCLE_EVENT_ID,                    /* 11 when runtime occurs exception,recycle the event id resources */
    RECYCLE_STREAM_SQCQ,                 /* 12 recycle streamid &sq cq when runtime occures exception */
    CREATE_CONTAINER_INFO,               /* 13 create container tflops */
    CONFIG_P2P_INFO,                     /* 14 for muti chip access */
    RECEIVE_RDMA_INFO,                   /* 15 for rdma */
    RESET_NOTIFY_ID,                     /* 16 reset hwts notify table id */
    SYNC_TIME,                           /* 17 sync time */
    APP_EXIT,                            /* 18 app process exit */
    RECEIVE_SHARED_MEMORY_INFO = 0x0014, /* 20 shared memory info */
    HOST_ID_INFO,                        /* 21 host_id info */
    CREATE_AICPU_CHANNEL,                /* 22 create aicpu channel */
    RELEASE_AICPU_CHANNEL,               /* 23 release aicpu channel */
    CREATE_CB_SQCQ,                      /* 24 create callback sqcq */
    RELEASE_CB_SQCQ,                     /* 25 release callback sqcq */
    CREATE_LOGIC_CBCQ,                   /* 26 create callback logic cq */
    RELEASE_LOGIC_CBCQ,                  /* 27 release callback logic cq */
    MANAGE_NOTIFY_INFO,                  /* 28 manage notify info */
    MANAGE_EVENT_INFO,                   /* 29 manage event info */
    CREATE_VF_MACHINE,                   /* 30 create vf */
    RELEASE_VF_MACHINE,                  /* 31 release vf */
    RESET_EVENT_ID,                      /* 32 reset stars event table id */
    CREATE_DRIVER_SQCQ,                  /* 33 create driver sqcq */
    RELEASE_DRIVER_SQCQ,                 /* 34 release driver sqcq */
    CREATE_LOGIC_CQ,                     /* 35 create logic cq */
    RELEASE_LOGIC_CQ,                    /* 36 release logic cq */
    CREATE_DRV_SQCQ,                     /* 37 create aicpu's topic scheduler sqid and cqid */
    RELEASE_DRV_SQCQ,                    /* 38 release create aicpu's topic scheduler sqid and cqid */
    NOTICE_RESOURCE_MAPPING,             /* 39 notice resource mapping */
    APP_EXIT_FINISH_CHECK,               /* 40 return whether if the app_exit process has finished */
    ALLOC_RUNTIME_STREAM_ID,             /* 41 alloc runtime stream id for bs9sx1a */
    FREE_RUNTIME_STREAM_ID,              /* 42 free runtime stream id for bs9sx1a */
    CREATE_TASK_CMD_DVPP_SQCQ,           /* 43 enable sqcq from dvpp */
    RELEASE_TASK_CMD_DVPP_SQCQ,          /* 44 release sqcq from dvpp */
    NOTICE_SSID_VF,                      /* 45 notice ssid info in vf */
    QUERY_SSID_VF,                       /* 46 notice ssid info in vf */
    CREATE_TS_SQCQ,                      /* 47 alloc ts sqid and cqid */
    RELEASE_TS_SQCQ,                     /* 48 free ts sqcq message */
    CREATE_CTRL_SQ,                      /* 49 create control sq */
    RELEASE_CTRL_SQ,                     /* 50 free control sq */
    NOTICE_SQ_TRIGGER,                   /* 51 mailbox notice sq trigger */
    CTRL_MSG_PROC,                       /* 52 ctrl msg process */
    CONFIG_STARS_URPC,                   /* 53 mailbox stars urpc config */
    MAINT_SQ_TASK_SEND,                  /* 54 maint sq task send */
    MAINT_CQ_REPORT_RECV,                /* 55 maint cq report recv */
    CHECK_RTSQ_RECYCLE_STATUS,           /* 56 query rtsq status */
    AIC_UTIL_START,                      /* 57 query aicore util start */
    AIC_UTIL_END,                        /* 58 query aicore util end */
    CONFIG_QUEUE_SUBSCRIBE,              /* 59 config dqs consumer queue subscribe info */
    FAST_RINGBUFFER_CTRL_MSG,            /* 60 fast ring buffer memory ctrl msg */
    DCMI_CTRL_MSG_PROC,                  /* 61 DCMI ctrl msg process, set op timeout */
    DVPP_CMDLIST_INFO,                   /* 62 set dvpp sqe cmdlist */
    UNMASK_CTRL_CPU_SUBSCRIBE_DOORBELL,  /* 63 unmask doorbell for ctrl cpu subscriber */
    SQ_SWITCH_STREAM = 64,               /* 64 update stream for sq */
#ifdef DAVINCI_CLOUD_V2
    INVALID_CMD = 0xFF,                  /* invalid flag */
#else
    INVALID_CMD,                         /* invalid flag */
#endif
};

#define TS_MAX_MAILBOX_FUNC_NUM          (INVALID_CMD)

typedef struct {
    volatile uint32_t ack_irq;
} tsCmdIrqInfo_t;

typedef struct {
    volatile uint64_t sq_addr; /* invalid:0x0 */
    volatile uint64_t cq_addr0;
    volatile uint16_t sq_idx;      /* invalid:0xFFFF */
    volatile uint16_t cq_idx0;     /* sq's return */
    volatile uint8_t app_flag : 1; /* 0:offline, 1:online */
    volatile uint8_t sq_ctrl_flag : 1; /* 1: sq has head and tail share memory at last 2 sqe, 0 without */
    volatile uint8_t vf_id : 6;
    volatile uint8_t sq_cq_side : 2;      /* online: bit 0 sq side, bit 1 cq side. device: 0 host: 1  */
    volatile uint8_t has_host_pid : 1;    /* 0:no, 1:yes get host_pid from info[4] */
    volatile uint8_t res : 5;
    volatile uint8_t sqe_size;
    volatile uint8_t cqe_size;
    volatile uint16_t cqdepth;
    volatile uint16_t sqdepth;
    volatile uint32_t pid;
    volatile uint16_t cq_irq;
    volatile uint16_t smmu_sub_streamid;
    volatile uint32_t info[5]; /* runtime self-define, info[0] is streamid, info[4] is host_pid when mc2 scene */
} tsCmdSQCQInfo_t;

typedef union {
    struct {
        uint32_t sq_lock : 1;
        uint32_t wait_lock : 1; // rt set
        uint32_t res0    : 30;
    } bits;
    uint32_t u32;
} tsStreamFlag_t;
enum {
    TS_SQCQ_NORMAL_TYPE = 0,
    TS_SQCQ_TOPIC_TYPE,
    TS_SUB_TYPE_INVALID
};

#pragma pack(4)  // single-byte alignment
typedef struct {
    volatile uint64_t valid_flag;         // bit 0: streamFlag is set, bit1:simt is set
    volatile tsStreamFlag_t stream_flag;
    volatile uint32_t kis_simt_stk_base_addr_l;         // rt set for simt operator
    volatile uint32_t kis_simt_stk_base_addr_h  : 16;   // rt set for simt operator
    volatile uint32_t res1 : 16;                        // reserved
    volatile uint32_t kis_simt_warp_stk_size;           // rt set for simt operator
    volatile uint32_t kis_simt_dvg_warp_stk_size;       // rt set for simt operator
    volatile uint32_t pciedma_sq_pasid_quat_va;         // drv set for pcie security
    volatile uint32_t pciedma_chn_request_id_quat_val;  // drv set for pcie security
    volatile uint32_t pool_id;                          // rt set for vf other resource
    volatile uint32_t pool_id_max;                      // rt set for vf aic aiv resource
} normalInfoExt_t;
 
typedef struct {
    volatile uint8_t  mb_spec   : 1;
    volatile uint8_t  mb_specid : 4;
    volatile uint8_t  res0      : 3;
} topicInfoExt_t;
 
#ifdef ENABLE_VERIFICATION_PATCH
struct trs_ext_info_header {
    uint32_t type;
    uint32_t host_ssid;
    uint32_t hccp_pid;
    uint32_t cp_pid;
    uint32_t vfid;
    uint32_t rsv[11];
    char data[0]; // indicates data following
};
#endif
typedef struct {
#ifdef STARS_CTRL_CPU
    struct trs_ext_info_header header;  /* msg type: 0 normal sqcq, 1 topic sqcq */
#endif
    union {
        normalInfoExt_t normal_info;
        topicInfoExt_t topic_info;
    } u;
} infoExMsg_t;
 
typedef struct {
    volatile uint32_t     msg_len;
    volatile infoExMsg_t  *msg;
    volatile uint32_t     *info; // SQCQ_INFO_LENGTH: 5
} infoExt_t;
#pragma pack()  // Cancels single-byte alignment
 
typedef struct {
    volatile uint64_t sq_addr; /* invalid:0x0 */
    volatile uint64_t cq_addr0;
    volatile uint16_t sq_idx;      /* invalid:0xFFFF */
    volatile uint16_t cq_idx0;     /* sq's return */
    volatile uint8_t app_flag : 1; /* 0:offline, 1:online */
    volatile uint8_t sq_ctrl_flag : 1; /* 1: sq has head and tail share memory at last 2 sqe, 0 without */
    volatile uint8_t vf_id : 6;
    volatile uint8_t sq_cq_side : 2;      /* online: bit 0 sq side, bit 1 cq side. device: 0 host: 1  */
    volatile uint8_t has_host_pid : 1;    /* 0:no, 1:yes get host_pid from info[4] */
    volatile uint8_t sq_addr_is_virtual : 1;
    volatile uint8_t cq_addr_is_virtual : 1;
    volatile uint8_t is_convert_pid  : 1;
    volatile uint8_t res : 2;
    volatile uint8_t sqe_size;
    volatile uint8_t cqe_size;
    volatile uint16_t cqdepth;
    volatile uint16_t sqdepth;
    volatile uint32_t pid;
    volatile uint16_t cq_irq;
    volatile uint16_t smmu_sub_streamid;
    volatile infoExt_t info_ext;
} tsCmdSQCQInfoExt_t;

typedef struct {
    volatile uint64_t sq_addr; /* invalid:0x0 */
    volatile uint64_t cq_addr0;
    volatile uint64_t cq_addr1;
    volatile uint64_t cq_addr2;
    volatile uint64_t cq_addr3;
    volatile uint16_t sq_idx;  /* invalid:0xFFFF */
    volatile uint16_t cq_idx0; /* sq's return */
    volatile uint16_t cq_idx1; /* ts's return */
    volatile uint16_t cq_idx2; /* ai cpu's return */
    volatile uint16_t cq_idx3; /* reserved */
    volatile uint16_t cq_irq;
    volatile uint8_t app_flag;     /* 0:offline, 1:online */
    volatile uint8_t cq_slot_size; /* reserved */
    volatile uint16_t group_id;
} tsSQCQInfo_t;

typedef struct {
    volatile uint16_t event_count;
    volatile uint16_t event_id[RECYCLE_EVENT_COUNT];
} ts_recycle_event_id_t;

typedef struct {
    volatile uint16_t group_id;
    volatile uint16_t tflops;
} ts_container_info_t;

#ifndef STARS_CTRL_CPU
typedef struct {
    volatile uint16_t notify_id;
    volatile uint16_t operation_type;  // ts_event_operation_type_t
    volatile uint32_t host_pid;
    volatile uint16_t vir_notify_id;
    volatile uint8_t vf_id;
    volatile uint8_t notify_type;  // 0 noitfy id, 1 event id
    volatile uint8_t reserved[44];
} ts_notify_info_t;
#else
typedef struct {
    volatile uint32_t notify_id;
    volatile uint32_t host_pid;
    volatile uint16_t operation_type;  // ts_event_operation_type_t
    volatile uint16_t vir_notify_id;
    volatile uint8_t vf_id;
    volatile uint8_t notify_type;  // 0 noitfy id, 1 event id, 2 count notify
    volatile uint8_t reserved[42];
} ts_notify_info_t;
#endif

typedef struct {
    volatile uint8_t vf_id;
    volatile uint8_t resource_type;   // 0:notify id, 1:event id
    volatile uint8_t operation_type;  // 0:map, 1:unmap
    volatile uint8_t reserve0;
    volatile uint16_t vir_id;
    volatile uint16_t phy_id;
    volatile uint32_t host_pid;
    volatile uint8_t reserve[44];
} ts_resource_mapping_notice_t;

typedef struct {
    volatile uint32_t event_id;
} ts_free_event_id_info_t;

typedef struct {
    volatile uint64_t first_addr;
    volatile uint32_t size;
} ts_shared_memory_info_t;

typedef struct {
    volatile uint8_t self_id;
    volatile uint8_t host_id;
} ts_hostid_info_t;

typedef struct {
    volatile uint32_t opcode;
    volatile uint32_t length;
    volatile uint64_t addr;
    volatile uint8_t reserve[40];
} ts_rdma_memory_info_t;

typedef struct {
    volatile uint32_t opcode;
    volatile uint32_t idx;       // sq info index
    volatile uint64_t sq_addr;     // sq addr
    volatile uint64_t temp_addr;   // moulde addr
    volatile uint64_t db_addr;     // doorbell addr
    volatile uint64_t dfx_addr;    // dfx addr
    volatile uint32_t sq_depth;    // sq depth
    volatile uint32_t temp_depth;  // moulde depth
    volatile uint64_t db_reg;      // doorbell register addr
} ts_rdma_sq_t;

typedef struct {
    union Data {
        ts_rdma_memory_info_t rdma_memory_info;
        ts_rdma_sq_t rdma_sq;
    } d;
} ts_rdma_info_t;

typedef struct {
    volatile uint8_t remote_devId;
    volatile uint8_t status;
    volatile uint8_t type;
    volatile uint8_t local_devId;
    volatile uint32_t reserved2;
    volatile uint64_t db_addr;
    volatile uint32_t db_size;
    volatile uint64_t sram_addr;
    volatile uint32_t sram_size;
    volatile uint64_t hwts_p2p_addr;
    volatile uint32_t hwts_size;
} ts_multi_chip_info_t;

typedef struct {
    volatile uint32_t app_count;
    volatile uint32_t app_pid[APP_EXIT_COUNT_MAX];
    volatile uint8_t app_flag;
    volatile uint8_t vf_id;
    volatile uint8_t reserved[2];
} ts_app_exit_info_t;

typedef struct {
    volatile uint64_t addr;
    volatile uint64_t len;
    volatile uint32_t pid;
    volatile uint8_t vf_id;
} ts_create_fast_ringbuffer_t;

typedef struct {
    ts_ctrl_msg_head_t head;
    ts_ctrl_msg_body_t body;
} ts_ctrl_msg_info_t;

enum ts_dcmi_msg_type_t {
    DCMI_TYPE_STARS_TASK_TIMEOUT_SET = 0,
    DCMI_TYPE_STARS_TASK_TIMEOUT_GET,
};

typedef struct {
    volatile uint32_t timeout_limit_exp;
    volatile uint8_t resv[36];
} ts_dcmi_task_timeout_t;

typedef struct {
    volatile uint32_t type;
    union {
        ts_dcmi_task_timeout_t set_task_timeout_info;
        ts_dcmi_task_timeout_t get_task_timeout_info;
    } u; // 40 bytes
} ts_dcmi_ctrl_msg_body_t; // 44 bytes

typedef struct {
    ts_ctrl_msg_head_t head;
    ts_dcmi_ctrl_msg_body_t body;
} ts_dcmi_ctrl_msg_info_t;

typedef struct {
    volatile uint64_t sq_addr;    // first 2 bytes mean head, second 2 bytes mean tail
    volatile uint64_t cq_addr0;
    volatile uint16_t channel_id;
    volatile uint16_t sqe_length;
    volatile uint16_t cqe_length;
    volatile uint16_t sqe_depth;
    volatile uint16_t cqe_depth;
} ts_create_aicpu_channel_t;

typedef struct {
    volatile uint16_t channel_id;
} ts_release_aicpu_channel_t;

typedef struct {
    volatile uint64_t phy_cq_addr;
    volatile uint16_t cqe_size;
    volatile uint16_t cq_depth;
    volatile uint32_t vpid;
    volatile uint16_t logic_cqid;
    volatile uint16_t phy_cqid;
    volatile uint16_t cq_irq;
    volatile uint8_t app_flag;
    volatile uint8_t thread_bind_irq_flag;
    volatile uint8_t vf_id;
    volatile uint8_t reserved[3]; // 3:for byte align
    volatile uint32_t info[5];
} ts_create_logic_cq_t;

typedef struct {
    volatile uint32_t vpid;
    volatile uint16_t logic_cqid;
    volatile uint16_t phy_cqid;
    volatile uint8_t vf_id;
    volatile uint8_t reserved[3]; // 3:for byte align
} ts_release_logic_cq_t;

typedef struct {
    volatile uint8_t vf_id;
    volatile uint8_t aic_num;
    volatile uint8_t reserved;
    volatile uint8_t vfg_id;
    volatile uint32_t vf_aicpu_bitmap;
    volatile uint32_t vfg_aicpu_bitmap;
} ts_create_vf_info_t;

typedef struct {
    volatile uint8_t vf_id;
    volatile uint8_t reserved[3];
} ts_release_vf_info_t;

typedef struct {
    uint32_t sq_id;
    uint32_t cq_id;
    uint32_t vf_id;
    uint32_t pid;
    uint16_t ssid;
    uint16_t app_flag : 1;
    uint16_t rsv : 15;
    uint32_t info[5];
} notice_ts_sqcq_t;

typedef struct {
    uint32_t priority;
    uint32_t stream_id;
    uint32_t vf_id;
    uint32_t pid;
} ts_alloc_runtime_stream_id_t;

typedef struct {
    uint32_t stream_id;
} ts_free_runtime_stream_id_t;

typedef struct {
    uint8_t vf_id;
    uint8_t vfg_id; // reserved for 1971
    uint16_t ss_id;
    uint32_t pid;
} notice_ts_ssid_t;

typedef struct {
    uint32_t db;
    uint32_t irq;
    uint8_t idx;  // trigger info index, pf: 0, vf: 1-16
    uint8_t reserved[3];
} ts_sq_trigger_msg_t;

typedef struct {
    volatile uint8_t vf_id;
    volatile uint8_t reserved[3];
} ts_stars_create_vf_info_t;

struct trs_urpc_jetty_config {
    uint16_t jetty_ci;
    uint16_t jetty_pi;
    uint16_t jetty_length_shift;
    uint64_t jetty_base_addr;
    uint64_t doorbell_addr;
    uint32_t jetty_id;
    uint8_t func_id;
    uint8_t die_id;
    
    uint32_t trans_obj_id : 24;
    uint32_t rsv : 2;
    uint32_t odr : 3;
    uint32_t token_en : 1;
    uint32_t rmt_jetty_type : 2;
    
    uint32_t rmt_jetty_or_seg_id;
    
    uint64_t rmt_eid_low;
    uint64_t rmt_eid_high;
    
    uint32_t rmt_token_value;

    union {
        uint8_t int_mode;                // cqe
        uint64_t sq_head_base_addr;      // sq head
    };

    uint32_t urpc_aw_wqe_setting_0;
    uint32_t urpc_db_wqe_setting_0;
    uint8_t urpc_aw_wqe_setting_1;
    uint8_t urpc_db_wqe_setting_1;
};

typedef struct {
    struct trs_urpc_jetty_config *cq_jetty_info;
    struct trs_urpc_jetty_config *sq_head_jetty_info;
    uint32_t vf_id;
} ts_config_stars_urpc_info_t;

typedef struct {
    uint32_t core_type;     // 0 aic, 1 aiv
    uint32_t vf_id;
} ts_aic_util_message_t;

// end for dqs
#pragma pack(4) 
#define  MAX_SWITCH_NODE_CNT (3)
typedef struct {
    uint16_t sq_id;
    uint16_t sq_depth;
    uint32_t stream_id;
    uint64_t sq_addr;
} sq_swtich_node_t;

typedef struct {
    uint32_t cnt;
    sq_swtich_node_t nodes[MAX_SWITCH_NODE_CNT];
} stars_switch_sq_for_stream_info_t;
#pragma pack() 
typedef struct tag_ts_mailbox {
    volatile uint16_t valid;  // valid:0x5a5a, invalid:0x0
    volatile uint16_t cmd_type;
    volatile uint32_t result;  // normal:0, unnormal: not 0
    union {
        tsCmdIrqInfo_t notice_irq_info;
        tsCmdSQCQInfo_t cmd_sqcq_info;
        tsSQCQInfo_t sq_cq_info;
        ts_recycle_event_id_t recycle_event;
        ts_container_info_t container_info;
        ts_notify_info_t notify_info;
        ts_resource_mapping_notice_t resource_mapping_notice;
        ts_free_event_id_info_t free_event_id_info;
        ts_rdma_info_t rdma_info;
        ts_multi_chip_info_t multi_chip_info;
        ts_app_exit_info_t app_exit_info;
        ts_shared_memory_info_t shared_memory_info;
        ts_hostid_info_t hostid_info;
        ts_create_aicpu_channel_t aicpu_channel_info;
        ts_release_aicpu_channel_t release_aicpu_channel_info;
        ts_create_callback_sqcq_t create_callback_sqcq;
        ts_release_callback_sqcq_t release_callback_sqcq;
        ts_create_callback_logic_cq_t create_callback_logic_cq;
        ts_release_callback_logic_cq_t release_callback_logic_cq;
        ts_create_vf_info_t create_vf_info;
        ts_release_vf_info_t release_vf_info;
        ts_create_logic_cq_t create_logic_cq;
        ts_release_logic_cq_t release_logic_cq;
        notice_ts_sqcq_t notice_ts_sqcq;
        ts_alloc_runtime_stream_id_t alloc_runtime_stream_id;
        ts_free_runtime_stream_id_t free_runtime_stream_id;
        notice_ts_ssid_t notice_ts_ssid;
        ts_sq_trigger_msg_t ts_sq_trigger_msg;
        ts_stars_create_vf_info_t stars_create_vf_info;
        tsCmdSQCQInfoExt_t cmd_sqcq_info_ext;
        ts_config_stars_urpc_info_t config_stars_urpc_info;
        ts_ctrl_msg_info_t ctrl_msg_info;
        ts_aic_util_message_t aic_util_msg;
        ts_dcmi_ctrl_msg_info_t dcmi_ctrl_msg_info;
        ts_create_fast_ringbuffer_t fast_ringbuffer;
        stars_switch_sq_for_stream_info_t sq_switch_info;
    } u;  // 56 bytes
} ts_mailbox_t;

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* MB_STRUCT_H */
