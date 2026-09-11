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
#include "devdrv_common.h"
#include "devdrv_id.h"
#include "devdrv_cqsq.h"
#include "tsdrv_drvops.h"
#include "tsdrv_device.h"
#include "tsdrv_hwinfo.h"
#include "tsdrv_sync.h"
#include "devdrv_interface.h"

STATIC int tsdrv_host_irq_trigger(u32 dev_id, u32 tsid)
{
    struct tsdrv_db_hwinfo_t *db_hwinfo = NULL;
    enum tsdrv_env_type env_type;
    static u32 value = 0;
    u32 *doorbell = NULL;

    env_type = tsdrv_get_env_type();
    if (env_type == TSDRV_ENV_OFFLINE) {
        db_hwinfo = tsdrv_get_db_hwinfo_t(dev_id, tsid, TSDRV_OFFLINE_MBOX_SEND_DB);
        if (db_hwinfo == NULL) {
            return -EFAULT;
        }
        doorbell = (u32 *)db_hwinfo->dbs;
        *doorbell = value;  // 0x3A;
        value++;
    } else if (env_type == TSDRV_ENV_ONLINE) {
        db_hwinfo = tsdrv_get_db_hwinfo_t(dev_id, tsid, TSDRV_ONLINE_MBOX_SEND_DB);
        if (db_hwinfo == NULL) {
            return -EFAULT;
        }
        doorbell = (u32 *)db_hwinfo->dbs;
        *doorbell = value;  // 0x3A;
        value++;
        TSDRV_PRINT_DEBUG("Online doorbell. (devid=%u; dbs=0x%pK; value=%u)\n", dev_id, db_hwinfo->dbs, *doorbell);
    }
    ka_mb();

    return 0;
}

STATIC void tsdrv_host_flush_cache(u64 base, u32 len)
{
}

STATIC int tsdrv_memcpy_to_device_sq(u32 dev_id, u64 dst, u64 src, u32 buf_len)
{
    int ret;

    ret = devdrv_dma_sync_copy(dev_id, DEVDRV_DMA_DATA_COMMON, src, dst, buf_len, DEVDRV_DMA_HOST_TO_DEVICE);
    if (ret != 0) {
        TSDRV_PRINT_ERR("devdrv_dma_sync_copy failed, ret(%d). dev_id(%u)\n", ret, dev_id);
        return -ENOMEM;
    }
    return 0;
}

struct tsdrv_drv_ops tsdrv_host_drv_ops = {
    .flush_cache = tsdrv_host_flush_cache,
    .irq_trigger = tsdrv_host_irq_trigger,

    .add_notify_msg_chan = devdrv_add_notify_msg_chan,
    .add_id_msg_chan = devdrv_add_id_msg_chan,

    .memcpy_to_device_sq = tsdrv_memcpy_to_device_sq,
#ifndef TSDRV_UT
    .hdc_register_symbol = hdcdrv_register_symbol_from_tsdrv,
    .hdc_unregister_symbol = hdcdrv_unregister_symbol_from_tsdrv,
#endif
};

struct tsdrv_drv_ops *tsdrv_get_drv_ops(void)
{
    return &tsdrv_host_drv_ops;
}

