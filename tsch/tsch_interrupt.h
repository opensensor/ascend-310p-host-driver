/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2021. All rights reserved.
 * Description: tsch_interrupt.h
 * Create: 2020-01-01
 */
#ifndef TSCH_INTERRUPT_H
#define TSCH_INTERRUPT_H

#include "tsch/tsch_defines.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define BSDONE_INTERRUPT_CLEAN_FOR_ESL_FLAG                               (0xffffffffULL)

#define SYS_DMA_TOTAL_INT_NUMS (SYS_DMA_END_INT_NO - SYS_DMA_START_INT_NO + 1U)

/**
 * @ingroup tsch
 * @brief the interrupt number between TS and pcie dma
 */
#define TS_INIIRUPT_NUM_PCIE_DMA_CHAN12_DONE  (604U)    /**< pcie dma done*/
#define TS_INIIRUPT_NUM_PCIE_DMA_CHAN12_ERROR (588U)   /**< pcie dma error*/
#define TS_INIIRUPT_NUM_PCIE_DMA_CHAN13_DONE  (605U)    /**< pcie dma done*/
#define TS_INIIRUPT_NUM_PCIE_DMA_CHAN13_ERROR (589U)   /**< pcie dma error*/
#define TS_INIIRUPT_NUM_PCIE_DMA_CHAN14_DONE  (606U)    /**< pcie dma done*/
#define TS_INIIRUPT_NUM_PCIE_DMA_CHAN14_ERROR (590U)   /**< pcie dma error*/
#define TS_INIIRUPT_NUM_PCIE_DMA_CHAN15_DONE  (607U)    /**< pcie dma done*/
#define TS_INIIRUPT_NUM_PCIE_DMA_CHAN15_ERROR (591U)   /**< pcie dma error*/

/**
 * @ingroup tsch
 * @brief the interrupt type of task scheduler
 */
#define TS_INTTRUPT_NUM_TASK_QUEUE              (0U)     /**< task cmd queue interrupt */
#define TS_INTTRUPT_NUM_DMA_DONE                (1U)     /**< dma done interrupt */
#ifndef __COMPILER_HUAWEILITEOS__
#define TS_INTTRUPT_NUM_TBS_DONE                (2U)     /**< the ai core has finished its task */
#endif
#define TS_INTTRUPT_NUM_TAISAN_DONE             (3U)     /**< the ai cpu has finished its task */
#define TS_INTTRUPT_NUM_REPORT_QUEUE_TIMER      (4U)     /**< the report queue timer interrupt */
#define TS_INTTRUPT_NUM_PAGEFAULT               (5U)     /**< page fault interrupt */
#define TS_INTTRUPT_NUM_TASK_SQ                 (6U)     /**< task SQ interrupt */
#define TS_INTTRUPT_NUM_CB_DONE_RESUME          (7U)     /**< callback done, resume stream */
#define TS_INTTRUPT_NUM_CTRL_TASK_SQ            (8U)     /**< ctrl task SQ interrupt */

typedef void (*int_cb_fun_t)(uint32_t);

typedef struct tag_ts_int_cb_s {
    int_cb_fun_t func;
    const char_t *info;
} ts_int_cb_t;

#define TS_INTTRUPT_NUM_RESERVED (100) /**< reserved */

extern uint64_t g_aicoreNeedDebug;
extern uint64_t g_aicoreNeedDebugPause;

typedef uint32_t ts_interrupt_num_t;

/**
 * @ingroup tsch
 * @brief the reture value of task scheduler module
 * @param [in] interruptID
 */
void interrupt_process_tasksq(uint32_t event);

/**
* @ingroup tsch
* @brief callback done, resume the correspond stream
* @param [in] interruptID
*/
void interrupt_process_callback_resume(uint32_t event);

/**
* @ingroup tsch
* @brief task queue interrupt process : means the task command queue has new tasks to process
* @param [in] interruptID
*/
void interrupt_process_task_queue(uint32_t event);

/**
 * @ingroup tsch
 * @brief trigger interrupt process : triger interrupt
 * @param [in] interruptID
 */
void ts_trigger_interrupt(ts_interrupt_num_t interrupt_num);

/**
 * @ingroup tsch
 * @brief trigger DFX interrupt process : triger DFX interrupt
 * @param [in] sqid
 * @param [in] is_online on_line or off_line mode
 */
void trigger_dfx_interrupt(uint16_t sqid, uint8_t is_online);

/**
 * @ingroup tsch
 * @brief trigger interrupt process : ts process aicpu interrupt done
 * @param [in] irq interrupt requests
 * @param [in] dev
 */
void interrupt_process_ai_cpu_done_(uint32_t irq, void *dev);

/**
 * @ingroup tsch
 * @brief trigger interrupt process : create aicpu interrupt
 * @param [in] irq interrupt requests
 */
uint32_t create_aicpu_interrupt(uint32_t irq);

/**
 * @ingroup tsch
 * @brief trigger interrupt process : send spi interrupt
 * @param [in] irq interrupt requests
 */
void send_interrupt(uint32_t irq);

/**
 * @ingroup tsch
 * @brief trigger interrupt process: send msi-x interrupt
 * @param [in] sqid
 * @param [in] is_online on_line or offline
 */
void send_msi_interrupt(uint32_t irq);

/**
 * @ingroup tsch
 * @brief increase test counter
 * @param [in] counter_type counter_type to be increased
 */
void increase_test_counter(uint32_t counter_type);

/**
 * @ingroup tsch
 * @brief increase test counter by num
 * @param [in] counter_type counter_type to be increased
 * @param [in] num increase counter by num
 */
void increase_test_counter_by_num(uint32_t counter_type, uint32_t num);

/**
 * @ingroup tsch
 * @brief init pcie_dma register
 */
void register_pcie_dma_int(void);

/**
 * @ingroup tsch
 * @brief trigger interrupt to process with sq/cq recycling
 * @param [in] event
 */
void interrupt_process_recycle_sqcq(uint32_t event);

/**
 * @ingroup tsch
 * @brief trigger interrupt to process with hccl mult device
 */
void interrupt_hccl_mult_device_process(void);

/**
 * @ingroup tsch
 * @brief send the interrupt when write a report to CQ
 * @param [in] drv irq
 */
uint32_t get_offline_cq_interrupt(uint16_t irq);

void ts_trigger_private_cq_send(void);
void recycle_callback_sqcq(uint32_t pid, uint16_t cq_id, uint8_t app_flag);
void send_recycle_eventid_msg(uint16_t cqId, uint8_t appFlag);
ts_error_t send_recycle_streamid_msg(uint16_t stream_id, uint16_t cqId, uint8_t appFlag);
ts_error_t send_recycle_sqid_msg(uint16_t sqId, uint16_t cqId, uint8_t appFlag);
ts_error_t appexit_reportCQ(uint16_t cqId, uint8_t appFlag, const void *report);
ts_error_t recycle_all_stream_record(uint32_t pid, uint16_t cq_id, uint8_t vf_id, uint8_t appFlag);
void recycle_event_record(uint32_t pid, uint16_t cqId, uint8_t vf_id, uint8_t appFlag, ts_bool_t is_slave);
void recycle_notify_record(uint32_t pid, uint16_t cq_id, uint8_t vf_id, uint8_t app_flag, ts_bool_t is_slave);
ts_error_t recycle_cq_record(uint32_t pid, uint16_t cqId, uint8_t vf_id, uint8_t appFlag);
ts_error_t recycle_sq_stream_record(uint32_t pid, uint16_t cqId, uint8_t appFlag, uint8_t vf_id);
void recycle_stream_by_sq(uint32_t sq_id, uint16_t cq_id, uint8_t appFlag);
void recycle_timewheel_manager(uint32_t pid, uint8_t vf_id);
void add_app_info_to_list_by_pid(uint32_t pid, uint16_t vf_id);
void *get_app_info_node_by_pid(uint32_t pid, uint16_t vf_id);
void del_app_info_from_list_by_pid(uint32_t pid, uint16_t vf_id);
ts_bool_t check_msg_valid_by_pid(uint32_t pid, uint16_t vf_id);
ts_error_t ts_reset_aicpu_task(uint16_t rt_sq_id, uint16_t cq_id, uint8_t app_flag, uint8_t vf_id);
ts_error_t ts_reset_aicpu_sched_info(uint16_t stream_id);
void interrupt_handle_hwts_debug_debug_pause(uint64_t *l2_debug_paused_status);
void interrupt_init_post(void);
void interrupt_deinit_post(void);
void interrupt_main_process(void);
ts_error_t appexit_reportcq_pre_check(uint16_t cq_id);
void hwts_swap_buffer_reset_BySQId(uint64_t sq_id);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TSCH_INTERRUPT_H */
