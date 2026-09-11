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

#ifndef _NVME_ADAPT_H_
#define _NVME_ADAPT_H_

#include <asm/io.h>

#define DEVDRV_MSG_CHAN_DB_OFFSET 0x4     /* doorbell offset */
#define DEVDRV_DB_QUEUE_TYPE 2

void devdrv_set_sq_doorbell(void __iomem *io_base, u32 val);
void devdrv_set_cq_doorbell(void __iomem *io_base, u32 val);


#endif
