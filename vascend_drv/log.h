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
 */

#ifndef LOG_H_
#define LOG_H_

#include <linux/types.h>

#define module_devdrv "vascend"
#ifdef DAVINCI_DEBUG
#define vascend_err(dev, fmt, ...) dev_err(dev, "[%s] [CPU %d] [%s/%d/%d] [%s %d] " \
    fmt, module_devdrv, smp_processor_id(), current->comm, current->tgid, current->pid, \
    __func__, __LINE__, ##__VA_ARGS__)
#define vascend_warn(dev, fmt, ...) dev_warn(dev, "[%s] [CPU %d] [%s/%d/%d] [%s %d] " \
    fmt, module_devdrv, smp_processor_id(), current->comm, current->tgid, current->pid, \
    __func__, __LINE__, ##__VA_ARGS__)
#define vascend_info(dev, fmt, ...) dev_info(dev, "[%s] [CPU %d] [%s/%d/%d] [%s %d] " \
    fmt, module_devdrv, smp_processor_id(), current->comm, current->tgid, current->pid, \
    __func__, __LINE__, ##__VA_ARGS__)
#define vascend_debug(fmt, ...) pr_info("[%s]" fmt, __func__, ##__VA_ARGS__)
#else
#define vascend_err(dev, fmt, ...) dev_err(dev, "[%s] [%s %d] " \
    fmt, module_devdrv, __func__, __LINE__, ##__VA_ARGS__)
#define vascend_warn(dev, fmt, ...) dev_warn(dev, "[%s] [%s %d] " \
    fmt, module_devdrv, __func__, __LINE__, ##__VA_ARGS__)
#define vascend_info(dev, fmt, ...) dev_info(dev, "[%s] [%s %d] " \
    fmt, module_devdrv, __func__, __LINE__, ##__VA_ARGS__)
#define vascend_debug(fmt, ...)
#endif /* DAVINCI_DEBUG */

#endif /* LOG_H_ */
