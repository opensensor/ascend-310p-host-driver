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

#include <linux/delay.h>
#ifndef AOS_LLVM_BUILD
#include <linux/irq.h>
#endif
#include <linux/module.h>

#include "devdrv_common.h"
#include "tsdrv_log.h"
#include "devdrv_interface.h"
#include "devdrv_devinit.h"
#include "tsdrv_osal_intr.h"
#include "user_cfg_interface.h"

#ifdef CFG_SOC_PLATFORM_HELPER
int tsdrv_get_cpu_index_range(u32 devid, u32 *first_ccpu, u32 *last_ccpu)
{
    dev_cpu_nums_cfg_t cpu_info;
    int i, ret;

    *first_ccpu = 0;
    *last_ccpu = 0;
    for (i = 0; i < devid; i++) {
        ret = dev_user_cfg_get_cpu_number(i, &cpu_info);    // this interface is supplied by devmng for helper
        if (ret != 0) {
            TSDRV_PRINT_ERR("Failed to get cpu info. devid(%u), ret(%u)", devid, ret);
            return -EINVAL;
        }
        *first_ccpu += cpu_info.aicpu_num * cpu_info.aicpu_os_sched + cpu_info.ccpu_num * cpu_info.ccpu_os_sched
            + cpu_info.dcpu_num * cpu_info.dcpu_os_sched;
    }

    ret = dev_user_cfg_get_cpu_number(devid, &cpu_info);
    if (ret != 0) {
        TSDRV_PRINT_ERR("Failed to calc total cpu num. devid(%u), ret(%u)", devid, ret);
        return -EINVAL;
    }

    *last_ccpu = *first_ccpu + cpu_info.ccpu_num * cpu_info.ccpu_os_sched;
    /* ctrl cpu range per dev:[first_ccpu, last_ccpu) */

    return 0;
}
#endif

int tsdrv_request_irq(u32 dev_id, int vector,
    ka_irqreturn_t (*callback_func)(int, void *), void *para, const char *name)
{
    int ret = 0;

#ifdef CFG_MANAGER_HOST_ENV
    ret = devdrv_register_irq_func_expand(dev_id, vector, callback_func, para, name);
#else
    if (tsdrv_get_pfvf_type_by_devid(dev_id) == DEVDRV_SRIOV_TYPE_PF) {
        ret = ka_system_request_irq(vector, callback_func, 0, name, para);
    }
#endif
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Register irq failed. (devid=%u; ret=%d)\n", dev_id, ret);
#endif
        return -EINVAL;
    }

    return 0;
}

int tsdrv_unrequest_irq(u32 dev_id, int vector, void *para)
{
#ifdef CFG_MANAGER_HOST_ENV
    int ret = devdrv_unregister_irq_func_expand(dev_id, vector, para);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Unregister irq failed. (devid=%u; ret=%d)\n", dev_id, ret);
#endif
        return -EINVAL;
    }
#else
    if (tsdrv_get_pfvf_type_by_devid(dev_id) == DEVDRV_SRIOV_TYPE_PF) {
        (void)ka_system_free_irq(vector, para);
    }
#endif

    return 0;
}

void tsdrv_irq_set_affinity_hint(u32 devid, int vector, u32 first_cpu_index, u32 last_cpu_index)
{
    int i;

    for (i = first_cpu_index; i < last_cpu_index; i++) {
        (void)ka_base_irq_set_affinity_hint(vector, ka_system_get_cpu_mask(i));
    }
    TSDRV_PRINT_INFO("Bind core. (devid=%u; irq=%d; first_cpu_index=%u; last_cpu_index=%u)\n",
        devid, vector, first_cpu_index, last_cpu_index);
}

