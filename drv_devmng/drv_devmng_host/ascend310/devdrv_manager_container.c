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

#include <linux/random.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/cred.h>
#include <linux/nsproxy.h>

#include "devdrv_manager.h"
#include "devdrv_manager_common.h"
#include "devdrv_manager_container.h"
#include "devdrv_user_common.h"
#include "devdrv_common.h"
#include "devdrv_manager_msg.h"
#include "devdrv_manager_container_noplugin.h"


/*
 * container device assignment mode
 */
extern u32 devmng_run_mode;

int devdrv_is_in_container(void)
{
    u64 current_mnt;
    u64 host_mnt;
    int is_in;
    int ret;

    ret = devdrv_manager_get_current_mnt_ns(&current_mnt);
    if (ret) {
        devdrv_drv_err("get current mnt ns error, ret = %d\n", ret);
        return -EINVAL;
    }
    host_mnt = devdrv_manager_get_host_mnt_ns();
    if (host_mnt == 0) {
        is_in = false;
    } else if (!devdrv_manager_container_is_admin() && current_mnt != host_mnt) {
        is_in = true;
    } else {
        is_in = false;
    }

    return is_in;
}
EXPORT_SYMBOL(devdrv_is_in_container);

int devdrv_manager_container_is_in_container(void)
{
    return devdrv_is_in_container();
}
EXPORT_SYMBOL(devdrv_manager_container_is_in_container);

int devdrv_virtual_id_to_physical_id(u32 virtual_id, u32 *physical_id)
{
    u32 vfid = 0;
    if (physical_id == NULL) {
        devdrv_drv_err("output para is null.\n");
        return -1;
    }

    return devdrv_manager_container_logical_id_to_physical_id(virtual_id, physical_id, &vfid);
}
EXPORT_SYMBOL(devdrv_virtual_id_to_physical_id);


/*
 * ioctl process
 */
STATIC int (*CONST devdrv_manager_container_process_handler[DEVDRV_CONTAINER_MAX_CMD])(
    struct devdrv_container_para *cmd) = {
        [DEVDRV_CONTAINER_GET_DAVINCI_DEVLIST] = devdrv_manager_container_get_davinci_devlist,
        [DEVDRV_CONTAINER_GET_BARE_PID] = devdrv_manager_container_get_bare_pid,
        [DEVDRV_CONTAINER_GET_BARE_TGID] = devdrv_manager_container_get_bare_tgid,
    };

int devdrv_manager_container_process(struct file *filep, unsigned long arg)
{
    struct devdrv_container_para container_cmd;
    int ret;

    if (!arg || filep == NULL || filep->private_data == NULL) {
        devdrv_drv_err("arg = %lu, filep = %pK\n", arg, filep);
        return -1;
    }

    ret = copy_from_user_safe(&container_cmd, (void *)((uintptr_t)arg), sizeof(struct devdrv_container_para));
    if (ret) {
        devdrv_drv_err("copy_from_user return error: %d.\n", ret);
        return -1;
    }

    if (container_cmd.para.cmd >= DEVDRV_CONTAINER_MAX_CMD) {
        devdrv_drv_err("invalid input container process cmd: %d.\n", container_cmd.para.cmd);
        return -1;
    }

    if (devdrv_manager_container_process_handler[container_cmd.para.cmd] == NULL) {
        devdrv_drv_err("not supported cmd: %d.\n", container_cmd.para.cmd);
        return -EINVAL;
    }

    ret = devdrv_manager_container_process_handler[container_cmd.para.cmd](&container_cmd);

    return ret;
}
