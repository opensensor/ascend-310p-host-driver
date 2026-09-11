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

#ifndef DAVINCI_INTF_UT
#include "ka_base_pub.h"
#include "ka_system_pub.h"
#include "ka_task_pub.h"
#include "ka_memory_pub.h"
#include "ka_list_pub.h"

#include "securec.h"
#include "davinci_interface.h"
#include "davinci_api.h"
#include "davinci_intf_init.h"
#include "davinci_intf_common.h"
#include "dbl_mem_alloc_interface.h"
#include "davinci_intf_process.h"
#ifdef CFG_HOST_ENV
#include "tsdrv_common.h"
#endif

/*
 * *proc_start_time <  *start_time: return <0
 * *proc_start_time == *start_time: return 0
 * *proc_start_time >  *start_time: return >0
 */
static inline int proc_start_time_compare(u64 *proc_start_time, u64 *start_time)
{
	if (*proc_start_time < *start_time)
		return -1;
	if (*proc_start_time > *start_time)
		return 1;
	return *proc_start_time - *start_time;
}

/* create file node info */
struct davinci_intf_file_stru *create_file_proc_entry(
    struct davinci_intf_process_stru *proc,
    ka_file_t *file)
{
    int ret = 0;
    struct davinci_intf_file_stru *file_node = NULL;
    static unsigned int seq = 0;

    file_node = (struct davinci_intf_file_stru *)dbl_kzalloc(sizeof(struct davinci_intf_file_stru),
        KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (file_node == NULL) {
        log_intf_err("kzalloc failed. (size=%lu)\n", sizeof(struct davinci_intf_file_stru));
        return NULL;
    }
    file_node->file_op = file;
    file_node->owner_pid = current->tgid;
    file_node->seq = seq++;
    file_node->open_time = ka_system_jiffies_to_msecs(jiffies);
    ret = strcpy_s(file_node->module_name, DAVINIC_MODULE_NAME_MAX, DAVINIC_UNINIT_FILE);
    if (ret != 0) {
        log_intf_err("strcpy_s failed. (module_name=\"%s\"; ret=%d)\n", file_node->module_name, ret);
        dbl_kfree(file_node);
        file_node = NULL;
        return NULL;
    }
    ka_list_add_tail(&file_node->list, &proc->file_list);
    return file_node;
}

struct davinci_intf_file_stru *get_file_proc_entry(
    struct davinci_intf_process_stru *proc,
    ka_file_t *file)
{
    struct davinci_intf_file_stru *file_node = NULL;
    ka_list_head_t *file_list = NULL;
    struct davinci_intf_file_stru *next = NULL;

    file_list = &proc->file_list;
    ka_list_for_each_entry_safe(file_node, next, file_list, list)
    {
        if (file_node->file_op == file) {
            return file_node;
        }
    }
    return NULL;
}

unsigned int get_file_module_cnt(struct davinci_intf_process_stru *proc, const char *module_name)
{
    struct davinci_intf_file_stru *file_node = NULL;
    ka_list_head_t *file_list = NULL;
    struct davinci_intf_file_stru *next = NULL;
    unsigned int cnt = 0;

    file_list = &proc->file_list;
    ka_list_for_each_entry_safe(file_node, next, file_list, list)
    {
        if (ka_base_strcmp(file_node->module_name, module_name) == 0) {
            cnt++;
        }
    }
    return cnt;
}

int check_module_file_close_in_process(
    struct davinci_intf_process_stru *proc,
    const char *module_name)
{
    struct davinci_intf_file_stru *file_node = NULL;
    ka_list_head_t *file_list = NULL;
    struct davinci_intf_file_stru *next = NULL;

    file_list = &proc->file_list;

    ka_list_for_each_entry_safe(file_node, next, file_list, list)
    {
        if (ka_base_strcmp(file_node->module_name, module_name) == 0) {
            return FALSE;
        }
    }
    return TRUE;
}

void destory_file_proc_list(struct davinci_intf_process_stru *proc)
{
    struct davinci_intf_file_stru *file_node = NULL;
    ka_list_head_t *file_list = NULL;
    struct davinci_intf_file_stru *next = NULL;
    file_list = &proc->file_list;

    ka_list_for_each_entry_safe(file_node, next, file_list, list)
    {
        ka_list_del(&file_node->list);
        dbl_kfree(file_node);
        file_node = NULL;
    }
    return;
}

STATIC int create_proc_free_list(struct davinci_intf_process_stru *proc)
{
    struct davinci_intf_free_list_stru *proc_free_list = NULL;

    if (proc->free_list != NULL) {
        return 0;
    }

    proc_free_list = (struct davinci_intf_free_list_stru *)dbl_kzalloc(
        sizeof(struct davinci_intf_free_list_stru), KA_GFP_ATOMIC | __KA_GFP_ACCOUNT);
    if (proc_free_list == NULL) {
        log_intf_err("kzalloc free list failed. (size=%lu)\n", sizeof(struct davinci_intf_free_list_stru));
        return -ENOMEM;
    }
    KA_INIT_LIST_HEAD(&proc_free_list->list);
    proc_free_list->owner_pid = proc->owner_pid;
    proc_free_list->owner_proc = proc;
    proc_free_list->all_flag = DAVINIC_FREE_LIST_NOT_INIT;
    proc_free_list->current_free_index = 0;
    ka_base_atomic_set(&proc_free_list->current_count, 0);

    proc->free_list = proc_free_list;

    return 0;
}

struct davinci_intf_process_stru *create_process_entry(
    struct davinci_intf_stru *cb,
    pid_t  proc_pid, u64 start_time)
{
    struct davinci_intf_process_stru *proc_node = NULL;
    ka_list_head_t *process_list = NULL;
    process_list = &cb->process_list;

    proc_node = (struct davinci_intf_process_stru *)dbl_kzalloc(
        sizeof(struct davinci_intf_process_stru),
        KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (proc_node == NULL) {
        log_intf_err("kzalloc failed. (size=%lu)\n", sizeof(struct davinci_intf_process_stru));
        return NULL;
    }
    proc_node->owner_pid = proc_pid;
    proc_node->start_time = start_time;
    proc_node->owner_cb = (void *)cb;

    ka_task_mutex_init(&proc_node->res_lock);
    KA_INIT_LIST_HEAD(&proc_node->file_list);
    ka_list_add_tail(&proc_node->list, &cb->process_list);
    ka_base_atomic_set(&proc_node->work_count, 0);
    return proc_node;
}

struct davinci_intf_process_stru *get_process_entry(
    struct davinci_intf_stru *cb,
    pid_t proc_pid, u64 start_time)
{
    struct davinci_intf_process_stru *proc_node = NULL;
    ka_list_head_t *process_list = NULL;
    struct davinci_intf_process_stru *next = NULL;

    process_list = &cb->process_list;

    ka_list_for_each_entry_safe(proc_node, next, process_list, list)
    {
        if ((proc_node->owner_pid == proc_pid) && (proc_start_time_compare(&proc_node->start_time, &start_time) == 0)) {
            return proc_node;
        }
    }
    return NULL;
}

struct davinci_intf_process_stru *get_process_entry_latest(
    struct davinci_intf_stru *cb,
    pid_t proc_pid)
{
    struct davinci_intf_process_stru *proc_node = NULL;
    ka_list_head_t *process_list = NULL;
    struct davinci_intf_process_stru *next = NULL;
    struct davinci_intf_process_stru *latest_proc = NULL;
    u64 *latest_time = NULL;

    process_list = &cb->process_list;

    ka_list_for_each_entry_safe(proc_node, next, process_list, list)
    {
        if (proc_node->owner_pid != proc_pid) {
            continue;
        }
        if (latest_time == NULL) {
            latest_time = &proc_node->start_time;
            latest_proc = proc_node;
            continue;
        }
        if (proc_start_time_compare(&proc_node->start_time, latest_time) > 0) {
            latest_time = &proc_node->start_time;
            latest_proc = proc_node;
        }
    }
    return latest_proc;
}

void free_process_entry(struct davinci_intf_process_stru *proc)
{
    if (proc == NULL) {
        return;
    }
    ka_task_mutex_destroy(&proc->res_lock);
    return;
}

void destroy_process_list(struct davinci_intf_stru *cb)
{
    struct davinci_intf_process_stru *proc_node = NULL;
    ka_list_head_t *process_list = NULL;
    struct davinci_intf_process_stru *next = NULL;

    process_list = &cb->process_list;

    ka_list_for_each_entry_safe(proc_node, next, process_list, list)
    {
        ka_list_del(&proc_node->list);
        destory_file_proc_list(proc_node);
        dbl_kfree(proc_node);
        proc_node = NULL;
    }
    return;
}

int check_module_file_close(
    struct davinci_intf_stru *cb,
    const char *module_name)
{
    struct davinci_intf_process_stru *proc_node = NULL;
    ka_list_head_t *process_list = NULL;
    struct davinci_intf_process_stru *next = NULL;

    process_list = &cb->process_list;

    ka_list_for_each_entry_safe(proc_node, next, process_list, list)
    {
        if (check_module_file_close_in_process(proc_node, module_name) == FALSE) {
            return FALSE;
        }
    }
    return TRUE;
}

STATIC int create_file_free_list_and_node(struct davinci_intf_process_stru *proc,
    struct davinci_intf_private_stru *private_data)
{
    int ret = 0;
    struct davinci_intf_free_file_stru *node = NULL;
    struct davinci_intf_free_list_stru *file_free_list = NULL;

    file_free_list = (struct davinci_intf_free_list_stru *)dbl_kzalloc(
        sizeof(struct davinci_intf_free_list_stru),
        KA_GFP_ATOMIC | __KA_GFP_ACCOUNT);
    if (file_free_list == NULL) {
        log_intf_err("kzalloc file free list failed. (size=%lu)\n", sizeof(struct davinci_intf_free_list_stru));
        return -ENOMEM;
    }
    KA_INIT_LIST_HEAD(&file_free_list->list);
    file_free_list->owner_pid = proc->owner_pid;
    file_free_list->owner_proc = proc;
    file_free_list->all_flag = DAVINIC_FREE_LIST_NOT_INIT;
    file_free_list->current_free_index = 0;
    ka_base_atomic_set(&file_free_list->current_count, 0);

    private_data->free_list = file_free_list;

    node = (struct davinci_intf_free_file_stru *)dbl_kzalloc(
        sizeof(struct davinci_intf_free_file_stru),
        KA_GFP_ATOMIC | __KA_GFP_ACCOUNT);
    if (node == NULL) {
        log_intf_err("kzalloc failed. (size=%lu)\n", sizeof(struct davinci_intf_free_file_stru));
        dbl_kfree(file_free_list);
        file_free_list = NULL;
        private_data->free_list = NULL;
        return -ENOMEM;
    }
    KA_INIT_LIST_HEAD(&node->list);
    node->owner_pid = private_data->owner_pid;
    node->file_private = private_data;
    node->free_type = private_data->free_type;
    node->owner_list = private_data->free_list;
    node->free_index = 0;
    ret = strcpy_s(node->module_name, DAVINIC_MODULE_NAME_MAX, DAVINIC_UNINIT_FILE);
    if (ret != 0) {
        log_intf_err("strcpy_s failed. (module_name=\"%s\"; ret=%d)\n", node->module_name, ret);
        dbl_kfree(file_free_list);
        file_free_list = NULL;
        private_data->free_list = NULL;

        dbl_kfree(node);
        node = NULL;
        return -ENOMEM;
    }
    ka_list_add_tail(&node->list, &file_free_list->list);

    return 0;
}

int add_file_to_list(struct davinci_intf_stru *cb, ka_file_t *file,
    struct davinci_intf_private_stru *private_data)
{
    int ret = 0;
    struct davinci_intf_process_stru *proc = NULL;
    struct davinci_intf_file_stru *file_op = NULL;
    pid_t proc_pid = current->tgid;
    u64 start_time = ka_task_get_current_group_starttime();

    proc = get_process_entry(cb, proc_pid, start_time);
    if (proc == NULL) {
        proc = create_process_entry(cb, proc_pid, start_time);
        if (proc == NULL) {
            return -ENOMEM;
        }
    }

    file_op = get_file_proc_entry(proc, file);
    if (file_op != NULL) {
        log_intf_err("File already init. (pid=%d; file=%llx)\n", proc_pid, (unsigned long long)(uintptr_t)file);
        return -EBADF;
    }

    ret = create_proc_free_list(proc);
    if (ret != 0) {
        log_intf_err("create proc free list failed. (ret=%d)\n", ret);
        return -ENOMEM;
    }

    ret = create_file_free_list_and_node(proc, private_data);
    if (ret != 0){
        log_intf_err("Create file free list failed. (pid=%d; file=%llx)\n",
            proc_pid, (unsigned long long)(uintptr_t)file);
        return -ENOMEM;
    }

    file_op = create_file_proc_entry(proc, file);
    if (file_op == NULL) {
        return -ENOMEM;
    }

    private_data->file_stru_node = file_op;

    return 0;
}

int add_module_to_list(struct davinci_intf_stru *cb,
    struct davinci_intf_private_stru *file_private,
    ka_file_t *file,
    const char *module_name)
{
    struct davinci_intf_process_stru *proc = NULL;
    struct davinci_intf_file_stru *file_op = NULL;
    pid_t  proc_pid = file_private->owner_pid;
    u64 start_time = file_private->start_time;
    int ret;

    proc = get_process_entry(cb, proc_pid, start_time);
    if (proc == NULL) {
        return -ESRCH;
    }

    file_op = get_file_proc_entry(proc, file);
    if (file_op == NULL) {
        return -EBADF;
    }

    ret = strcpy_s(file_op->module_name, DAVINIC_MODULE_NAME_MAX, module_name);
    if (ret != 0) {
        log_intf_err("strcpy_s error. (module_name=\"%s\"; ret=%d)\n",
            module_name,
            ret);
        return ret;
    }
    return 0;
}

#else
void remove_file_from_list(void)
{
}
#endif
