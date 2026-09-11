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

#include "ascend_hal_define.h"
#include "svm_master_addr_map.h"
#include "devmm_channel.h"
#include "svm_msg_client.h"
#include "svm_master_dev_capability.h"

STATIC int devmm_send_map_addr_msg(struct devmm_svm_process *svm_process,
    struct devmm_map_dev_reserve_para *addr_para, u32 devid, u32 vfid)
{
    struct devmm_chan_map_dev_reserve chan_map_addr = {{{0}}};
    u32 addr_type = addr_para->addr_type;
    uint64_t addr = addr_para->va;
    int ret;

    chan_map_addr.addr_type = addr_type;
    chan_map_addr.va = addr;
    chan_map_addr.head.process_id.hostpid = svm_process->process_id.hostpid;
    chan_map_addr.head.process_id.vfid = (u16)vfid;
    chan_map_addr.head.dev_id = (u16)devid;
    chan_map_addr.head.msg_id = DEVMM_CHAN_MAP_DEV_RESERVE_H2D_ID;
    ret = devmm_chan_msg_send(&chan_map_addr, sizeof(struct devmm_chan_map_dev_reserve),
        sizeof(struct devmm_chan_map_dev_reserve));
    if (ret != 0) {
        devmm_drv_err("Map address failed. (ret=%d; addr_type=%u; devid=%u; vfid=%u; addr=0x%llx)\n",
            ret, addr_type, devid, vfid, addr);
        return ret;
    }

    addr_para->va = chan_map_addr.va;
    addr_para->len = chan_map_addr.len;

    devmm_drv_debug("Check output. (va=0x%llx; len=%lld)\n", addr_para->va, addr_para->len);
    return 0;
}

STATIC int devmm_map_dev_reserve(struct devmm_svm_process *svm_process, struct devmm_map_dev_reserve_para *addr_para,
    u32 devid, u32 vfid)
{
    u32 addr_type = addr_para->addr_type;
    int ret;

    if (addr_type >= ADDR_MAP_TYPE_MAX) {
        devmm_drv_err("Addr_type is invalid. (addr_type=%u; devid=%u)\n", addr_type, devid);
        return -EINVAL;
    }

    if (((addr_type == ADDR_MAP_TYPE_REG_AIC_CTRL) || (addr_type == ADDR_MAP_TYPE_REG_AIC_PMU_CTRL)) &&
        (devmm_dev_capability_support_aic_reg_map(devid) == false)) {
        devmm_drv_run_info("Aic reg map capability might had been closed. (addr_type=%u; devid=%u)\n", addr_type, devid);
        return -EOPNOTSUPP;
    }

    ret = devmm_send_map_addr_msg(svm_process, addr_para, devid, vfid);
    if (ret != 0) {
        devmm_drv_err("Send map address message failed. (addr_type=%u; devid=%u)\n", addr_type, devid);
        return ret;
    }

    return 0;
}

int devmm_ioctl_map_dev_reserve(struct devmm_svm_process *svm_process, struct devmm_ioctl_arg *arg)
{
    struct devmm_map_dev_reserve_para *addr_para = &arg->data.map_dev_reserve_para;
    u32 devid = arg->head.devid;
    u32 vfid = arg->head.vfid;
    int ret;

    ret = devmm_map_dev_reserve(svm_process, addr_para, devid, vfid);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_err_if((ret != -EOPNOTSUPP), "Map device reserve address failed. (devid=%u; vfid=%u)\n", devid, vfid);
#endif
        return ret;
    }

    return 0;
}

