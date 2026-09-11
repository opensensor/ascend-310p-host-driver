/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
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
 * Create: 2022-08-13
 */

#ifndef DRV_LOG_H
#define DRV_LOG_H

#if defined(DAVINCI_DEVICE) || defined(CFG_BUILD_NOT_SUPPORT_KERNEL_OPEN) || defined(CFG_FEATURE_MDC_DRV_LOG) || defined(UT_VCAST)
#ifndef DVPP_UTST
#include <linux/types.h>
#include <linux/mm_types.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/preempt.h>
#endif
#define log_printk printk
#define log_pr_debug pr_debug
#define log_strrchr strrchr
#define log_printk_ratelimited printk_ratelimited
#define LOG_KERN_ERR KERN_ERR
#define LOG_KERN_NOTICE KERN_NOTICE
#define LOG_KERN_INFO KERN_INFO
#define LOG_KERN_DEBUG KERN_DEBUG
#define LOG_KERN_WARNING KERN_WARNING
#else
#include "ka_dfx_pub.h"
#include "ka_base_pub.h"
#define log_printk ka_dfx_printk
#define log_pr_debug ka_dfx_pr_debug
#define log_strrchr ka_base_strrchr
#define log_printk_ratelimited ka_dfx_printk_ratelimited
#define LOG_KERN_ERR KA_KERN_ERR
#define LOG_KERN_NOTICE KA_KERN_NOTICE
#define LOG_KERN_INFO KA_KERN_INFO
#define LOG_KERN_DEBUG KA_KERN_DEBUG
#define LOG_KERN_WARNING KA_KERN_WARNING
#endif

#define LOG_LEVEL_INFO_INPUT_LEN                (2U)
#define LOG_LEVEL_FILE_INFO_LEN                 (3U)
#define DEVDRV_HOST_LOG_FILE_CREAT_AUTHORITY    (436U)  /* 0664 */
int log_level_file_init(void);
void log_level_file_remove(void);
int log_level_get(void);
void log_save_to_ring_buf(const char *fmt, ...);
void log_to_printk_and_ringbuf(const char *fmt, ...);

/* host ko depmod */
#define PCI_DEVICE_CLOUD (0xa126U)
#define LOG_BUF_SIZE_MAX (100U)

typedef struct log_buf_info {
    unsigned int log_size;
    char log_buf[LOG_BUF_SIZE_MAX];
} log_buf_info_t;

#define FILENAME (log_strrchr(__FILE__, '/') ? log_strrchr(__FILE__, '/') + 1 : __FILE__)
#define drv_pr_debug(module, fmt, ...) \
    log_pr_debug("[ascend] [%s] [%s %d] " fmt, module, __func__, __LINE__, ##__VA_ARGS__)

#ifdef CFG_FEATURE_MDC_DRV_LOG
#define DRV_FACILITY ((int)86)
int __attribute__((weak)) drv_vprintk_emit(int level, const char *fmt, ...)
{
    va_list args;
    int r;

    va_start(args, fmt);
    r = vprintk_emit(DRV_FACILITY, level, NULL, fmt, args);
    va_end(args);

    return r;
}

#define drv_printk(level, module, fmt, ...) \
    (void)drv_vprintk_emit(level, "[ascend] [%s] [%s %d] " fmt, module, __func__, __LINE__, ##__VA_ARGS__)

#define drv_err(module, fmt...) drv_printk(LOGLEVEL_ERR, module, fmt)
#define drv_warn(module, fmt, ...) drv_printk(LOGLEVEL_WARNING, module, fmt, ##__VA_ARGS__)
#define drv_info(module, fmt, ...) drv_printk(LOGLEVEL_INFO, module, fmt, ##__VA_ARGS__)
#define drv_debug(module, fmt, ...) drv_printk(LOGLEVEL_DEBUG, module, fmt, ##__VA_ARGS__)
#define drv_event(module, fmt, ...) drv_printk(LOGLEVEL_NOTICE, module, fmt, ##__VA_ARGS__)
#else
#ifdef CFG_FEATURE_HOST_LOG
#define drv_log_print(kern_level, level, module, fmt, ...) \
    log_save_to_ring_buf(kern_level "[ascend] [%s] [%s] [%s %d] " fmt, \
        module, level, __func__, __LINE__, ##__VA_ARGS__)

#define drv_err(module, fmt, ...) \
    log_to_printk_and_ringbuf(LOG_KERN_NOTICE "[ascend] [%s] [ERROR] [%s %d] " fmt, \
        module, __func__, __LINE__, ##__VA_ARGS__)
#define drv_warn(module, fmt, ...) drv_log_print(LOG_KERN_WARNING, "WARN", module, fmt, ##__VA_ARGS__)
#define drv_info(module, fmt, ...) drv_log_print(LOG_KERN_INFO, "INFO", module, fmt, ##__VA_ARGS__)
#define drv_event(module, fmt, ...) drv_log_print(LOG_KERN_NOTICE, "NOTICE", module, fmt, ##__VA_ARGS__)
#define drv_debug(module, fmt, ...) \
    (void)log_printk(LOG_KERN_DEBUG "[ascend] [%s] [%s %d] " fmt, module, __func__, __LINE__, ##__VA_ARGS__)
#else

#define drv_printk(level, module, fmt, ...) \
    (void)log_printk(level "[ascend] [%s] [%s %d] " fmt, module, __func__, __LINE__, ##__VA_ARGS__)

#if (defined(LOG_UT) || defined(CFG_FEATURE_DRV_LOG_ERR))
#define drv_err(module, fmt...) drv_printk(LOG_KERN_ERR, module, fmt)
#else
#define logflow_printk(level, module, fmt, ...) \
    (void)log_printk(level "[ascend] [ERROR] [%s] [%s %d] " fmt, module, __func__, __LINE__, ##__VA_ARGS__)
/* drv_err is KERN_NOTICE level to avoid too much serial print cause system watchdog reset.
 * if you want to change this, you must call SE to check this */
#if defined(DAVINCI_DEVICE) || defined(CFG_BUILD_NOT_SUPPORT_KERNEL_OPEN) || defined(UT_VCAST)
#define drv_err(module, fmt...) logflow_printk(LOG_KERN_ERR, module, fmt)
#else
#define drv_err(module, fmt...) logflow_printk(LOG_KERN_NOTICE, module, fmt)
#endif

#endif
/* only dev use fault event log */
void log_drv_get_date(char *date, unsigned int len);
void log_user_write_fault_mng(const char *module, int pid, const char *comm, const char *date, const char *file, int line, const char *fmt, ...);
#define drv_warn(module, fmt, ...) drv_printk(LOG_KERN_WARNING, module, fmt, ##__VA_ARGS__)
#define drv_info(module, fmt, ...) drv_printk(LOG_KERN_INFO, module, fmt, ##__VA_ARGS__)
#define drv_debug(module, fmt, ...) drv_printk(LOG_KERN_DEBUG, module, fmt, ##__VA_ARGS__)
#define drv_event(module, fmt, ...) drv_printk(LOG_KERN_NOTICE, module, fmt, ##__VA_ARGS__)
#define drv_slog_event(module, fmt, ...) do { \
    char date[64] = { 0 }; \
    log_drv_get_date(date, sizeof(date)); \
    log_user_write_fault_mng(module, (int)current->pid, current->comm, date, FILENAME, __LINE__, fmt, ##__VA_ARGS__);\
} while (0)

#endif
#endif

#define drv_err_spinlock(module, fmt, ...) drv_err(module, fmt, ##__VA_ARGS__)
#define drv_warn_spinlock(module, fmt, ...) drv_warn(module, fmt, ##__VA_ARGS__)
#define drv_info_spinlock(module, fmt, ...) drv_info(module, fmt, ##__VA_ARGS__)
#define drv_debug_spinlock(module, fmt, ...) drv_debug(module, fmt, ##__VA_ARGS__)
#define drv_event_spinlock(module, fmt, ...) drv_event(module, fmt, ##__VA_ARGS__)

#define drv_printk_ratelimited(level, module, fmt, ...) \
    log_printk_ratelimited(level "[ascend] [%s] [%s %d] " fmt, module, __func__, __LINE__, ##__VA_ARGS__)

#define drv_info_ratelimited(module, fmt, ...) drv_printk_ratelimited(LOG_KERN_INFO, module, fmt, ##__VA_ARGS__)
#define drv_err_ratelimited(module, fmt, ...) drv_printk_ratelimited(LOG_KERN_ERR, module, fmt, ##__VA_ARGS__)

/**
 * Description of log interfaces used in special scenarios
 * for example: record logs in the spin_lock
 * drv_log_init() // only can be called once within a function
 * spin_lock_xxx
 * drv_err_log_save() // can be called multiple times within a function
 * spin_unlock_xxx
 * drv_log_output() // can be called multiple times within a function
 *
 * Precautions:
 * (1) drv_log_init(), drv_err_log_save() and drv_log_output() must be used together in the same function;
 * (2) drv_log_init() must be called at the variable definition, otherwise there will be a compilation error;
 * (3) the max size of logs can be saved continuously is LOG_BUF_SIZE_MAX bytes;
 * (4) only error logs can be recorded.
 */
#define drv_log_init() \
    log_buf_info_t buf_info = {0}

#define DRV_LOG_SAVE(level_code, module, fmt, ...) do { \
    int ret; \
    ret = snprintf_s(buf_info.log_buf + buf_info.log_size, LOG_BUF_SIZE_MAX - buf_info.log_size, \
        LOG_BUF_SIZE_MAX - buf_info.log_size - 1, \
        level_code "[ascend] [ERROR] [%s] [%s %d] " fmt, module, __func__, __LINE__, ##__VA_ARGS__); \
    if (ret > 0) { \
        buf_info.log_size += ret; \
    } \
} while (0)

#define drv_err_log_save(module, fmt, ...) do { \
    if (buf_info.log_size < (LOG_BUF_SIZE_MAX - 1)) { \
        if (buf_info.log_size == 0) { \
            DRV_LOG_SAVE(LOG_KERN_NOTICE, module, fmt, ##__VA_ARGS__); \
        } else { \
            DRV_LOG_SAVE("", module, fmt, ##__VA_ARGS__); \
        } \
    } \
} while (0)

#define drv_log_output() do { \
    if (buf_info.log_size > 0) { \
        (void)log_printk(buf_info.log_buf); \
        buf_info.log_size = 0; \
    } \
} while (0)

#endif /* _DRV_LOG_H_ */
