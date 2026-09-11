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

#include "ka_task_pub.h"

#include "dbl/runenv_config.h"
#include "urd_define.h"
#include "kernel_version_adapt.h"

#ifdef CFG_FEATURE_SRIOV
#  include "devdrv_interface.h"
#endif

#ifdef CFG_FEATURE_RC_MODE
#  include "devdrv_user_common.h"
#endif
#include "urd_container.h"

#ifdef CFG_HOST_ENV
#include "dbl/uda.h"
#endif

#ifndef CFG_RUNENV_SUPPORT
STATIC int urd_container_task_struct_check(ka_task_struct_t *tsk)
{
    if (tsk == NULL) {
        dms_err("tsk is NULL.\n");
        return -EINVAL;
    }
    if (tsk->nsproxy == NULL) {
        dms_err("tsk->nsproxy is NULL.\n");
        return -ENODEV;
    }
    if (tsk->nsproxy->mnt_ns == NULL) {
        dms_err("tsk->nsproxy->mnt_ns is NULL.\n");
        return -EFAULT;
    }
    return 0;
}

STATIC int urd_get_current_mnt_ns(u64 *current_mnt_ns)
{
    int ret;

    ret = urd_container_task_struct_check(ka_task_get_current());
    if (ret) {
        dms_err("current is invalid, ret=%d\n", ret);
        return -EINVAL;
    }

    *current_mnt_ns = (u64)(uintptr_t)(ka_task_get_current()->nsproxy->mnt_ns);
    return 0;
}

STATIC int urd_container_check_current(void)
{
    /* current->nsproxy is NULL when the release function is called */
    if (ka_task_get_current() == NULL || ka_task_get_current()->nsproxy == NULL || ka_task_get_current()->nsproxy->mnt_ns == NULL) {
        dms_err("(current == NULL) is %d, (current->nsproxy == NULL) is %d\n",
            (ka_task_get_current() == NULL), ((ka_task_get_current() == NULL) ? (-EINVAL) : (ka_task_get_current()->nsproxy == NULL)));
        return -EINVAL;
    }

    return 0;
}

static inline struct mnt_namespace *urd_get_host_mnt_ns(void)
{
    return init_task.nsproxy->mnt_ns;
}

static bool urd_container_is_admin(void)
{
    return dbl_current_is_admin();
}

bool urd_is_pf_device(unsigned int dev_id)
{
#ifdef CFG_FEATURE_SRIOV
    if (devdrv_get_pfvf_type_by_devid(dev_id) == DEVDRV_SRIOV_TYPE_VF) {
        return false;
    }
#elif (defined CFG_FEATURE_RC_MODE)
#ifdef CFG_FEATURE_DEVICE_ENV
    if (VDAVINCI_IS_VDEV(dev_id)) {
        return false;
    }
#endif
#endif

    return true;
}

/**
 * Check the process is running in the physical machine or in container
 */
STATIC bool urd_container_is_host_system(struct mnt_namespace *mnt_ns)
{
    if (mnt_ns == urd_get_host_mnt_ns()) {
        return true;
    }

    return false;
}

int urd_container_is_in_admin_container(void)
{
    if (urd_container_check_current()) {
        return false;
    }

    if (urd_container_is_host_system(ka_task_get_current()->nsproxy->mnt_ns)) {
        return false;
    }

    if (urd_container_is_admin()) {
        return true;
    }

    return false;
}

/*
 * The implementation of this function is the same as that of @devdrv_is_in_container.
 */
int urd_container_is_in_container(void)
{
    u64 current_mnt;
    u64 host_mnt;
    int is_in;
    int ret;

    ret = urd_get_current_mnt_ns(&current_mnt);
    if (ret) {
        dms_err("get current mnt ns error, ret = %d\n", ret);
        return -EINVAL;
    }

    host_mnt = (u64)urd_get_host_mnt_ns();
    if (host_mnt == 0) {
        is_in = false;
    } else if ((current_mnt != host_mnt) && (!urd_container_is_admin())) {
        is_in = true;
    } else {
        is_in = false;
    }

    return is_in;
}
#endif   /* CFG_RUNENV_SUPPORT */

typedef int (*dms_container_logical_id_to_physical_id_t)(u32 logical_dev_id, u32 *physical_dev_id, u32 *vfid);

int urd_container_logical_id_to_physical_id(u32 logical_dev_id, u32 *physical_dev_id, u32 *vfid)
{
#ifdef CFG_HOST_ENV
    return uda_devid_to_phy_devid(logical_dev_id, physical_dev_id, vfid);
#else
    static dms_container_logical_id_to_physical_id_t handler = NULL;

    if (handler == NULL) {
        handler = (dms_container_logical_id_to_physical_id_t)(uintptr_t)
            __kallsyms_lookup_name("devdrv_manager_container_logical_id_to_physical_id");
    }

    if (handler != NULL) {
        return handler(logical_dev_id, physical_dev_id, vfid);
    }

    if (physical_dev_id != NULL) {
        *physical_dev_id = logical_dev_id;
    }

    return 0;
#endif
}

