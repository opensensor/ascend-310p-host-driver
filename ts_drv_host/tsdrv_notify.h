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
#ifndef TSDRV_NOTIFY_H
#define TSDRV_NOTIFY_H

#include <linux/types.h>
#include "ka_barrier_pub.h"
#include "ka_memory_pub.h"
#define HWTS_NOTIFY_TABLE_LEN 0x8U
#ifdef CFG_SOC_PLATFORM_CLOUD
#define CHIP_NOTIFY_ADDR_OFFSET 0xE000UL
#endif
#ifdef CFG_SOC_PLATFORM_MINIV2
#define CHIP_NOTIFY_ADDR_OFFSET 0x90000UL
#endif


int tsdrv_hwts_init(u32 devid);
void tsdrv_hwts_uninit(u32 devid);
int devdrv_write_notify_register(u32 devid, u32 notify_id);

#endif
