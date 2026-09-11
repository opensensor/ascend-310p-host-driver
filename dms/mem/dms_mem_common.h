/*
* Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
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
* Create: 2022-11-15
*/
#ifndef __DMS_MEM_COMMON_H__
#define __DMS_MEM_COMMON_H__

#include "drv_type.h"

#define MEMINFO_PATH_LEN 128
#define MEMINFO_READ_INFO_NUM 5
#define MEMINFO_HUGEPAGE_READ_INFO_NUM 4
#define HUGE_PAGE_UNIT_SIZE (1024 * 2)
#define DIGITAL_NUM_TO_PER 100
#define NUMA_ID_SIZE 64
#define BYTES_TO_KBYTE(b) ((b) >> 10ULL)

#if (!defined (CFG_SOC_PLATFORM_MDC_V51)) && (!defined (CFG_SOC_PLATFORM_HELPER))
#define UNUSED_MEMINFO_LINE_NUM 32
#else
#define UNUSED_MEMINFO_LINE_NUM 27
#endif

#ifdef CFG_MEMORY_NODE_CONFIG_1
/*
 * | devnum |  1  |    2   |     3     |       4      |
 * | devid  |  0  |  0   1 |  0  1  2  |  0  1  2  3  |
 * |--------|-----|--------|-----------|--------------|
 * |  DDR   |  0  |  0   1 |  0  1  2  |  0  1  2  3  |
 * |--------|-----|--------|-----------|--------------|
 * |  HBM   |  1  |  2   4 |  3  5  7  |  4  6  8  10 |
 * |SERVICE |  2  |  3   5 |  4  6  8  |  5  7  9  11 |
 * |--------|-----|--------|-----------|--------------|
 * |  DDR   |  3  |  6   7 |  9  10 11 |  12 13 14 15 |
 */
#define MEM_DDR_NUMA_NUM 2 /* Number of per device */
#define MEM_GET_NUMA_ID(devid, index, devnum) (devid + (index * devnum * 3))
#define MEM_SYSTEM_HBM_NUMA_NUM 0
#define MEM_SERVICE_HBM_NUMA_NUM 2
#elif defined CFG_MEMORY_NODE_CONFIG_2
/*
 * | devnum |  1  |    2   |
 * | devid  |  0  |  0   1 |
 * |--------|-----|--------|
 * |  DDR   |  0  |  0   1 |
 * |        |  1  |  2   3 |
 * |        |  2  |  4   5 |
 */
#define MEM_DDR_NUMA_NUM 3
#define MEM_GET_NUMA_ID(devid, index, devnum) (devid + index * devnum)
#elif defined CFG_MEMORY_NODE_CONFIG_3
/*
 * | devnum |   1   |      2      |
 * | devid  |   0   |   0     1   |
 * |--------|-------|-------------|
 * |             HBM node         |
 * | SYSTEM |  0 31 |  0 31  1 63 |
 * | SERVICE| 32~59 | 32~59 64~91 |
 */
#define MEM_DDR_NUMA_NUM 0
#define MEM_GET_NUMA_ID(devid, index, devnum) (index)
#define MEM_SYSTEM_HBM_NUMA_NUM 2
#define MEM_SERVICE_HBM_NUMA_NUM 28
#elif defined CFG_MEMORY_NODE_CONFIG_4
/*
 * | devnum |  1  |    2   |
 * | devid  |  0  |  0   1 |
 * |--------|-----|--------|
 * |  DDR   |  0  |  0   1 |
 * |        |  2  |  2   3 |
 */
#define MEM_DDR_NUMA_NUM 2
#define MEM_GET_NUMA_ID(devid, index, devnum) (devid + index * 2)
#elif defined CFG_MEMORY_NODE_CONFIG_5
/*
 * | devnum |  1  |
 * | devid  |  0  |
 * |--------|-----|
 * |  DDR   |  0  |
 * |        |  1  |
 */
#define MEM_DDR_NUMA_NUM 2
#define MEM_GET_NUMA_ID(devid, index, devnum) (index)
#else
/*
 * | devnum |  1  |
 * | devid  |  0  |
 * |--------|-----|
 * |  DDR   |  0  |
 */
#define MEM_DDR_NUMA_NUM 1
#define MEM_GET_NUMA_ID(devid, index, devnum) (index)
#endif

#define HPAGE_1G_SIZE (1024*1024)
#define HPAGE_2M_SIZE (2*1024)

ssize_t mem_read_file(struct file *fp, char *dst_addr, loff_t fsize, loff_t *pos);
int mem_get_info_single(u32 node_index, u64 *total_size, u64 *free_size);
int mem_get_hpage_info(u32 node_index, u64 *hpage_2M_num, struct mem_hpage_1G *hpage_1G_info);

#endif
