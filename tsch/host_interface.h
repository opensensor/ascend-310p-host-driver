/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2021. All rights reserved.
 * Description: host_interface.h
 * Create: 2020-01-01
 */
#ifndef TS_HOST_INTERFACE_H
#define TS_HOST_INTERFACE_H
#include "tsch/task_struct.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern uint8_t g_ts_task_cmd_queue_status;
/**
 * @ingroup tsch
 * @brief
 */
typedef struct tag_ts_task_cmd_queue {
    ts_task_t task_command_slot[TS_SIZE_OF_PER_TASK_CMD_QUEUE];
    volatile uint16_t read_idx;
    volatile uint16_t write_idx;
} ts_task_cmd_queue_t;

/**
 * @ingroup tsch
 * @brief return the task cmd queue addr
 * @param [in] priority the queue's priority
 * @return ts_task_cmd_queue_t*
 */
ts_task_cmd_queue_t *ts_get_task_cmd_queues(uint8_t priority);

/**
 * @ingroup tsch
 * @brief task cmd queue array struct
 */
typedef struct tag_ts_task_cmd_queues_array {
    /**
     * @ingroup tsch
     * @brief push a task cmd to the queue
     * @param [in] task which would be writed into the queue
     * @param [in|out] queue the object queue
     * @return TS_SUCCESS TS_ERROR_TASK_CMD_QUEUE_FULL
     */
    ts_error_t (*en_queue)(ts_task_cmd_queue_t *queue, ts_task_t *task);

    /**
     * @ingroup tsch
     * @brief pop a task cmd from the queue
     * @param [out] task
     * @param [in|out] queue the object queue
     * @return TS_SUCCESS TS_ERROR_TASK_CMD_QUEUE_EMPTY
     */
    ts_error_t (*dn_queue)(ts_task_cmd_queue_t *queue, ts_task_t *task);

    ts_error_t (*dnSQ)(uint16_t idx, ts_task_t *task);
    /**
     * @ingroup tsch
     * @brief want to know the queue is empty or not
     * @param [in|out] queue the object queue
     * @return TS_TURE TS_FALSE
     */
    ts_bool_t (*is_queue_empty)(const ts_task_cmd_queue_t *queue);
} ts_task_cmd_queues_manager_t;

ts_task_cmd_queues_manager_t *ts_get_task_cmd_queues_manager(void);

/**
 * @ingroup tsch
 * @brief
 */
typedef struct tag_ts_task_report_queue {
    ts_task_report_msg_t task_report_slot[TS_TASK_REPORT_QUEUE_SLOTS_COUNT];
    volatile uint16_t read_idx;
    volatile uint16_t write_idx;
} ts_task_report_queue_t;

/**
 * @ingroup tsch
 * @brief return the task report queue addr
 * @return ts_task_report_queue_t*
 */
ts_task_report_queue_t *ts_get_task_report_queue(void);

typedef struct tag_host_ctrl {
    ts_task_cmd_queue_t task_cmd_queues[TS_TASK_CMD_QUEUE_PRIORITIES_LEVEL];
    ts_task_report_queue_t task_report_queue;
} ts_host_ctrl_t;

extern ts_host_ctrl_t *g_host_ctrl;
/**
 * @ingroup tsch
 * @brief task cmd queue array struct
 */
typedef struct tag_ts_task_report_queue_manager {
    ts_error_t (*enCQ)(uint16_t idx, ts_task_report_msg_t *task);
    ts_error_t (*en_queue)(ts_task_report_msg_t *task);
    ts_bool_t (*is_queue_empty)(void);
    ts_bool_t (*is_queue_full)(void);
} ts_task_report_queue_manager_t;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TS_HOST_INTERFACE_H */
