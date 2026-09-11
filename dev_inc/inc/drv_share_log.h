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
 * Create: 2024-11-12
 */
#ifndef _DRV_SHARE_LOG_H_
#define _DRV_SHARE_LOG_H_

#include "ka_memory_pub.h"
#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_system_pub.h"

#ifdef CFG_FEATURE_SHARE_LOG
#include "securec.h"

#define SHARE_LOG_PAGE_WRITE      1
#define SHARE_LOG_MAGIC_LENGTH    24
#define SHARE_LOG_RECORD_OFFSET   100
#define SHARE_LOG_MAX_SIZE        (4 * 1024) /* only support 1 page, don't change this value */
#define SHARE_LOG_RECORD_SIZE     (SHARE_LOG_MAX_SIZE - SHARE_LOG_RECORD_OFFSET)
#define SHARE_LOG_PAGE_NUM        (roundup(SHARE_LOG_MAX_SIZE, KA_MM_PAGE_SIZE) / KA_MM_PAGE_SIZE)
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
void __attribute__((weak)) share_log_update_write(struct share_log_info *info, u32 tmp_write, u32 size, const char* fmt, va_list arg);
void __attribute__((weak)) share_log_record_sub(struct share_log_info *info, const char* fmt, va_list arg);
ka_rw_semaphore_t* __attribute__((weak)) share_log_sem(ka_mm_struct_t *mm);
void __attribute__((weak)) share_log_record(ka_page_t *page, unsigned long start, const char* fmt,ka_mm_struct_t *mm, va_list arg);
void __attribute__((weak)) _share_log_no_kthread_or_no_interrupt_record(unsigned long start, const char* fmt, va_list arg);
void __attribute__((weak)) share_log_no_kthread_or_no_interrupt_record(unsigned long start, const char* fmt, ...);
long __attribute__((weak)) _log_get_user_pages_remote(ka_task_struct_t *tsk, ka_mm_struct_t *mm, u64 va, u32 num, ka_page_t **pages);
int __attribute__((weak)) log_get_user_pages_remote(ka_task_struct_t *tsk, ka_mm_struct_t *mm,u64 va, u32 num, ka_page_t **pages);
void __attribute__((weak)) _share_log_record_by_pid(unsigned long start, u32 pid, const char* fmt, va_list arg);
void __attribute__((weak)) share_log_record_by_pid(unsigned long start, u32 pid, const char* fmt, ...);
void __attribute__((weak)) share_log_err_by_pid_ex(unsigned long start, u32 pid, const char* fmt, va_list arg);
void __attribute__((weak)) share_log_run_info_by_pid_ex(unsigned long start, u32 pid, const char* fmt, va_list arg);
void __attribute__((weak)) share_log_err_ex(unsigned long start, const char* fmt, va_list arg);
void __attribute__((weak)) share_log_run_info_ex(unsigned long start, const char* fmt, va_list arg);

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
    const char* fmt, va_list arg)
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
    u32 remain_size = (u32)info->record_size - tmp_write;

    if (ka_base_strcmp(info->magic, SHARE_LOG_MAGIC) == 0 && info->total_size == SHARE_LOG_MAX_SIZE &&
        tmp_write <= SHARE_LOG_RECORD_SIZE && remain_size <= SHARE_LOG_RECORD_SIZE  &&
        (tmp_write + remain_size) <= SHARE_LOG_RECORD_SIZE) {
        share_log_formatting(info, &tmp_write, &remain_size);
        share_log_update_write(info, tmp_write, remain_size, fmt, arg);
    }
}

ka_rw_semaphore_t* __attribute__((weak)) share_log_sem(ka_mm_struct_t *mm)
{
    return ka_mm_get_mmap_sem(mm);
}

void __attribute__((weak)) share_log_record(ka_page_t *page, unsigned long start, const char* fmt,
    ka_mm_struct_t *mm, va_list arg)
{
    ka_vm_area_struct_t *vma = NULL;
    /*
     * The use of mmap_sem is to ensure that the log records are serialized when multi-threaded,
     * and no logging is performed when find_vma is NULL
     */
    if (ka_task_down_write_trylock(share_log_sem(mm)) != 0) {
        vma = ka_mm_find_vma(mm, start);
        if ((vma != NULL) && (ka_mm_get_vm_start(vma) <= start)) {
            struct share_log_info *sha_info = ka_mm_page_address(page);
            share_log_record_sub(sha_info, fmt, arg);
        }
        ka_task_up_write(share_log_sem(mm));
    }
}

/*
 * share_log will not record logs if the interface is called in mmap, unmap or release process, because :
 * 1.mmap, unmap : os first performs down_write and then calls the mmap interface registered by each module,
 *   which will cause down_write or down_read to fail if the share_log_err call occurs;
 * 2.release : current->mm is null.
 */
void __attribute__((weak)) _share_log_no_kthread_or_no_interrupt_record(unsigned long start, const char* fmt, va_list arg)
{
    if ((ka_task_get_current_flags() & KA_TASK_PF_KTHREAD) == 0 && ka_system_in_interrupt() == 0 && ka_task_get_current_mm() != NULL &&
        ka_task_down_write_trylock(share_log_sem(ka_task_get_current_mm())) != 0) { /* Prevent read_lock nested */
        ka_page_t *sha_page = NULL;
        ka_task_up_write(share_log_sem(ka_task_get_current_mm()));

        if (ka_mm_get_user_pages_fast(start, SHARE_LOG_PAGE_NUM, KA_FOLL_WRITE, &sha_page) == (int)SHARE_LOG_PAGE_NUM) {
            share_log_record(sha_page, start, fmt, ka_task_get_current_mm(), arg);
            ka_mm_put_page(sha_page);
        }
    }
}

void __attribute__((weak)) share_log_no_kthread_or_no_interrupt_record(unsigned long start, const char* fmt, ...)
{
    va_list arg;
    va_start(arg, fmt);
    _share_log_no_kthread_or_no_interrupt_record(start, fmt, arg);
    va_end(arg);
}

#define TSDRV_SHARE_LOG_START   (0xE0000000000ULL)
#define DEVMM_SHARE_LOG_START   (0xE0000080000ULL)
#define DEVMNG_SHARE_LOG_START  (0xE0000100000ULL)
#define HDC_SHARE_LOG_START     (0xE0000180000ULL)
#define ESCHED_SHARE_LOG_START  (0xE0000200000ULL)
#define XSMEM_SHARE_LOG_START   (0xE0000280000ULL)
#define QUEUE_SHARE_LOG_START   (0xE0000300000ULL)
#define COMMON_SHARE_LOG_START  (0xE0000380000ULL)

#define TSDRV_SHARE_LOG_RUNINFO_START  (0xE0000040000ULL)
#define DEVMM_SHARE_LOG_RUNINFO_START  (0xE00000C0000ULL)
#define DEVMNG_SHARE_LOG_RUNINFO_START (0xE0000140000ULL)
#define HDC_SHARE_LOG_RUNINFO_START    (0xE00001C0000ULL)
#define ESCHED_SHARE_LOG_RUNINFO_START (0xE0000240000ULL)
#define XSMEM_SHARE_LOG_RUNINFO_START  (0xE00002C0000ULL)
#define QUEUE_SHARE_LOG_RUNINFO_START  (0xE0000340000ULL)
#define COMMON_SHARE_LOG_RUNINFO_START (0xE00003C0000ULL)

long __attribute__((weak)) _log_get_user_pages_remote(ka_task_struct_t *tsk, ka_mm_struct_t *mm,
    u64 va, u32 num, ka_page_t **pages)
{
    long got_num = -1;
    int locked;

    locked = 1;
    if (ka_task_down_write_trylock(share_log_sem(mm)) == 0) { /* Prevent read_lock nested */
        return got_num;
    }
    ka_task_up_write(share_log_sem(mm));

    if (ka_task_down_read_trylock(share_log_sem(mm)) == 0) {
        return got_num;
    }
    got_num = ka_mm_get_user_pages_remote(tsk, mm, va, KA_FOLL_WRITE, num, pages);

    ka_task_up_read(share_log_sem(mm));

    return got_num;
}

/*
 * num is 1, when get page failed ,directly return
 */
int __attribute__((weak)) log_get_user_pages_remote(ka_task_struct_t *tsk, ka_mm_struct_t *mm,
    u64 va, u32 num, ka_page_t **pages)
{
    return (_log_get_user_pages_remote(tsk, mm, va, num, pages) == num) ? 0 : -ENOMEM;
}

/*
 * in_interrupt is not allow
 */
void __attribute__((weak)) _share_log_record_by_pid(unsigned long start, u32 pid, const char* fmt, va_list arg)
{
    ka_task_struct_t *task = NULL;
    ka_page_t *sha_page = NULL;
    ka_mm_struct_t *mm = NULL;
    ka_struct_pid_t *pro_id = NULL;
    int ret;

    if (ka_system_in_interrupt() != 0) {
        return;
    }

    pro_id = ka_task_find_get_pid((int)pid);
    if (pro_id == NULL) {
        return;
    }

    task = ka_task_get_pid_task(pro_id, KA_PIDTYPE_PID);
    if (task == NULL) {
        ka_task_put_pid(pro_id);
        return;
    }
    mm = ka_task_get_task_mm(task);
    if (mm == NULL) {
        ka_task_put_task_struct(task);
        ka_task_put_pid(pro_id);
        return;
    }
    ret = log_get_user_pages_remote(task, mm, start, (u32)SHARE_LOG_PAGE_NUM, &sha_page);
    if (ret != 0) {
        ka_mm_mmput(mm);
        ka_task_put_task_struct(task);
        ka_task_put_pid(pro_id);
        return;
    }
    share_log_record(sha_page, start, fmt, mm, arg);
    ka_mm_put_page(sha_page);
    ka_mm_mmput(mm);
    ka_task_put_task_struct(task);
    ka_task_put_pid(pro_id);
}

void __attribute__((weak)) share_log_record_by_pid(unsigned long start, u32 pid, const char* fmt, ...)
{
    va_list arg;
    va_start(arg, fmt);
    _share_log_record_by_pid(start, pid, fmt, arg);
    va_end(arg);
}

void __attribute__((weak)) share_log_err_by_pid_ex(unsigned long start, u32 pid, const char* fmt, va_list arg)
{
    _share_log_record_by_pid(start, pid, fmt, arg);
}

void __attribute__((weak)) share_log_run_info_by_pid_ex(unsigned long start, u32 pid, const char* fmt, va_list arg)
{
    _share_log_record_by_pid(start, pid, fmt, arg);
}

void __attribute__((weak)) share_log_err_ex(unsigned long start, const char* fmt, va_list arg)
{
    _share_log_no_kthread_or_no_interrupt_record(start, fmt, arg);
}

void __attribute__((weak)) share_log_run_info_ex(unsigned long start, const char* fmt, va_list arg)
{
    _share_log_no_kthread_or_no_interrupt_record(start, fmt, arg);
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
#endif /* _DRV_SHARE_LOG_H_ */
