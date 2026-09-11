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

#ifndef TS_AGENT_COMMON_H
#define TS_AGENT_COMMON_H

// AR20241204266067 ts_agent不能直接包含除<linux/types.h>外的内核头文件
#include <linux/types.h>
#include "hvtsdrv_tsagent.h"
#include "ascend_kernel_hal.h"
#include "ka_memory_pub.h"
#include "ka_task_pub.h"
#include "ka_barrier_pub.h"

#ifndef TS_AGENT_UT
#define STATIC static
#else
#define STATIC
#endif

// max device num is 64, reference driver inner define DEVDRV_MAX_DAVINCI_NUM
#define TS_AGENT_MAX_DEVICE_NUM 64

// dc only support 1 ts
#define TS_AGENT_MAX_TS_NUM 1

#define TS_AGENT_MAX_VSQ_SLOT_SIZE 1024

// used for work queue name
#define TS_AGENT_MAX_WQ_NAME_LEN 64

#define TS_STARS_EVENT_OFFSET      (0x4ULL)
#define TS_STARS_EVENT_MASK        (0xFFFFULL)
#define TS_STARS_EVENT_TABLE_OFFSET  (0x10000ULL)
#define TS_STARS_EVENT_TABLE_MASK  (0xF0000ULL)
#define TS_STARS_EVENT_NUM_OF_SINGLE_TABLE (0x1000ULL)
#define TS_STARS_EVENT_BASE_ADDR   (0x200000ULL)

#define TS_STARS_NOTIFY_OFFSET     (0x4ULL)
#define TS_STARS_NOTIFY_MASK        (0x7FFFULL)
#define TS_STARS_NOTIFY_TABLE_OFFSET  (0x10000ULL)
#define TS_STARS_NOTIFY_TABLE_MASK  (0xF0000ULL)
#define TS_STARS_NOTIFY_BASE_ADDR   (0x100000ULL)

#define TS_STARS_NOTIFY_POD_TABLE_OFFSET (0x0001100000ULL)

#define TS_ROCEE_BASE_ADDR         (0x2000000000ULL)
#define TS_ROCEE_VF_DB_CFG0_REG    (0x230ULL)

#define TS_STARS_SINGLE_DEV_ADDR_MASK (0xFFFFFFFFFFULL)

#define TS_STARS_PCIE_BASE_ADDR (0x400004008000ULL) // remember runtime
#define TS_STARS_PCIE_BASE_MASK (0xF0000FFFFFFFULL)


#ifdef CFG_SOC_PLATFORM_CLOUD_V2
#define TS_AGENT_MAX_STREAM_NUM  (32*1024)      //extend to 32K
#define TS_AGENT_MAX_SQ_NUM 2048
#define TS_AGENT_VF_ID_MIN 0
#define TS_AGENT_VF_ID_MAX 16
#define TS_STARS_BASE_ADDR (0x06a0000000ULL)
#define TS_STARS_NOTIFY_NUM_OF_SINGLE_TABLE (0x200ULL)

#elif defined(CFG_SOC_PLATFORM_MINIV3)
#define TS_AGENT_MAX_STREAM_NUM 512
#define TS_AGENT_MAX_SQ_NUM 512
#define TS_AGENT_VF_ID_MIN 0
#define TS_AGENT_VF_ID_MAX 4
#define TS_STARS_BASE_ADDR (0x520000000ULL)
#define TS_STARS_NOTIFY_NUM_OF_SINGLE_TABLE (0x80ULL)
#else
#define TS_AGENT_MAX_STREAM_NUM 512
#define TS_AGENT_MAX_SQ_NUM 512
#define TS_AGENT_VF_ID_MIN 1 // min vf id is 1, 0 is reserve for physic
#define TS_AGENT_VF_ID_MAX 16
#endif

#ifdef CFG_SOC_PLATFORM_STARS
#define TS_AGENT_WQ_VF_MAX 0 // stars create wq by pf instead of vf
#else
#define TS_AGENT_WQ_VF_MAX TS_AGENT_VF_ID_MAX
#endif

#ifndef U32_MAX
#define U32_MAX 0xFFFFFFFFU
#endif

#ifndef U16_MAX
#define U16_MAX 0xFFFFU
#endif

typedef struct ts_agent_vsq_base_info {
    u32 dev_id;
    u32 vf_id;
    u32 ts_id;
    u32 vsq_id;
    const void *vsq_base_addr; // vsq base addr alloc by drv
    u32 vsq_dep;  // 1024
    u32 vsq_slot_size; // 64
    enum vsqcq_type vsq_type; // value range enum vsqcq_type
} vsq_base_info_t;

#endif // TS_AGENT_COMMON_H
