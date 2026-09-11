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

#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/reboot.h>

#include "dbl/uda.h"
#include "devmm_common.h"
#include "svm_hot_reset.h"
#include "ka_dfx_pub.h"

#define DEVMM_IS_READY 1

STATIC struct svm_business_info *g_svm_business_info = NULL;
STATIC bool g_svm_active_reboot = false;

bool devmm_is_active_reboot_status(void)
{
    return g_svm_active_reboot;
}

int devmm_alloc_business_info(void)
{
    u32 i;

    g_svm_business_info = devmm_kvzalloc(sizeof(struct svm_business_info));
    if (g_svm_business_info == NULL) {
        devmm_drv_err("Devmm_kvzalloc svm_business_info fail.\n");
        return -ENOMEM;
    }

    for (i = 0; i < DEVMM_MAX_DEVICE_NUM; i++) {
        ka_task_init_rwsem(&g_svm_business_info->business_ref_cnt_rw_sema[i]);
        KA_INIT_LIST_HEAD(&g_svm_business_info->pid_info[i].list);
    }
    ka_task_init_rwsem(&g_svm_business_info->stop_business_rw_sema);

    return 0;
}

void devmm_free_business_info(void)
{
    if (g_svm_business_info != NULL) {
        devmm_kvfree(g_svm_business_info);
        g_svm_business_info = NULL;
    }
}

static bool _devmm_get_stop_business_flag(u32 devid)
{
    return (uda_is_phy_dev(devid) == true) ?
        ((g_svm_business_info->stop_business_flag & (1UL << devid)) != 0) : false;
}

bool devmm_get_stop_business_flag(u32 devid)
{
    bool result;
    ka_task_down_read(&g_svm_business_info->stop_business_rw_sema);
    result = _devmm_get_stop_business_flag(devid);
    ka_task_up_read(&g_svm_business_info->stop_business_rw_sema);
    return result;
}

static void devmm_set_stop_business_flag(u32 devid)
{
    if (uda_is_phy_dev(devid) == true) {
        ka_task_down_write(&g_svm_business_info->stop_business_rw_sema);
        g_svm_business_info->stop_business_flag |= 1 << devid;
        ka_task_up_write(&g_svm_business_info->stop_business_rw_sema);
    }
}

static void devmm_clear_stop_business_flag(u32 devid)
{
    if (uda_is_phy_dev(devid) == true) {
        ka_task_down_write(&g_svm_business_info->stop_business_rw_sema);
        g_svm_business_info->stop_business_flag &= ~(1 << devid);
        ka_task_up_write(&g_svm_business_info->stop_business_rw_sema);
    }
}

STATIC int devmm_add_pid_into_business_inner(u32 devid, ka_pid_t pid)
{
    struct svm_business_pid_info *business_pid_info_of_list = NULL;
    struct svm_business_pid_info *business_pid_info = NULL;
    ka_list_head_t *head = NULL;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;

    business_pid_info = devmm_kvzalloc(sizeof(struct svm_business_pid_info));
    if (business_pid_info == NULL) {
        devmm_drv_err("Devmm_kvzalloc svm business info failed.\n");
        return -ENOMEM;
    }

    ka_task_down_write(&g_svm_business_info->business_ref_cnt_rw_sema[devid]);
    /* Traverse the list to check whether the PID exists. If the PID does not exist, add it. Otherwise, exit. */
    head = &g_svm_business_info->pid_info[devid].list;
    ka_list_for_each_safe(pos, n, head) {
        business_pid_info_of_list = ka_list_entry(pos, struct svm_business_pid_info, list);
        if (business_pid_info_of_list->pid == pid) {
            goto existed_pid_info;
        }
    }

    business_pid_info->pid = pid;
    ka_list_add(&business_pid_info->list, head);
    g_svm_business_info->business_ref_cnt[devid]++;
    ka_task_up_write(&g_svm_business_info->business_ref_cnt_rw_sema[devid]);
    devmm_drv_debug("Add current pid info to svm_business_info. (devid=%u; pid=%d)\n", devid, pid);
    return 0;
existed_pid_info:
    ka_task_up_write(&g_svm_business_info->business_ref_cnt_rw_sema[devid]);
    devmm_kvfree(business_pid_info);
    return 0;
}

int devmm_add_pid_into_business(u32 devid, ka_pid_t pid)
{
    int ret;

    ka_task_down_read(&g_svm_business_info->stop_business_rw_sema);
    if (_devmm_get_stop_business_flag(devid)) {
        ka_task_up_read(&g_svm_business_info->stop_business_rw_sema);
        devmm_drv_err("Hotreset stop flag has been set, the process will return. (devid=%u; pid=%d)\n",
                      devid, pid);
        return -EBUSY;
    }

    ret = devmm_add_pid_into_business_inner(devid, pid);
    ka_task_up_read(&g_svm_business_info->stop_business_rw_sema);
    if (ret != 0) {
        devmm_drv_err("Add current pid info to svm_business_info failed. (devid=%u; pid=%d)\n", devid, pid);
    }

    return ret;
}

void devmm_remove_pid_from_business(u32 devid, ka_pid_t pid)
{
    struct svm_business_pid_info *business_pid_info = NULL;
    ka_list_head_t *head = NULL;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;

    if(devid >= DEVMM_MAX_DEVICE_NUM) {
        return;
    }

    ka_task_down_write(&g_svm_business_info->business_ref_cnt_rw_sema[devid]);
    head = &g_svm_business_info->pid_info[devid].list;
    ka_list_for_each_safe(pos, n, head) {
        business_pid_info = ka_list_entry(pos, struct svm_business_pid_info, list);
        if (business_pid_info->pid == pid) {
            ka_list_del(pos);
            g_svm_business_info->business_ref_cnt[devid]--;
            devmm_drv_debug("Remove pid info from svm_business_info. (devid=%u; pid=%d; business_refcount=%u)\n",
                devid, pid, g_svm_business_info->business_ref_cnt[devid]);
            ka_task_up_write(&g_svm_business_info->business_ref_cnt_rw_sema[devid]);
            devmm_kvfree(business_pid_info);
            return;
        }
    }
    ka_task_up_write(&g_svm_business_info->business_ref_cnt_rw_sema[devid]);
}

void devmm_remove_pid_from_all_business(ka_pid_t pid)
{
    u32 stamp = (u32)ka_jiffies;
    u32 dev_id;

    for (dev_id = 0; dev_id < DEVMM_MAX_DEVICE_NUM; dev_id++) {
        devmm_remove_pid_from_business(dev_id, pid);
        devmm_try_cond_resched(&stamp);
    }
}

void devmm_svm_business_info_init(u32 devid)
{
    /* Restore the hot reset flag of the device that has completed the hot reset. */
    devmm_clear_stop_business_flag(devid);

    /* set bit is ready */
    ka_base_atomic_set(&g_svm_business_info->devmm_is_ready[devid], DEVMM_IS_READY);

    devmm_drv_debug(
        "Restore stop business flag when pcie probe callback svm init_instance. (devid=%u)\n", devid);
    return;
}

void devmm_svm_business_info_uninit(u32 devid)
{
    devmm_set_stop_business_flag(devid);

    /* clear bit of ready */
    ka_base_atomic_set(&g_svm_business_info->devmm_is_ready[devid], 0);

    devmm_drv_info("Set stop business flag when pcie remove callback svm uninit_instance. (devid=%u)\n", devid);
    return;
}

#define WAIT_BUSINESS_FINISH_TRY_CNT          600
#define WAIT_BUSINESS_FINISH_PER_TIME     1 /* 1s */

bool devmm_wait_business_finish(u32 devid)
{
    u32 i;

    devmm_svm_business_info_uninit(devid);

    for (i = 0; i < WAIT_BUSINESS_FINISH_TRY_CNT; i++) {
        if (devmm_is_active_reboot_status()) {
#ifndef EMU_ST
            devmm_drv_info("Is active reboot status, no wait. (devid=%u)\n", devid);
            return false;
#endif
        }
        ka_task_down_read(&g_svm_business_info->business_ref_cnt_rw_sema[devid]);
        if (g_svm_business_info->business_ref_cnt[devid] == 0) {
            ka_task_up_read(&g_svm_business_info->business_ref_cnt_rw_sema[devid]);
            devmm_drv_info("Business finish. (devid=%u; i=%u)\n", devid, i);
            return true;
        }
        ka_task_up_read(&g_svm_business_info->business_ref_cnt_rw_sema[devid]);
#ifndef EMU_ST
        devmm_drv_debug("Try wait business finish. (devid=%u; i=%u)\n", devid, i);
        ka_system_ssleep(WAIT_BUSINESS_FINISH_PER_TIME);
#endif
    }

    devmm_drv_err("Business not finish. (devid=%u)\n", devid);
    return false;
}

int devmm_hotreset_pre_handle(u32 dev_id)
{
    u32 business_ref_cnt;

    if (ka_base_atomic_read(&g_svm_business_info->devmm_is_ready[dev_id]) == 0) {
        devmm_drv_err("Devmm is not ready. (devid=%u)\n", dev_id);
        return -EBUSY;
    }

    devmm_set_stop_business_flag(dev_id);
    ka_task_down_read(&g_svm_business_info->business_ref_cnt_rw_sema[dev_id]);
    business_ref_cnt = g_svm_business_info->business_ref_cnt[dev_id];
    ka_task_up_read(&g_svm_business_info->business_ref_cnt_rw_sema[dev_id]);
    if (business_ref_cnt != 0) {
        devmm_clear_stop_business_flag(dev_id);
        devmm_drv_err("Business count of device is not 0. (devid=%u; business_ref_cnt=%u)\n", dev_id, business_ref_cnt);
        return -EBUSY;
    }

    devmm_drv_info("No devmm business running in device. (devid=%u)\n", dev_id);
    return 0;
}

int devmm_hotreset_cancel_handle(u32 dev_id)
{
    devmm_clear_stop_business_flag(dev_id);
    devmm_drv_info("Hotreset stop business flag is canceled. (devid=%u)\n", dev_id);
    return 0;
}

STATIC int devmm_reboot_notify_handle(ka_notifier_block_t *notifier, unsigned long event, void *data)
{
#ifndef EMU_ST
    if (event != SYS_RESTART && event != SYS_HALT && event != SYS_POWER_OFF) {
        return KA_NOTIFY_DONE;
    }

    g_svm_active_reboot = true;
#endif

    return NOTIFY_OK;
}

STATIC ka_notifier_block_t devmm_reboot_notifier = {
    .notifier_call = devmm_reboot_notify_handle,
    .priority = 1,
};

int devmm_register_reboot_notifier(void)
{
    return ka_dfx_register_reboot_notifier(&devmm_reboot_notifier);
}

void devmm_unregister_reboot_notifier(void)
{
    (void)ka_dfx_unregister_reboot_notifier(&devmm_reboot_notifier);
}

