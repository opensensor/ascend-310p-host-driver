/*
* Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
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
* Create: 2022-08-08
*/

#include "ka_kernel_def_pub.h"
#include "ka_fs_pub.h"
#include "ka_system_pub.h"
#include "ka_task_pub.h"

#include "dbl/runenv_config.h"

#include "runenv_config_module.h"
#include "docker_query.h"

extern struct task_struct init_task;

static bool is_admin_task(struct task_struct *tsk)
{
    const struct cred *cred = NULL;
    ka_kernel_cap_t effective;
    unsigned int user_id;

    rcu_read_lock();
    cred = __task_cred(tsk); //lint !e1058 !e64 !e666
    effective = cred->cap_effective;
    user_id = cred->uid.val;
    rcu_read_unlock();

#ifndef EMU_ST
    if (cred->user_ns != &init_user_ns) {
        return false;
    }
#endif

    if (ka_system_kernel_cap_compare(effective, ka_system_get_privileged_kernel_cap()) == false) {
        return false;
    }
    return (user_id == 0);
}

STATIC bool cgroup_mount_is_ro(void)
{
    ka_path_t kernel_path;
    int ret;
    bool is_ro = false;

    ret = ka_fs_kern_path("/sys/fs/cgroup/memory", 0, &kernel_path);
    if (ret != 0) {
        return false;
    }
    if (kernel_path.mnt == NULL) {
        recfg_err("Cgroup memory path mount is NULL.\n");
        ka_fs_path_put(&kernel_path);
        return false;
    }
    is_ro = __ka_fs_mnt_is_readonly(kernel_path.mnt);
    ka_fs_path_put(&kernel_path);
    return is_ro;
}

bool dbl_current_is_admin(void)
{
    if (!is_admin_task(ka_task_get_current())) {
        return false;
    }
    /* for compatiblity, only check ro as not admin, in other scenarios, admin is returned */
    if (cgroup_mount_is_ro()) {
        return false;
    }
    return true;
}
KA_EXPORT_SYMBOL(dbl_current_is_admin);

static int task_struct_check(ka_task_struct_t *tsk)
{
    if (tsk == NULL) {
        recfg_err("The tsk is NULL\n");
        return -EINVAL;
    }

    if (tsk->nsproxy == NULL) {
        recfg_err("The tsk->nsproxy is NULL\n");
        return -ENODEV;
    }
    if (tsk->nsproxy->mnt_ns == NULL) {
        recfg_err("The tsk->nsproxy->mnt_ns is NULL\n");
        return -EFAULT;
    }
    return 0;
}

static ka_mnt_namespace_t *get_current_mnt_ns(void)
{
    int ret;

    ret = task_struct_check(ka_task_get_current());
    if (ret) {
        recfg_err("Current is invalid. (ret=%d)\n", ret);
        return NULL;
    }

    return ka_task_get_current()->nsproxy->mnt_ns;
}

static ka_mnt_namespace_t *get_host_mnt_ns(void)
{
    return init_task.nsproxy->mnt_ns;
}

static bool check_in_host_mach(void)
{
    ka_mnt_namespace_t *current_ns = NULL;
    ka_mnt_namespace_t *host_ns = NULL;

    current_ns = get_current_mnt_ns();
    if (current_ns == NULL) {
        return false;
    }

    host_ns = get_host_mnt_ns();
    if (host_ns == NULL) {
        return false;
    }

    if (current_ns == host_ns) {
        return true;
    }

    return false;
}

bool run_in_normal_docker(void)
{
    if ((!check_in_host_mach()) && (!dbl_current_is_admin())) {
        recfg_debug("In normal docker.\n");
        return true;
    }

    return false;
}
KA_EXPORT_SYMBOL(run_in_normal_docker);

bool run_in_admin_docker(void)
{
    if ((!check_in_host_mach()) && dbl_current_is_admin()) {
        recfg_debug("In admin docker.\n");
        return true;
    }

    return false;
}

bool run_in_docker(void)
{
    if (!check_in_host_mach()) {
        recfg_debug("In docker.\n");
        return true;
    }

    return false;
}
KA_EXPORT_SYMBOL(run_in_docker);
