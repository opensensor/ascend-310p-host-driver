/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
 * Create: 2025-07-14
 */

#ifndef _UDIS_LOG_H_
#define _UDIS_LOG_H_


#include "drv_log.h"
#include "ascend_hal_error.h"

#define MODULE_UDIS "ascend_udis"

#define udis_err(fmt, ...) drv_err(MODULE_UDIS, fmt, ##__VA_ARGS__)
#define udis_warn(fmt, ...) drv_warn(MODULE_UDIS, fmt, ##__VA_ARGS__)
#define udis_info(fmt, ...) drv_info(MODULE_UDIS, fmt, ##__VA_ARGS__)
#define udis_event(fmt, ...) drv_event(MODULE_UDIS, fmt, ##__VA_ARGS__)
#define udis_debug(fmt, ...) drv_pr_debug(MODULE_UDIS, fmt, ##__VA_ARGS__)

#define udis_ex_notsupport_err(ret, fmt, ...) do {                      \
    if (((ret) != (int)DRV_ERROR_NOT_SUPPORT) && ((ret) != -EOPNOTSUPP)) {  \
        udis_err(fmt, ##__VA_ARGS__);                                   \
    }                                                                  \
} while (0)

#endif