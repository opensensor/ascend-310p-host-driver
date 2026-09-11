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
#include "tsdrv_parse.h"
#include "tsdrv_irq_parse.h"
#include "devdrv_interface.h"

enum {
    MBOX_IRQ = 0,
    DFX_SQCQ_IRQ = 1,
    PHY_SQCQ_IRQ_START = 2,
};

int tsdrv_irq_parse_init(u32 devid, u32 tsid, struct devdrv_info *dev_info)
{
    struct devdrv_platform_data *pdata = NULL;
    u32 phy_cq_irq_index = (u32)PHY_SQCQ_IRQ_START;
    u32 phy_cq_index;
    u32 irq_request;
    u32 irq;
    int err;

    pdata = dev_info->pdata;
    err = devdrv_get_ts_drv_irq_vector_id(devid, MBOX_IRQ, &irq);
    if (err != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("get mbox irq fail, devid=%u err=%d\n", devid, err);
        return -ENODEV;
#endif
    }
    pdata->ts_pdata[tsid].irq_mailbox_ack = irq;

    err = devdrv_get_irq_vector(devid, irq, &irq_request);
    if (err != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("get mbox irq fail, err=%d\n", err);
        return -ENODEV;
#endif
    }
    pdata->ts_pdata[tsid].irq_mailbox_ack_request = irq_request;

    err = devdrv_get_ts_drv_irq_vector_id(devid, DFX_SQCQ_IRQ, &irq);
    if (err != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("get dfx sqcq irq fail, devid=%u err=%d\n", devid, err);
        return -ENODEV;
#endif
    }
    pdata->ts_pdata[tsid].irq_functional_cq = irq;

    err = devdrv_get_irq_vector(devid, irq, &irq_request);
    if (err != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("get dfx sqcq irq fail, err=%d\n", err);
        return -ENODEV;
#endif
    }
    pdata->ts_pdata[tsid].irq_functional_cq_request = irq_request;

    for (phy_cq_index = 0; phy_cq_index < DEVDRV_CQ_IRQ_NUM; phy_cq_index++, phy_cq_irq_index++) {
        err = devdrv_get_ts_drv_irq_vector_id(devid, phy_cq_irq_index, &irq);
        if (err != 0) {
            pdata->ts_pdata[tsid].irq_cq_update[phy_cq_index] = 0;
            break;
        }
        pdata->ts_pdata[tsid].irq_cq_update[phy_cq_index] = irq;

        err = devdrv_get_irq_vector(devid, irq, &irq_request);
        if (err != 0) {
            TSDRV_PRINT_ERR("get dfx sqcq irq fail, err=%d\n", err);
            return -ENODEV;
        }
        pdata->ts_pdata[tsid].irq_cq_update_request[phy_cq_index] = irq_request;
    }

    TSDRV_PRINT_INFO("devid=%u, tsid=%u, mbox_irq=%u, dfx_sqcq_irq=%u, cq_base_irq=%u, cq_irq_num=%u\n",
        devid, tsid, pdata->ts_pdata[tsid].irq_mailbox_ack, pdata->ts_pdata[tsid].irq_functional_cq,
        pdata->ts_pdata[tsid].irq_cq_update[0], phy_cq_index);

    pdata->ts_pdata[tsid].cq_irq_num = phy_cq_index;
    return 0;
}

void tsdrv_irq_parse_exit(u32 devid, u32 tsid, struct devdrv_info *dev_info)
{
}
#else
int tsdrv_irq_parse_ascend310(void)
{
    return 0;
}
#endif
