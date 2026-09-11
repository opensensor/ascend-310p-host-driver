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

#ifndef DEVMM_RDMA_H
#define DEVMM_RDMA_H

#include <linux/dma-mapping.h>

#include "ka_task_pub.h"
#include "ka_base_pub.h"
#include "ka_memory_pub.h"

#ifdef STATIC_SKIP
#  define STATIC
#else
#  define STATIC static
#endif

#define PROCESS_SIGN_LENGTH 49
struct peer_memory_data {
    u32 host_pid;
    u32 devid;
    u32 vfid;
    char pid_sign[PROCESS_SIGN_LENGTH];
};

#define IB_PEER_MEMORY_NAME_MAX 64
#define IB_PEER_MEMORY_VER_MAX 16
struct peer_memory_client {
    char name[IB_PEER_MEMORY_NAME_MAX];
    char version[IB_PEER_MEMORY_VER_MAX];
    int (*acquire)(unsigned long addr, size_t size, void *memory_data, char *peer_mem_name,
        void **client_context);
    int (*get_pages)(unsigned long addr, size_t size, int write, int force, ka_sg_table_t *sg_head,
        void *client_context, u64 core_context);
    void (*put_pages)(ka_sg_table_t *sg_head, void *client_context);
    int (*dma_map)(ka_sg_table_t *sg_head, void *client_context,
        ka_device_t *dma_device, int dmasync, int *nmap);
    int (*dma_unmap)(ka_sg_table_t *sg_head, void *client_context, ka_device_t *dma_device);
    unsigned long (*get_page_size)(void *client_context);
    void (*release)(void *client_context);
    void *(*get_context_private_data)(u64 peer_id);
    void (*put_context_private_data)(void *context);
};

typedef int (*invalidate_peer_memory)(int *reg_handle, u64 core_context);

void *ib_register_ai_peer_memory_client(const struct peer_memory_client *peer_client,
    invalidate_peer_memory *invalidate_callback);
void ib_unregister_ai_peer_memory_client(int *reg_handle);

void svm_peer_mem_release(void *mm_context);
unsigned long svm_peer_mem_get_page_size(void *mm_context);

#endif /* __DEVMM_RDMA_H__ */
