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
#ifndef TSDRV_MAILBOX_DFX_H
#define TSDRV_MAILBOX_DFX_H

#include <linux/time.h>
#include "kernel_version_adapt.h"
#include "ka_system_pub.h"
#include "ka_fs_pub.h"
#include "ka_kernel_def_pub.h"
#ifdef AOS_LLVM_BUILD
#include <linux/timer.h>
#endif
#define MAILBOX_DFX_COUNT 5
struct tsdrv_mailbox_dfx {
    u64 send_num;
    u64 receive_num;
    u64 lost_num;

    u64 irq_num;
    u64 up_num;

    u32 free_num; /* record receive irq num when mailbox status is free */
    u32 free_type[MAILBOX_DFX_COUNT]; /* record receive irq mailbox_type when mailbox status is free */
    u32 free_valid[MAILBOX_DFX_COUNT]; /* record receive irq mailbox_valid when mailbox status is free */
    u64 free_time[MAILBOX_DFX_COUNT]; /* record receive irq time when mailbox status is free */
};

int tsdrv_mailbox_dfx_file_create(u32 devid, u32 fid);
struct tsdrv_mailbox_dfx *tsdrv_get_mbox_dfx_addr(u32 devid, u32 tsid);
int tsdrv_create_mbox_dfx_addr(u32 devid, u32 tsid);
void tsdrv_destroy_mbox_dfx_addr(u32 devid, u32 tsid);

#endif /* TSDRV_MAILBOX_DFX_H */
