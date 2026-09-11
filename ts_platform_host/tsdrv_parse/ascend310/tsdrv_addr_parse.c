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
#include "tsdrv_log.h"
#include "devdrv_interface.h"
#include "tsdrv_addr_parse.h"

static int tsdrv_get_db_addr(u32 devid, u64 *db_paddr, size_t *db_size)
{
    int err;

    err = devdrv_get_addr_info(devid, DEVDRV_ADDR_TS_DOORBELL, 0, db_paddr, db_size);
    if (err != 0) {
        TSDRV_PRINT_ERR("get db addr fail, devid=%u err=%d\n", devid, err);
    }
    return err;
}

static int tsdrv_get_sram_addr(u32 devid, u64 *sram_paddr, size_t *sram_size)
{
    int err;

    err = devdrv_get_addr_info(devid, DEVDRV_ADDR_TS_SRAM, 0, sram_paddr, sram_size);
    if (err != 0) {
        TSDRV_PRINT_ERR("get sram addr fail, devid=%u err=%d\n", devid, err);
    }
    return err;
}

static int tsdrv_get_sq_addr(u32 devid, u64 *sq_paddr, size_t *sq_size)
{
    int err;

    err = devdrv_get_addr_info(devid, DEVDRV_ADDR_TS_SQ_BASE, 0, sq_paddr, sq_size);
    if (err != 0) {
        TSDRV_PRINT_ERR("get sq addr fail, devid=%u err=%d\n", devid, err);
    }
    return err;
}

int tsdrv_addr_parse_init(u32 devid, u32 tsid, struct devdrv_info *dev_info)
{
    struct devdrv_platform_data *pdata = NULL;
    int err;

    pdata = dev_info->pdata;
    err = tsdrv_get_db_addr(devid, &pdata->ts_pdata[tsid].doorbell_paddr, &pdata->ts_pdata[tsid].doorbell_size);
    if (err != 0) {
#ifndef TSDRV_UT
        return err;
#endif
    }

    err = tsdrv_get_sram_addr(devid, &pdata->ts_pdata[tsid].sram_paddr, &pdata->ts_pdata[tsid].sram_size);
    if (err != 0) {
#ifndef TSDRV_UT
        return err;
#endif
    }

    err = tsdrv_get_sq_addr(devid, &pdata->ts_pdata[tsid].ts_sq_static_addr, &pdata->ts_pdata[tsid].ts_sq_static_size);
    if (err != 0) {
#ifndef TSDRV_UT
        return err;
#endif
    }

    pdata->ts_pdata[tsid].doorbell_vaddr = ka_mm_ioremap(pdata->ts_pdata[tsid].doorbell_paddr,
        pdata->ts_pdata[tsid].doorbell_size);
    if (pdata->ts_pdata[tsid].doorbell_vaddr == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("ioremap db addr fail, devid=%u tsid=%u\n", devid, tsid);
        goto err_ioremap_db_addr;
#endif
    }

    pdata->ts_pdata[tsid].ts_mbox_send_vaddr = ka_mm_ioremap(pdata->ts_pdata[tsid].sram_paddr, DEVDRV_MAILBOX_PAYLOAD_LENGTH);
    if (pdata->ts_pdata[tsid].ts_mbox_send_vaddr == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("ioremap mbox send addr fail, devid=%u tsid=%u\n", devid, tsid);
        goto err_ioremap_mbox_send_addr;
#endif
    }
    pdata->ts_pdata[tsid].ts_mbox_send_paddr = pdata->ts_pdata[tsid].sram_paddr;
    pdata->ts_pdata[tsid].ts_mbox_send_size = DEVDRV_MAILBOX_PAYLOAD_LENGTH;

    pdata->ts_pdata[tsid].ts_mbox_rcv_vaddr = ka_mm_ioremap(pdata->ts_pdata[tsid].sram_paddr + DEVDRV_MAILBOX_PAYLOAD_LENGTH,
        DEVDRV_MAILBOX_PAYLOAD_LENGTH);
    if (pdata->ts_pdata[tsid].ts_mbox_rcv_vaddr == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("ioremap mbox rcv addr fail, devid=%u tsid=%u\n", devid, tsid);
        goto err_ioremap_mbox_rcv_addr;
#endif
    }
    pdata->ts_pdata[tsid].ts_mbox_rcv_paddr = pdata->ts_pdata[tsid].sram_paddr + DEVDRV_MAILBOX_PAYLOAD_LENGTH;
    pdata->ts_pdata[tsid].ts_mbox_rcv_size = DEVDRV_MAILBOX_PAYLOAD_LENGTH;
    return 0;
#ifndef TSDRV_UT
err_ioremap_mbox_rcv_addr:
    pdata->ts_pdata[tsid].ts_mbox_rcv_paddr = 0;
    pdata->ts_pdata[tsid].ts_mbox_rcv_size = 0;

    ka_mm_iounmap(pdata->ts_pdata[tsid].ts_mbox_send_vaddr);
    pdata->ts_pdata[tsid].ts_mbox_send_vaddr = NULL;
    pdata->ts_pdata[tsid].ts_mbox_send_paddr = 0;
    pdata->ts_pdata[tsid].ts_mbox_send_size = 0;
err_ioremap_mbox_send_addr:
    ka_mm_iounmap(pdata->ts_pdata[tsid].doorbell_vaddr);
    pdata->ts_pdata[tsid].doorbell_vaddr = NULL;
err_ioremap_db_addr:
    pdata->ts_pdata[tsid].doorbell_paddr = 0;
    pdata->ts_pdata[tsid].doorbell_size  = 0;

    pdata->ts_pdata[tsid].sram_paddr = 0;
    pdata->ts_pdata[tsid].sram_size = 0;

    pdata->ts_pdata[tsid].ts_sq_static_addr = 0;
    pdata->ts_pdata[tsid].ts_sq_static_size = 0;
    return -ENODEV;
#endif
}

int tsdrv_addr_parse_exit(u32 devid, u32 tsid, struct devdrv_info *dev_info)
{
    struct devdrv_platform_data *pdata = dev_info->pdata;

    if ((void __iomem *)(uintptr_t)pdata->ts_pdata[tsid].ts_mbox_rcv_vaddr != NULL) {
        ka_mm_iounmap((void __iomem *)(uintptr_t)pdata->ts_pdata[tsid].ts_mbox_rcv_vaddr);
        pdata->ts_pdata[tsid].ts_mbox_rcv_vaddr = NULL;
    }

    if (pdata->ts_pdata[tsid].ts_mbox_send_vaddr != NULL) {
        ka_mm_iounmap(pdata->ts_pdata[tsid].ts_mbox_send_vaddr);
        pdata->ts_pdata[tsid].ts_mbox_send_vaddr = NULL;
    }

    if (pdata->ts_pdata[tsid].doorbell_vaddr != NULL) {
        ka_mm_iounmap(pdata->ts_pdata[tsid].doorbell_vaddr);
        pdata->ts_pdata[tsid].doorbell_vaddr = NULL;
    }

    pdata->ts_pdata[tsid].ts_mbox_rcv_paddr = 0;
    pdata->ts_pdata[tsid].ts_mbox_rcv_size = 0;

    pdata->ts_pdata[tsid].ts_mbox_send_paddr = 0;
    pdata->ts_pdata[tsid].ts_mbox_send_size = 0;

    pdata->ts_pdata[tsid].doorbell_paddr = 0;
    pdata->ts_pdata[tsid].doorbell_size  = 0;

    pdata->ts_pdata[tsid].sram_paddr = 0;
    pdata->ts_pdata[tsid].sram_size = 0;

    pdata->ts_pdata[tsid].ts_sq_static_addr = 0;
    pdata->ts_pdata[tsid].ts_sq_static_size = 0;

    return 0;
}
#else
int tsdrv_addr_parse_ascend310(void)
{
    return 0;
}
#endif
