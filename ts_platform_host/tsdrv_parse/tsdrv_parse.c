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
#include "tsdrv_log.h"
#include "tsdrv_parse.h"
#include "tsdrv_irq_parse.h"
#include "tsdrv_addr_parse.h"

int tsdrv_parse_init(u32 devid, struct devdrv_info *dev_info)
{
    u32 tsid = 0;
    int err;

    if (dev_info == NULL || dev_info->pdata == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid dev_info, devid=%u\n", devid);
        return -ENOMEM;
#endif
    }
    err = tsdrv_irq_parse_init(devid, tsid, dev_info);
    if (err != 0) {
#ifndef TSDRV_UT
        return -ENODEV;
#endif
    }
    err = tsdrv_addr_parse_init(devid, tsid, dev_info);
    if (err != 0) {
#ifndef TSDRV_UT
        goto err_addr_parse_init;
#endif
    }
    return 0;
#ifndef TSDRV_UT
err_addr_parse_init:
    tsdrv_irq_parse_exit(devid, tsid, dev_info);
    return -ENODEV;
#endif
}

void tsdrv_parse_exit(u32 devid, struct devdrv_info *dev_info)
{
    u32 tsid = 0;

    tsdrv_addr_parse_exit(devid, tsid, dev_info);
    tsdrv_irq_parse_exit(devid, tsid, dev_info);
}

