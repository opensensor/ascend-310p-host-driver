/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2021. All rights reserved.
 * Description: task_scheduler.h
 * Create: 2020-01-01
 */
#ifndef TS_TASK_SCHEDULER_H
#define TS_TASK_SCHEDULER_H

#include "tsch_defines.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @ingroup tsch
 * @brief start to run the task scheduler
 */
void start_task_scheduler(void);

/**
 * @ingroup tsch
 * @brief stop the task scheduler
 */
void stop_task_scheduler(void);

/**
 * @ingroup tsch
 * @brief get the information about platform
 */
extern uint32_t get_platform(void);

/**
 * @ingroup
 * @brief get product mode
 * @return TS_PRODUCT_DC,TS_PRODUCT_MDC,TS_PRODUCT_DC_SAMPLEBACK
 */
uint32_t get_product_mode(void);

/**
 * @ingroup
 * @brief get ts chip
 * @return TS_CHIP_AIC,TS_CHIP_AIV
 */
uint32_t get_ts_id(void);

void smp_init(void);
void smp_destroy(void);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TS_TASK_SCHEDULER_H */
