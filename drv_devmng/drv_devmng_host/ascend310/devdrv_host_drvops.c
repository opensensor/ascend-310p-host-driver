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

#include <linux/slab.h>
#include <linux/time.h>

#include "devdrv_common.h"
#include "devdrv_manager_msg.h"
#ifndef DEVMNG_UT
#include "devdrv_manager_common.h"
#include "devdrv_ipc_msg.h"
#include "devdrv_platform_resource.h"
#include "devdrv_interface.h"

STATIC int devdrv_manager_host_irq_trigger(u32 dev_id, u32 irq_num)
{
    struct devdrv_info *dev_info = devdrv_manager_get_devdrv_info(dev_id);
    struct devdrv_platform_data *pdata = NULL;
    static u32 read_done_value = 0;
    static u32 value = 0;
    u32 *doorbell = NULL;
    u8 *addr = NULL;

    if (dev_info == NULL) {
        devdrv_drv_err("dev info is null!\n");
        return -EINVAL;
    }

    pdata = dev_info->pdata;
    if (irq_num == DEVDRV_MAILBOX_SEND_OFFLINE_IRQ) {
        addr = pdata->ts_pdata[0].doorbell_vaddr;
        addr = addr + (unsigned long)(510 + DEVDRV_TS_DOORBELL_SQ_NUM) * DEVDRV_TS_DOORBELL_STRIDE;
        doorbell = (u32 *)addr;
        *doorbell = value; // 0x3A;
        value++;
    } else if (irq_num == DEVDRV_MAILBOX_SEND_ONLINE_IRQ) {
        addr = pdata->ts_pdata[0].doorbell_vaddr;
        addr = addr + (unsigned long)(510 + DEVDRV_TS_DOORBELL_SQ_NUM - 16) * DEVDRV_TS_DOORBELL_STRIDE;
        doorbell = (u32 *)addr;
        *doorbell = value; // 0x3A;
        value++;
    } else if (irq_num == DEVDRV_MAILBOX_READ_DONE_IRQ) {
        addr = pdata->ts_pdata[0].doorbell_vaddr;
        addr = addr + (unsigned long)(511 + DEVDRV_TS_DOORBELL_SQ_NUM) * DEVDRV_TS_DOORBELL_STRIDE;
        doorbell = (u32 *)addr;
        *doorbell = read_done_value; // 0x3A;
        read_done_value++;
    }
    mb();

    return 0;
}

STATIC void devdrv_manager_host_flush_cache(u64 base, u32 len) {}

STATIC int devdrv_memcpy_to_device_sq(u32 dev_id, u64 dst, u64 src, u32 buf_len)
{
    int ret;

    ret = devdrv_dma_sync_copy(dev_id, DEVDRV_DMA_DATA_COMMON, src, dst, buf_len, DEVDRV_DMA_HOST_TO_DEVICE);
    if (ret) {
        devdrv_drv_err("devdrv_dma_sync_copy error !\n");
        return -ENOMEM;
    }

    return 0;
}
#else
int devdrv_manager_host_irq_trigger(u32 dev_id, u32 irq_num) {}
void devdrv_manager_host_flush_cache(u64 base, u32 len) {}
int devdrv_memcpy_to_device_sq(u32 dev_id, u64 dst, u64 src, u32 buf_len) {}
#endif

struct tsdrv_drv_ops devdrv_host_drv_ops = {
    .alloc_mem = NULL, // devdrv_manager_alloc_device_mem,
    .free_mem = NULL,  // devdrv_manager_free_device_mem,
    .svm_va_to_devid = NULL,
    .flush_cache = devdrv_manager_host_flush_cache,
    .irq_trigger = devdrv_manager_host_irq_trigger,

    .get_dev_info = devdrv_manager_h2d_sync_get_devinfo,
    .memcpy_to_device_sq = devdrv_memcpy_to_device_sq,
};
