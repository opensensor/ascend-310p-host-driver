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

#ifndef _VHDC_ADAPTER_H_
#define _VHDC_ADAPTER_H_

#include "ka_base_pub.h"

#ifndef u32
typedef unsigned int u32;
#endif

#ifndef u64
    typedef unsigned long long u64;
#endif

#define HDCDRV_SUPPORT_MAX_SESSION_PER_VDEV 136 /* 32 * 4(log + tsd + dvpp + reserved) + 8 */
#define HDCDRV_SUPPORT_MAX_SHORT_SESSION_PER_VDEV 8
#define HDCDRV_SUPPORT_MAX_LONG_SESSION_PER_VDEV \
    (HDCDRV_SUPPORT_MAX_SESSION_PER_VDEV - HDCDRV_SUPPORT_MAX_SHORT_SESSION_PER_VDEV)

#define HDCDRV_SUPPORT_MAX_MSG_CHAN_PER_VDEV 48
#define HDCDRV_VDEV_FAST_MSG_CHAN_START 2
#define HDCDRV_VDEV_NORMAL_CHAN_CNT 2
#define HDCDRV_MAX_CORE_NUM_PER_DEV 32
#define HDCDRV_VDAVINCI_TYPE_MAX 5
#define HDCDRV_VDEV_MAX_CTX_NUM 200
#define HDCDRV_VDEV_MAX_FAST_NODE_NUM 0x80000
#define HDCDRV_VDEV_MAX_FNODE_PHY_NUM (HDCDRV_MEM_MAX_PHY_NUM * 128) /* 32GB / 256MB */


struct vhdch_fast_node {
    ka_rb_node_t mem_node;
    u64 hash_va;
};

int vhdch_init(void);
int vhdch_uninit(void);

typedef int (*get_aicore_num)(u32, u32, u32 *);

#endif /* _VHDC_ADAPTER_H_ */
