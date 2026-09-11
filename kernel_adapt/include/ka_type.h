/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
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
 * Create: 2024-11-05
 */

#ifndef KA_TYPE_H
#define KA_TYPE_H

#include <linux/version.h>
#include <linux/types.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
#include <linux/stdarg.h>
#else
#include <stdarg.h>
#endif

typedef long long s64;
typedef unsigned long size_t;
typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef u64 phys_addr_t;
typedef u64 gfn_t;
#ifndef EMU_ST
typedef long long loff_t;
#endif
typedef long off_t;
typedef unsigned short umode_t;
typedef long intptr_t;
typedef unsigned long uintptr_t;

typedef u32 __wsum;
typedef u32 __be32;
typedef long clock_t;
typedef long long time64_t;
typedef phys_addr_t resource_size_t;

#endif