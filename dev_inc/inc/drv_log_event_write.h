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
 * Create: 2025-02-020
 */
#ifndef __DRV_LOG_EVENT_WRITE_H__
#define __DRV_LOG_EVENT_WRITE_H__
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/rtc.h>
#include "securec.h"

#define LOG_PRINT_LEN         0x400     // 1024 Bytes

typedef struct log_drv_fault_mng {
    u32 buf_read;
    u32 buf_write;
    u32 buf_size;
    spinlock_t spinlock;
    wait_queue_head_t wq;
    atomic_t status;
    char printk_buf[LOG_PRINT_LEN];
    u8 *vir_addr; // Log Ring Buffer
    u8 *temp_buf;
} log_drv_fault_mng_t;

void log_set_fault_mng_info(struct log_drv_fault_mng *fault_mng);
#endif