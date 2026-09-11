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
#ifndef TSDRV_KERNEL_UT
#include "tsdrv_set_runtime_conflict_check.h"
#include "devdrv_driver_pm.h"
#include "devdrv_common.h"
#include "tsdrv_device.h"

#define DEVDRV_AICPU_MONITOR_NAME "aicpu_m_ipc_"

static inline int devdrv_is_aicpumonitor_open(u32 devid)
{
    enum tsdrv_env_type env_type = tsdrv_get_env_type();
    if ((env_type == TSDRV_ENV_OFFLINE) &&
        !ka_base_strncmp(current->comm, DEVDRV_AICPU_MONITOR_NAME, ka_base_strlen(DEVDRV_AICPU_MONITOR_NAME))) {
        return 0;
    }
    return -EINVAL;
}

void tsdrv_set_runtime_run_init(u32 devid)
{
}

STATIC int devdrv_is_devcdev_open(u32 devid)
{
    enum tsdrv_env_type env_type = tsdrv_get_env_type();
    u64 get_status = 0;
    u64 local_status;

    local_status = (env_type == TSDRV_ENV_ONLINE) ? DEVDRV_RUNTIME_ON_HOST : DEVDRV_RUNTIME_ON_DEVICE;

    if (devdrv_get_runtime_runningplat(devid, &get_status)) {
        TSDRV_PRINT_ERR("dev(%u) get runtime status faild, plat type:%d, status:0x%llx.\n", devid,
            (int)env_type, get_status);
        return -ENODEV;
    }

    if (get_status == local_status) {
        return 0;
    } else if (get_status == DEVDRV_RUNTIME_AVAILABLE) {
        if (devdrv_set_runtime_runningplat(devid, local_status)) {
            TSDRV_PRINT_ERR("dev(%u) set runtime status faild, plat_type:%d, local status:0x%llx.\n", devid,
                (int)env_type, local_status);
            return -ENODEV;
        }
        TSDRV_PRINT_DEBUG("dev(%u) set runtime status succ, plat_type:%d, local status:0x%llx.\n", devid,
            (int)env_type, local_status);
        return 0;
    }

    TSDRV_PRINT_WARN("dev(%u) runtime is running on other plat, cannot be open at sametime, "
        "plat_type:%d, get status:0x%llx, local status:0x%llx.\n",
        devid, (int)env_type, get_status, local_status);
    return -EAGAIN;
}

int tsdrv_set_runtime_run_conflict_check(u32 devid)
{
    int ret;

    if (devdrv_is_aicpumonitor_open(devid) == 0) {
        TSDRV_PRINT_INFO("/dev/davinci%u is opened by aicpu monitor.\n", devid);
        return 0;
    }
    ret = devdrv_is_devcdev_open(devid);
    return ret;
}

int tsdrv_set_runtime_available_conflict_check(u32 devid)
{
    u32 ctx_sum_num = 0;
    u32 i;

    for (i = 0; i < TSDRV_MAX_FID_NUM; i++) {
        ctx_sum_num += tsdrv_dev_get_ctx_num(devid, i);
    }

    if (ctx_sum_num != 0 || (devdrv_is_aicpumonitor_open(devid) == 0)) {
        TSDRV_PRINT_INFO("dev(%u) ctx_num(%u).\n", devid, ctx_sum_num);
        return 0;
    }

    // no runtime process running in the system, so set running plat to available
    if (devdrv_set_runtime_runningplat(devid, DEVDRV_RUNTIME_AVAILABLE)) {
        TSDRV_PRINT_ERR("dev(%u) devdrv_set_runtime_runningplat() faild.\n", devid);
        return -ENODEV;
    }
    TSDRV_PRINT_DEBUG("dev(%u) set runtime avaliable succ, plat_type:%d.\n", devid,
        (int)tsdrv_get_env_type());
    return 0;
}
#else
int tsdrv_set_runtime_conflict_check_ascend310(void)
{
    return 0;
}
#endif
