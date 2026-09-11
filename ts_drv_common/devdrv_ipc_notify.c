/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2023. All rights reserved.
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
#ifdef CFG_FEATURE_IPC_NOTIFY
#include <linux/of_address.h>
#include <linux/platform_device.h>

#include "ka_task_pub.h"
#include "devdrv_ipc_notify.h"
#include "devdrv_manager_common.h"
#include "devdrv_manager.h"
#include "devdrv_nsm.h"
#include "devdrv_manager_container.h"
#include "tsdrv_device.h"
#include "devdrv_id.h"
#include "devdrv_interface.h"
#include "tsdrv_notify.h"
#include "ascend_hal_define.h"
#include "tsdrv_sync.h"
#if defined(CFG_SOC_PLATFORM_MINI) && (!defined(CFG_SOC_PLATFORM_MINIV2)) && (!defined(CFG_SOC_PLATFORM_MINIV3))
#include "devdrv_manager_container_noplugin.h"
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
#ifndef AOS_LLVM_BUILD
#include <linux/sched/signal.h>
#endif
#endif

#define TRS_SHR_ID_WLIST_FIRST_IDX      1
#define TRS_WAKEUP_TIMEINTERVAL 500 /* 0.5s */
#ifndef TSDRV_UT
static void devdrv_try_cond_resched_by_time(u32 *pre_stamp, u32 time)
{
    u32 timeinterval;

    timeinterval = ka_system_jiffies_to_msecs(jiffies - *pre_stamp);
    if ((timeinterval > time) && !ka_base_in_atomic()) {
        ka_task_cond_resched();
        *pre_stamp = (u32)ka_jiffies;
    }
}

static void devdrv_try_cond_resched(u32 *pre_stamp)
{
    devdrv_try_cond_resched_by_time(pre_stamp, TRS_WAKEUP_TIMEINTERVAL);
}
#endif
STATIC u32 devdrv_get_ts_num(void)
{
    u32 tsid;

#ifndef CFG_SOC_PLATFORM_MINIV2
    tsid = DEVDRV_MAX_TS_NUM;
#else
    tsid = 1;
#endif
    return tsid;
}

STATIC int devdrv_manager_ipc_notify_param_check(struct devdrv_notify_ioctl_info *notify_ioctl_info,
    struct ipc_notify_info *ipc_notify_info)
{
    u32 maxid;
    u32 tsnum;

    if ((notify_ioctl_info->dev_id >= TSDRV_MAX_DAVINCI_NUM) || (notify_ioctl_info->tsid >= DEVDRV_MAX_TS_NUM)
        || (ipc_notify_info == NULL) || (notify_ioctl_info->id_type != NOTIFY_ID_TYPE)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid parameter, dev_id = %u, ts_id = %u, idtype = %u, (ipc_notify_info == NULL)(%d).\n",
            notify_ioctl_info->dev_id, notify_ioctl_info->tsid, notify_ioctl_info->id_type, (ipc_notify_info == NULL));
        return -ENODEV;
#endif
    }

    tsnum = devdrv_get_ts_num();
    if (notify_ioctl_info->tsid >= tsnum) {
#ifndef TSDRV_UT
        devdrv_drv_err("invalid ts id, dev_id = %u, ts_id = %u.\n",
            notify_ioctl_info->dev_id, notify_ioctl_info->tsid);
        return -ENODEV;
#endif
    }

    maxid = (notify_ioctl_info->id_type == IPC_EVENT_TYPE) ? DEVDRV_MAX_HW_EVENT_ID : DEVDRV_MAX_NOTIFY_ID;

    if (notify_ioctl_info->notify_id >= maxid) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid notify id %d.\n", notify_ioctl_info->notify_id);
        return -ENODEV;
#endif
    }

    return 0;
}

static inline struct tsdrv_id_info *devdrv_manager_get_notify_info(struct tsdrv_ts_resource *ts_res,
    u32 notify_id, u32 type)
{
    struct tsdrv_id_info *notify_info = NULL;

    if (type == IPC_EVENT_TYPE) {
        /* In devdrv_event_id_init func, id addr stores in TSDRV_EVENT_SW type, so not use TSDRV_EVENT_HW type,
         * otherwise, ts_res->id_res[TSDRV_EVENT_HW_ID].id_addr is null ptr, causes coredump.
        */
        notify_info = (struct tsdrv_id_info *)(ts_res->id_res[TSDRV_EVENT_SW_ID].id_addr +
                                                       (long)sizeof(struct tsdrv_id_info) * notify_id);
    } else {
        notify_info = (struct tsdrv_id_info *)(ts_res->id_res[TSDRV_NOTIFY_ID].id_addr +
                                                       (long)sizeof(struct tsdrv_id_info) * notify_id);
    }

    return notify_info;
}

static void devdrv_manager_notify_node_proc(u32 dev_id, struct ipc_notify_node *notify_node,
    struct tsdrv_id_info *id_info, struct devdrv_notify_ioctl_info *ioctl_info, struct ipc_notify_info *ipc_info)
{
    u64 current_time = ka_system_ktime_to_ns(ka_system_ktime_get());
    int i;

    ka_task_mutex_init(&notify_node->mutex);
    ka_base_atomic_inc(&id_info->ref);

    ka_task_mutex_lock(&ipc_info->info_mutex);
    ka_task_mutex_lock(&notify_node->mutex);
    notify_node->status = 0;
    notify_node->dev_id = dev_id;
    notify_node->tsid = ioctl_info->tsid;
    notify_node->notify_id = ioctl_info->notify_id;
    notify_node->type = ioctl_info->id_type;
    notify_node->open_info[0].pid = (ka_pid_t)current->tgid;
    notify_node->open_info[0].set_time = current_time;

    for (i = 0; i < DEVDRV_PID_MAX_NUM; i++) {
        notify_node->open_info[i].node = notify_node;
    }
    ka_list_add(&notify_node->create_list_node, &ipc_info->create_list_head);
    ka_task_mutex_unlock(&notify_node->mutex);

    ipc_info->create_fd_num++;
    TSDRV_PRINT_DEBUG("Create ipc notify. (phyid=%u; notify_id=%u; id_ref=%d; create_fd_num=%u).\n",
        dev_id, ioctl_info->notify_id, ka_base_atomic_read(&id_info->ref), ipc_info->create_fd_num);
    ka_task_mutex_unlock(&ipc_info->info_mutex);

    return;
}

STATIC int devdrv_manager_local_check_notify(struct tsdrv_ts_resource *ts_res, u32 notify_id, u32 type)
{
    struct tsdrv_id_info *notify_info = devdrv_manager_get_notify_info(ts_res, notify_id, type);
#ifndef TSDRV_UT
    ka_task_spin_lock(&notify_info->spinlock);
    if (notify_info->ctx == NULL) {
        ka_task_spin_unlock(&notify_info->spinlock);
        TSDRV_PRINT_ERR("notify id is not allocated yet, notify_id(%u)\n", notify_id);
        return -ENODEV;
    }

    if (notify_info->id != notify_id) {
        ka_task_spin_unlock(&notify_info->spinlock);
        TSDRV_PRINT_ERR("invalid notify id(%d), notify_id = %u\n", notify_info->id, notify_id);
        return -EINVAL;
    }
    ka_task_spin_unlock(&notify_info->spinlock);
#endif
    return 0;
}

static int devdrv_manager_ipc_notify_creat_check(u32 dev_id, struct devdrv_notify_ioctl_info *ioctl_info,
    struct tsdrv_ts_resource *ts_res)
{
    struct tsdrv_id_info *notify_info = devdrv_manager_get_notify_info(ts_res,
        ioctl_info->notify_id, ioctl_info->id_type);
    int ret;
    u32 id_type;

#ifndef TSDRV_UT
    if ((ioctl_info->flag & TSDRV_FLAG_REMOTE_ID) != 0) {
        id_type = (ioctl_info->id_type == IPC_EVENT_TYPE) ? TSDRV_EVENT_HW_ID : TSDRV_NOTIFY_ID;
        ret = tsdrv_msg_check_res_id(dev_id, ioctl_info->tsid, id_type, ioctl_info->notify_id);
        if (ret != 0) {
            TSDRV_PRINT_ERR("check remote notify failed. (dev_id=%u; tsid=%u; id_type=%u; id=%u; ret=%d)\n",
                dev_id, ioctl_info->tsid, ioctl_info->id_type, ioctl_info->notify_id, ret);
            return ret;
        }
    } else {
        ret = devdrv_manager_local_check_notify(ts_res, ioctl_info->notify_id, ioctl_info->id_type);
        if (ret != 0) {
            TSDRV_PRINT_ERR("check local notify failed. (dev_id=%u; tsid=%u; id_type=%u; id=%u; ret=%d)\n",
                dev_id, ioctl_info->tsid, ioctl_info->id_type, ioctl_info->notify_id, ret);
            return ret;
        }
        /* Set ref to 1 when calling devdrv_alloc_notify_id , each notify_id corresponds to an ipc_name */
        if (ka_base_atomic_read(&notify_info->ref) != 1) {
            TSDRV_PRINT_ERR("ipc_notify had been created. (dev_id=%u; tsid=%u; id_type=%u; id=%u)\n",
                dev_id, ioctl_info->tsid, ioctl_info->id_type, ioctl_info->notify_id);
            return -EINVAL;
        }
    }
#endif
    return 0;
}

int devdrv_manager_ipc_notify_create(void *c, unsigned long arg,  void *n)
{
#ifndef AOS_LLVM_BUILD
    struct devdrv_notify_ioctl_info *ioctl_info  = (struct devdrv_notify_ioctl_info *)n;
    struct devdrv_manager_context *context = (struct devdrv_manager_context *)c;
    struct ipc_notify_info *ipc_notify_info = context->ipc_notify_info;
    struct ipc_notify_node *notify_node = NULL;
    struct tsdrv_id_info *notify_info = NULL;
    struct tsdrv_device *ts_dev = NULL;
    u32 dev_id = 0;
    u32 vfid = 0;
    int ret;

    if (devdrv_manager_ipc_notify_param_check(ioctl_info, ipc_notify_info)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid parameter. devid(%u)\n", ioctl_info->dev_id);
        return -ENODEV;
#endif
    }
    TSDRV_PRINT_DEBUG("devid=%u; tsid=%u; notify_id=%u; id_type=%u; flag=%u\n",
        ioctl_info->dev_id, ioctl_info->tsid, ioctl_info->notify_id, ioctl_info->id_type, ioctl_info->flag);

    ret = devdrv_manager_container_logical_id_to_physical_id(ioctl_info->dev_id, &dev_id, &vfid);
    if ((ret != 0) || (vfid != TSDRV_PM_FID)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("logical id convert to physical id failed, ret(%d), logid(%u), phyid(%u), vfid(%u).\n", ret,
            ioctl_info->dev_id, dev_id, vfid);
        return -EFAULT;
#endif
    }

    ts_dev = tsdrv_get_dev(dev_id);
    if (ts_dev == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("dev_info(%pK) or cce_ctrl is NULL. phyid(%u)\n", ts_dev, dev_id);
        return -EINVAL;
#endif
    }
    if ((tsdrv_dev_ref_inc_return(dev_id, TSDRV_PM_FID) > TSDRV_MAX_DEV_REF) ||
        (tsdrv_get_dev_status(dev_id, TSDRV_PM_FID) != TSDRV_DEV_ACTIVE)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("device%u will be unregistered, cannot ioctl again.\n", dev_id);
        tsdrv_dev_ref_dec(dev_id, TSDRV_PM_FID);
        return -EEXIST;
#endif
    }

    ret = devdrv_manager_ipc_notify_creat_check(dev_id, ioctl_info,
        &(ts_dev->dev_res[TSDRV_PM_FID].ts_resource[ioctl_info->tsid]));
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Notify res check failed. (dev_id=%u; tsid=%u; id_type=%u; id=%u; ret=%d)\n",
            dev_id, ioctl_info->tsid, ioctl_info->id_type, ioctl_info->notify_id, ret);
        tsdrv_dev_ref_dec(dev_id, TSDRV_PM_FID);
        return ret;
#endif
    }

    if (devdrv_ipc_create(ioctl_info->name, (void *)&notify_node, DEVDRV_IPC_NOTIFY, ioctl_info->flag)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("create ipc notify failed.\n");
        tsdrv_dev_ref_dec(dev_id, TSDRV_PM_FID);
        return -EINVAL;
#endif
    }

    ret = copy_to_user_safe((void *)((uintptr_t)arg), (void *)ioctl_info, sizeof(struct devdrv_notify_ioctl_info));
    if (ret != 0) {
#ifndef TSDRV_UT
        (void)devdrv_ipc_del(ioctl_info->name, DEVDRV_IPC_NAME_SIZE, DEVDRV_IPC_NOTIFY);
        tsdrv_dev_ref_dec(dev_id, TSDRV_PM_FID);
        TSDRV_PRINT_ERR("copy to user failed, ret(%d).\n", ret);
        return -EFAULT;
#endif
    }

    notify_info = devdrv_manager_get_notify_info(&(ts_dev->dev_res[TSDRV_PM_FID].ts_resource[notify_node->tsid]),
        ioctl_info->notify_id, ioctl_info->id_type);
    devdrv_manager_notify_node_proc(dev_id, notify_node, notify_info, ioctl_info, ipc_notify_info);
    tsdrv_dev_ref_dec(dev_id, TSDRV_PM_FID);

#endif
    return 0;
}

STATIC int devdrv_manager_match_pid(struct ipc_notify_node *notify_node, ka_pid_t pid)
{
    int i;

    for (i = 0; i < DEVDRV_PID_MAX_NUM; i++) {
        if (notify_node->open_info[i].pid == pid) {
            return i;
        }
    }
    return DEVDRV_PID_MAX_NUM;
}

#define DEVDRV_NSEC_PER_SEC 1000000000L
STATIC u64 devdrv_manager_get_context_start_time(struct devdrv_manager_context *context)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
    return context->start_time;
#else
    return ((u64)(context->start_time.tv_sec * DEVDRV_NSEC_PER_SEC) + context->start_time.tv_nsec);
#endif
}

STATIC int devdrv_manager_check_pid(struct ipc_notify_node *notify_node, u64 start_time, ka_pid_t pid)
{
    int index;

    index = devdrv_manager_match_pid(notify_node, pid);
    if ((index >= 0) && (index < DEVDRV_PID_MAX_NUM) &&
        (start_time <= notify_node->open_info[index].set_time)) {
        return index;
    }

    return DEVDRV_PID_MAX_NUM;
}

STATIC void devdrv_manager_ipc_notify_pid_show(struct ipc_notify_node *notify_node)
{
    int i;

    for (i = 0; i < DEVDRV_PID_MAX_NUM; i++) {
        TSDRV_PRINT_DEBUG("pid[%d] = %d start_time(%llu).devid(%u) notify(%u).\n", i, notify_node->open_info[i].pid,
            notify_node->open_info[i].set_time, notify_node->dev_id, notify_node->notify_id);
    }
}

int devdrv_manager_ipc_notify_set_attr(void *ctx, void *notify_info)
{
    struct devdrv_notify_ioctl_info *ioctl_info = NULL;
    struct devdrv_manager_context *context = NULL;
    struct ipc_notify_node *notify_node = NULL;

    ioctl_info = (struct devdrv_notify_ioctl_info *)notify_info;
    context = (struct devdrv_manager_context *)ctx;
    if (devdrv_ipc_find(ioctl_info->name, (void *)&notify_node, DEVDRV_IPC_NOTIFY)) {
        TSDRV_PRINT_ERR("Find ipc notify failed.\n");
        return -EINVAL;
    }

    ka_task_mutex_lock(&notify_node->mutex);
    if (((notify_node->need_wlist == true) && (notify_node->open_info[TRS_SHR_ID_WLIST_FIRST_IDX].pid != 0)) || (notify_node->open_info[0].pid != current->tgid)) {
        ka_task_mutex_unlock(&notify_node->mutex);
        return -EPERM;
    }
    notify_node->need_wlist = false;
    ka_task_mutex_unlock(&notify_node->mutex);
    TSDRV_PRINT_INFO("Set attribute success.(name=%s)\n", ioctl_info->name);
    return 0;
}

int devdrv_manager_ipc_notify_get_attr(void *ctx, unsigned long arg, void *notify_info)
{
    struct devdrv_notify_ioctl_info *ioctl_info = NULL;
    struct devdrv_manager_context *context = NULL;
    struct ipc_notify_node *notify_node = NULL;
    int ret;

    ioctl_info = (struct devdrv_notify_ioctl_info *)notify_info;
    context = (struct devdrv_manager_context *)ctx;
    if (devdrv_ipc_find(ioctl_info->name, (void *)&notify_node, DEVDRV_IPC_NOTIFY)) {
        TSDRV_PRINT_ERR("find ipc notify failed.\n");
        return -EINVAL;
    }
    ioctl_info->enableFlag = notify_node->need_wlist? SHRID_WLIST_ENABLE: SHRID_NO_WLIST_ENABLE;
    ret = copy_to_user_safe((void *)(uintptr_t)arg, (void *)ioctl_info, sizeof(struct devdrv_notify_ioctl_info));
    if (ret != 0) {
        TSDRV_PRINT_ERR("copy to user failed. dev_id(%u)\n", ioctl_info->dev_id);
        return ret;
    }

    return 0;
}

int devdrv_manager_ipc_notify_get_info(void *ctx, unsigned long arg, void *notify_info)
{
    struct devdrv_notify_ioctl_info *ioctl_info = NULL;
    struct devdrv_manager_context *context = NULL;
    struct ipc_notify_node *notify_node = NULL;
    int ret;

    ioctl_info = (struct devdrv_notify_ioctl_info *)notify_info;
    context = (struct devdrv_manager_context *)ctx;
    if (devdrv_ipc_find(ioctl_info->name, (void *)&notify_node, DEVDRV_IPC_NOTIFY)) {
        TSDRV_PRINT_ERR("find ipc notify failed.\n");
        return -EINVAL;
    }
    ioctl_info->dev_id = notify_node->dev_id;
    ret = copy_to_user_safe((void *)(uintptr_t)arg, (void *)ioctl_info, sizeof(struct devdrv_notify_ioctl_info));
    if (ret != 0) {
        TSDRV_PRINT_ERR("copy to user failed. dev_id(%u)\n", ioctl_info->dev_id);
        return ret;
    }

    return 0;
}

/* open func passed to the user the physical id */
int devdrv_manager_ipc_notify_open(void *c, unsigned long arg, void *n)
{
    struct devdrv_notify_ioctl_info *ioctl_info  = (struct devdrv_notify_ioctl_info *)n;
    struct devdrv_manager_context *context = (struct devdrv_manager_context *)c;
    struct ipc_notify_info *ipc_notify_info = context->ipc_notify_info;
    struct ipc_notify_node *notify_node = NULL;
    struct tsdrv_id_info *notify_info = NULL;
    struct tsdrv_device *ts_dev = NULL;
    u64 start_time = devdrv_manager_get_context_start_time(context);
    int pid_index;
    int ret;

    if (devdrv_ipc_find(ioctl_info->name, (void *)&notify_node, DEVDRV_IPC_NOTIFY)) {
        TSDRV_PRINT_ERR("find ipc notify failed.\n");
        return -EINVAL;
    }

    ka_task_mutex_lock(&notify_node->mutex);
    pid_index = devdrv_manager_match_pid(notify_node, current->tgid);
    if (pid_index == DEVDRV_PID_MAX_NUM) {
        if (notify_node->need_wlist) {
            ka_task_mutex_unlock(&notify_node->mutex);
            devdrv_manager_ipc_notify_pid_show(notify_node);
            TSDRV_PRINT_ERR("Current tgid has no permission, pid_index(%d), start_time(%llu), notifyid(%d), devid(%u)\n",
            pid_index, start_time, notify_node->notify_id, notify_node->dev_id);
            return -EINVAL;
        } else {
            pid_index = devdrv_manager_match_pid(notify_node, 0);
            if (pid_index == DEVDRV_PID_MAX_NUM) {
                ka_task_mutex_unlock(&notify_node->mutex);
                TSDRV_PRINT_ERR("Open pid has reached the maximum value, "
                                "the pid following failed to open(pid=%d).\n", current->tgid);
                return -EBUSY;
            }
            notify_node->open_info[pid_index].pid = current->tgid;
            notify_node->open_info[pid_index].set_time = ktime_to_ns(ktime_get());
        }
    }
    ka_task_mutex_unlock(&notify_node->mutex);

    if (start_time >= notify_node->open_info[pid_index].set_time) {
        return -ESRCH;
    }

    ka_task_mutex_lock(&ipc_notify_info->info_mutex);
    if (ka_base_atomic_inc_return(&notify_node->open_info[pid_index].ref) >= NOTIFY_MAX_OPEM_TIMES) {
#ifndef TSDRV_UT
        ka_base_atomic_dec(&notify_node->open_info[pid_index].ref);
        ka_task_mutex_unlock(&ipc_notify_info->info_mutex);
        TSDRV_PRINT_ERR("open notify times out of range.\n");
        return -EINVAL;
#endif
    }

    ts_dev = tsdrv_get_dev(notify_node->dev_id);
    if (ts_dev == NULL) {
#ifndef TSDRV_UT
        ka_base_atomic_dec(&notify_node->open_info[pid_index].ref);
        ka_task_mutex_unlock(&ipc_notify_info->info_mutex);
        TSDRV_PRINT_ERR("dev_info(%pK) or cce_ctrl is NULL. dev_id(%u)\n", ts_dev, notify_node->dev_id);
        return -EINVAL;
#endif
    }

    if ((tsdrv_dev_ref_inc_return(notify_node->dev_id, TSDRV_PM_FID) > TSDRV_MAX_DEV_REF) ||
        (tsdrv_get_dev_status(notify_node->dev_id, TSDRV_PM_FID) != TSDRV_DEV_ACTIVE)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("device%u will be unregistered, cannot ioctl again.\n", notify_node->dev_id);
        ka_base_atomic_dec(&notify_node->open_info[pid_index].ref);
        tsdrv_dev_ref_dec(notify_node->dev_id, TSDRV_PM_FID);
        ka_task_mutex_unlock(&ipc_notify_info->info_mutex);
        return -EEXIST;
#endif
    }

    if ((notify_node->flag & TSDRV_FLAG_REMOTE_ID) == 0) {
        ret = devdrv_manager_local_check_notify(&(ts_dev->dev_res[TSDRV_PM_FID].ts_resource[notify_node->tsid]),
            notify_node->notify_id, notify_node->type);
        if (ret != 0) {
#ifndef TSDRV_UT
            ka_base_atomic_dec(&notify_node->open_info[pid_index].ref);
            tsdrv_dev_ref_dec(notify_node->dev_id, TSDRV_PM_FID);
            ka_task_mutex_unlock(&ipc_notify_info->info_mutex);
            TSDRV_PRINT_ERR("check notify id(%u) failed, dev_id(%u)\n", notify_node->notify_id, notify_node->dev_id);
            return ret;
#endif
        }
    }

    notify_info = devdrv_manager_get_notify_info(&(ts_dev->dev_res[TSDRV_PM_FID].ts_resource[notify_node->tsid]),
        notify_node->notify_id, notify_node->type);

    ioctl_info->dev_id = notify_node->dev_id;
    ioctl_info->tsid = notify_node->tsid;
    ioctl_info->notify_id = notify_node->notify_id;
    ioctl_info->id_type = notify_node->type;
    if ((ka_base_atomic_read(&notify_node->open_info[pid_index].ref) == 1) &&
        ((notify_node->flag & TSDRV_FLAG_REMOTE_ID) != 0)) {
#ifndef TSDRV_UT
            ioctl_info->flag = DEVDRV_SHRID_OP_CONFIG;
#endif
    }
    ret = copy_to_user_safe((void *)(uintptr_t)arg, (void *)ioctl_info, sizeof(struct devdrv_notify_ioctl_info));
    if (ret == 0) {
        if (ka_base_atomic_read(&notify_node->open_info[pid_index].ref) == 1) {
            ka_base_atomic_inc(&notify_info->ref);
            ipc_notify_info->open_fd_num++;
            ka_task_mutex_lock(&notify_node->mutex);
            ka_list_add(&notify_node->open_info[pid_index].open_list_node, &ipc_notify_info->open_list_head);
            ka_task_mutex_unlock(&notify_node->mutex);
        }
    } else {
        TSDRV_PRINT_ERR("copy to user failed. dev_id(%u)\n", ioctl_info->dev_id);
        ka_base_atomic_dec(&notify_node->open_info[pid_index].ref);
    }
    TSDRV_PRINT_DEBUG("open ok. (id=%u; ref=%d; open_ref=%u; flag=%u).\n", ioctl_info->notify_id,
        ka_base_atomic_read(&notify_info->ref), ka_base_atomic_read(&notify_node->open_info[pid_index].ref), ioctl_info->flag);
    tsdrv_dev_ref_dec(notify_node->dev_id, TSDRV_PM_FID);
    ka_task_mutex_unlock(&ipc_notify_info->info_mutex);

    return ret;
}

STATIC int devdrv_manager_ipc_notify_update_open_info(struct ipc_notify_node *node, ka_pid_t pid,
    u64 current_time)
{
    u32 i;

    for (i = 0; i < DEVDRV_PID_MAX_NUM; i++) {
        if (node->open_info[i].pid == pid) {
            node->open_info[i].set_time = current_time;
            TSDRV_PRINT_DEBUG("set pid succ. dev_id(%u) notify(%u) set_time(%llu)\n",
                node->dev_id, node->notify_id, node->open_info[i].set_time);
            return 0;
        }
        if (node->open_info[i].pid == 0) {
            node->open_info[i].pid = pid;
            node->open_info[i].set_time = current_time;
            TSDRV_PRINT_DEBUG("set pid succ. dev_id(%u) notify(%u) set_time(%llu)\n",
                node->dev_id, node->notify_id, node->open_info[i].set_time);
            return 0;
        }
    }

    return -EINVAL;
}

int devdrv_manager_ipc_notify_set_pid(void *c, void *n)
{
    struct devdrv_notify_ioctl_info *notify_ioctl_info  = (struct devdrv_notify_ioctl_info *)n;
    struct ipc_notify_node *notify_node = NULL;
    u64 current_time = ka_system_ktime_to_ns(ka_system_ktime_get());
    unsigned int i;
    int ret;

    if (devdrv_ipc_find(notify_ioctl_info->name, (void *)&notify_node, DEVDRV_IPC_NOTIFY)) {
        TSDRV_PRINT_ERR("find ipc notify failed.\n");
        return -EINVAL;
    }

    if (notify_node->open_info[0].pid != (ka_pid_t)current->tgid) {
        TSDRV_PRINT_ERR("Invalid process, curr_tgid[%d], creat_tgid[%d].\n",
            current->tgid, notify_node->open_info[0].pid);
        return -EINVAL;
    }

    ka_task_mutex_lock(&notify_node->mutex);
    if (!notify_node->need_wlist) {
        ka_task_mutex_unlock(&notify_node->mutex);
        TSDRV_PRINT_ERR("Process can't set pid.(pid=%d; name=%s)\n",
            current->tgid, notify_ioctl_info->name);
        return -EPERM;
    }
    for (i = 0; i < DEVDRV_PID_MAX_NUM; i++) {
        if (notify_ioctl_info->pid[i] == 0) {
            break;
        }
        ret = devdrv_manager_ipc_notify_update_open_info(notify_node, notify_ioctl_info->pid[i], current_time);
        if (ret != 0) {
            ka_task_mutex_unlock(&notify_node->mutex);
            TSDRV_PRINT_ERR("set pid has reached the maximum value, the pid following index:%d fail to be set.\n", i);
            return -EINVAL;
        }
    }
    ka_task_mutex_unlock(&notify_node->mutex);

    return 0;
}

int devdrv_manager_ipc_notify_record(void *context, void *notify_ioctl_info)
{
#ifndef TSDRV_UT
    struct devdrv_notify_ioctl_info *info = (struct devdrv_notify_ioctl_info *)notify_ioctl_info;
    struct devdrv_manager_context *ctx = (struct devdrv_manager_context *)context;
    u64 start_time = devdrv_manager_get_context_start_time(ctx);
    struct ipc_notify_node *notify_node = NULL;
    int pid_index, ret;
    u32 chip_type;
    size_t name_len;

    name_len = ka_base_strnlen(info->name, DEVDRV_IPC_NAME_SIZE);
    if ((name_len == 0) || (name_len >= DEVDRV_IPC_NAME_SIZE)) {
        TSDRV_PRINT_ERR("Length out of range. (name_len=%lu)\n", name_len);
        return -EINVAL;
    }

    if (devdrv_ipc_find(info->name, (void *)&notify_node, DEVDRV_IPC_NOTIFY)) {
        TSDRV_PRINT_ERR("Failed to find ipc notify. (name=%s)\n", info->name);
        return -EINVAL;
    }

    chip_type = devdrv_get_dev_chip_type(notify_node->dev_id);
    if ((chip_type != HISI_CLOUD_V1) && (chip_type != HISI_MINI_V2)) {
        TSDRV_PRINT_WARN("Not support.\n");
        return -EOPNOTSUPP;
    }

    pid_index = devdrv_manager_check_pid(notify_node, start_time, current->tgid);
    if (pid_index == DEVDRV_PID_MAX_NUM || pid_index == 0) {
        devdrv_manager_ipc_notify_pid_show(notify_node);
        TSDRV_PRINT_ERR("Current tgid has no permission to record. (pid_index=%d; start_time=0x%llx; "
            "devid=%u; notify_id=%d)\n", pid_index, start_time, notify_node->dev_id, notify_node->notify_id);
        return -EINVAL;
    }
    if (ka_base_atomic_read(&notify_node->open_info[pid_index].ref) == 0) {
        TSDRV_PRINT_ERR("Not open IPC notify yet. (devid=%u; notify_id=%u)\n",
            notify_node->dev_id, notify_node->notify_id);
        return -EINVAL;
    }

    ret = devdrv_write_notify_register(notify_node->dev_id, notify_node->notify_id);
    if (ret != 0) {
        TSDRV_PRINT_ERR("Failed to write notify register. (ret=%d; devid=%u; notify_id=%u)\n",
            ret, notify_node->dev_id, notify_node->notify_id);
        return ret;
    }
#endif
    return 0;
}

static bool devdrv_manager_notify_should_del(struct ipc_notify_node *notify_node)
{
    int i;

    for (i = 0; i < DEVDRV_PID_MAX_NUM; i++) {
        if (ka_base_atomic_read(&notify_node->open_info[i].ref) != 0) {
            return false;
        }
    }

    if (notify_node->status == 0) {
        return false;
    }

    return true;
}

static int devdrv_manager_ipc_notify_id_free(struct tsdrv_device *ts_dev,
    struct ipc_notify_node *notify_node)
{
    struct tsdrv_ts_resource *ts_resource = NULL;
    enum tsdrv_id_type id_type = TSDRV_NOTIFY_ID;
    struct tsdrv_id_info *notify_info = NULL;
    u32 notify_id;

    notify_id = notify_node->notify_id;
    ts_resource = &(ts_dev->dev_res[TSDRV_PM_FID].ts_resource[notify_node->tsid]);
    notify_info = devdrv_manager_get_notify_info(ts_resource, notify_id, notify_node->type);
    if (notify_info == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_INFO("unable to check notify id, notify_id(%u), dev_id(%u)"
            "do not care it in recycle process\n", notify_id, notify_node->dev_id);
        return -ENODEV;
#endif
    }

    if (NOTIFY_ID_TYPE == IPC_EVENT_TYPE) {
        notify_id |= IPC_EVENT_MASK;
        id_type = TSDRV_EVENT_HW_ID;    /* id_available_list: ipc event use hw type. */
    }

    /* if using ipc event,
     * the mutex should use TSDRV_EVENT_SW_ID's mutex,
     * but the id_available_list should use hw type.
      */
    ka_task_mutex_lock(&ts_resource->id_res[devdrv_manager_id_type_replace(id_type)].id_mutex_t);
    if (ka_base_atomic_dec_and_test(&notify_info->ref) && (notify_info->valid == 1)) {
        if (devdrv_notify_ts_msg(notify_node->dev_id, notify_node->tsid, notify_node->type,
            (int)notify_id, notify_info)) {
            TSDRV_PRINT_ERR("send ts notify reset msg failed. dev_id(%u)\n", notify_node->dev_id);
        }
        ka_task_spin_lock(&ts_resource->id_res[devdrv_manager_id_type_replace(id_type)].spinlock);
        ka_list_add(&notify_info->list, &ts_resource->id_res[id_type].id_available_list);
        notify_info->valid = 0;
        ts_resource->id_res[id_type].id_available_num++;
        ka_task_spin_unlock(&ts_resource->id_res[devdrv_manager_id_type_replace(id_type)].spinlock);
    }
    ka_task_mutex_unlock(&ts_resource->id_res[devdrv_manager_id_type_replace(id_type)].id_mutex_t);

    return 0;
}

STATIC void devdrv_manager_ipc_notify_free(struct ipc_notify_node *notify_node)
{
    struct tsdrv_device *ts_dev = NULL;
    int ret;

    ts_dev = tsdrv_get_dev(notify_node->dev_id);
    if (ts_dev == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("dev_info(%pK) or cce_ctrl is NULL. dev_id(%u)\n", ts_dev, notify_node->dev_id);
        return;
#endif
    }
    if ((tsdrv_dev_ref_inc_return(notify_node->dev_id, TSDRV_PM_FID) > TSDRV_MAX_DEV_REF) ||
        (tsdrv_get_dev_status(notify_node->dev_id, TSDRV_PM_FID) != TSDRV_DEV_ACTIVE)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("device%u will be unregistered, cannot ioctl again.\n", notify_node->dev_id);
        tsdrv_dev_ref_dec(notify_node->dev_id, TSDRV_PM_FID);
        return;
#endif
    }

    ret = devdrv_manager_ipc_notify_id_free(ts_dev, notify_node);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Id free failed. (devid=%u)\n", notify_node->dev_id);
        tsdrv_dev_ref_dec(notify_node->dev_id, TSDRV_PM_FID);
        return;
#endif
    }
    tsdrv_dev_ref_dec(notify_node->dev_id, TSDRV_PM_FID);

    TSDRV_PRINT_DEBUG("Node free ok. (notify_id=%u).\n", notify_node->notify_id);
    return;
}

int devdrv_manager_ipc_notify_destroy(void *c, void *n)
{
#ifndef AOS_LLVM_BUILD
    struct devdrv_manager_context *context = (struct devdrv_manager_context *)c;
    struct devdrv_notify_ioctl_info *notify_ioctl_info  = (struct devdrv_notify_ioctl_info *)n;
    struct ipc_notify_info *ipc_notify_info = context->ipc_notify_info;
    struct ipc_notify_node *notify_node = NULL;
    bool notify_node_del;
    size_t name_len;
    int ret;

    name_len = ka_base_strnlen(notify_ioctl_info->name, DEVDRV_IPC_NAME_SIZE);
    if ((name_len == 0) || (name_len >= DEVDRV_IPC_NAME_SIZE)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Length out of range. (name_len=%lu)\n", name_len);
        return -EINVAL;
#endif
    }
    ka_task_mutex_lock(&ipc_notify_info->info_mutex);
    if (devdrv_ipc_find(notify_ioctl_info->name, (void *)&notify_node, DEVDRV_IPC_NOTIFY)) {
#ifndef TSDRV_UT
        ka_task_mutex_unlock(&ipc_notify_info->info_mutex);
        TSDRV_PRINT_ERR("Failed to find ipc notify. (name=%s)\n", notify_ioctl_info->name);
        return -EINVAL;
#endif
    }

    if (notify_node->open_info[0].pid != current->tgid) {
#ifndef TSDRV_UT
        ka_task_mutex_unlock(&ipc_notify_info->info_mutex);
        TSDRV_PRINT_ERR("current tgid has no permission to destroy the notify node.\n");
        return -EFAULT;
#endif
    }

    if (ipc_notify_info->create_fd_num == 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("destroy notify node fail.(%u %d)\n", ipc_notify_info->create_fd_num,
            ka_base_atomic_read(&notify_node->open_info[0].ref));
        ka_task_mutex_unlock(&ipc_notify_info->info_mutex);
        return -EFAULT;
#endif
    }

    ipc_notify_info->create_fd_num--;
    ka_task_mutex_lock(&notify_node->mutex);
    ka_list_del(&notify_node->create_list_node);
    notify_node->status = 1;
    notify_node_del = devdrv_manager_notify_should_del(notify_node);

    notify_ioctl_info->dev_id = notify_node->dev_id;
    notify_ioctl_info->tsid = notify_node->tsid;
    notify_ioctl_info->id_type = notify_node->type;
    notify_ioctl_info->notify_id = notify_node->notify_id;

    devdrv_manager_ipc_notify_free(notify_node);
    ka_task_mutex_unlock(&notify_node->mutex);

    if (notify_node_del == true) {
        ret = devdrv_ipc_del(notify_node->name, DEVDRV_IPC_NAME_SIZE, DEVDRV_IPC_NOTIFY);
        if (ret != 0) {
            TSDRV_PRINT_ERR("ipc del failed, dev_id(%u)\n", notify_node->dev_id);
        }
    }
    ka_task_mutex_unlock(&ipc_notify_info->info_mutex);
#endif
    return 0;
}

/* the user passed the physical id */
int devdrv_manager_ipc_notify_close(void *c, void *n)
{
    struct devdrv_manager_context *context = (struct devdrv_manager_context *)c;
    struct devdrv_notify_ioctl_info *notify_ioctl_info  = (struct devdrv_notify_ioctl_info *)n;
    struct ipc_notify_info *ipc_notify_info = context->ipc_notify_info;
    struct ipc_notify_node *notify_node = NULL;
    bool notify_node_del;
    size_t name_len;
    int pid_index, ret;

    name_len = ka_base_strnlen(notify_ioctl_info->name, DEVDRV_IPC_NAME_SIZE);
    if ((name_len == 0) || (name_len >= DEVDRV_IPC_NAME_SIZE)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Length out of range. (name_len=%lu)\n", name_len);
        return -EINVAL;
#endif
    }
    ka_task_mutex_lock(&ipc_notify_info->info_mutex);
    if (devdrv_ipc_find(notify_ioctl_info->name, (void *)&notify_node, DEVDRV_IPC_NOTIFY)) {
#ifndef TSDRV_UT
        ka_task_mutex_unlock(&ipc_notify_info->info_mutex);
        TSDRV_PRINT_ERR("Failed to find ipc notify. (name=%s)\n", notify_ioctl_info->name);
        return -EINVAL;
#endif
    }

    pid_index = devdrv_manager_check_pid(notify_node, devdrv_manager_get_context_start_time(context), current->tgid);
    if (pid_index == DEVDRV_PID_MAX_NUM) {
#ifndef TSDRV_UT
        devdrv_manager_ipc_notify_pid_show(notify_node);
        ka_task_mutex_unlock(&ipc_notify_info->info_mutex);
        TSDRV_PRINT_ERR("current tgid has no permission, pid_index(%d), start_time(%llu), notifyid(%d), devid(%u)\n",
            pid_index, devdrv_manager_get_context_start_time(context), notify_node->notify_id, notify_node->dev_id);
        return -EFAULT;
#endif
    }

    if (ka_base_atomic_read(&notify_node->open_info[pid_index].ref) > (int)DEVDRV_IPC_REF_INIT) {
#ifndef TSDRV_UT
        TSDRV_PRINT_DEBUG("multi close notify: index(%d),notify_id(%u),ref(%d).\n",
            pid_index, notify_node->notify_id, ka_base_atomic_read(&notify_node->open_info[pid_index].ref));

        ka_base_atomic_dec(&notify_node->open_info[pid_index].ref);
        ka_task_mutex_unlock(&ipc_notify_info->info_mutex);
        return 0;
#endif
    }

    if ((ipc_notify_info->open_fd_num == 0) || (ka_base_atomic_read(&notify_node->open_info[pid_index].ref) == 0)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("close notify fail.(%u %d)\n", ipc_notify_info->open_fd_num,
            ka_base_atomic_read(&notify_node->open_info[pid_index].ref));
        ka_task_mutex_unlock(&ipc_notify_info->info_mutex);
        return -EFAULT;
#endif
    }
    TSDRV_PRINT_DEBUG("valid close notify: index(%d),notify_id(%u),ref(%d).\n",
        pid_index, notify_node->notify_id, ka_base_atomic_read(&notify_node->open_info[pid_index].ref));

    ka_task_mutex_lock(&notify_node->mutex);
    ka_list_del(&notify_node->open_info[pid_index].open_list_node);

    ipc_notify_info->open_fd_num--;
    ka_base_atomic_dec(&notify_node->open_info[pid_index].ref);
    notify_node_del = devdrv_manager_notify_should_del(notify_node);

    notify_ioctl_info->dev_id = notify_node->dev_id;
    notify_ioctl_info->tsid = notify_node->tsid;
    notify_ioctl_info->id_type = notify_node->type;
    notify_ioctl_info->notify_id = notify_node->notify_id;
    if ((ka_base_atomic_read(&notify_node->open_info[pid_index].ref) == 0) &&
        ((notify_node->flag & TSDRV_FLAG_REMOTE_ID) != 0)) {
#ifndef TSDRV_UT
        notify_ioctl_info->flag = DEVDRV_SHRID_OP_DECONFIG;
#endif
    }

    devdrv_manager_ipc_notify_free(notify_node);
    ka_task_mutex_unlock(&notify_node->mutex);
    if (notify_node_del == true) {
#ifndef TSDRV_UT
        ret = devdrv_ipc_del(notify_node->name, DEVDRV_IPC_NAME_SIZE, DEVDRV_IPC_NOTIFY);
        if (ret != 0) {
            TSDRV_PRINT_ERR("ipc del failed, dev_id(%u)\n", notify_node->dev_id);
        }
#endif
    }
    ka_task_mutex_unlock(&ipc_notify_info->info_mutex);

    TSDRV_PRINT_DEBUG("Close ok. (dev_id=%d; tsid=%d; id_type=%u; notify_id=%u; flag=%d).\n",
        notify_ioctl_info->dev_id, notify_ioctl_info->tsid, notify_ioctl_info->id_type,
        notify_ioctl_info->notify_id, notify_ioctl_info->flag);

    return 0;
}

static inline void devdrv_manager_ipc_notify_leak_print(struct devdrv_manager_context *context)
{
    struct ipc_notify_info *ipc_notify_info = context->ipc_notify_info;

    TSDRV_PRINT_WARN("Process Name: %s  "
                     "PID: %d  "
                     "TGID: %d\n",
                     context->task->comm, context->pid, context->tgid);

    TSDRV_PRINT_WARN("ipc_notify_info->create_fd_num = %d, "
                     "ipc_notify_info->open_fd_num = %d\n",
                     ipc_notify_info->create_fd_num, ipc_notify_info->open_fd_num);
}

STATIC void devdrv_manager_ipc_notify_recycle_opened(struct devdrv_manager_context *context,
                                                     struct ipc_notify_info *ipc_notify_info)
{
#ifndef TSDRV_UT
    struct ipc_open_info *open_info = NULL;
    struct ipc_notify_node *notify_node = NULL;
    ka_list_head_t *pos = NULL, *n = NULL;
    u32 stamp = (u32)ka_jiffies;

    ka_task_mutex_lock(&ipc_notify_info->info_mutex);
    if (!ka_list_empty_careful(&ipc_notify_info->open_list_head)) {
        ka_list_for_each_safe(pos, n, &ipc_notify_info->open_list_head) {
            bool node_del;
            int pid_index;

            open_info = ka_list_entry(pos, struct ipc_open_info, open_list_node);
            ipc_notify_info->open_fd_num--;
            notify_node = open_info->node;
            ka_task_mutex_lock(&notify_node->mutex);
            devdrv_manager_ipc_notify_free(notify_node);

            pid_index = devdrv_manager_match_pid(notify_node, context->tgid);
            if (pid_index == DEVDRV_PID_MAX_NUM) {
                ka_task_mutex_unlock(&notify_node->mutex);
                TSDRV_PRINT_ERR("can't find valid pid(%d).\n", context->tgid);
                continue;
            }
            ka_base_atomic_set(&notify_node->open_info[pid_index].ref, 0);
            node_del = devdrv_manager_notify_should_del(notify_node);
            ka_task_mutex_unlock(&notify_node->mutex);
            if (node_del && (devdrv_ipc_del(notify_node->name, DEVDRV_IPC_NAME_SIZE, DEVDRV_IPC_NOTIFY) != 0)) {
                TSDRV_PRINT_ERR("ipc del failed, dev_id(%u)\n", notify_node->dev_id);
            }
            devdrv_try_cond_resched(&stamp);
        }
    }
    TSDRV_PRINT_INFO("finish recycle opened ipc node, open_fd_num(%u)\n", ipc_notify_info->open_fd_num);
    ka_task_mutex_unlock(&ipc_notify_info->info_mutex);
#endif
}

STATIC void devdrv_manager_ipc_notify_recycle_created(struct devdrv_manager_context *context,
                                                      struct ipc_notify_info *ipc_notify_info)
{
#ifndef TSDRV_UT
    struct ipc_notify_node *notify_node = NULL;
    ka_list_head_t *pos = NULL, *n = NULL;
    u32 stamp = (u32)ka_jiffies;
    int pid_index;

    TSDRV_PRINT_INFO("start recycle created ipc node, create_fd_num(%u)\n", ipc_notify_info->create_fd_num);

    ka_task_mutex_lock(&ipc_notify_info->info_mutex);
    if (!ka_list_empty_careful(&ipc_notify_info->create_list_head)) {
        ka_list_for_each_safe(pos, n, &ipc_notify_info->create_list_head) {
            notify_node = ka_list_entry(pos, struct ipc_notify_node, create_list_node);
            ipc_notify_info->create_fd_num--;
            pid_index = devdrv_manager_match_pid(notify_node, context->tgid);
            if (pid_index != DEVDRV_PID_MAX_NUM) {
                bool node_del;

                ka_task_mutex_lock(&notify_node->mutex);
                notify_node->status = 1;
                devdrv_manager_ipc_notify_free(notify_node);
                node_del = devdrv_manager_notify_should_del(notify_node);
                ka_task_mutex_unlock(&notify_node->mutex);
                if (node_del && (devdrv_ipc_del(notify_node->name, DEVDRV_IPC_NAME_SIZE, DEVDRV_IPC_NOTIFY) != 0)) {
                    TSDRV_PRINT_ERR("ipc del failed, dev_id(%u)\n", notify_node->dev_id);
                }
            }
            devdrv_try_cond_resched(&stamp);
        }
    }
    TSDRV_PRINT_INFO("finish recycle created ipc node, create_fd_num(%u)\n", ipc_notify_info->create_fd_num);
    ka_task_mutex_unlock(&ipc_notify_info->info_mutex);
#endif
}

STATIC void devdrv_manager_ipc_notify_recycle_work(struct devdrv_manager_context *context,
                                                   struct ipc_notify_info *ipc_notify_info)
{
#ifndef TSDRV_UT
    devdrv_manager_ipc_notify_recycle_opened(context, ipc_notify_info);
    devdrv_manager_ipc_notify_recycle_created(context, ipc_notify_info);
#endif
}

void devdrv_manager_ipc_notify_release_recycle(void *ctx)
{
#ifndef TSDRV_UT
    struct devdrv_manager_context *context = (struct devdrv_manager_context *)ctx;
    struct ipc_notify_info *ipc_notify_info = NULL;

    if ((context == NULL) || (context->ipc_notify_info == NULL)) {
        TSDRV_PRINT_ERR("context(%pK) or context->ipc_notify_info is NULL\n", context);
        return;
    }
    ipc_notify_info = context->ipc_notify_info;

    if ((ipc_notify_info->create_fd_num > 0) || (ipc_notify_info->open_fd_num > 0)) {
        devdrv_manager_ipc_notify_leak_print(context);
        devdrv_manager_ipc_notify_recycle_work(context, ipc_notify_info);
    }
#endif
}
#endif

