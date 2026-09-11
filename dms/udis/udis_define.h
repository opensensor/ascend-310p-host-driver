/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
 * Create: 2025-07-14
 */

#ifndef _UDIS_DEFINE_H_
#define _UDIS_DEFINE_H_

#include <linux/list.h>
#include <linux/rwsem.h>
#include <linux/slab.h>

#include "devdrv_interface.h"
#include "udis.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

struct udis_info_stu {
    char name[UDIS_MAX_NAME_LEN];
    unsigned int acc_ctrl;
    UDIS_UPDATE_TYPE update_type;
    unsigned long last_update_time;
    char reserved[12]; // reserved for head
    unsigned int data_len;
    char data[UDIS_MAX_DATA_LEN];
};

struct udis_dma_node {
    struct list_head list;
    dma_addr_t host_dma_addr;
    dma_addr_t dev_dma_addr;
    unsigned int acc_ctrl;
    unsigned int data_len;
    UDIS_UPDATE_TYPE update_type;
    UDIS_MODULE_TYPE module_type;
    char name[UDIS_MAX_NAME_LEN];
};

struct udis_link_dma_nodes {
    struct devdrv_dma_node *dma_nodes;
    unsigned int node_num;
    unsigned int capacity;
};

enum udis_dev_state {
    UDIS_DEV_UNINIT = 0,
    UDIS_DEV_CHANREADY,
    UDIS_DEV_READY,
    UDIS_DEV_HOTRESET,
    UDIS_DEV_PREHOTRESET,
    UDIS_DEV_HEART_BEAT_LOSS,
    UDIS_DEV_STATE_MAX
};

enum udis_data_status {
    UDIS_DATA_VALID = 0,
    UDIS_DATA_NEEDS_UPDATE,
    UDIS_DATA_UPDATE_IMMEDIATELY,
    UDIS_DATA_INVALID
};

enum udis_search_scope {
    UDIS_INFO_ALL_SPACE = 0,
    UDIS_INFO_DISCRE_SPACE = 1
};

struct udis_ctrl_block {
    struct list_head addr_list[UPDATE_TYPE_MAX];
    struct rw_semaphore addr_list_lock;
    struct udis_info_stu *udis_info_buf;
    struct rw_semaphore udis_info_lock;
    dma_addr_t udis_info_buf_dma;
    enum udis_dev_state state;
};

void udis_release_dma_node(unsigned int udevid, struct udis_dma_node *dma_node);
int period_link_dma_task_init(unsigned int udevid);
void period_link_dma_task_uninit(unsigned int udevid);
int udis_dma_sync_copy(unsigned int udevid, struct udis_ctrl_block *udis_cb, const struct udis_dma_node *node);
int udis_update_info_by_dma(unsigned int udevid, UDIS_MODULE_TYPE module_id, const char *name);
#endif
