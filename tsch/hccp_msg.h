/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: hccp_msg.h
 * Create: 2025-04-22
 */

#ifndef TS_HCCP_MSG_H
#define TS_HCCP_MSG_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct tag_ts_ccu_task_info {
    uint8_t udie_id;
    uint8_t rsv;
    uint16_t mission_id;
} ts_ccu_task_info_t;

typedef struct tag_ts_ub_task_info {
    uint8_t udie_id;
    uint8_t function_id;
    uint16_t jetty_id;
} ts_ub_task_info_t;

typedef struct tag_ts_ccu_task_report {
    uint8_t num;
    ts_ccu_task_info_t array[8];
    uint8_t rsv[63]; // rsv 63B
} ts_ccu_task_report_t;

typedef struct tag_ts_ub_task_report {
    uint8_t num;
    ts_ub_task_info_t array[4];
    uint8_t rsv[79]; // rsv 79B
} ts_ub_task_report_t;

typedef struct tag_ts_hccp_msg {
    // head 32B
    int32_t hccp_pid;  // apm_query_slave_tgid_by_master
    uint32_t host_pid;
    uint8_t cmd_type;   // 0:ub force kill; 1:ccu force kill
    uint8_t vf_id;
    uint16_t sq_id;
    uint16_t is_app_exit;
    uint16_t sqe_type;
    uint8_t rsv[16];
    // info 96B
    union {
        ts_ub_task_report_t ub_task_info;
        ts_ccu_task_report_t ccu_task_info;
    } u;
} ts_hccp_msg;

typedef enum tag_ts_hccp_sub_event_id {
    TOPIC_SEND_KILL_MSG = 1U,
    TOPIC_KILL_DONE_MSG,
} ts_hccp_sub_event_id;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TS_HCCP_MSG_H */