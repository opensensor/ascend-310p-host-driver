/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
 * Create: 2025-02-020
 */
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/rtc.h>
#include <linux/ktime.h>
#include <linux/uaccess.h>

#include "ka_kernel_def_pub.h"
#include "ka_task_pub.h"

#include "drv_log.h"
#include "securec.h"
#include "drv_log_event_write.h"

#ifndef DRVFAULT_UT
#define MODULE_LOG             "drv_log_fault_mng" 
#define slog_drv_err(fmt, ...)   \
    drv_err(MODULE_LOG, "<%s:%d,%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__)
#define slog_drv_info(fmt, ...)  \
    drv_info(MODULE_LOG, "<%s:%d,%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__)
#define slog_drv_warn(fmt, ...)  \
    drv_warn(MODULE_LOG, "<%s:%d,%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__)
#define STATIC static
#else
#define slog_drv_err(fmt, ...)   printf(fmt, ##__VA_ARGS__)
#define slog_drv_info(fmt, ...)  printf(fmt, ##__VA_ARGS__)
#define slog_drv_warn(fmt, ...)  printf(fmt, ##__VA_ARGS__)
#define STATIC 
#endif

#define LOG_MODE_KERNEL 0
#define LOG_MODE_USER 1
#define TWENTY_CENTURY        1900
#define JANUARY               1U
#define MEGA	              1000000UL
#define KILO                  1000U
#define LOG_RET_ERROR         (-1)
#define LOG_RET_OK            0

enum log_session_status {
    LOG_SESSION_STATUS_INIT = 0, 
    LOG_SESSION_STATUS_READABLE,
    LOG_SESSION_STATUS_READING
};

STATIC struct log_drv_fault_mng *g_drv_fault_mng = { NULL };
void log_set_fault_mng_info(struct log_drv_fault_mng *fault_mng)
{
    g_drv_fault_mng = fault_mng;
}
KA_EXPORT_SYMBOL_GPL(log_set_fault_mng_info);

#if (!defined(CFG_FEATURE_HOST_LOG) && !defined(CFG_FEATURE_MDC_DRV_LOG))
STATIC struct log_drv_fault_mng *log_get_drv_fault_mng_info(void)
{
    if (g_drv_fault_mng == NULL) {
        slog_drv_err("g_drv_fault_mng is null, uninit.\n");
        return NULL;
    }
    return g_drv_fault_mng;
}

void log_drv_get_date(char *date, unsigned int len)
{
#ifndef DRVFAULT_UT
    struct timespec64 sys_time = { 0 };
    struct rtc_time tm = { 0 };
    int ret;

    ktime_get_real_ts64(&sys_time);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
    tm = rtc_ktime_to_tm(timespec64_to_ktime(sys_time));
#else
    rtc_time_to_tm(sys_time.tv_sec, &tm);
#endif
    ret = snprintf_s(date, len, len - 1, "%04ld-%02d-%02d-%02d:%02d:%02d.%03lu.%03ld",
                     tm.tm_year + TWENTY_CENTURY, tm.tm_mon + JANUARY, tm.tm_mday, tm.tm_hour,
                     tm.tm_min, tm.tm_sec, (sys_time.tv_nsec / MEGA) % KILO, (sys_time.tv_nsec / KILO) % KILO);
    if (ret < 0) {
        slog_drv_err("Log snprintf_s failed. (ret=%d)\n", ret);
    }
#endif
    return;
}
KA_EXPORT_SYMBOL_GPL(log_drv_get_date);

STATIC u32 log_drv_get_ring_buffer_data_len(u32 buf_read, u32 buf_write, u32 buf_len)
{
    u32 data_len = 0;;

    if (buf_read <= buf_write) {
        data_len = buf_write - buf_read;
    } else {
        data_len = (buf_len - buf_read) + buf_write;
    }

    return data_len;
}

STATIC s32 log_write_ring_buffer(struct log_drv_fault_mng *fault_mng, bool *is_overwrite)
{
    u32 data_len, buf_left, src_len, buf_len;
    s32 ret;
    void *data_base;
    u32 w_ptr;

    if ((fault_mng->buf_read >= fault_mng->buf_size) || (fault_mng->buf_write >= fault_mng->buf_size)) {
        return LOG_RET_ERROR;
    }

    data_len = log_drv_get_ring_buffer_data_len(fault_mng->buf_read, fault_mng->buf_write, fault_mng->buf_size);
    buf_len = fault_mng->buf_size;
    buf_left = buf_len - data_len;
    src_len = strnlen(fault_mng->printk_buf, LOG_PRINT_LEN);
    if ((src_len == 0) || (src_len >= LOG_PRINT_LEN)) {
        return LOG_RET_ERROR;
    }

    if (src_len > buf_left) {
        *is_overwrite = true;
    }

    data_base = fault_mng->vir_addr;
    w_ptr = fault_mng->buf_write;
    buf_left = buf_len - w_ptr;
    if (src_len > buf_left) {
        ret = memcpy_s((void *)(data_base + w_ptr), buf_left, (void *)fault_mng->printk_buf, buf_left);
        if (ret != 0) {
            return LOG_RET_ERROR;
        }

        ret = memcpy_s((void *)data_base, buf_len, (void *)(fault_mng->printk_buf + buf_left), src_len - buf_left);
        if (ret != 0) {
            return LOG_RET_ERROR;
        }
    } else {
        ret = memcpy_s((void *)(data_base + w_ptr), buf_left, (void *)fault_mng->printk_buf, src_len);
        if (ret != 0) {
            return LOG_RET_ERROR;
        }
    }

    fault_mng->buf_write = (fault_mng->buf_write + src_len) % buf_len;
    return LOG_RET_OK;
}

void log_user_write_fault_mng(const char *module, int pid, const char *comm, const char *date, const char *file, int line, const char *fmt, ...)
{
    STATIC char new_fmt[LOG_PRINT_LEN];
    va_list args;
    int ret;
    unsigned long flags;
    bool is_overwrite = false;
    struct log_drv_fault_mng *fault_mng = NULL;

    fault_mng = log_get_drv_fault_mng_info();
    if (fault_mng == NULL) {
        slog_drv_err("g_drv_fault_mng is null.\n");
        return;
    }
    
    ka_task_spin_lock_irqsave(&fault_mng->spinlock, flags);
    ret = snprintf_s(new_fmt, LOG_PRINT_LEN, LOG_PRINT_LEN - 1, "[EVENT] DRV(%d,%s):%s [%s:%d][%s]%s", 
                     pid, comm, date, file, line, module, fmt);
    if (ret < 0) {
#ifndef DRVFAULT_UT
        spin_unlock_irqrestore(&fault_mng->spinlock, flags);
        slog_drv_err("Log snprintf_s failed. (ret=%d)\n", ret);
        return;
#endif
    }
    va_start(args, fmt);
    ret = vsnprintf_s(fault_mng->printk_buf, LOG_PRINT_LEN, LOG_PRINT_LEN - 1, new_fmt, args);
    va_end(args);
    if (ret <= 0) {
#ifndef DRVFAULT_UT
        spin_unlock_irqrestore(&fault_mng->spinlock, flags);
        slog_drv_err("Log vsnprintf_s failed. (ret=%d)\n", ret);
        return;
#endif
    }

    ret = log_write_ring_buffer(fault_mng, &is_overwrite);
    if (ret != 0) {
        spin_unlock_irqrestore(&fault_mng->spinlock, flags);
        slog_drv_err("Log write ringbuffer failed. (w_ptr=%u, r_ptr=%u, buf_size=%u)\n", fault_mng->buf_write, fault_mng->buf_read, fault_mng->buf_size);
        return;
    }
    spin_unlock_irqrestore(&fault_mng->spinlock, flags);
    atomic_set(&fault_mng->status, (s32)LOG_SESSION_STATUS_READABLE);
    wake_up(&fault_mng->wq);

    if (is_overwrite) {
        slog_drv_warn("Ring buffer is full. Data lost. (src_len=%lu, w_ptr=%u, r_ptr=%u, buf_size=%u)\n", strnlen(fault_mng->printk_buf, LOG_PRINT_LEN),
                      fault_mng->buf_write, fault_mng->buf_read, fault_mng->buf_size);
    }

    return;
}
KA_EXPORT_SYMBOL_GPL(log_user_write_fault_mng);
#endif