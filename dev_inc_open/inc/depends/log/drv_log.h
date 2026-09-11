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
 * Create: 2024-03-21
 */

#ifndef DRV_LOG_H
#define DRV_LOG_H

#ifndef DVPP_UTST
#include <linux/types.h>
#include <linux/mm_types.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/preempt.h>
#endif

#define LOG_LEVEL_INFO_INPUT_LEN                (2U)
#define LOG_LEVEL_FILE_INFO_LEN                 (3U)
#define DEVDRV_HOST_LOG_FILE_CREAT_AUTHORITY    (436U) /* 0664 */
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

#define drv_pr_debug(module, fmt, ...) \
    pr_debug("[ascend] [%s] [%s %d] " fmt, module, __func__, __LINE__, ##__VA_ARGS__)

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
    log_to_printk_and_ringbuf(KERN_NOTICE "[ascend] [%s] [ERROR] [%s %d] " fmt, \
        module, __func__, __LINE__, ##__VA_ARGS__)
#define drv_warn(module, fmt, ...) drv_log_print(KERN_WARNING, "WARN", module, fmt, ##__VA_ARGS__)
#define drv_info(module, fmt, ...) drv_log_print(KERN_INFO, "INFO", module, fmt, ##__VA_ARGS__)
#define drv_event(module, fmt, ...) drv_log_print(KERN_NOTICE, "NOTICE", module, fmt, ##__VA_ARGS__)
#define drv_debug(module, fmt, ...) \
    (void)printk(KERN_DEBUG "[ascend] [%s] [%s %d] " fmt, module, __func__, __LINE__, ##__VA_ARGS__)
#else

#define drv_printk(level, module, fmt, ...) \
    (void)printk(level "[ascend] [%s] [%s %d] " fmt, module, __func__, __LINE__, ##__VA_ARGS__)

#if (defined(LOG_UT) || defined(CFG_FEATURE_DRV_LOG_ERR))
#define drv_err(module, fmt...) drv_printk(KERN_ERR, module, fmt)
#else
#define logflow_printk(level, module, fmt, ...) \
    (void)printk(level "[ascend] [ERROR] [%s] [%s %d] " fmt, module, __func__, __LINE__, ##__VA_ARGS__)
/* drv_err is KERN_NOTICE level to avoid too much serial print cause system watchdog reset.
 * if you want to change this, you must call SE to check this */
#ifdef DAVINCI_DEVICE
#define drv_err(module, fmt...) logflow_printk(KERN_ERR, module, fmt)
#else
#define drv_err(module, fmt...) logflow_printk(KERN_NOTICE, module, fmt)
#endif

#endif

#define drv_warn(module, fmt, ...) drv_printk(KERN_WARNING, module, fmt, ##__VA_ARGS__)
#define drv_info(module, fmt, ...) drv_printk(KERN_INFO, module, fmt, ##__VA_ARGS__)
#define drv_debug(module, fmt, ...) drv_printk(KERN_DEBUG, module, fmt, ##__VA_ARGS__)
#define drv_event(module, fmt, ...) drv_printk(KERN_NOTICE, module, fmt, ##__VA_ARGS__)
#endif
#endif

#define drv_err_spinlock(module, fmt, ...) drv_err(module, fmt, ##__VA_ARGS__)
#define drv_warn_spinlock(module, fmt, ...) drv_warn(module, fmt, ##__VA_ARGS__)
#define drv_info_spinlock(module, fmt, ...) drv_info(module, fmt, ##__VA_ARGS__)
#define drv_debug_spinlock(module, fmt, ...) drv_debug(module, fmt, ##__VA_ARGS__)
#define drv_event_spinlock(module, fmt, ...) drv_event(module, fmt, ##__VA_ARGS__)

#define drv_printk_ratelimited(level, module, fmt, ...) \
    printk_ratelimited(level "[ascend] [%s] [%s %d] " fmt, module, __func__, __LINE__, ##__VA_ARGS__)

#define drv_info_ratelimited(module, fmt, ...) drv_printk_ratelimited(KERN_INFO, module, fmt, ##__VA_ARGS__)

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
            DRV_LOG_SAVE(KERN_NOTICE, module, fmt, ##__VA_ARGS__); \
        } else { \
            DRV_LOG_SAVE("", module, fmt, ##__VA_ARGS__); \
        } \
    } \
} while (0)

#define drv_log_output() do { \
    if (buf_info.log_size > 0) { \
        (void)printk(buf_info.log_buf); \
        buf_info.log_size = 0; \
    } \
} while (0)

#ifdef CFG_FEATURE_SHARE_LOG
#include <linux/version.h>
#include "securec.h"
#if ((!defined(LOG_UT))  && (!defined(EMU_ST)) && (!defined(EVENT_SCHED_UT)) && (!defined(TSDRV_UT)) && \
    (!defined(DRV_UT)) && (!defined(DEVDRV_UT)))
#include <linux/sched/mm.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/task.h>
#endif

#endif

#define SHARE_LOG_PAGE_WRITE      1
#define SHARE_LOG_MAGIC_LENGTH    24
#define SHARE_LOG_RECORD_OFFSET   100
#define SHARE_LOG_MAX_SIZE        (4 * 1024) /* only support 1 page, don't change this value */
#define SHARE_LOG_RECORD_SIZE     (SHARE_LOG_MAX_SIZE - SHARE_LOG_RECORD_OFFSET)
#define SHARE_LOG_PAGE_NUM        (roundup(SHARE_LOG_MAX_SIZE, PAGE_SIZE) / PAGE_SIZE)
#define SHARE_LOG_MAGIC           "drvshartlogab90cd78ef56"

struct share_log_info {
    char magic[SHARE_LOG_MAGIC_LENGTH];
    int total_size;
    void *record_base;
    int record_size;
    int read;
    int write;
};

void __attribute__((weak)) share_log_formatting(struct share_log_info *info, u32 *tmp_write, u32 *remain_size);
void __attribute__((weak)) share_log_update_write(struct share_log_info *info, u32 tmp_write, u32 size,
    const char *fmt, va_list arg);
void __attribute__((weak)) share_log_record_sub(struct share_log_info *info, const char* fmt, va_list arg);
struct rw_semaphore* __attribute__((weak)) share_log_sem(struct mm_struct *mm);
void __attribute__((weak)) share_log_record(struct page *page, unsigned long start, const char* fmt,
    struct mm_struct *mm, va_list arg);
void __attribute__((weak)) share_log_no_kthread_or_no_interrupt_record(unsigned long start, const char* fmt, ...);
long __attribute__((weak)) _log_get_user_pages_remote(struct task_struct *tsk, struct mm_struct *mm,
    u64 va, u32 num, struct page **pages);
int __attribute__((weak)) log_get_user_pages_remote(struct task_struct *tsk, struct mm_struct *mm,
    u64 va, u32 num, struct page **pages);
void __attribute__((weak)) share_log_record_by_pid(unsigned long start, u32 pid, const char* fmt, ...);

void __attribute__((weak)) share_log_formatting(struct share_log_info *info, u32 *tmp_write, u32 *remain_size)
{
    if (info->read >= (int)*tmp_write) {
        info->read = 0;
        info->write = 0;
        *tmp_write = 0;
        *remain_size = SHARE_LOG_RECORD_SIZE;
    }
}

void __attribute__((weak)) share_log_update_write(struct share_log_info *info, u32 tmp_write, u32 size,
    const char *fmt, va_list arg)
{
    int write_offset = vsnprintf_s((u8 *)info + SHARE_LOG_RECORD_OFFSET + tmp_write, size, size - 1, fmt, arg);
    if (write_offset > 0) {
        info->write += write_offset;
    } else {
        info->write = 0;
        info->read = 0;
    }
}

void __attribute__((weak)) share_log_record_sub(struct share_log_info *info, const char* fmt, va_list arg)
{
    u32 tmp_write = (u32)info->write;
    u32 remain_size = info->record_size - tmp_write;

    if (strcmp(info->magic, SHARE_LOG_MAGIC) == 0 && info->total_size == SHARE_LOG_MAX_SIZE &&
        tmp_write <= SHARE_LOG_RECORD_SIZE && remain_size <= SHARE_LOG_RECORD_SIZE &&
        (tmp_write + remain_size) <= SHARE_LOG_RECORD_SIZE) {
        share_log_formatting(info, &tmp_write, &remain_size);
        share_log_update_write(info, tmp_write, remain_size, fmt, arg);
    }
}

struct rw_semaphore* __attribute__((weak)) share_log_sem(struct mm_struct *mm)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
    return &mm->mmap_lock;
#else
    return &mm->mmap_sem;
#endif
}

void __attribute__((weak)) share_log_record(struct page *page, unsigned long start, const char* fmt,
    struct mm_struct *mm, va_list arg)
{
    /*
     * The use of mmap_sem is to ensure that the log records are serialized when multi-threaded,
     * and no logging is performed when find_vma is NULL
    */
    if (down_write_trylock(share_log_sem(mm)) != 0) {
        if (find_vma(mm, start) != NULL) {
            struct share_log_info *sha_info = page_address(page);
            share_log_record_sub(sha_info, fmt, arg);
        }
        up_write(share_log_sem(mm));
    }
}

/*
 * share_log will not record logs if the interface is called in mmap, unmap or release process, because :
 * 1.mmap, unmap : os first performs down_write and then calls the mmap interface registered by each module,
 * which will cause down_write or down_read to fail if the share_log_err call occurs;
 * 2.release : current->mm is null.
 */
void __attribute__((weak)) share_log_no_kthread_or_no_interrupt_record(unsigned long start, const char* fmt, ...)
{
    if ((current->flags & PF_KTHREAD) == 0 && in_interrupt() == 0 && current->mm != NULL &&
        down_read_trylock(share_log_sem(current->mm)) != 0) {
        struct page *sha_page = NULL;
        up_read(share_log_sem(current->mm));
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 2, 0)
        if (get_user_pages_fast(start, SHARE_LOG_PAGE_NUM, SHARE_LOG_PAGE_WRITE, &sha_page) ==
            (int)SHARE_LOG_PAGE_NUM) {
#else
        if (get_user_pages_fast(start, SHARE_LOG_PAGE_NUM, FOLL_WRITE, &sha_page) == (int)SHARE_LOG_PAGE_NUM) {
#endif
            va_list arg;
            va_start(arg, fmt);
            share_log_record(sha_page, start, fmt, current->mm, arg);
            va_end(arg);
            put_page(sha_page);
        }
    }
}

#define TSDRV_SHARE_LOG_START (0xE0000000000ULL)
#define DEVMM_SHARE_LOG_START (0xE0000080000ULL)
#define DEVMNG_SHARE_LOG_START (0xE0000100000ULL)
#define DP_PROC_MNG_SHARE_LOG_START (0xE0000180000ULL)
#define ESCHED_SHARE_LOG_START (0xE0000200000ULL)
#define XSMEM_SHARE_LOG_START (0xE0000280000ULL)
#define QUEUE_SHARE_LOG_START (0xE0000300000ULL)
#define COMMON_SHARE_LOG_START (0xE0000380000ULL)

#define TSDRV_SHARE_LOG_RUNINFO_START (0xE0000040000ULL)
#define DEVMM_SHARE_LOG_RUNINFO_START (0xE00000C0000ULL)
#define DEVMNG_SHARE_LOG_RUNINFO_START (0xE0000140000ULL)
#define DP_PROC_MNG_SHARE_LOG_RUNINFO_START (0xE00001C0000ULL)
#define ESCHED_SHARE_LOG_RUNINFO_START (0xE0000240000ULL)
#define XSMEM_SHARE_LOG_RUNINFO_START (0xE00002C0000ULL)
#define QUEUE_SHARE_LOG_RUNINFO_START (0xE0000340000ULL)
#define COMMON_SHARE_LOG_RUNINFO_START (0xE00003C0000ULL)

long __attribute__((weak)) _log_get_user_pages_remote(struct task_struct *tsk, struct mm_struct *mm,
    u64 va, u32 num, struct page **pages)
{
    long got_num = -1;
    int locked;

    locked = 1;
    if (down_read_trylock(share_log_sem(mm)) == 0) {
        return got_num;
    }
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 5, 0)
    got_num = pin_user_pages_remote(mm, va, num, 1, pages, NULL);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
    got_num = pin_user_pages_remote(mm, va, num, FOLL_WRITE, pages, NULL, NULL);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
    got_num = get_user_pages_remote(mm, va, num, FOLL_WRITE, pages, NULL, NULL);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
    got_num = get_user_pages_remote(tsk, mm, va, num, FOLL_WRITE, pages, NULL, NULL);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)
    got_num = get_user_pages_remote(tsk, mm, va, num, FOLL_WRITE, pages, NULL);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 6, 0)
    got_num = get_user_pages_remote(tsk, mm, va, num, FOLL_WRITE, 0, pages, NULL);
#else
    got_num = get_user_pages_locked(tsk, mm, va, num, FOLL_WRITE, 0, pages, &locked);
#endif
    up_read(share_log_sem(mm));

    return got_num;
}

/*
 * num is 1, when get page failed, directly return
 */
int __attribute__((weak)) log_get_user_pages_remote(struct task_struct *tsk, struct mm_struct *mm,
    u64 va, u32 num, struct page **pages)
{
    return (_log_get_user_pages_remote(tsk, mm, va, num, pages) == num) ? 0 : -ENOMEM;
}

/*
 * in_interrupt is not allow
 */
void __attribute__((weak)) share_log_record_by_pid(unsigned long start, u32 pid, const char* fmt, ...)
{
    struct task_struct *task = NULL;
    struct page *sha_page = NULL;
    struct mm_struct *mm = NULL;
    struct pid *pro_id = NULL;
    va_list arg;
    int ret;

    if (in_interrupt() != 0) {
        return;
    }

    pro_id = find_get_pid(pid);
    if (pro_id == NULL) {
        return;
    }

    task = get_pid_task(pro_id, PIDTYPE_PID);
    if (task == NULL) {
        put_pid(pro_id);
        return;
    }
    mm = get_task_mm(task);
    if (mm == NULL) {
        put_task_struct(task);
        put_pid(pro_id);
        return;
    }
    ret = log_get_user_pages_remote(task, mm, start, SHARE_LOG_PAGE_NUM, &sha_page);
    if (ret != 0) {
        mmput(mm);
        put_task_struct(task);
        put_pid(pro_id);
        return;
    }
    va_start(arg, fmt);
    share_log_record(sha_page, start, fmt, mm, arg);
    va_end(arg);
    put_page(sha_page);
    mmput(mm);
    put_task_struct(task);
    put_pid(pro_id);
}

#define share_log_err_by_pid(start, pid, fmt, ...) \
    share_log_record_by_pid(start, pid, fmt, ##__VA_ARGS__)

#define share_log_err(start, fmt, ...) \
    share_log_no_kthread_or_no_interrupt_record(start, fmt, ##__VA_ARGS__)

#define share_log_run_info_by_pid(start, pid, fmt, ...) \
    share_log_record_by_pid(start, pid, fmt, ##__VA_ARGS__)

#define share_log_run_info(start, fmt, ...) \
    share_log_no_kthread_or_no_interrupt_record(start, fmt, ##__VA_ARGS__)

#else
#define TSDRV_SHARE_LOG_START
#define ESCHED_SHARE_LOG_START
#define share_log_err(start, fmt, ...)
#define share_log_err_by_pid(start, pid, fmt, ...)
#endif

#endif /* _DRV_LOG_H_ */
