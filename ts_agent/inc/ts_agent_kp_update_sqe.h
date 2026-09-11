/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
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
 * Description: sqe update in the Kunpeng scenario
 * Author: huawei
 * Create: 2023-10-13
 */
#ifndef TS_AGENT_KP_UPDATE_SQE_H
#define TS_AGENT_KP_UPDATE_SQE_H

#if defined(CFG_SOC_PLATFORM_KPSTARS)
#include <linux/types.h>
#include "tsch/task_struct.h"

#define STARS_DWORD_SIZE                   32
#define STARS_RDMA_QP_ADDR_LOW_MASK        0xFFFFFFFF
#define CHIP_NUM                            4 /* allowing maximum 4 chips interconnected */
#define CPU_DIE_NUM                         2 /* currently each chip has 2 CPU DIEs */
#define STARS_DEV_NUM                       (CHIP_NUM * CPU_DIE_NUM)
#define UNSIGNED_SHORT_LEN 16
int sqe_proc_rdma(u32 devid, u32 tsid, int pid, u32 sqid, ts_stars_sqe_t *sqe);
int kp_sqe_proc_sdma(u32 devid, u32 tsid, int pid, u32 sqid, ts_stars_sqe_t *sqe);
// kstars.ko提供鉴权接口
bool sdma_check_auth(int own_pid, u32 *own_passid, int sumitter_pid, u32 *sumitter_passid);
#endif // CFG_SOC_PLATFORM_KPSTARS
#endif // TS_AGENT_KP_UPDATE_SQE_H