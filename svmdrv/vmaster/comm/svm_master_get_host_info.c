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

#include "devdrv_manager_comm.h"
#include "svm_proc_mng.h"
#include "svm_cmd.h"
#include "svm_master_get_host_info.h"

STATIC int devmm_get_device_accounting_pids(u32 docker_id, u32 phy_devid, u32 vfid, int *pids, u32 cnt)
{
    int got_cnt = 0;

    if (phy_devid >= SVM_MAX_AGENT_NUM) {
        devmm_drv_err("Invalid devid. (devid=%u)\n", phy_devid);
        return -ENODEV;
    }
    if (vfid >= DEVMM_MAX_VF_NUM) {
        devmm_drv_err("Invalid vfid. (vfid=%u)\n", vfid);
        return -ENODEV;
    }
    if (cnt == 0) {
        devmm_drv_err("Cnt is 0.\n");
        return -EINVAL;
    }
    if (pids == NULL) {
        devmm_drv_err("Pids is NULL.\n");
        return -EINVAL;
    }

    got_cnt = devmm_get_hostpid_by_docker_id(docker_id, phy_devid, vfid, pids, cnt);
    devmm_drv_debug("Get_device_accounting_pids. (docker_id=%u; phy_devid=%u; vfid=%u; cnt=%u; got_cnt=%d)\n",
        docker_id, phy_devid, vfid, cnt, got_cnt);
    return got_cnt;
}

void devmm_register_query_func(void)
{
    (void)devdrv_manager_get_process_pids_register(devmm_get_device_accounting_pids);
}

void devmm_unregister_query_func(void)
{
    devdrv_manager_get_process_pids_unregister();
}
