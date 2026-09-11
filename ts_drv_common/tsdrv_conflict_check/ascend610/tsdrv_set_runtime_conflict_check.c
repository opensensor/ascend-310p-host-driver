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
#include <linux/spinlock.h>

#include "devdrv_driver_pm.h"
#include "devdrv_common.h"
#include "tsdrv_device.h"
#include "tsdrv_set_runtime_conflict_check.h"

ka_task_spinlock_t tsdrv_run_lock[TSDRV_MAX_DAVINCI_NUM];
ka_atomic_t tsdrv_run_set_refs[TSDRV_MAX_DAVINCI_NUM];
void tsdrv_set_runtime_run_init(u32 devid)
{
    ka_task_spin_lock_init(&tsdrv_run_lock[devid]);
    ka_base_atomic_set(&tsdrv_run_set_refs[devid], 0);
}

int tsdrv_set_runtime_run_conflict_check(u32 devid)
{
    /* means it is mdc */
#if defined(CFG_SOC_PLATFORM_MDC) || defined(CFG_SOC_PLATFORM_HELPER) || defined(CFG_SOC_PLATFORM_MINIV3)
    TSDRV_PRINT_INFO("enter func.");
    return 0;
#else /* means it is dc */
    u64 local_status, remoted_status;
    u64 get_status = 0;
    int ret = 0;

    local_status = (tsdrv_get_env_type() == TSDRV_ENV_ONLINE) ? DEVDRV_RUNTIME_ON_HOST : DEVDRV_RUNTIME_ON_DEVICE;
    remoted_status = (tsdrv_get_env_type() == TSDRV_ENV_ONLINE) ? DEVDRV_RUNTIME_ON_DEVICE : DEVDRV_RUNTIME_ON_HOST;

    ka_task_spin_lock(&tsdrv_run_lock[devid]);
    if (devdrv_get_runtime_runningplat(devid, &get_status)) {
#ifndef TSDRV_UT
        ka_task_spin_unlock(&tsdrv_run_lock[devid]);
        TSDRV_PRINT_ERR("Get runtime status faild. (devid=%u; local_status=0x%llx; get_status=0x%llx)\n",
            devid, local_status, get_status);
        return -ENODEV;
#endif
    }

    if (get_status == local_status) {
        ret = 0;
    } else if (get_status == DEVDRV_RUNTIME_AVAILABLE) {
        if (devdrv_set_runtime_runningplat(devid, local_status)) {
#ifndef TSDRV_UT
            ka_task_spin_unlock(&tsdrv_run_lock[devid]);
            TSDRV_PRINT_ERR("Set runtime status faild. (devid=%u; plat_type=%d; local_status=0x%llx)\n",
                devid, (int)tsdrv_get_env_type(), local_status);
            return -ENODEV;
#endif
        }

        ret = 0;
    } else if (get_status == remoted_status) {
#ifndef TSDRV_UT
        ka_task_spin_unlock(&tsdrv_run_lock[devid]);
        TSDRV_PRINT_WARN("Runtime running on the other side. (local_status=0x%llx; get_status=0x%llx)\n",
            local_status, get_status);
        return -EAGAIN;
#endif
    } else {
        ka_task_spin_unlock(&tsdrv_run_lock[devid]);
        TSDRV_PRINT_WARN("Get invalid status. (devid=%u; local_status=0x%llx, get_status=0x%llx)\n",
            devid, local_status, get_status);
        return -EINVAL;
    }

    ka_base_atomic_inc(&tsdrv_run_set_refs[devid]);
    ka_task_spin_unlock(&tsdrv_run_lock[devid]);
    TSDRV_PRINT_DEBUG("Run refs after open. (devid=%u; refs=%u)\n", devid, atomic_read(&tsdrv_run_set_refs[devid]));

    return ret;
#endif /* CFG_SOC_PLATFORM_MDC */
}

int tsdrv_set_runtime_available_conflict_check(u32 devid)
{
    /* means it is mdc */
#if defined(CFG_SOC_PLATFORM_MDC) || defined(CFG_SOC_PLATFORM_HELPER) || defined(CFG_SOC_PLATFORM_MINIV3)
    return 0;
#else /* means it is dc */

    ka_task_spin_lock(&tsdrv_run_lock[devid]);
    if (ka_base_atomic_read(&tsdrv_run_set_refs[devid]) == 0) {  /* Vm、Container not need set */
        ka_task_spin_unlock(&tsdrv_run_lock[devid]);
        TSDRV_PRINT_DEBUG("Do not need set running status. (devid=%u)\n", devid);
        return 0;
    }

    if (ka_base_atomic_dec_and_test(&tsdrv_run_set_refs[devid])) {
        /* no runtime process running in the system, so set running plat to available */
        if (devdrv_set_runtime_runningplat(devid, DEVDRV_RUNTIME_AVAILABLE)) {
            ka_task_spin_unlock(&tsdrv_run_lock[devid]);
            TSDRV_PRINT_ERR("Set runtime run plat faild. (devid=%u)\n", devid);
            return -ENODEV;
        }
    }
    ka_task_spin_unlock(&tsdrv_run_lock[devid]);
    TSDRV_PRINT_DEBUG("Run refs after close. (devid=%u; refs=%u)\n", devid, atomic_read(&tsdrv_run_set_refs[devid]));

    return 0;
#endif /* CFG_SOC_PLATFORM_MDC */
}
