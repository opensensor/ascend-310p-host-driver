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

#include <linux/pm.h>

#include "ka_memory_pub.h"
#include "ka_task_pub.h"
#include "ka_system_pub.h"

#include "devdrv_mailbox.h"
#include "devdrv_common.h"
#include "devdrv_driver_pm.h"
#include "devdrv_manager_common.h"
#include "dbl_mem_alloc_interface.h"
#include "devdrv_pm.h"
#include "devdrv_manager.h"
#include "devdrv_interface.h"
#include "devdrv_black_box.h"
#include "devdrv_pcie.h"

#include "dms_notifier.h"
#include "dms_time.h"
#include "kernel_version_adapt.h"

#include "tsdrv_status.h"

#if defined(DEVDRV_MANAGER_HOST_UT_TEST)
#define STATIC
#else
#define STATIC static
#endif

#define DEVDRV_H2D_CYCLE 6
ka_workqueue_struct_t *g_aicore_info_wq = NULL;
struct devdrv_aicore_info *g_aicore_info[DEVDRV_MAX_DAVINCI_NUM];

struct devdrv_pm *devdrv_manager_register_pm(int (*suspend)(u32 devid), int (*resume)(u32 devid))
{
    struct devdrv_manager_info *d_info = NULL;
    struct devdrv_pm *pm = NULL;

    if ((suspend == NULL) || (resume == NULL)) {
        devdrv_drv_err("suspend or resume is NULL.\n");
        return NULL;
    }

    pm = dbl_kzalloc(sizeof(struct devdrv_pm), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (pm == NULL) {
        devdrv_drv_err("kmalloc failed.\n");
        return NULL;
    }

    pm->suspend = suspend;
    pm->resume = resume;
    pm->run_stage = DEVDRV_PLATFORM;

    d_info = devdrv_get_manager_info();

    ka_task_mutex_lock(&d_info->pm_list_lock);
    ka_list_add(&pm->list, &d_info->pm_list_header);
    ka_task_mutex_unlock(&d_info->pm_list_lock);

    return pm;
}
EXPORT_SYMBOL(devdrv_manager_register_pm);

void devdrv_manager_unregister_pm(struct devdrv_pm *pm)
{
    struct devdrv_manager_info *d_info = NULL;

    if (pm == NULL) {
        devdrv_drv_err("invalid input argument.\n");
        return;
    }
    if ((pm->suspend == NULL) || (pm->resume == NULL)) {
        devdrv_drv_err("callback func is null.\n");
        return;
    }

    d_info = devdrv_get_manager_info();

    ka_task_mutex_lock(&d_info->pm_list_lock);
    ka_list_del(&pm->list);
    ka_task_mutex_unlock(&d_info->pm_list_lock);

    dbl_kfree(pm);
    pm = NULL;
}
EXPORT_SYMBOL(devdrv_manager_unregister_pm);

STATIC int devdrv_host_manager_suspend(struct devdrv_info *info)
{
    struct devdrv_manager_info *d_info = NULL;
    ka_list_head_t *pos = NULL, *n = NULL;
    ka_list_head_t *stop = NULL;
    struct devdrv_pm *pm = NULL;
    u32 tsid = 0;
    int ret;

    d_info = devdrv_get_manager_info();

    (void)dms_notifyer_call(DMS_DEVICE_SUSPEND, info);
    tsdrv_set_ts_status(info->dev_id, tsid, TS_SUSPEND);

    ka_task_mutex_lock(&d_info->pm_list_lock);
    ka_list_for_each_safe(pos, n, &d_info->pm_list_header) {
        stop = pos;
        pm = ka_list_entry(pos, struct devdrv_pm, list);
        if (pm->suspend != NULL) {
#ifndef DEVDRV_MANAGER_HOST_UT_TEST
            ret = pm->suspend(info->dev_id);
#endif
            if (ret) {
                ka_task_mutex_unlock(&d_info->pm_list_lock);
                devdrv_drv_err("one suspend callback func failed. devid(%u)\n", info->dev_id);
                goto error;
            }
        }
    }
    ka_task_mutex_unlock(&d_info->pm_list_lock);

    return 0;

error:
    ka_task_mutex_lock(&d_info->pm_list_lock);
    ka_list_for_each_safe(pos, n, &d_info->pm_list_header) {
        pm = ka_list_entry(pos, struct devdrv_pm, list);
        if (pm->resume != NULL) {
            (void)pm->resume(info->dev_id);
        }
        if (pos == stop) {
            break;
        }
    }
    ka_task_mutex_unlock(&d_info->pm_list_lock);

    tsdrv_set_ts_status(info->dev_id, tsid, TS_WORK);
    (void)dms_notifyer_call(DMS_DEVICE_RESUME, info);

    return ret;
}

STATIC int devdrv_host_manager_resume(struct devdrv_info *info)
{
    struct devdrv_manager_info *d_info = NULL;
    ka_list_head_t *pos = NULL, *n = NULL;
    struct devdrv_pm *pm = NULL;
    u32 tsid = 0;

    d_info = devdrv_get_manager_info();

    /* 1. resume devdrv driver */
    /* 2. set ts work */
    tsdrv_set_ts_status(info->dev_id, tsid, TS_WORK);
    /* 3. resume all registered resume callback func */
    ka_task_mutex_lock(&d_info->pm_list_lock);
    ka_list_for_each_safe(pos, n, &d_info->pm_list_header) {
        pm = ka_list_entry(pos, struct devdrv_pm, list);
        if (pm->resume != NULL) {
            (void)pm->resume(info->dev_id);
        }
    }
    ka_task_mutex_unlock(&d_info->pm_list_lock);
    (void)dms_notifyer_call(DMS_DEVICE_RESUME, info);

    return 0;
}

int devdrv_host_manager_device_suspend(struct devdrv_info *dev_info)
{
    return devdrv_host_manager_suspend(dev_info);
}

int devdrv_host_manager_device_resume(struct devdrv_info *dev_info)
{
    return devdrv_host_manager_resume(dev_info);
}

void devdrv_host_manager_device_exception(struct devdrv_info *info)
{
    struct devdrv_manager_info *d_info = NULL;
    ka_list_head_t *pos = NULL, *n = NULL;
    struct devdrv_pm *pm = NULL;
    u32 tsid = 0;

    d_info = devdrv_get_manager_info();
    tsdrv_set_ts_status(info->dev_id, 0, TS_DOWN);

    ka_task_mutex_lock(&d_info->pm_list_lock);
    if (!ka_list_empty_careful(&d_info->pm_list_header)) {
        ka_list_for_each_safe(pos, n, &d_info->pm_list_header) {
            pm = ka_list_entry(pos, struct devdrv_pm, list);
#ifndef DEVDRV_MANAGER_HOST_UT_TEST
            if (pm->ts_status_notify != NULL) {
                (void)pm->ts_status_notify(info->dev_id, TS_DOWN);
            }
#endif
        }
    }
    ka_task_mutex_unlock(&d_info->pm_list_lock);

    devdrv_driver_hardware_exception(info, tsid);
}

STATIC void devdrv_manager_update_ai_info(struct devdrv_aicore_info *aicore_info)
{
    u32 core_bitmap = 0;
    u32 core_count = 0;
    u32 cpu_bitmap = 0;
    u32 cpu_count = 0;
    u32 i, aicore_num_max;
    struct devdrv_info *info = NULL;
    struct devdrv_manager_info *d_info = NULL;
    const struct devdrv_aicore_msg *cq = NULL;

    cq = (struct devdrv_aicore_msg *)aicore_info->exception_info;
    d_info = devdrv_get_manager_info();
    info = d_info->dev_info[aicore_info->dev_id];
    if (info == NULL) {
        devdrv_drv_err("dev_info is NULL. (dev_id=%u)\n", aicore_info->dev_id);
        return ;
    }

    if (cq->aicpu_heart_beat_exception) {
        for (i = 0; i < info->ai_cpu_core_num; i++) {
            if (cq->aicpu_heart_beat_exception & (0x01U << i)) {
                cpu_bitmap |= (0x01U << i);
                if (!(info->inuse.ai_cpu_error_bitmap & (0x01U << i))) {
                    devdrv_drv_err("receive message ai cpu: %d heart beat exception. dev_id(%u)\n", i, info->dev_id);
                }
            } else {
                cpu_count++;
            }
        }
    }

    if (cq->aicore_bitmap) {
        aicore_num_max = info->inuse.ai_core_num + __sw_hweight32(info->inuse.ai_core_error_bitmap);
        if (aicore_num_max > BITS_PER_BYTE * sizeof(u32)) {
            devdrv_drv_err("Aicore num and error bitmap not valid. (dev_id=%u; num=%u; err_bitmap=0x%x)\n",
                info->dev_id, info->inuse.ai_core_num, info->inuse.ai_core_error_bitmap);
            aicore_num_max = BITS_PER_BYTE * sizeof(u32);
        }

        for (i = 0; i < aicore_num_max; i++) {
            if (cq->aicore_bitmap & (0x01U << i)) {
                core_bitmap |= (0x01U << i);
                if (!(info->inuse.ai_core_error_bitmap & (0x01U << i))) {
                    devdrv_drv_err("receive message ai core: %d exception. dev_id(%u)\n", i, info->dev_id);
                }
            } else {
                core_count++;
            }
        }
    }

    if (cq->syspcie_sysdma_status & 0xFFFF) {
        devdrv_drv_err("device(%u) ts sysdma is broken.\n", info->dev_id);
        info->ai_subsys_ip_broken_map |= (0x01U << DEVDRV_AI_SUBSYS_SDMA_WORKING_STATUS_OFFSET);
    }
    /* check the high 16 bits of syspcie_sysdma_status */
    if ((cq->syspcie_sysdma_status >> 16) & 0xFFFF) {
        devdrv_drv_err("device(%u) ts syspcie is broken.\n", info->dev_id);
        info->ai_subsys_ip_broken_map |= (0x01U << DEVDRV_AI_SUBSYS_SPCIE_WORKING_STATUS_OFFSET);
    }

    ka_task_spin_lock_bh(&info->spinlock);
    info->inuse.ai_cpu_num = cpu_count;
    info->inuse.ai_cpu_error_bitmap = cpu_bitmap;
    info->inuse.ai_core_num = core_count;
    info->inuse.ai_core_error_bitmap = core_bitmap;
    ka_task_spin_unlock_bh(&info->spinlock);
}

STATIC int is_cq_invalid(struct devdrv_aicore_msg *cq)
{
    if (cq->ts_status || cq->syspcie_sysdma_status || cq->aicpu_heart_beat_exception || cq->aicore_bitmap) {
        return -EINVAL;
    }
    return 0;
}

STATIC void devdrv_refresh_aicore_info_work(struct work_struct *work)
{
    int ret;
    u32 out_len;
    struct devdrv_manager_msg_info dev_manager_msg_info = {{0}};
    struct devdrv_aicore_msg *h2d_msg = NULL;
    struct devdrv_aicore_info *info = NULL;

    info = container_of(work, struct devdrv_aicore_info, work);

    dev_manager_msg_info.header.msg_id = DEVDRV_MANAGER_CHAN_H2D_RERESH_AICORE_INFO;
    dev_manager_msg_info.header.valid = DEVDRV_MANAGER_MSG_VALID;
    /* give a random value for checking result later */
    dev_manager_msg_info.header.result = (u16)0x1A;
    /* inform corresponding devid to device side */
    dev_manager_msg_info.header.dev_id = info->dev_id;

    ret = devdrv_common_msg_send(info->dev_id, &dev_manager_msg_info, sizeof(struct devdrv_manager_msg_info),
                                 sizeof(struct devdrv_manager_msg_info), &out_len,
                                 DEVDRV_COMMON_MSG_DEVDRV_MANAGER);
    if (ret || dev_manager_msg_info.header.result != 0) {
        devdrv_drv_warn("devdrv_manager_send_msg unsuccessful. (ret=%d; dev_id=%u)\n", ret, info->dev_id);
    }

    h2d_msg = (struct devdrv_aicore_msg *)dev_manager_msg_info.payload;
    if (is_cq_invalid(h2d_msg) != 0) {
        info->exception_info = (void *)h2d_msg;
        devdrv_manager_update_ai_info(info);
    }
}

STATIC enum hrtimer_restart devdrv_refresh_aicore_info(struct hrtimer *t)
{
    struct devdrv_aicore_info *info = NULL;
#ifndef DEVDRV_MANAGER_HOST_UT_TEST

    info = container_of(t, struct devdrv_aicore_info, hrtimer);

    /* 调用新框架的心跳函数判断心跳状态，心跳丢失时要停止发送消息，否则会刷屏 */
    if (dms_heartbeat_is_stop(info->dev_id)) {
        devdrv_drv_info("(Device=%u heart beat is lost, stop send h2d message \n", info->dev_id);
        return HRTIMER_NORESTART;
    }
    ka_task_queue_work(info->aicore_info_wq, &info->work);

    ka_system_hrtimer_forward_now(&info->hrtimer, ka_system_ktime_set(DEVDRV_H2D_CYCLE, 0));
    return HRTIMER_RESTART;
#endif
}

STATIC int devdrv_aicore_info_mem_alloc(u32 dev_id)
{
    g_aicore_info[dev_id] = dbl_kzalloc(sizeof(struct devdrv_aicore_info), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (g_aicore_info[dev_id] == NULL) {
        devdrv_drv_info("Memory alloc for aicore info not success. (device id=%u)\n", dev_id);
        return -ENOMEM;
    }

    return 0;
}

STATIC void devdrv_aicore_info_mem_free(u32 dev_id)
{
    if (g_aicore_info[dev_id] != NULL) {
        dbl_kfree(g_aicore_info[dev_id]);
        g_aicore_info[dev_id] = NULL;
    }
}

int devdrv_refresh_aicore_info_init(u32 dev_id)
{
#ifndef DEVDRV_MANAGER_HOST_UT_TEST
    int ret;

    ret = devdrv_aicore_info_mem_alloc(dev_id);
    if (ret != 0) {
        devdrv_drv_err("Memory alloc for aicore info failed. (device id=%u)\n", dev_id);
        return -ENOMEM;
    }

    g_aicore_info[dev_id]->aicore_info_wq = alloc_ordered_workqueue("%s", WQ_HIGHPRI | WQ_MEM_RECLAIM,
                                                                    "aicore_info_wq");
    KA_TASK_INIT_WORK(&g_aicore_info[dev_id]->work, devdrv_refresh_aicore_info_work);
    g_aicore_info[dev_id]->dev_id = dev_id;

    ka_system_hrtimer_init(&g_aicore_info[dev_id]->hrtimer, KA_CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    g_aicore_info[dev_id]->hrtimer.function = devdrv_refresh_aicore_info;
    ka_system_hrtimer_start(&g_aicore_info[dev_id]->hrtimer,
                            ka_system_ktime_set(DEVDRV_H2D_CYCLE, 0), HRTIMER_MODE_REL);

    g_aicore_info[dev_id]->inited_flag = 1;

    devdrv_drv_info("began hrtimer succ, devid=%u\n", dev_id);
#endif
    return 0;
}

void devdrv_refresh_aicore_info_exit(u32 dev_id)
{
#ifndef DEVDRV_MANAGER_HOST_UT_TEST
    devdrv_drv_info("now to cancel hrtimer, devid=%u\n", dev_id);

    if (g_aicore_info[dev_id] == NULL) {
        return;
    }

    if (g_aicore_info[dev_id]->inited_flag == 0) {
        devdrv_drv_info("No hrtimer, return. (devid=%u)\n", dev_id);
        return;
    }

    ka_system_hrtimer_cancel(&g_aicore_info[dev_id]->hrtimer);
    if (g_aicore_info[dev_id]->aicore_info_wq != NULL) {
        ka_task_destroy_workqueue(g_aicore_info[dev_id]->aicore_info_wq);
        g_aicore_info[dev_id]->aicore_info_wq = NULL;
    }

    devdrv_aicore_info_mem_free(dev_id);
    devdrv_drv_info("cancel hrtimer succ, devid=%u\n", dev_id);

#endif
}

