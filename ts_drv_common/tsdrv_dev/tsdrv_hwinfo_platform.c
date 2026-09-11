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
#include "tsdrv_device.h"
#include "tsdrv_hwinfo.h"
#include "tsdrv_common.h"

/* move to platform later */
STATIC int tsdrv_set_mbox_hwinfo(u32 devid, u32 tsnum, struct devdrv_platform_data *pdata)
{
    struct tsdrv_mbox_hwinfo mbox_hwinfo;
    u32 tsid;
    int err;

    for (tsid = 0; tsid < tsnum; tsid++) {
        mbox_hwinfo.chann_id = TSDRV_TS_MBOX;
        mbox_hwinfo.mbox_tx = pdata->ts_pdata[tsid].ts_mbox_send_vaddr;
        mbox_hwinfo.mbox_rx = pdata->ts_pdata[tsid].ts_mbox_rcv_vaddr;
        mbox_hwinfo.mbox_ack_irq = pdata->ts_pdata[tsid].irq_mailbox_ack;
        mbox_hwinfo.mbox_ack_request_irq = pdata->ts_pdata[tsid].irq_mailbox_ack_request;
        mbox_hwinfo.mbox_rx_irq = pdata->ts_pdata[tsid].irq_mailbox_data_ack;
        mbox_hwinfo.mbox_rx_request_irq = pdata->ts_pdata[tsid].irq_mailbox_data_ack_request;
        mbox_hwinfo.mbox_size = pdata->ts_pdata[tsid].sram_size;
        TSDRV_PRINT_DEBUG("set mbox hwinfo, devid(%u) tsid(%u) chann_id(%d)"
            "mbox_ack_irq(%u) mbox_ack_request_irq(%u) mbox_rx_irq(%d) mbox_rx_request_irq(%d) mbox_size(%u)\n",
            devid, tsid, mbox_hwinfo.chann_id, mbox_hwinfo.mbox_ack_irq, mbox_hwinfo.mbox_ack_request_irq,
            mbox_hwinfo.mbox_rx_irq, mbox_hwinfo.mbox_rx_request_irq, mbox_hwinfo.mbox_size);
        err = tsdrv_set_ts_mbox_hwinfo(devid, tsid, &mbox_hwinfo);
        if (err != 0) {
            return -ENODEV;
        }
    }
    return 0;
}

static int tsdrv_set_dev_ts_db_info(u32 devid, u32 tsnum, struct devdrv_platform_data *pdata)
{
    struct tsdrv_db_info db_info;
    u32 tsid;
    int err;

    for (tsid = 0; tsid < tsnum; tsid++) {
        db_info.db_paddr = pdata->ts_pdata[tsid].doorbell_paddr;
        db_info.db_size = pdata->ts_pdata[tsid].doorbell_size;
        db_info.dbs = pdata->ts_pdata[tsid].doorbell_vaddr;
        db_info.num = TSDRV_DB_NUM;
        db_info.db_stride = DEVDRV_TS_DOORBELL_STRIDE;
        TSDRV_PRINT_DEBUG("(env_type=%d; devid=%u; tsid=%u; dbs=0x%pK; num=%u; db_stride=0x%lx)\n",
            tsdrv_get_env_type(), devid, tsid, db_info.dbs, db_info.num, db_info.db_stride);
        err = tsdrv_set_ts_db_info(devid, tsid, &db_info);
        if (err != 0) {
            return -ENODEV;
        }
#ifdef CFG_SOC_PLATFORM_CLOUD_V2
        db_info.num = TS_DB_NUM_EACH_VF;
        tsdrv_set_mbox_db_info(devid, tsid, &db_info);
#endif
    }

    return 0;
}

static int tsdrv_set_dev_stars_db_info(u32 devid, u32 tsnum, struct devdrv_platform_data *pdata)
{
    struct tsdrv_db_info db_info;
    u32 tsid;
    int err;

    for (tsid = 0; tsid < tsnum; tsid++) {
        if (pdata->ts_pdata[tsid].stars_ctrl_size == 0) {
            continue;
        }
        db_info.db_paddr = pdata->ts_pdata[tsid].stars_ctrl_paddr;
        db_info.db_size = pdata->ts_pdata[tsid].stars_ctrl_size;
        db_info.dbs = pdata->ts_pdata[tsid].stars_ctrl_vaddr;
        db_info.num = TSDRV_STARS_DB_NUM;
        db_info.db_stride = DEVDRV_TS_DOORBELL_STRIDE;
        TSDRV_PRINT_DEBUG("(devid=%u; tsid=%u; dbs=0x%pK; num=%u; db_stride=0x%lx)\n", devid, tsid,
            db_info.dbs, db_info.num, db_info.db_stride);
        err = tsdrv_set_stars_db_info(devid, tsid, &db_info);
        if (err != 0) {
            return -ENODEV;
        }
    }
    return 0;
}

/* move to platform later */
STATIC int tsdrv_set_dev_db_info(u32 devid, u32 tsnum, struct devdrv_platform_data *pdata)
{
    int ret;

    ret = tsdrv_set_dev_ts_db_info(devid, tsnum, pdata);
    if (ret != 0) {
        TSDRV_PRINT_ERR("Failed to set ts doorbell info. (devid=%u)\n", devid);
        return ret;
    }

    ret = tsdrv_set_dev_stars_db_info(devid, tsnum, pdata);
    if (ret != 0) {
        TSDRV_PRINT_ERR("Failed to set stars doorbell info. (devid=%u)\n", devid);
        return ret;
    }

    return 0;
}

STATIC int tsdrv_set_dev_sq_hwinfo(u32 devid, u32 tsnum, struct devdrv_platform_data *pdata)
{
    struct tsdrv_sq_hwinfo sq_hwinfo = {0};
    int dev_index;
    u32 tsid;
    int err;

    dev_index = tsdrv_get_device_index(devid);
    if (dev_index < 0) {
        TSDRV_PRINT_ERR("get_device_index failed, devid(%u), device_index(%d)\n", devid, dev_index);
        return -EINVAL;
    }

    for (tsid = 0; tsid < tsnum; tsid++) {
        sq_hwinfo.bar_addr = pdata->ts_pdata[tsid].ts_sq_static_addr;
#ifdef CFG_SOC_PLATFORM_CLOUD_V2
        /* sq reserve mem only use on device, it needs to consider chipid and dieid */
        sq_hwinfo.paddr = DEVDRV_RESERVE_MEM_BASE + ((long)(unsigned)tsid * DEVDRV_RESERVE_MEM_SIZE) +
                      ((long)(unsigned)dev_index * DIE_BASEADDR_PA_OFFSET);
#else
        /* host sq reserve mem distribution: ts0 sq resv mem(32M) + ts1 sq resv mem(32M) */
        /* dc on host, ts1 sq resv mem not used for normal sqcq, it is use for host functional sqcq mem */
        sq_hwinfo.paddr = DEVDRV_RESERVE_MEM_BASE + ((long)(unsigned)tsid * DEVDRV_RESERVE_MEM_SIZE) +
                      ((long)(unsigned)dev_index * CHIP_BASEADDR_PA_OFFSET);
#endif
        sq_hwinfo.size = DEVDRV_RESERVE_MEM_SIZE;

        TSDRV_PRINT_DEBUG("set dev sq hwinfo, devid(%u %u) tsid(%u) bar_addr(0x%pK) paddr(0x%pK) size(%lu)\n",
            devid, dev_index, tsid, (void *)(uintptr_t)sq_hwinfo.bar_addr,
            (void *)(uintptr_t)sq_hwinfo.paddr, sq_hwinfo.size);
        err = tsdrv_set_ts_sq_hwinfo(devid, tsid, &sq_hwinfo);
        if (err != 0) {
            return -ENODEV;
        }
    }
    return 0;
}

STATIC int tsdrv_set_dev_cq_hwinfo(u32 devid, u32 tsnum, struct devdrv_platform_data *pdata)
{
    struct tsdrv_cq_hwinfo cq_hwinfo;
    u32 irq_id;
    u32 tsid;
    int err;

    for (tsid = 0; tsid < tsnum; tsid++) {
        cq_hwinfo.cq_irq_num = 0;
        TSDRV_PRINT_DEBUG("set dev cq hwinfo, cq_hwinfo.cq_irq_num(%u)\n", cq_hwinfo.cq_irq_num);
        for (irq_id = 0; irq_id < DEVDRV_CQ_IRQ_NUM; irq_id++) {
            if (pdata->ts_pdata[tsid].irq_cq_update_request[irq_id] == 0) {
                break;
            }
            cq_hwinfo.cq_irq[irq_id] = pdata->ts_pdata[tsid].irq_cq_update[irq_id];
            cq_hwinfo.cq_request_irq[irq_id] = pdata->ts_pdata[tsid].irq_cq_update_request[irq_id];
            cq_hwinfo.cq_irq_num++;
            TSDRV_PRINT_DEBUG("cqid[%u]=%u, cq_request_irq[%u]=%u\n", irq_id, cq_hwinfo.cq_irq[irq_id], irq_id,
                cq_hwinfo.cq_request_irq[irq_id]);
        }
        TSDRV_PRINT_INFO("devid %u, tsid %u, cq_irq_num %u \n", devid, tsid, cq_hwinfo.cq_irq_num);
        cq_hwinfo.dfx_cq_irq = pdata->ts_pdata[tsid].irq_functional_cq;
        cq_hwinfo.dfx_cq_request_irq = pdata->ts_pdata[tsid].irq_functional_cq_request;
        TSDRV_PRINT_DEBUG("dfx_cq_irq=%u dfx_cq_request_irq=%u\n", cq_hwinfo.dfx_cq_irq, cq_hwinfo.dfx_cq_request_irq);
        err = tsdrv_set_ts_cq_hwinfo(devid, tsid, &cq_hwinfo);
        if (err != 0) {
            return -ENODEV;
        }
    }
    return 0;
}

STATIC int tsdrv_set_dev_tsfw_hwinfo(u32 devid, u32 tsnum, struct devdrv_platform_data *pdata)
{
    struct tsdrv_tsfw_hwinfo tsfw_hwinfo;
    u32 tsid;

    for (tsid = 0; tsid < tsnum; tsid++) {
        TSDRV_PRINT_DEBUG("tsfw load tsid(%u) vaddr(0x%pK) size(0x%llx)\n", tsid,
            pdata->ts_pdata[tsid].ts_load_addr, (u64)DEVDRV_TS_MEMORY_SIZE);
        tsfw_hwinfo.vaddr = pdata->ts_pdata[tsid].ts_load_addr;
        tsfw_hwinfo.size = DEVDRV_TS_MEMORY_SIZE;
        (void)tsdrv_set_tsfw_hwinfo(devid, tsid, &tsfw_hwinfo);
    }

    return 0;
}

int tsdrv_set_dev_hwinfo(struct devdrv_info *dev_info)
{
    u32 devid = dev_info->dev_id;
    u32 tsnum = dev_info->ts_num;
    int err;

    tsdrv_set_dev_p(devid, dev_info->dev);
    err = tsdrv_set_dev_tsnum(devid, tsnum);
    if (err != 0) {
        return -ENODEV;
    }
    err = tsdrv_set_dev_tsfw_hwinfo(devid, tsnum, dev_info->pdata);
    if (err != 0) {
        return -ENODEV;
    }
    err = tsdrv_set_mbox_hwinfo(devid, tsnum, dev_info->pdata);
    if (err != 0) {
        return -ENODEV;
    }
    err = tsdrv_set_dev_db_info(devid, tsnum, dev_info->pdata);
    if (err != 0) {
        return -ENODEV;
    }
    err = tsdrv_set_dev_sq_hwinfo(devid, tsnum, dev_info->pdata);
    if (err != 0) {
        return -ENODEV;
    }
    err = tsdrv_set_dev_cq_hwinfo(devid, tsnum, dev_info->pdata);
    if (err != 0) {
        return -ENODEV;
    }
    return 0;
}
#else
void tsdrv_hwinfo_platform(void)
{
    return;
}
#endif
