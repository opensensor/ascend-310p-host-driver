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
#ifndef DEVDRV_IPC_EVENT_H
#define DEVDRV_IPC_EVENT_H

#ifdef CFG_FEATURE_IPC_NOTIFY
#define NOTIFY_MAX_OPEM_TIMES ((int)(~0U >> 1))

void devdrv_manager_ipc_notify_release_recycle(void *context);
int devdrv_manager_ipc_notify_create(void *context, unsigned long arg, void *notify_ioctl_info);
int devdrv_manager_ipc_notify_open(void *context, unsigned long arg, void *notify_ioctl_info);
int devdrv_manager_ipc_notify_close(void *context, void *notify_ioctl_info);
int devdrv_manager_ipc_notify_destroy(void *context, void *notify_ioctl_info);
int devdrv_manager_ipc_notify_set_pid(void *context, void *notify_ioctl_info);
int devdrv_manager_ipc_notify_set_attr(void *context, void *notify_ioctl_info);
int devdrv_manager_ipc_notify_get_info(void *context, unsigned long arg, void *notify_ioctl_info);
int devdrv_manager_ipc_notify_get_attr(void *context, unsigned long arg, void *notify_ioctl_info);
int devdrv_manager_ipc_notify_record(void *context, void *notify_ioctl_info);
#else
static inline void devdrv_manager_ipc_notify_release_recycle(void *context)
{
}
static inline int devdrv_manager_ipc_notify_create(void *context, unsigned long arg, void *notify_ioctl_info)
{
    return 0;
}
static inline int devdrv_manager_ipc_notify_open(void *context, unsigned long arg, void *notify_ioctl_info)
{
    return 0;
}
static inline int devdrv_manager_ipc_notify_close(void *context, void *notify_ioctl_info)
{
    return 0;
}
static inline int devdrv_manager_ipc_notify_destroy(void *context, void *notify_ioctl_info)
{
    return 0;
}
static inline int devdrv_manager_ipc_notify_set_pid(void *context, void *notify_ioctl_info)
{
    return 0;
}
static inline int devdrv_manager_ipc_notify_record(void *context, void *notify_ioctl_info)
{
    return 0;
}
#endif /* CFG_FEATURE_IPC_NOTIFY */
#endif /* DEVDRV_IPC_EVENT_H */
