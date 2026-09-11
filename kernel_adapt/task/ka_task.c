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
 * Create: 2024-11-11
 */

#include "securec.h"
#include "ka_system_pub.h"
#include "ka_memory_pub.h"
#include "ka_task_pub.h"

void ka_task_do_exit(long code)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
    kthread_complete_and_exit(NULL, code);
#else
    do_exit(code);
#endif
}
EXPORT_SYMBOL_GPL(ka_task_do_exit);

u64 ka_task_get_starttime(ka_task_struct_t *task)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0))
    return task->start_time;
#else
    return task->start_time.tv_sec * NSEC_PER_SEC + task->start_time.tv_nsec;
#endif
}
EXPORT_SYMBOL_GPL(ka_task_get_starttime);

ka_fs_struct_t *ka_task_get_task_fs(ka_task_struct_t *task)
{
    return task->fs;
}
EXPORT_SYMBOL_GPL(ka_task_get_task_fs);

ka_nsproxy_t *ka_task_get_nsproxy(ka_task_struct_t *task)
{
    return task->nsproxy;
}
EXPORT_SYMBOL_GPL(ka_task_get_nsproxy);

ka_mnt_namespace_t *ka_task_get_mnt_ns(ka_task_struct_t *task)
{
    return task->nsproxy->mnt_ns;
}
EXPORT_SYMBOL_GPL(ka_task_get_mnt_ns);

u64 ka_task_get_start_boottime(ka_task_struct_t *task)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
    return task->start_boottime;
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0))
    return task->real_start_time;
#else
    return task->real_start_time.tv_sec * NSEC_PER_SEC + task->real_start_time.tv_nsec;
#endif
}
EXPORT_SYMBOL_GPL(ka_task_get_start_boottime);

int ka_task_get_tgid_for_task(ka_task_struct_t *task)
{
    return task->tgid;
}
EXPORT_SYMBOL_GPL(ka_task_get_tgid_for_task);

int ka_task_get_pid_for_task(ka_task_struct_t *task)
{
    return task->pid;
}
EXPORT_SYMBOL_GPL(ka_task_get_pid_for_task);

ka_mm_struct_t *ka_task_get_mm_for_task(ka_task_struct_t *task)
{
    return task->mm;
}
EXPORT_SYMBOL_GPL(ka_task_get_mm_for_task);

ka_pid_namespace_t *ka_task_get_pid_ns_for_task(ka_task_struct_t *task)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
    return task->nsproxy->pid_ns_for_children;
#else
    return task->nsproxy->pid_ns;
#endif
}
EXPORT_SYMBOL_GPL(ka_task_get_pid_ns_for_task);

ka_task_struct_t *ka_task_get_current(void)
{
    return current;
}
EXPORT_SYMBOL_GPL(ka_task_get_current);

char *ka_task_get_current_comm(void)
{
    return current->comm;
}
EXPORT_SYMBOL_GPL(ka_task_get_current_comm);

ka_task_struct_t *ka_task_get_init_task(void)
{
    return &init_task;
}
EXPORT_SYMBOL_GPL(ka_task_get_init_task);

int ka_task_get_current_tgid(void)
{
    return current->tgid;
}
EXPORT_SYMBOL_GPL(ka_task_get_current_tgid);

int ka_task_get_current_pid(void)
{
    return current->pid;
}
EXPORT_SYMBOL_GPL(ka_task_get_current_pid);

ka_mm_struct_t *ka_task_get_current_mm(void)
{
    return current->mm;
}
EXPORT_SYMBOL_GPL(ka_task_get_current_mm);

unsigned long ka_task_get_current_get_unmapped_area(ka_file_t *filep,
        unsigned long addr, unsigned long len, unsigned long pgoff, unsigned long flags)
{
    return current->mm->get_unmapped_area(filep, addr, len, pgoff, flags);
}
EXPORT_SYMBOL_GPL(ka_task_get_current_get_unmapped_area);

ka_mm_struct_t *ka_task_get_current_active_mm(void)
{
    return current->active_mm;
}
EXPORT_SYMBOL_GPL(ka_task_get_current_active_mm);

ka_mem_cgroup_t *ka_task_get_current_active_memcg(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)
#ifdef CONFIG_MEMCG
    return current->active_memcg;
#else
    return NULL;
#endif
#else
    return NULL;
#endif
}
EXPORT_SYMBOL_GPL(ka_task_get_current_active_memcg);

void ka_task_set_current_active_memcg(ka_mem_cgroup_t *memcg)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)
#ifdef CONFIG_MEMCG
    current->active_memcg = memcg;
#endif
#endif
}
EXPORT_SYMBOL_GPL(ka_task_set_current_active_memcg);

u64 ka_task_get_current_starttime(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
    return current->start_time;
#else
    return current->start_time.tv_sec * NSEC_PER_SEC + current->start_time.tv_nsec;
#endif
}
EXPORT_SYMBOL_GPL(ka_task_get_current_starttime);

u64 ka_task_get_current_group_starttime(void)
{
    u64 start_time;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
    start_time = current->group_leader->start_time;
#else
    start_time = current->group_leader->start_time.tv_sec * NSEC_PER_SEC + current->group_leader->start_time.tv_nsec;
#endif
    return start_time;
}
EXPORT_SYMBOL_GPL(ka_task_get_current_group_starttime);

const ka_cred_t *ka_task_get_current_cred(void)
{
    return current->cred;
}
EXPORT_SYMBOL_GPL(ka_task_get_current_cred);

int ka_task_get_current_cred_euid(void)
{
    return current->cred->euid.val;
}
EXPORT_SYMBOL_GPL(ka_task_get_current_cred_euid);

int ka_task_get_current_cred_uid(void)
{
    return current->cred->uid.val;
}
EXPORT_SYMBOL_GPL(ka_task_get_current_cred_uid);

u64 ka_task_get_current_parent_tgid(void)
{
    return current->parent->tgid;
}
EXPORT_SYMBOL_GPL(ka_task_get_current_parent_tgid);

u64 ka_task_get_current_start_boottime(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
    return current->start_boottime;
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
    return current->real_start_time;
#else
    return current->real_start_time.tv_sec * NSEC_PER_SEC + current->real_start_time.tv_nsec;
#endif
}
EXPORT_SYMBOL_GPL(ka_task_get_current_start_boottime);

ka_nsproxy_t *ka_task_get_current_nsproxy(void)
{
    return current->nsproxy;
}
EXPORT_SYMBOL_GPL(ka_task_get_current_nsproxy);

ka_mnt_namespace_t *ka_task_get_current_mnt_ns(void)
{
    return current->nsproxy->mnt_ns;
}
EXPORT_SYMBOL_GPL(ka_task_get_current_mnt_ns);

ka_cgroup_namespace_t *ka_task_get_current_cgroup_ns(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 6, 0)
    return current->nsproxy->cgroup_ns;
#else
    return NULL;
#endif
}
EXPORT_SYMBOL_GPL(ka_task_get_current_cgroup_ns);

ka_pid_namespace_t *ka_task_get_current_pid_ns(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
    return current->nsproxy->pid_ns_for_children;
#else
    return current->nsproxy->pid_ns;
#endif
}
EXPORT_SYMBOL_GPL(ka_task_get_current_pid_ns);

unsigned int ka_task_get_current_flags(void)
{
    return current->flags;
}
EXPORT_SYMBOL_GPL(ka_task_get_current_flags);

unsigned int ka_task_get_cred_uid_val(const ka_cred_t *cred)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 5, 0)
    return (unsigned int)(((struct cred *)cred)->uid.val);
#else
    return (unsigned int)(((struct cred *)cred)->uid);
#endif
}
EXPORT_SYMBOL_GPL(ka_task_get_cred_uid_val);

ka_pid_namespace_t *ka_task_get_init_pid_ns_addr(void)
{
    return &init_pid_ns;
}
EXPORT_SYMBOL_GPL(ka_task_get_init_pid_ns_addr);