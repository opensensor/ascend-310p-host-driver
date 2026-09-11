/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2025. All rights reserved.
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

#ifndef VTSDRV_INIT_H
#define VTSDRV_INIT_H

#include <linux/cdev.h>

#include "vtsdrv_common.h"

typedef unsigned long long phys_addr_t;
#define PCI_VENDOR_ID_HUAWEI 0x19e5

struct vtsdrv_sqcq_hwinfo {
    phys_addr_t sqcq_bar_addr; /* pcie bar address assigned from */
    size_t size;               /* bar_size */
};

struct vtsdrv_ctrl {
    u32 devid;
    dev_t dev_no;
    s64 unique_num;
    struct cdev cdev;
    ka_device_t *dev;
    struct vtsdrv_sqcq_hwinfo sqcq_hwinfo;
    struct tsdrv_id_capacity id_capacity[DEVDRV_MAX_TS_NUM];
};

struct vtsdrv_info {
    u32 dev_num;
    u32 vtsdrv_major;
    ka_class_t *vtsdrv_class;
    struct vtsdrv_ctrl *tsdrv_ctrl[TSDRV_MAX_DAVINCI_NUM];
};

struct vtsdrv_ctrl *vtsdrv_get_ctrl(u32 dev_id);

#endif
