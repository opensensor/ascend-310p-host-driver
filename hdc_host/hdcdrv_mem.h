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

#ifndef _HDCDRV_MEM_H_
#define _HDCDRV_MEM_H_

#include "hdcdrv_mem_com.h"

#include "ka_list_pub.h"
#include "ka_memory_pub.h"

extern int alloc_mem(int pool_type, int dev_id, int len, void **buf, ka_dma_addr_t *addr, ka_list_head_t *wait_head);
#ifdef CFG_FEATURE_MIRROR
struct mirror_alloc_para {
    int dev_id;
    int len;
    ka_list_head_t *wait_head;
    int service_type;
};
extern int alloc_mem_page(int pool_type, struct mirror_alloc_para para, void **buf, ka_dma_addr_t *addr, int *mem_id);
extern int get_pool_type(int data_len);
extern void free_mem_page(void *buf, int mem_id);
#endif
extern void free_mem(void *buf);
extern int alloc_mem_pool(int pool_type, int dev_id, u32 segment, u32 num);
extern void free_mem_pool(int pool_type, int dev_id, u32 segment);

extern void hdcdrv_mem_block_head_init(void *buf, ka_dma_addr_t addr, int pool_type, int dev_id, u32 segment);
extern int hdcdrv_mem_block_head_check(void *buf);
extern int hdcdrv_init_mem_pool(u32 dev_id);
extern void hdcdrv_uninit_mem_pool(u32 dev_id);
extern int hdcdrv_mem_block_capacity(void);
extern void hdcdrv_recycle_mem_work(ka_work_struct_t *p_work);

ka_device_t* hdcdrv_get_pdev_dev(int dev_id);
void *hdcdrv_kvmalloc(size_t size, int level);

#endif
