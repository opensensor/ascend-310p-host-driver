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

#ifndef _VHDC_MEM_H_
#define _VHDC_MEM_H_

#include "hdcdrv_mem_com.h"
#include "vhdc_agent.h"

#define VHDCA_MEM_POOL_SIZE_MIN 4

#define VHDCA_ALLOC_MEM_SLEEP_MIN 50
#define VHDCA_ALLOC_MEM_SLEEP_MAX 100
#define VHDCA_ALLOC_MEM_RETRY_TIME 10000

#define VHDCA_MEM_WORK_TIMEOUT (5 * KA_HZ) /* 5s */

int vhdca_alloc_mem(struct vhdca_alloc_mem_para *para, void **buf, ka_dma_addr_t *addr);
int vhdca_free_mem(void *buf);
void vhdca_delay_free_mem(void *buf);
int vhdca_host_free_mem(void *buf);
int vhdca_init_mempool(struct vhdca_pdev *pdev);
void vhdca_uninit_mempool(struct vhdca_pdev *pdev);
int vhdca_mem_block_capacity(void);

extern void hdcdrv_recycle_mem_work(ka_work_struct_t *p_work);


#endif /* _VHDC_MEM_H_ */
