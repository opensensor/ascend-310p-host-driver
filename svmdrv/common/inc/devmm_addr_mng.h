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
#ifndef DEVMM_ADDR_MNG_H
#define DEVMM_ADDR_MNG_H

#include "devdrv_interface.h"
#include "svm_mem_mng.h"
#include "virtmng_interface.h"
#include "ka_task_pub.h"
#include "ka_base_pub.h"

#define DEVMM_MEM_NODE_VA_ALIGN 2097152 /* 2M (2 * 1024 * 1024) */
#ifndef DEVMM_UT
#define DEVMM_DMA_LEN_MASK_IN_HANDLE 0xffful
#else
/* ut use malloc to realize dma map, addr just last 8 bit is zero  */
#define DEVMM_DMA_LEN_MASK_IN_HANDLE 0xfful
#endif
struct devmm_addr_info {
    u32 dev_id;
    u32 len;
    ka_dma_addr_t addr;
    ka_page_t *page;
};

struct devmm_mem_node {
    ka_rb_node_t node;
    u64 va;
    u64 len;
    u32 page_prot;
    u32 resv;
    u32 page_size;
    ka_atomic_t user_cnt;
    ka_atomic_t valid_page_num;
    struct devmm_addr_info *addr_info; /* addr_info is null means not dma map */
};

struct devmm_addr_mng {
    bool is_need_dma_map;
    ka_rw_semaphore_t rbtree_mutex;
    ka_rb_root_t rbtree;
};

struct devmm_mem_node *devmm_search_mem_node(struct devmm_addr_mng *addr_mng, u64 va, u64 len);
struct devmm_mem_node *devmm_get_mem_node(struct devmm_addr_mng *addr_mng, ka_pid_t devpid, u64 va,
    u64 len, u32 page_size);
int devmm_try_free_mem_node(struct devmm_addr_mng *addr_mng, struct devmm_mem_node *mem_node);
struct devmm_addr_info *devmm_get_addr_info(struct devmm_mem_node *mem_node, u64 va);
int devmm_dma_map_page(u32 dev_id, ka_page_t *page, u32 len,
    struct devmm_mem_node *mem_node, struct devmm_addr_info *addr_info);
void devmm_dma_unmap_page(struct devmm_mem_node *mem_node, struct devmm_addr_info *addr_info);
void devmm_addr_mng_free_res(struct devmm_addr_mng *addr_mng);
void devmm_addr_mng_free_res_by_addr(struct devmm_addr_mng *addr_mng, u64 start, u64 end);
void devmm_set_ka_dma_addr_to_addr_info(const struct devmm_addr_info *in_addr_info,
    struct devmm_mem_node *mem_node, struct devmm_addr_info *addr_info);
struct devmm_mem_node *devmm_get_addr_mem_node(struct devmm_addr_mng *addr_mng, u64 va, u64 len);
void devmm_put_addr_mem_node(struct devmm_addr_mng *addr_mng, u64 va, u64 len);
bool devmm_mem_node_is_in_use(struct devmm_mem_node *mem_node);
bool devmm_mem_attr_is_match(struct devmm_addr_mng *addr_mng, u64 va, u64 size, u32 page_size, u32 mem_attr);
bool devmm_mem_node_is_store_addr_info(struct devmm_mem_node *mem_node);

#endif
