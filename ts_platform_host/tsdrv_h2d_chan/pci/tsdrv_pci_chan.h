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
#ifndef TSDRV_PCI_CHAN_H
#define TSDRV_PCI_CHAN_H

#include "devdrv_interface.h"
#include "tsdrv_h2d_chan_ops.h"

const struct tsdrv_h2d_chan_ops *tsdrv_get_h2d_chan_ops(void);

#endif /* __TSDRV_PCI_CHAN_H */
