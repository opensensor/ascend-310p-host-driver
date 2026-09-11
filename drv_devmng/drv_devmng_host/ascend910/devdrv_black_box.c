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

#include "ka_memory_pub.h"
#include "ka_task_pub.h"
#include "ka_list_pub.h"

#include "devdrv_common.h"
#include "devdrv_manager_common.h"
#include "dbl_mem_alloc_interface.h"
#include "devdrv_manager.h"
#include "devdrv_interface.h"
#include "devdrv_black_box.h"
#include "tsdrv_status.h"

#define DEVDRV_BLACK_BOX_MAX_EXCEPTION_NUM 1024

STATIC int devdrv_host_pcie_add_exception(u32 pci_devid, u32 code, struct timespec stamp)
{
    struct devdrv_manager_info *manager_info = devdrv_get_manager_info();

    if (manager_info == NULL) {
        devdrv_drv_err("device manager is not inited. dev_id(%u)\n", pci_devid);
        return -EFAULT;
    }

    if (pci_devid < DEVDRV_MAX_DAVINCI_NUM) {
        if (code == DEVDRV_SYSTEM_START_FAIL) {
            manager_info->device_status[pci_devid] = DRV_STATUS_COMMUNICATION_LOST;
        }
        return devdrv_host_black_box_add_exception(pci_devid, code, stamp, NULL);
    }

    devdrv_drv_err("no such device, pci device id: %u.\n", pci_devid);
    return -ENODEV;
}

void devdrv_host_black_box_init(void)
{
    int i, ret;
    struct devdrv_manager_info *manager_info = devdrv_get_manager_info();
    struct devdrv_black_callback black_callback;
    unsigned long flags;

    if (manager_info == NULL) {
        devdrv_drv_err("device manager is not inited.\n");
        return;
    }

    ka_task_spin_lock_init(&manager_info->black_box.spinlock);
    ka_task_spin_lock_irqsave(&manager_info->black_box.spinlock, flags);
    for (i = 0; i < MAX_EXCEPTION_THREAD; i++) {
        ka_task_sema_init(&manager_info->black_box.black_box_sema[i], 0);
        manager_info->black_box.exception_num[i] = 0;
        manager_info->black_box.black_box_pid[i] = 0;
        KA_INIT_LIST_HEAD(&manager_info->black_box.exception_list[i]);
    }
    manager_info->black_box.thread_should_stop = 0;
    ka_task_spin_unlock_irqrestore(&manager_info->black_box.spinlock, flags);

    black_callback.callback = devdrv_host_pcie_add_exception;

    ret = devdrv_register_black_callback(&black_callback);
    if (ret) {
        devdrv_drv_err("devdrv_register_black_callback failed.\n");
    }
}

void devdrv_host_black_box_exit(void)
{
    uint32_t i;
    struct devdrv_manager_info *manager_info = devdrv_get_manager_info();
    struct devdrv_black_callback black_callback;
    struct devdrv_exception *exception = NULL;
    ka_list_head_t *pos = NULL, *n = NULL;
    unsigned long flags;

    if (manager_info == NULL) {
        devdrv_drv_err("invalid.\n");
        return;
    }

    black_callback.callback = devdrv_host_pcie_add_exception;
    devdrv_unregister_black_callback(&black_callback);

    ka_task_spin_lock_irqsave(&manager_info->black_box.spinlock, flags);
    manager_info->black_box.thread_should_stop = 1;
    for (i = 0; i < MAX_EXCEPTION_THREAD; i++) {
        ka_list_for_each_safe(pos, n, &manager_info->black_box.exception_list[i])
        {
            exception = list_entry(pos, struct devdrv_exception, list);
            ka_list_del(&exception->list);
            if (exception->data != NULL) {
                dbl_kfree(exception->data);
                exception->data = NULL;
            }
            dbl_kfree(exception);
            exception = NULL;
        }
        manager_info->black_box.exception_num[i] = 0;
        manager_info->black_box.black_box_pid[i] = 0;
        ka_task_up(&manager_info->black_box.black_box_sema[i]);
    }
    ka_task_spin_unlock_irqrestore(&manager_info->black_box.spinlock, flags);
}

void devdrv_host_black_box_close_check(pid_t pid)
{
    struct devdrv_manager_info *manager_info = devdrv_get_manager_info();
    struct devdrv_exception *exception = NULL;
    ka_list_head_t *pos = NULL, *n = NULL;
    unsigned long flags;
    int i;

    if (manager_info == NULL) {
        devdrv_drv_err("invalid.\n");
        return;
    }

    ka_task_spin_lock_irqsave(&manager_info->black_box.spinlock, flags);
    for (i = 0; i < MAX_EXCEPTION_THREAD; i++) {
        if (manager_info->black_box.black_box_pid[i] == pid) {
            manager_info->black_box.black_box_pid[i] = 0;
            ka_list_for_each_safe(pos, n, &manager_info->black_box.exception_list[i])
            {
                exception = list_entry(pos, struct devdrv_exception, list);
                ka_list_del(&exception->list);
                if (exception->data != NULL) {
                    dbl_kfree(exception->data);
                    exception->data = NULL;
                }
                dbl_kfree(exception);
                exception = NULL;
            }
            manager_info->black_box.exception_num[i] = 0;
            ka_task_up(&manager_info->black_box.black_box_sema[i]);
        }
    }
    ka_task_spin_unlock_irqrestore(&manager_info->black_box.spinlock, flags);
}

STATIC struct devdrv_exception *devdrv_host_black_box_alloc_exception_data(u32 devid, u32 code,
    struct timespec stamp, const void *data)
{
    int ret = 0;
    struct devdrv_exception *exception = NULL;

    exception = dbl_kzalloc(sizeof(struct devdrv_exception), KA_GFP_ATOMIC | __KA_GFP_ACCOUNT);
    if (exception == NULL) {
        devdrv_drv_err_spinlock("alloc exception node failed. devid(%u)\n", devid);
        return NULL;
    }

    exception->devid = devid;
    exception->code = code;
    exception->stamp = stamp;

    if ((exception->code == DEVDRV_BB_DEVICE_ID_INFORM) && (data != NULL)) {
        exception->data = dbl_kzalloc(sizeof(struct devdrv_black_box_devids), KA_GFP_ATOMIC | __KA_GFP_ACCOUNT);
        if (exception->data == NULL) {
            dbl_kfree(exception);
            exception = NULL;
            devdrv_drv_err_spinlock("alloc exception data failed.\n");
            return NULL;
        }
        ret = memcpy_s(exception->data, sizeof(struct devdrv_black_box_devids),
                       data, sizeof(struct devdrv_black_box_devids));
    } else if ((exception->code == DEVDRV_BB_DEVICE_STATE_INFORM) && (data != NULL)) {
        exception->data = dbl_kzalloc(sizeof(struct devdrv_black_box_state_info), KA_GFP_ATOMIC | __KA_GFP_ACCOUNT);
        if (exception->data == NULL) {
            dbl_kfree(exception);
            exception = NULL;
            devdrv_drv_err_spinlock("alloc exception data failed.\n");
            return NULL;
        }
        ret = memcpy_s(exception->data, sizeof(struct devdrv_black_box_state_info),
                       data, sizeof(struct devdrv_black_box_state_info));
    } else {
        exception->data = NULL;
    }
    if (ret) {
        devdrv_drv_err_spinlock("memcpy_s exception data failed ret = %d.\n", ret);
        dbl_kfree(exception->data);
        exception->data = NULL;
        dbl_kfree(exception);
        exception = NULL;
        return NULL;
    }

    return exception;
}

/**
 * @data: must be alloced by user
 *
 */
int devdrv_host_black_box_add_exception(u32 devid, u32 code,
    struct timespec stamp, const void *data)
{
    unsigned long flags;
    uint32_t i;
    u32 more = 1;
    struct devdrv_manager_info *manager_info = devdrv_get_manager_info();
    struct devdrv_exception *exception = NULL;
    struct devdrv_exception *old = NULL;

    if ((devid >= DEVDRV_MAX_DAVINCI_NUM) || (manager_info == NULL)) {
        devdrv_drv_err("Invalid dev_id. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    for (i = 0; i < MAX_EXCEPTION_THREAD; i++) {
        ka_task_spin_lock_irqsave(&manager_info->black_box.spinlock, flags);
        exception = devdrv_host_black_box_alloc_exception_data(devid, code, stamp, data);
        if (exception == NULL) {
            ka_task_spin_unlock_irqrestore(&manager_info->black_box.spinlock, flags);
            devdrv_drv_err("devdrv_host_black_box_alloc_exception_data failed. (dev_id=%u)\n", devid);
            return -ENOMEM;
        }

        if (manager_info->black_box.exception_num[i] >= DEVDRV_BLACK_BOX_MAX_EXCEPTION_NUM) {
            old = ka_list_first_entry(&manager_info->black_box.exception_list[i], struct devdrv_exception, list);
            ka_list_del(&old->list);
            if (old->data != NULL) {
                dbl_kfree(old->data);
                old->data = NULL;
            }
            dbl_kfree(old);
            old = NULL;
            more = 0;
            manager_info->black_box.exception_num[i]--;
        }
        ka_list_add_tail(&exception->list, &manager_info->black_box.exception_list[i]);
        manager_info->black_box.exception_num[i]++;
        ka_task_spin_unlock_irqrestore(&manager_info->black_box.spinlock, flags);
        devdrv_drv_debug("black_box_add_exception. (code=0x%x; dev_id=%d; black_box.exception_num[%d]=%d)\n",
            exception->code, exception->devid, i, manager_info->black_box.exception_num[i]);
        if (more) {
            ka_task_up(&manager_info->black_box.black_box_sema[i]);
        }
    }

    return 0;
}

STATIC void devdrv_host_black_box_device_id_process(struct devdrv_black_box_user *black_box_user,
                                                    struct devdrv_exception *exception)
{
    struct devdrv_black_box_devids *bbox_devids = NULL;
    u32 dev_num;
    u32 i;

    bbox_devids = (struct devdrv_black_box_devids *)exception->data;
    if (bbox_devids == NULL) {
        devdrv_drv_err("exception->data is NULL\n");
        return;
    }
    dev_num = bbox_devids->dev_num;
    if (dev_num > DEVDRV_MAX_DAVINCI_NUM) {
        devdrv_drv_err("invalid dev_num(%u).\n", dev_num);
        return;
    }

    devdrv_drv_info("*** bbox inform dev_num = %d ***\n", dev_num);

    black_box_user->priv_data.bbox_devids.dev_num = dev_num;
    for (i = 0; i < dev_num; i++) {
        black_box_user->priv_data.bbox_devids.devids[i] = bbox_devids->devids[i];
    }
}

STATIC void devdrv_host_black_box_status_info_process(struct devdrv_black_box_user *black_box_user,
                                                      struct devdrv_exception *exception)
{
    struct devdrv_black_box_state_info *bbox_state_info = NULL;

    bbox_state_info = (struct devdrv_black_box_state_info *)exception->data;
    if (bbox_state_info == NULL) {
        devdrv_drv_err("bbox state info is NULL.\n");
        return;
    }

    if ((bbox_state_info->devId >= DEVDRV_MAX_DAVINCI_NUM) || (bbox_state_info->state >= STATE_TO_MAX)) {
        devdrv_drv_err("dev id(%u) or state(%u) is out of range.\n", bbox_state_info->devId, bbox_state_info->state);
        return;
    }

    devdrv_drv_info("status is inform through pcie driver with"
                    "dev id(%u), state(%u), code(0x%x).\n",
                    bbox_state_info->devId, (u32)bbox_state_info->state, black_box_user->exception_code);
    black_box_user->priv_data.bbox_state.state = bbox_state_info->state;
    black_box_user->priv_data.bbox_state.devId = bbox_state_info->devId;
}

STATIC void devdrv_host_black_box_priv_data_process(struct devdrv_black_box_user *black_box_user,
                                                    struct devdrv_exception *exception)
{
    switch (black_box_user->exception_code) {
        case DEVDRV_BB_DEVICE_ID_INFORM:
            devdrv_host_black_box_device_id_process(black_box_user, exception);
            break;
        case DEVDRV_BB_DEVICE_STATE_INFORM:
            devdrv_host_black_box_status_info_process(black_box_user, exception);
            break;
        default:
            break;
    }
    return;
}

void devdrv_host_black_box_get_exception(struct devdrv_black_box_user *black_box_user, int index)
{
    unsigned long flags;
    int ret;
    struct devdrv_manager_info *manager_info = devdrv_get_manager_info();
    struct devdrv_exception *exception = NULL;

    if ((black_box_user == NULL) || (manager_info == NULL)) {
        devdrv_drv_err("Invalid input. (black_box_user_is_null=%d)\n", black_box_user == NULL ? 1 : 0);
        return;
    }

    ka_task_spin_lock_irqsave(&manager_info->black_box.spinlock, flags);
    if (manager_info->black_box.thread_should_stop) {
        black_box_user->thread_should_stop = 1;
        ka_task_spin_unlock_irqrestore(&manager_info->black_box.spinlock, flags);
        devdrv_drv_err("Thread should stop.\n");
        return;
    }

    ret = memset_s(black_box_user, sizeof(struct devdrv_black_box_user), 0, sizeof(struct devdrv_black_box_user));
    if (ret != 0) {
        ka_task_spin_unlock_irqrestore(&manager_info->black_box.spinlock, flags);
        devdrv_drv_err("Set black_box_user to 0 failed. (ret=%d)\n", ret);
        return;
    }

    if (!ka_list_empty_careful(&manager_info->black_box.exception_list[index])) {
        exception = ka_list_first_entry(&manager_info->black_box.exception_list[index], struct devdrv_exception, list);

        black_box_user->thread_should_stop = 0;
        black_box_user->devid = exception->devid;
        black_box_user->exception_code = exception->code;
        black_box_user->tv_sec = exception->stamp.tv_sec;
        black_box_user->tv_nsec = exception->stamp.tv_nsec;

        ka_list_del(&exception->list);
        manager_info->black_box.exception_num[index]--;
        ka_task_spin_unlock_irqrestore(&manager_info->black_box.spinlock, flags);

        devdrv_host_black_box_priv_data_process(black_box_user, exception);
        devdrv_drv_warn("Exception code: 0x%x.\n", exception->code);
        if (exception->data != NULL) {
            dbl_kfree(exception->data);
            exception->data = NULL;
        }
        devdrv_drv_debug("black_box_get_exception. (code=0x%x; dev_id=%d; black_box.pid=%d)\n",
            exception->code, exception->devid, manager_info->black_box.black_box_pid[index]);
        dbl_kfree(exception);
        exception = NULL;
        return;
    }
    ka_task_spin_unlock_irqrestore(&manager_info->black_box.spinlock, flags);
}
