/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
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
 * Create: 2024-05-28
 */

#ifndef LOG_UT
#include "ka_task_pub.h"
#include "ka_system_pub.h"
#include "ka_dfx_pub.h"
#include "ka_pci_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_base_pub.h"

#include "securec.h"
#include "log_drv_agent.h"

#define LOG_LEVEL_OFFSET      2U
#define DATATIME_MAXLEN       50U
#define TWENTY_CENTURY        1900
#define JANUARY               1U
#define KILO                  1000U
#define LOG_RINGBUF_SIZE      0x1400000  // 20M
#define LOG_PRINT_LEN         0x400     // 1024 Bytes
#define MODULE_HOST_LOG       "ascend_logdrv"

#define PCI_VENDOR_ID_HUAWEI 0x19e5
#define DEVDRV_DIVERSITY_PCIE_VENDOR_ID 0xFFFF

#define LOG_S_TO_US 1000000

#define SCHED_CURRENT_TGID (ka_task_get_current_tgid())
#define SCHED_CURRENT_COMM (ka_task_get_current_comm())
#define SCHED_CURRENT_PID (ka_task_get_current_pid())

#ifndef LOG_UNIT_TEST
#define slog_drv_err(fmt, ...)   \
    drv_err(MODULE_HOST_LOG, "<%s:%d,%d> " fmt, SCHED_CURRENT_COMM, SCHED_CURRENT_TGID, SCHED_CURRENT_PID, ##__VA_ARGS__)
#define slog_drv_info(fmt, ...)  \
    drv_info(MODULE_HOST_LOG, "<%s:%d,%d> " fmt, SCHED_CURRENT_COMM, SCHED_CURRENT_TGID, SCHED_CURRENT_PID, ##__VA_ARGS__)
#else
#define slog_drv_err(fmt, ...)   printf(fmt, ##__VA_ARGS__)
#define slog_drv_info(fmt, ...)  printf(fmt, ##__VA_ARGS__)
#endif

#ifdef LOG_UNIT_TEST
#define STATIC
#else
#define STATIC static
#endif

typedef struct {
    char *log_buf;
    u32 point;
    u32 size;
    ka_spinlock_t logbuf_lock;
    char printk_buf[LOG_PRINT_LEN];  /* temporary log_buf */
} log_ring_buf_t;

STATIC const ka_pci_device_id_t g_log_driver_tbl[] = {{ KA_PCI_VDEVICE(HUAWEI, 0xd100), 0 },
                                                        { KA_PCI_VDEVICE(HUAWEI, 0xd105), 0 },
                                                        { KA_PCI_VDEVICE(HUAWEI, 0xa126), 0 },
                                                        { KA_PCI_VDEVICE(HUAWEI, 0xd801), 0 },
                                                        { KA_PCI_VDEVICE(HUAWEI, 0xd500), 0 },
                                                        { KA_PCI_VDEVICE(HUAWEI, 0xd501), 0 },
                                                        { KA_PCI_VDEVICE(HUAWEI, 0xd802), 0 },
                                                        { KA_PCI_VDEVICE(HUAWEI, 0xd803), 0 },
                                                        { KA_PCI_VDEVICE(HUAWEI, 0xd804), 0 },
                                                        { KA_PCI_VDEVICE(HUAWEI, 0xd805), 0 },
                                                        { DEVDRV_DIVERSITY_PCIE_VENDOR_ID, 0xd500,
                                                          KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
                                                        { 0x20C6, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
                                                        { 0x203F, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
                                                        { 0x20C6, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
                                                        { 0x203F, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
                                                        { 0x20E9, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
                                                        { 0x20E9, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
                                                        {}};
KA_MODULE_DEVICE_TABLE(pci, g_log_driver_tbl);

STATIC log_ring_buf_t g_log_ring_buf = { 0 };
static DEFINE_RATELIMIT_STATE(drv_log_err_ratelimit, 1 * KA_HZ, 5);

STATIC int log_get_date(char *date, u32 len)
{
    ka_timespec64_t sys_time = { 0 };
    ka_rtc_time_t tm = { 0 };
    int ret;

    ka_system_ktime_get_real_ts64(&sys_time);
    ka_system_rtc_time_convert(&tm, sys_time);

    ret = snprintf_s(date, len, len - 1, "%04ld-%02d-%02d-%02d:%02d:%02d.%06llu",
                     ka_system_get_rtc_time_year(&tm) + TWENTY_CENTURY, ka_system_get_rtc_time_mon(&tm) + JANUARY,
                     ka_system_get_rtc_time_mday(&tm), ka_system_get_rtc_time_hour(&tm),
                     ka_system_get_rtc_time_min(&tm), ka_system_get_rtc_time_sec(&tm),
                     ka_system_timespec64_get_tv_nsec(&sys_time) / KILO);
    if (ret < 0) {
        slog_drv_err("Log snprintf_s failed. (ret=%d)\n", ret);
        return ret;
    }
    return 0;
}

STATIC int log_write_ringbuffer(void)
{
    u32 buf_left, str_len;
    int ret;

    buf_left = LOG_RINGBUF_SIZE - g_log_ring_buf.point;
    str_len = ka_base_strnlen(g_log_ring_buf.printk_buf, LOG_PRINT_LEN);
    if ((str_len == 0) || (str_len >= LOG_PRINT_LEN)) {
        return -EINVAL;
    }

    if (str_len > buf_left) {
        ret = memcpy_s(g_log_ring_buf.log_buf + g_log_ring_buf.point, buf_left, g_log_ring_buf.printk_buf, buf_left);
        if (ret != 0) {
            return ret;
        }
        ret = memcpy_s(g_log_ring_buf.log_buf, LOG_RINGBUF_SIZE, (char*)g_log_ring_buf.printk_buf + buf_left,
            str_len - buf_left);
        if (ret != 0) {
            return ret;
        }
    } else {
        ret = memcpy_s(g_log_ring_buf.log_buf + g_log_ring_buf.point, buf_left, g_log_ring_buf.printk_buf, str_len);
        if (ret != 0) {
            return ret;
        }
    }
    g_log_ring_buf.point = (g_log_ring_buf.point + str_len) % LOG_RINGBUF_SIZE;
    g_log_ring_buf.size = ((g_log_ring_buf.size + str_len) >= LOG_RINGBUF_SIZE) ?
        LOG_RINGBUF_SIZE : (g_log_ring_buf.size + str_len);

    return 0;
}

static int log_save_to_ringbuf(const char *fmt, va_list args)
{
    char date[DATATIME_MAXLEN] = { 0 };
    static char new_fmt[LOG_PRINT_LEN];
    char *module = NULL;
    unsigned long flags;
    va_list args_backup;
    int ret;
    ka_ktime_t kt = ka_system_ktime_get();
    unsigned long long usec = ka_system_ktime_to_us(kt);

    if (g_log_ring_buf.log_buf == NULL) {
        return -EINVAL;
    }

    ret = log_get_date(date, DATATIME_MAXLEN);
    if (ret != 0) {
        return ret;
    }

    ka_task_spin_lock_irqsave(&g_log_ring_buf.logbuf_lock, flags);
    ret = snprintf_s(new_fmt, LOG_PRINT_LEN, LOG_PRINT_LEN - 1, "[%s] [%llu.%06u] %s", date, (usec / LOG_S_TO_US), (usec % LOG_S_TO_US), fmt + LOG_LEVEL_OFFSET);
    if (ret < 0) {
        ka_task_spin_unlock_irqrestore(&g_log_ring_buf.logbuf_lock, flags);
        slog_drv_err("Log snprintf_s failed. (ret=%d)\n", ret);
        return -EINVAL;
    }

    va_copy(args_backup, args);
    ret = vsnprintf_s(g_log_ring_buf.printk_buf, LOG_PRINT_LEN, LOG_PRINT_LEN - 1, new_fmt, args);
    if (ret <= 0) {
        ka_task_spin_unlock_irqrestore(&g_log_ring_buf.logbuf_lock, flags);
        module = va_arg(args_backup, char*);
        va_end(args_backup);
        if ((module != NULL) && (ka_base_strnlen(module, LOG_PRINT_LEN) < LOG_PRINT_LEN)) {
            slog_drv_err("Log format is incorrect. (module=%s, ret=%d)\n", module, ret);
        }
        return -EINVAL;
    }
    va_end(args_backup);

    ret = log_write_ringbuffer();
    if (ret != 0) {
        ka_task_spin_unlock_irqrestore(&g_log_ring_buf.logbuf_lock, flags);
        slog_drv_err("Log write ringbuffer failed. (ret=%d)\n", ret);
        return ret;
    }

    ka_task_spin_unlock_irqrestore(&g_log_ring_buf.logbuf_lock, flags);
    return 0;
}

void log_save_to_ring_buf(const char *fmt, ...)
{
    va_list args;
    int ret;

    va_start(args, fmt);
    ret = log_save_to_ringbuf(fmt, args);
    va_end(args);
    if (ret != 0) {
        slog_drv_err("Log save to ring_buf failed. (ret=%d)\n", ret);
        return;
    }

    return;
}
KA_EXPORT_SYMBOL_GPL(log_save_to_ring_buf);

void log_to_printk_and_ringbuf(const char *fmt, ...)
{
    va_list args;
    int ret;

    if (__ka_base_ratelimit(&drv_log_err_ratelimit)) {
        va_start(args, fmt);
        (void)ka_dfx_vprintk(fmt, args);
        va_end(args);
    }

    va_start(args, fmt);
    ret = log_save_to_ringbuf(fmt, args);
    va_end(args);
    if (ret != 0) {
        slog_drv_err("Log save to ring_buf failed. (ret=%d)\n", ret);
        return;
    }

    return;
}
KA_EXPORT_SYMBOL_GPL(log_to_printk_and_ringbuf);

int log_get_ringbuffer(char *buff, u32 buf_len, u32 *out_len)
{
    unsigned long flags;
    u32 pos, actual_len;
    char *tmp_buf;
    int ret;

    if ((buff == NULL) || (buf_len != LOG_RINGBUF_SIZE) || (out_len == NULL)) {
        slog_drv_err("Invalid paras. (buff=%d, buf_len=%u, out_len=%d)\n", (int)(buff == NULL), buf_len,
            (int)(out_len == NULL));
        return -EINVAL;
    }

    tmp_buf = log_drv_vzalloc(LOG_RINGBUF_SIZE);
    if (tmp_buf == NULL) {
        slog_drv_err("Vzalloc tmp_buf failed.\n");
        return -ENOMEM;
    }

    ka_task_spin_lock_irqsave(&g_log_ring_buf.logbuf_lock, flags);
    pos = g_log_ring_buf.point;

    if ((g_log_ring_buf.size > LOG_RINGBUF_SIZE) || (pos >= LOG_RINGBUF_SIZE)) {
        ka_task_spin_unlock_irqrestore(&g_log_ring_buf.logbuf_lock, flags);
        log_drv_vfree(tmp_buf);
        tmp_buf = NULL;
        slog_drv_err("Invalid paras. (max_len=%u, size=%u, pos=%u)\n", LOG_RINGBUF_SIZE, g_log_ring_buf.size, pos);
        return -EINVAL;
    }

    if (g_log_ring_buf.size < LOG_RINGBUF_SIZE) {
        ret = memcpy_s((void *)(uintptr_t)tmp_buf, LOG_RINGBUF_SIZE, g_log_ring_buf.log_buf, pos);
        if (ret != 0) {
            ka_task_spin_unlock_irqrestore(&g_log_ring_buf.logbuf_lock, flags);
            goto copy_fail;
        }
    } else {
        ret = memcpy_s((void *)(uintptr_t)tmp_buf, LOG_RINGBUF_SIZE, g_log_ring_buf.log_buf + pos, LOG_RINGBUF_SIZE - pos);
        if (ret != 0) {
            ka_task_spin_unlock_irqrestore(&g_log_ring_buf.logbuf_lock, flags);
            goto copy_fail;
        }
        ret = memcpy_s((void *)(uintptr_t)tmp_buf + (LOG_RINGBUF_SIZE - pos), pos, g_log_ring_buf.log_buf, pos);
        if (ret != 0) {
            ka_task_spin_unlock_irqrestore(&g_log_ring_buf.logbuf_lock, flags);
            goto copy_fail;
        }
    }

    actual_len = g_log_ring_buf.size;
    ka_task_spin_unlock_irqrestore(&g_log_ring_buf.logbuf_lock, flags);

    ret = ka_base_copy_to_user((void *)(uintptr_t)buff, tmp_buf, actual_len);
    if (ret != 0) {
        goto copy_fail;
    }

    *out_len = actual_len;
    log_drv_vfree(tmp_buf);
    tmp_buf = NULL;

    return 0;

copy_fail:
    log_drv_vfree(tmp_buf);
    tmp_buf = NULL;
    slog_drv_err("Log copy failed. (ret=%d)\n", ret);
    return ret;
}
KA_EXPORT_SYMBOL_GPL(log_get_ringbuffer);

STATIC int log_ringbuffer_init(void)
{
    g_log_ring_buf.log_buf = log_drv_vzalloc(LOG_RINGBUF_SIZE);
    if (g_log_ring_buf.log_buf == NULL) {
        slog_drv_err("Vzalloc g_log_ring_buf failed.\n");
        return -ENOMEM;
    }

    g_log_ring_buf.point = 0;
    g_log_ring_buf.size = 0;
    ka_task_spin_lock_init(&g_log_ring_buf.logbuf_lock);

    return 0;
}

STATIC void log_ringbuffer_uninit(void)
{
    if (g_log_ring_buf.log_buf != NULL) {
        log_drv_vfree(g_log_ring_buf.log_buf);
        g_log_ring_buf.log_buf = NULL;
    }
}

STATIC int __init log_drv_module_init(void)
{
    int ret;

    ret = log_ringbuffer_init();
    if (ret != 0) {
        slog_drv_err("Log ringbuffer init failed. (ret=%d)\n", ret);
        return ret;
    }

    slog_drv_info("Host logdrv module init successfully.\n");
    return 0;
}
ka_module_init(log_drv_module_init);

STATIC void __exit log_drv_module_exit(void)
{
    log_ringbuffer_uninit();

    return;
}
ka_module_exit(log_drv_module_exit);

KA_MODULE_DESCRIPTION("log driver");
KA_MODULE_LICENSE("GPL");
KA_MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
#else
int drv_log_ut(void)
{
    return 0;
}
#endif