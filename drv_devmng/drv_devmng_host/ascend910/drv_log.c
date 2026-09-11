/*
* Copyright (c) Huawei Technologies Co., Ltd. 2019-2022. All rights reserved.
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
* Create: 2019-10-15
*/

#ifndef LOG_UT

#include "ka_fs_pub.h"
#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_errno_pub.h"

#include "drv_log.h"
#include "ascend_hal_error.h"
#include "securec.h"

#define ASCEND_DRV_BASE_FILE_NAME "ascend_drv"
#define ASCEND_DRV_LOG_LEVEL_FILE_NAME "log_level"
int console_log_level = 3;   /* default log level */
struct proc_dir_entry *file_base_path = NULL;
struct proc_dir_entry *log_level_file = NULL;
char console_log_level_info[LOG_LEVEL_FILE_INFO_LEN];
struct mutex log_level_mutex;

ssize_t log_level_file_read(struct file *file, char __user *data, size_t len, loff_t *off);
ssize_t log_level_file_write(struct file *file, const char __user *data, size_t len, loff_t *off);

ka_procfs_ops_t log_level_file_ops = {
    ka_fs_init_pf_owner(THIS_MODULE)
    ka_fs_init_pf_read(log_level_file_read)
    ka_fs_init_pf_write(log_level_file_write)
};

char *module_str = "drv_log";
void log_level_file_remove(void)
{
    if (log_level_file != NULL) {
        ka_fs_remove_proc_entry(ASCEND_DRV_LOG_LEVEL_FILE_NAME, file_base_path);
    }

    if (file_base_path != NULL) {
        ka_fs_remove_proc_entry(ASCEND_DRV_BASE_FILE_NAME, NULL);
    }

    ka_task_mutex_destroy(&log_level_mutex);
    drv_event(module_str, "log_level_file has been removed!!!\n");
}

ssize_t log_level_file_read(struct file *file, char __user *data, size_t len, loff_t *off)
{
    char *ptr = NULL;
    int count = 0;
    int ret;

    if (file == NULL || data == NULL || off == NULL) {
        drv_err(module_str, "file_ptr, user_data or off is NULL!!!.\n");
        return -EINVAL;
    }

    if ((*off < 0) || (*off >= LOG_LEVEL_FILE_INFO_LEN)) {
        return 0;
    }

    ptr = ka_base_pde_data(ka_fs_file_inode(file));

    if (len < (size_t)(LOG_LEVEL_FILE_INFO_LEN - (*off))) {
        count = len + *off;
    } else {
        len = (size_t)(LOG_LEVEL_FILE_INFO_LEN - (*off));
        count = LOG_LEVEL_FILE_INFO_LEN;
    }

    ret = ka_base_copy_to_user((void *)((uintptr_t)data), ptr + (*off), len);
    if (ret != 0) {
        drv_err(module_str, "copy_to_user failed, ret = %d.\n", ret);
        return -EFAULT;
    }

    *off = count;

    return len;
}

ssize_t log_level_file_write(struct file *file, const char __user *data, size_t len, loff_t *off)
{
    char tmp[LOG_LEVEL_FILE_INFO_LEN];
    char *ptr = NULL;
    int ret;

    if (file == NULL || data == NULL || off == NULL) {
        drv_err(module_str, "file_ptr, user_data or off is NULL!!!.\n");
        return -EINVAL;
    }

    if ((*off != 0) || (len != LOG_LEVEL_INFO_INPUT_LEN)) {
        return -EINVAL;
    }

    ret = memset_s(tmp, LOG_LEVEL_FILE_INFO_LEN, 0, LOG_LEVEL_FILE_INFO_LEN);
    if (ret != DRV_ERROR_NONE) {
        drv_err(module_str, "call memset_s failed! ret = %d\n", ret);
        return -EFAULT;
    }

    ka_task_mutex_lock(&log_level_mutex);

    ptr = ka_base_pde_data(ka_fs_file_inode(file));

    ret = ka_base_copy_from_user(tmp, data, len);
    if (ret != DRV_ERROR_NONE) {
        ka_task_mutex_unlock(&log_level_mutex);
        drv_err(module_str, "copy_from_user failed, ret = %d.\n", ret);
        return -EFAULT;
    }

    if ((*tmp >= '0') && (*tmp <= '7')) {
        ret = memcpy_s(ptr, LOG_LEVEL_FILE_INFO_LEN, tmp, LOG_LEVEL_FILE_INFO_LEN);
        if (ret != DRV_ERROR_NONE) {
            ka_task_mutex_unlock(&log_level_mutex);
            drv_err(module_str, "memcpy_s failed, ret = %d.\n", ret);
            return -EFAULT;
        }

        console_log_level = *tmp - '0';
    } else {
        ka_task_mutex_unlock(&log_level_mutex);
        return -EINVAL;
    }

    ka_task_mutex_unlock(&log_level_mutex);

    return len;
}

int log_level_get(void)
{
    return console_log_level;
}

int log_level_info_creat(char *proc_info_in, const char *format, ...)
{
    int ret = 0;
    va_list args;

    va_start(args, format);
    ret = vsnprintf_s(proc_info_in, LOG_LEVEL_FILE_INFO_LEN, LOG_LEVEL_FILE_INFO_LEN - 1, format, args);
    va_end(args);

    return ret;
}

int log_level_file_init(void)
{
    int ret;

    file_base_path = ka_fs_proc_mkdir(ASCEND_DRV_BASE_FILE_NAME, NULL);
    if (file_base_path == NULL) {
        drv_err(module_str, "proc create proc/ascend_drv/ failed!\n");
        return -EFAULT;
    }

    ret = memset_s(console_log_level_info, LOG_LEVEL_FILE_INFO_LEN, 0, LOG_LEVEL_FILE_INFO_LEN);
    if (ret != DRV_ERROR_NONE) {
        drv_err(module_str, "call memset_s failed! ret = %d\n", ret);
        ka_fs_remove_proc_entry(ASCEND_DRV_BASE_FILE_NAME, NULL);
        file_base_path = NULL;
        return ret;
    }

    ret = log_level_info_creat(console_log_level_info, "%d\n", console_log_level);
    if (ret < 0) {
        drv_err(module_str, "call log_level_info_create failed! ret = %d\n", ret);
        ka_fs_remove_proc_entry(ASCEND_DRV_BASE_FILE_NAME, NULL);
        file_base_path = NULL;
        return ret;
    }

    log_level_file = ka_fs_proc_create_data(ASCEND_DRV_LOG_LEVEL_FILE_NAME, DEVDRV_HOST_LOG_FILE_CREAT_AUTHORITY,
        file_base_path, &log_level_file_ops, console_log_level_info);
    if (log_level_file == NULL) {
        drv_err(module_str, "proc/ascend_drv/log_level create failed!\n");
        ka_fs_remove_proc_entry(ASCEND_DRV_BASE_FILE_NAME, NULL);
        file_base_path = NULL;
        return -EFAULT;
    }

    ka_task_mutex_init(&log_level_mutex);
    drv_event(module_str, "log_level_file init successfully!!! log_level = %d.\n", console_log_level);
    return 0;
}

#else
int drv_log_proc_fs_ut(void)
{
    return 0;
}
#endif
