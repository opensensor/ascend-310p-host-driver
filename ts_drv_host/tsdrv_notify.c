/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Description:
 * Author: huawei
 * Create: 2023-2-17
 */
#include <linux/io.h>

#include "tsdrv_kernel_common.h"
#include "devdrv_interface.h"
#include "tsdrv_log.h"
#include "tsdrv_notify.h"
#include "ka_memory_pub.h"

#if defined(CFG_SOC_PLATFORM_CLOUD) || defined(CFG_SOC_PLATFORM_MINIV2)
static void __iomem *g_hwts_base[TSDRV_MAX_DAVINCI_NUM] = {NULL};

STATIC void __iomem *tsdrv_get_hwts_base_addr(u32 devid)
{
    return g_hwts_base[devid];
}
#endif

int tsdrv_hwts_init(u32 devid)
{
#if defined(CFG_SOC_PLATFORM_CLOUD) || defined(CFG_SOC_PLATFORM_MINIV2)
    size_t size;
    u64 addr;
    int ret;

    if (devid >= TSDRV_MAX_DAVINCI_NUM) {
        TSDRV_PRINT_ERR("Invalid devid. (devid=%u)\n", devid);
        return -EINVAL;
    }

    ret = devdrv_get_addr_info(devid, DEVDRV_ADDR_HWTS, 0, &addr, &size);
    if (ret != 0) {
        TSDRV_PRINT_ERR("Get hwts base addr failed. (devid=%u; ret=%d)\n", devid, ret);
        return ret;
    }

    g_hwts_base[devid] = ka_mm_ioremap(addr, size);
    if (g_hwts_base[devid] == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Failed to ka_mm_ioremap. (devid=%u; size=%lu)\n", devid, size);
        return -EFAULT;
#endif
    }

    TSDRV_PRINT_DEBUG("devid=%u; size=%lu\n", devid, size);
#endif
    return 0;
}

void tsdrv_hwts_uninit(u32 devid)
{
#if defined(CFG_SOC_PLATFORM_CLOUD) || defined(CFG_SOC_PLATFORM_MINIV2)
    void __iomem *hwts_base_addr = tsdrv_get_hwts_base_addr(devid);
    if (hwts_base_addr != NULL) {
        ka_mm_iounmap(hwts_base_addr);
        g_hwts_base[devid] = NULL;
    }
#endif
}

int devdrv_write_notify_register(u32 devid, u32 notify_id)
{
#ifndef TSDRV_UT
#if defined(CFG_SOC_PLATFORM_CLOUD) || defined(CFG_SOC_PLATFORM_MINIV2)
    void __iomem *hwts_base_addr = NULL;
    void __iomem *notify_base_addr = NULL;
    u64 offset;
    u64 val;
    int ret = -EINVAL;

    hwts_base_addr = tsdrv_get_hwts_base_addr(devid);
    if (hwts_base_addr == NULL) {
        TSDRV_PRINT_ERR("Failed to get hwts base addr. (devid=%u)\n", devid);
        return ret;
    }

    notify_base_addr = hwts_base_addr + CHIP_NOTIFY_ADDR_OFFSET;
    offset = (u64)notify_id * HWTS_NOTIFY_TABLE_LEN;

    ka_mm_writeq_relaxed(0ULL, notify_base_addr + offset);
    ka_mb();
    ka_mm_writeq_relaxed(1ULL, notify_base_addr + offset);
    ka_mb();
    val = *((volatile u64* volatile)((uintptr_t)(notify_base_addr + offset)));
    TSDRV_PRINT_DEBUG("Write. (devid=%u; notify_id=%u; val=0x%llx)\n", devid, notify_id, val);

#endif
    return 0;
#endif
}

