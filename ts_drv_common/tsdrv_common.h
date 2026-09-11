/*
 * Copyright (c) 2019-2022. Huawei Technologies Co., Ltd. All rights reserved.
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
#ifndef TSDRV_COMMON_H
#define TSDRV_COMMON_H

#include <linux/types.h>

#include "ka_kernel_def_pub.h"
#include "ka_memory_pub.h"
#include "ka_base_pub.h"
#include "ka_system_pub.h"
#include "ka_common_pub.h"
#include "ka_list_pub.h"
#include "ka_fs_pub.h"
#include "ka_task_pub.h"
#include "ka_dfx_pub.h"
#include "ka_ioctl_pub.h"
#include "ka_barrier_pub.h"
#include "ka_errno_pub.h"
#include "ka_driver_pub.h"

#ifndef __KA_GFP_ACCOUNT
#ifdef __KA_GFP_KMEMCG
#define __KA_GFP_ACCOUNT __KA_GFP_KMEMCG /* for linux version 3.10 */
#endif

#ifdef __KA_GFP_NOACCOUNT
#define __KA_GFP_ACCOUNT 0 /* for linux version 4.1 */
#endif
#endif

#if ((defined CFG_BUILD_DEBUG) && (!defined EXPORT_SYMBOL_UNRELEASE))
#define EXPORT_SYMBOL_UNRELEASE(symbol) KA_EXPORT_SYMBOL_GPL(symbol)
#elif (!defined EXPORT_SYMBOL_UNRELEASE)
#define EXPORT_SYMBOL_UNRELEASE(symbol)
#endif

extern int memset_s(void *dest, size_t destMax, int c, size_t count);

struct tsdrv_msg_sync_ssid {
    u32 hpid;
    u32 vfid;
    int ssid;
};

/**
 * get chip type
 */
#define CHIP_TYPE_MDC_ASCEND610 (1)
#define CHIP_TYPE_MDC_BS9SX1A (2)
#define CHIP_TYPE_NOT_SET (-1)

/**
 * @Return: chip type. 1: ASCEND610; 2: BS9SX1A; other: invalid value
 */
int tsdrv_get_chip_type(void);

#ifdef CFG_SOC_PLATFORM_MINIV2
#define DIE_BASEADDR_PA_OFFSET              (0x0ULL)
#elif defined (CFG_SOC_PLATFORM_CLOUD_V2)
#define DIE_BASEADDR_PA_OFFSET              (0x10000000000ULL)
#else
#define DIE_BASEADDR_PA_OFFSET              (0x0ULL)
#endif /* CFG_SOC_PLATFORM_MINIV2 */

#endif
