/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2021. All rights reserved.
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
#include "nvme_adapt.h"

static void devdrv_nvme_reg_wr(void __iomem *io_base, u32 offset, u32 val)
{
    writel(val, io_base + offset);
}

void devdrv_set_sq_doorbell(void __iomem *io_base, u32 val)
{
    devdrv_nvme_reg_wr(io_base, 0, val);
}

void devdrv_set_cq_doorbell(void __iomem *io_base, u32 val)
{
    devdrv_nvme_reg_wr(io_base, DEVDRV_MSG_CHAN_DB_OFFSET, val);
}

