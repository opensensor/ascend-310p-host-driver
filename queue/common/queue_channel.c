/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
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
 * Create: 2023-8-30
 */
#include <securec.h>
#include "ascend_hal_define.h"
#include "queue_module.h"
#include "queue_channel.h"
#include "ka_compiler_pub.h"

#define QUEUE_CHAN_MIN_VA_NUM 2

/* Supported remote page size */
#define QUEUE_CHAN_PAGE_SIZE_4K     (4 * 1024)
#define QUEUE_CHAN_PAGE_SIZE_64K    (64 * 1024)

struct queue_chan_dma_node_attr {
    enum devdrv_dma_direction dir;
    u32 dst_page_size;
    u32 src_page_size;
    int loc_passid;
    struct queue_chan_dma *chan_dma;
    struct queue_chan_mem_node *mem_node;
};

static inline struct queue_chan_enque *_queue_chan_enque_create(u64 size)
{
    return queue_drv_kvmalloc(size, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
}

static inline void _queue_chan_enque_destroy(struct queue_chan_enque *enque)
{
    queue_drv_kvfree(enque);
}

static void *_queue_chan_alloc_node(struct queue_chan *que_chan, size_t size, unsigned int flags)
{
    void *addr = NULL;
    int i;

    for (i = 0; i < que_chan->attr.nids.nid_num; i++) {
        addr = queue_drv_kvmalloc_node(size, flags, que_chan->attr.nids.nids[i]);
        if (addr != NULL) {
            return addr;
        }
    }
    return addr;
}

void *queue_chan_alloc_node(struct queue_chan *que_chan, size_t size)
{
    unsigned int flags = KA_GFP_KERNEL | __KA_GFP_ACCOUNT;
    void *addr = NULL;

    if (que_chan->attr.nids.nid_num == 0) {
       return queue_drv_kvmalloc(size, flags);
    }
    /*
     * For the first round, scan numa nodes to allocate memroy with KA_GFP_NOWAIT flag
     * If current node has no enough memroy, just skip it.
     */
    addr = _queue_chan_alloc_node(que_chan, size, flags | KA_GFP_NOWAIT | __KA_GFP_NOWARN);
    if (addr == NULL) {
        /*
         * For the second round, scan numa nodes to allocate memory without KA_GFP_NOWAIT flag.
         * If current node has no enough memory, it will stall for direct reclaim
         */
        addr = _queue_chan_alloc_node(que_chan, size, flags | __KA_GFP_NOWARN);
    }
    return addr;
}

static int _queue_chan_enque_init(struct queue_chan *que_chan, struct queue_chan_enque *enque,
    struct queue_chan_attr *attr, u64 size, u64 que_chan_addr)
{
    int ret;

    ret = (int)ka_base_copy_from_user(enque->head.msg, attr->msg, attr->msg_len);
    if (ret != 0) {
        queue_err("Copy from user fail. (ret=%d; msg_len=%ld)\n", ret, attr->msg_len);
        return ret;
    }
    enque->head.devid = attr->devid;
    enque->head.hostpid = attr->host_pid;
    enque->head.msg_type = attr->msg_type;
    enque->head.event_info = attr->event_info;
    enque->memory_type = attr->memory_type;

    enque->size = size;
    enque->que_chan_addr = que_chan_addr;
    enque->serial_num = attr->serial_num;
    enque->qid = attr->qid;
    enque->page_size = KA_MM_PAGE_SIZE;

    enque->copyed_va_node_num  = 0;
    enque->total_va_num = que_chan->local_va_num;
    enque->total_va_len = que_chan->local_va_len;
    enque->total_dma_blk_num = que_chan->local_dma_blk_num;
    enque->va_node_num = 0;
    enque->dma_node_num = 0;

    enque->max_mem_node_num = (u32)((enque->size - sizeof(struct queue_chan_enque)) /
        sizeof(struct queue_chan_mem_node));

    return 0;
}

static inline void queue_chan_enque_reinit(struct queue_chan_enque *enque)
{
    enque->va_node_num = 0;
    enque->dma_node_num = 0;
}

static struct queue_chan_enque *queue_chan_enque_create(struct queue_chan *que_chan)
{
    u64 mem_node_num = que_chan->local_va_num + que_chan->local_dma_blk_num;
    u64 size = ka_base_min_t(u64, queue_chan_max_msg_size(), queue_chan_get_enque_size(mem_node_num));
    struct queue_chan_enque *enque = NULL;
    int ret;

    enque = _queue_chan_enque_create(size);
    if (enque == NULL) {
        queue_err("Que chan enque create fail. (size=%llu)\n", size);
        return NULL;
    }
    ret = _queue_chan_enque_init(que_chan, enque, &que_chan->attr, size, (u64)(uintptr_t)que_chan);
    if (ret != 0) {
        _queue_chan_enque_destroy(enque);
        queue_err("Que chan enque init fail. (ret=%d)\n", ret);
        return NULL;
    }

    return enque;
}

static struct queue_chan_dma *_queue_get_chan_dma(struct queue_chan *que_chan)
{
    u64 va_index = que_chan->local_va_num;
    struct queue_chan_dma *chan_dma = que_chan->local_chan_dma;
    if (ka_likely((chan_dma != NULL) && (va_index < que_chan->local_total_va_num))) {
        return &chan_dma[va_index];
    }
    return NULL;
}

static struct queue_chan_mem_node *_queue_get_mem_node(struct queue_chan *que_chan)
{
    u64 mem_node_index = que_chan->remote_dma_blk_num + que_chan->remote_va_num;
    struct queue_chan_mem_node *mem_node = que_chan->remote_mem_node;
    if (ka_likely((mem_node != NULL) &&
        (mem_node_index < (que_chan->remote_total_va_num + que_chan->remote_total_dma_blk_num)))) {
        return &mem_node[mem_node_index];
    }
    return NULL;
}

static inline void _queue_chan_dma_iovec_add(struct queue_chan_dma *chan_dma,
    u64 va, u64 len, bool dma_flag)
{
    chan_dma->dma_list.va = va;
    chan_dma->dma_list.len = len;
    chan_dma->dma_list.dma_flag = dma_flag;
}

static inline int _queue_chan_dma_map(ka_device_t *dev, bool hccs_vm_flag, u32 dev_id, struct queue_dma_list *dma_list)
{
    return queue_make_dma_list(dev, hccs_vm_flag, dev_id, dma_list);
}

static inline void _queue_chan_dma_unmap(ka_device_t *dev, bool hccs_vm_flag, struct queue_dma_list *dma_list)
{
    queue_clear_dma_list(dev, hccs_vm_flag, dma_list);
}

static inline u32 _queue_chan_get_va_num(struct queue_chan *que_chan, enum queue_dma_side side)
{
    return (side == QUEUE_DMA_LOCAL) ? que_chan->local_va_num : que_chan->remote_va_num;
}

static int _queue_chan_dma_add(struct queue_chan *que_chan, struct queue_chan_dma *chan_dma)
{
    u64 local_va_len, local_dma_blk_num, mem_node_num;

    local_va_len = que_chan->local_va_len + chan_dma->dma_list.len;
    if (ka_unlikely((local_va_len < que_chan->local_va_len) || (local_va_len < chan_dma->dma_list.len))) {
        return -EOVERFLOW;
    }
    local_dma_blk_num = que_chan->local_dma_blk_num + chan_dma->dma_list.blks_num;
    if (ka_unlikely((local_dma_blk_num < que_chan->local_dma_blk_num) ||
        (local_dma_blk_num < chan_dma->dma_list.blks_num))) {
        return -EOVERFLOW;
    }
    mem_node_num = que_chan->local_va_num + 1 + local_dma_blk_num;
    if (ka_unlikely((mem_node_num < que_chan->local_va_num) || (mem_node_num < local_dma_blk_num))) {
        return -EOVERFLOW;
    }
    que_chan->local_va_num = que_chan->local_va_num + 1;
    que_chan->local_va_len = local_va_len;
    que_chan->local_dma_blk_num = local_dma_blk_num;
    return 0;
}

static void _queue_chan_destroy_all_dma(struct queue_chan *que_chan)
{
    if (que_chan->local_chan_dma != NULL) {
        u64 i;
        for (i = 0; i < que_chan->local_va_num; i++) {
            struct queue_chan_dma *chan_dma = &que_chan->local_chan_dma[i];
            _queue_chan_dma_unmap(que_chan->attr.dev, que_chan->attr.hccs_vm_flag, &chan_dma->dma_list);
        }
        que_chan->local_va_num = 0;
        queue_drv_kvfree(que_chan->local_chan_dma);
        que_chan->local_chan_dma = NULL;
    }
    if (que_chan->remote_mem_node != NULL) {
        queue_drv_kvfree(que_chan->remote_mem_node);
        que_chan->remote_mem_node = NULL;
    }
}

static int _queue_chan_send(struct queue_chan *que_chan, int time_out)
{
    struct queue_chan_enque *enque = que_chan->enque;
    u64 mem_node_num = enque->va_node_num + enque->dma_node_num;

    if (ka_unlikely((enque->copyed_va_node_num + enque->va_node_num) > que_chan->local_va_num)) {
        queue_err("Invalid va node num. (copyed_va_num=%u; va_num=%u; total_va_num=%u)\n",
            enque->copyed_va_node_num, enque->va_node_num, que_chan->local_va_num);
        return -EINVAL;
    }

    if (mem_node_num != 0) {
        u64 size = ka_base_min_t(u64, queue_chan_get_enque_size(mem_node_num), queue_chan_max_msg_size());
        int ret = que_chan->attr.send(enque, (size_t)size, que_chan->attr.priv, time_out);
        if (ka_unlikely(ret != 0)) {
            queue_err("Que chan send fail. (ret=%d; mem_node_num=%llu; size=%llu; copyed_va_num=%u; va_num=%u)\n",
                ret, mem_node_num, size, enque->copyed_va_node_num, enque->va_node_num);
            /* Return to user space. do not modify it */
            return DRV_ERROR_SEND_MESG;
        }
        enque->copyed_va_node_num += enque->va_node_num;
        queue_chan_enque_reinit(enque);
    }
    return 0;
}

int queue_chan_iovec_add(struct queue_chan *que_chan, struct queue_chan_iovec *iovec)
{
    struct queue_chan_dma *chan_dma = NULL;
    int ret;

    chan_dma = _queue_get_chan_dma(que_chan);
    if (ka_unlikely(chan_dma == NULL)) {
        queue_err("Get chan dma fail. (local_va_num=%u)\n", que_chan->local_va_num);
        return -ENODEV;
    }
    _queue_chan_dma_iovec_add(chan_dma, iovec->va, iovec->len, iovec->dma_flag);
    ret = _queue_chan_dma_map(que_chan->attr.dev, que_chan->attr.hccs_vm_flag, que_chan->attr.devid, &chan_dma->dma_list);
    if (ka_unlikely(ret != 0)) {
        queue_err("Dma map fail. (ret=%d; va=0x%pK; len=%llu; dma_flag=%d)\n",
            ret, (void *)(uintptr_t)iovec->va, iovec->len, iovec->dma_flag);
        return ret;
    }
    ret = _queue_chan_dma_add(que_chan, chan_dma);
    if (ret != 0) {
        _queue_chan_dma_unmap(que_chan->attr.dev, que_chan->attr.hccs_vm_flag, &chan_dma->dma_list);
        queue_err("Chan dma add fail. (ret=%d)\n", ret);
    }
    return ret;
}

/* Get user ctx data size */
static int _queue_chan_get_ctx_size(struct queue_chan *que_chan, enum queue_dma_side side, u64 *size)
{
    if (side == QUEUE_DMA_LOCAL) {
        struct queue_chan_dma *chan_dma = que_chan->local_chan_dma;
        if (ka_unlikely((chan_dma == NULL) || (que_chan->local_va_num == 0))) {
            return -ENODEV;
        }
        *size = chan_dma[0].dma_list.len;
    } else {
        struct queue_chan_mem_node *mem_node = que_chan->remote_mem_node;
        if (ka_unlikely((mem_node == NULL) || (que_chan->remote_va_num == 0))) {
            queue_err("Get ctx size fail. (remote_va_num=%u)\n", que_chan->remote_va_num);
            return -ENODEV;
        }
        *size = mem_node[0].va_node.len;
    }
    return 0;
}

static inline u64 _queue_chan_get_total_va_size(struct queue_chan *que_chan, enum queue_dma_side side)
{
    return (side == QUEUE_DMA_LOCAL) ? que_chan->local_va_len : que_chan->remote_total_va_len;
}

static void _queue_chan_dma_update_index(u64 src_dma_size, u64 dst_dma_size,
    u64 *src_dma_index, u64 *dst_dma_index)
{
    if (src_dma_size < dst_dma_size) {
        (*src_dma_index)++;
    } else if (src_dma_size > dst_dma_size) {
        (*dst_dma_index)++;
    } else {
        (*src_dma_index)++;
        (*dst_dma_index)++;
    }
}

static void _queue_chan_dma_update_offset(u64 src_dma_size, u64 dst_dma_size,
    u64 *src_offset, u64 *dst_offset)
{
    if (src_dma_size < dst_dma_size) {
        *src_offset = 0;
        *dst_offset += src_dma_size;
    } else if (src_dma_size > dst_dma_size) {
        *dst_offset = 0;
        *src_offset += dst_dma_size;
    } else {
        *dst_offset = 0;
        *src_offset = 0;
    }
}

static inline void queue_chan_dma_node_attr_pack(struct queue_chan *que_chan,
    struct queue_chan_dma_node_attr *dma_node_attr)
{
    if (dma_node_attr->dir == DEVDRV_DMA_HOST_TO_DEVICE) {
        dma_node_attr->src_page_size = que_chan->attr.remote_page_size;
        dma_node_attr->dst_page_size = KA_MM_PAGE_SIZE;
    } else { /* DEVDRV_DMA_DEVICE_TO_HOST */
        dma_node_attr->src_page_size = KA_MM_PAGE_SIZE;
        dma_node_attr->dst_page_size = que_chan->attr.remote_page_size;
    }
    dma_node_attr->loc_passid = que_chan->attr.loc_passid;
}

static void _queue_chan_destroy(struct queue_chan *que_chan)
{
    _queue_chan_destroy_all_dma(que_chan);
    queue_drv_kvfree(que_chan);
}

struct queue_chan *queue_chan_create(struct queue_chan_attr *attr)
{
    struct queue_chan *que_chan = queue_drv_kvmalloc(sizeof(struct queue_chan), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (que_chan == NULL) {
        queue_err("Que chan create fail. (devid=%u; hostpid=%d; qid=%u; size=%ld)\n",
            attr->devid, attr->host_pid, attr->qid, sizeof(struct queue_chan));
        return NULL;
    }
    que_chan->attr = *attr;
    que_chan->enque = NULL;

    /* On host side, remote page size is not used */
    que_chan->remote_page_size = attr->remote_page_size;
    que_chan->remote_total_va_num = 0;
    que_chan->remote_total_va_len = 0;
    que_chan->remote_total_dma_blk_num = 0;
    que_chan->remote_dma_blk_num = 0;
    que_chan->remote_va_num = 0;
    que_chan->remote_mem_node = NULL;

    que_chan->local_total_va_num = 0;
    que_chan->local_va_len = 0;
    que_chan->local_dma_blk_num = 0;
    que_chan->local_va_num = 0;
    que_chan->local_chan_dma = NULL;

    ka_task_sema_init(&que_chan->tx_complete, 0);
    return que_chan;
}

void queue_chan_destroy(struct queue_chan *que_chan)
{
    if (ka_likely(que_chan != NULL)) {
        _queue_chan_destroy(que_chan);
    }
}

int queue_chan_dma_create(struct queue_chan *que_chan, u32 local_total_va_num)
{
    size_t size = (size_t)local_total_va_num * sizeof(struct queue_chan_dma);
    struct queue_chan_dma *chan_dma = NULL;

    if (ka_unlikely((local_total_va_num == 0) || (size < (size_t)local_total_va_num))) {
        queue_err("Local total va num invalid. (local_total_va_num=%u; size=%ld)\n", local_total_va_num, size);
        return -EINVAL;
    }

    chan_dma = queue_chan_alloc_node(que_chan, size);
    if (ka_unlikely(chan_dma == NULL)) {
        queue_err("Local chan dma alloc fail. (total_va_node_num=%u; size=%ld)\n",
            local_total_va_num, sizeof(struct queue_chan_dma));
        return -ENOMEM;
    }
    que_chan->local_chan_dma = chan_dma;
    que_chan->local_total_va_num = local_total_va_num;
    return 0;
}

int queue_chan_iovec_get(struct queue_chan *que_chan, struct iovec_info *iovec, u32 num, u32 *real_num)
{
    u64 mem_node_num = que_chan->remote_total_va_num + que_chan->remote_total_dma_blk_num;
    unsigned long stamp = ka_jiffies;
    u64 mem_node_index = 0;
    u32 iovec_index = 0;

    for (; (mem_node_index < mem_node_num) && (iovec_index < num);) {
        struct queue_chan_mem_node *mem_node = &que_chan->remote_mem_node[mem_node_index];

        if (ka_unlikely(mem_node->mem_node_type != QUEUE_CHAN_MEM_NODE_VA)) {
            queue_err("Mem node type invalid. (i=%llu; type=%d)\n", mem_node_index, mem_node->mem_node_type);
            return -ENODEV;
        }

        if (mem_node_index == 0) {
            /* First mem node is ctx addr. skip it */
            mem_node_index += mem_node->va_node.blks_num + 1;
            continue;
        }
        iovec[iovec_index].iovec_base = (void *)(uintptr_t)mem_node->va_node.va;
        iovec[iovec_index].len = mem_node->va_node.len;
        mem_node_index += mem_node->va_node.blks_num + 1;
        iovec_index++;
        queue_try_cond_resched(&stamp);
    }
    if (ka_likely(real_num != NULL)) {
        *real_num = iovec_index;
    }
    return 0;
}

static int queue_chan_enque_va_node_pack(struct queue_chan *que_chan, struct queue_chan_dma *chan_dma, int time_out)
{
    struct queue_chan_enque *enque = que_chan->enque;
    u64 id;
    int ret;

    id = (u64)enque->va_node_num + enque->dma_node_num;
    if (id == enque->max_mem_node_num) {
        ret = _queue_chan_send(que_chan, time_out);
        if (ret != 0) {
            return ret;
        }
        id = 0;
    }
    enque->mem_node[id].mem_node_type = QUEUE_CHAN_MEM_NODE_VA;
    enque->mem_node[id].va_node.va = chan_dma->dma_list.va;
    enque->mem_node[id].va_node.len = chan_dma->dma_list.len;
    enque->mem_node[id].va_node.blks_num = chan_dma->dma_list.blks_num;
    enque->va_node_num++;
    return 0;
}

static int queue_chan_enque_dma_pack(struct queue_chan *que_chan, struct queue_chan_dma *chan_dma,
    u64 dma_blk_index, int time_out)
{
    struct queue_chan_enque *enque = que_chan->enque;
    u64 index;

    index = (u64)enque->va_node_num + enque->dma_node_num;
    if (index == enque->max_mem_node_num) {
        int ret = _queue_chan_send(que_chan, time_out);
        if (ret != 0) {
            return ret;
        }
        index = 0;
    }
    enque->mem_node[index].mem_node_type = QUEUE_CHAN_MEM_NODE_DMA;
    enque->mem_node[index].dma_node.dma = chan_dma->dma_list.blks[dma_blk_index].dma;
    enque->mem_node[index].dma_node.size = chan_dma->dma_list.blks[dma_blk_index].sz;
    enque->dma_node_num++;
    return 0;
}

int queue_chan_send(struct queue_chan *que_chan, int time_out)
{
    unsigned long va_stamp;
    int ret;
    u64 i;

    if (ka_unlikely(que_chan->attr.send == NULL)) {
        queue_err("Send is not supported.\n");
        return -ENOSPC;
    }
    que_chan->enque = queue_chan_enque_create(que_chan);
    if (ka_unlikely(que_chan->enque == NULL)) {
        return -ENOMEM;
    }

    va_stamp = ka_jiffies;
    for (i = 0; i < que_chan->local_va_num; i++) {
        struct queue_chan_dma *chan_dma = &que_chan->local_chan_dma[i];
        unsigned long dma_stamp = ka_jiffies;
        u64 blk_index;
        ret = queue_chan_enque_va_node_pack(que_chan, chan_dma, time_out);
        if (ka_unlikely(ret != 0)) {
            goto out;
        }
        for (blk_index = 0; blk_index < chan_dma->dma_list.blks_num; blk_index++) {
            ret = queue_chan_enque_dma_pack(que_chan, chan_dma, blk_index, time_out);
            if (ka_unlikely(ret != 0)) {
                goto out;
            }
            queue_try_cond_resched(&dma_stamp);
        }
        queue_try_cond_resched(&va_stamp);
    }
    ret = _queue_chan_send(que_chan, time_out);
out:
    _queue_chan_enque_destroy(que_chan->enque);
    que_chan->enque = NULL;
    return ret;
}

int queue_chan_mem_node_create(struct queue_chan *que_chan, u32 total_va_num, u64 total_va_len, u64 total_dma_blk_num)
{
    u64 mem_node_num = total_va_num + total_dma_blk_num;
    size_t size = (size_t)mem_node_num * sizeof(struct queue_chan_mem_node);

    if (ka_unlikely(size < (size_t)mem_node_num)) {
        queue_err("Mem node size overflow. (size=%ld; mem_node_num=%llu)\n", size, mem_node_num);
        return -EOVERFLOW;
    }

    que_chan->remote_mem_node = queue_chan_alloc_node(que_chan, size);
    if (ka_unlikely(que_chan->remote_mem_node == NULL)) {
        queue_err("Remote mem node alloc fail. (devid=%u; hostpid=%d; qid=%u; mem_node_num=%llu; size=%ld)\n",
            que_chan->attr.devid, que_chan->attr.host_pid, que_chan->attr.qid, mem_node_num,
            sizeof(struct queue_chan_mem_node));
        return -ENOMEM;
    }
    que_chan->remote_total_va_num = total_va_num;
    que_chan->remote_total_va_len = total_va_len;
    que_chan->remote_total_dma_blk_num = total_dma_blk_num;
    return 0;
}

static int queue_chan_enque_add_check(struct queue_chan *que_chan, struct queue_chan_enque *enque)
{
    u64 mem_node_num;

    if (ka_unlikely(((enque->dma_node_num + que_chan->remote_dma_blk_num) > que_chan->remote_total_dma_blk_num) ||
        ((enque->dma_node_num + que_chan->remote_dma_blk_num) < que_chan->remote_dma_blk_num))) {
        queue_err("dma_node_num=%u; remote_dma_blk_num=%llu; remote_total_dma_blk_num=%llu\n",
            enque->dma_node_num, que_chan->remote_dma_blk_num, que_chan->remote_total_dma_blk_num);
        return -EINVAL;
    }

    if (ka_unlikely(((enque->va_node_num + que_chan->remote_va_num) > que_chan->remote_total_va_num) ||
        ((enque->va_node_num + que_chan->remote_va_num) < que_chan->remote_va_num))) {
        queue_err("va_node_num=%u; remote_va_num=%u; remote_total_dma_blk_num=%u\n",
            enque->va_node_num, que_chan->remote_va_num, que_chan->remote_va_num);
        return -EINVAL;
    }
    mem_node_num = enque->va_node_num + enque->dma_node_num;
    if (ka_unlikely((mem_node_num == 0) || (mem_node_num < enque->va_node_num) || (mem_node_num < enque->dma_node_num))) {
        queue_err("mem_node_num=%llu; va_node_num=%u; dma_node_num=%u\n",
            mem_node_num, enque->va_node_num, enque->dma_node_num);
        return -EINVAL;
    }

    return 0;
}

int queue_chan_enque_add(struct queue_chan *que_chan, struct queue_chan_enque *enque)
{
    u64 remain_dma_blk_num, remain_va_num, remain_mem_node_num, mem_node_num;
    struct queue_chan_mem_node *mem_node = NULL;
    int ret;

    ret = queue_chan_enque_add_check(que_chan, enque);
    if (ka_unlikely(ret != 0)) {
        return ret;
    }
    mem_node = _queue_get_mem_node(que_chan);
    if (ka_unlikely(mem_node == NULL)) {
        return -EACCES;
    }
    remain_dma_blk_num = que_chan->remote_total_dma_blk_num - que_chan->remote_dma_blk_num;
    remain_va_num = que_chan->remote_total_va_num - que_chan->remote_va_num;
    mem_node_num = enque->va_node_num + enque->dma_node_num;
    remain_mem_node_num = remain_va_num + remain_dma_blk_num;
    ret = memcpy_s(mem_node, remain_mem_node_num * sizeof(struct queue_chan_mem_node),
        enque->mem_node, mem_node_num * sizeof(struct queue_chan_mem_node));
    if (ka_unlikely(ret != EOK)) {
        queue_err("Memcpy fail. (remain_mem_node_num=%llu; mem_node_num=%llu)\n", remain_mem_node_num, mem_node_num);
        return ret;
    }
    que_chan->remote_va_num += enque->va_node_num;
    que_chan->remote_dma_blk_num += enque->dma_node_num;
    return 0;
}

bool queue_chan_enque_add_finished(struct queue_chan *que_chan)
{
    return (que_chan->remote_dma_blk_num == que_chan->remote_total_dma_blk_num) &&
        (que_chan->remote_va_num == que_chan->remote_total_va_num);
}

static struct devdrv_dma_node *queue_chan_dma_node_create(struct queue_chan *que_chan)
{
    u64 total_blks_num = que_chan->remote_dma_blk_num + que_chan->local_dma_blk_num;
    u64 size = total_blks_num * sizeof(struct devdrv_dma_node);
    struct devdrv_dma_node *dma_node = NULL;

    if (ka_unlikely((total_blks_num < que_chan->remote_dma_blk_num) || (total_blks_num < que_chan->local_dma_blk_num) ||
        (size < total_blks_num))) {
#ifndef EMU_ST
        return NULL;
#endif
    }
    dma_node = queue_chan_alloc_node(que_chan, size);
    if (dma_node == NULL) {
        queue_err("Create dma node fail. (remote_blks_num=%llu; local_blks_num=%llu; size=%ld)\n",
            que_chan->remote_dma_blk_num, que_chan->local_dma_blk_num, sizeof(struct devdrv_dma_node));
    }
    return dma_node;
}

static void queue_chan_dma_node_destroy(struct devdrv_dma_node *dma_node)
{
    queue_drv_kvfree(dma_node);
}

static int queue_chan_dma_node_pack_remote_to_local(struct queue_chan_dma_node_attr *attr,
    struct devdrv_dma_node *dma_node, u64 num, u64 *real_num)
{
    struct queue_chan_mem_node *src_va_mem_node = attr->mem_node;
    struct queue_chan_dma *dst_chan_dma = attr->chan_dma;
    u64 src_dma_index = 0, dst_dma_index = 0;
    u64 src_offset, dst_offset, left_size;
    u64 index;

    if ((dst_chan_dma->dma_list.blks_num == 0) || (src_va_mem_node->va_node.blks_num == 0)) {
        return 0;
    }
    left_size = src_va_mem_node->va_node.len;
    src_offset = (src_va_mem_node->va_node.va & (attr->src_page_size - 1));
    dst_offset = (dst_chan_dma->dma_list.va & (attr->dst_page_size - 1));

    for (index = 0; (index < num) && (src_dma_index < src_va_mem_node->va_node.blks_num) &&
        (dst_dma_index < dst_chan_dma->dma_list.blks_num) && (left_size != 0); index++) {
        struct queue_dma_block *dst_dma_blk = &dst_chan_dma->dma_list.blks[dst_dma_index];
        struct queue_chan_mem_node *src_dma_mem_node = &src_va_mem_node[1 + src_dma_index];
        u64 src_dma_size, dst_dma_size;

        dma_node[index].direction = attr->dir;
        dma_node[index].src_addr = src_dma_mem_node->dma_node.dma + src_offset;
        dma_node[index].dst_addr = dst_dma_blk->dma + dst_offset;
        src_dma_size = ka_base_min_t(u64, src_dma_mem_node->dma_node.size - src_offset, left_size);
        dst_dma_size = ka_base_min_t(u64, dst_dma_blk->sz - dst_offset, left_size);
        dma_node[index].size = ka_base_min_t(u64, src_dma_size, dst_dma_size);
        dma_node[index].loc_passid = (u32)attr->loc_passid;
        _queue_chan_dma_update_index(src_dma_size, dst_dma_size, &src_dma_index, &dst_dma_index);
        _queue_chan_dma_update_offset(src_dma_size, dst_dma_size, &src_offset, &dst_offset);
        left_size -= dma_node[index].size;
    }
    *real_num += index;
    if (left_size != 0) {
        queue_err("Pack dma node fail. (left_size=%llu; i=%llu; srv_va=0x%pK; src_index=%llu "
            "dst_va=0x%pK; dst_index=%llu; passid=%d)\n",
            left_size, index, (void *)(uintptr_t)src_va_mem_node->va_node.va, src_dma_index,
            (void *)(uintptr_t)dst_chan_dma->dma_list.va, dst_dma_index, attr->loc_passid);
        return -ENODEV;
    }
    return 0;
}

static int queue_chan_dma_node_pack_local_to_remote(struct queue_chan_dma_node_attr *attr,
    struct devdrv_dma_node *dma_node, u64 num, u64 *real_num)
{
    struct queue_chan_mem_node *dst_va_mem_node = attr->mem_node;
    struct queue_chan_dma *src_chan_dma = attr->chan_dma;
    u64 src_dma_index = 0, dst_dma_index = 0;
    u64 src_offset, dst_offset, left_size;
    u64 index;

    if ((src_chan_dma->dma_list.blks_num == 0) || (dst_va_mem_node->va_node.blks_num == 0)) {
        return 0;
    }
    left_size = src_chan_dma->dma_list.len;
    src_offset = (src_chan_dma->dma_list.va & (attr->src_page_size - 1));
    dst_offset = (dst_va_mem_node->va_node.va & (attr->dst_page_size - 1));

    for (index = 0; (index < num) && (src_dma_index < src_chan_dma->dma_list.blks_num) &&
        (dst_dma_index < dst_va_mem_node->va_node.blks_num) && (left_size != 0); index++) {
        struct queue_dma_block *src_dma_blk = &src_chan_dma->dma_list.blks[src_dma_index];
        struct queue_chan_mem_node *dst_dma_mem_node = &dst_va_mem_node[1 + dst_dma_index];
        u64 src_dma_size, dst_dma_size;

        dma_node[index].direction = attr->dir;
        dma_node[index].src_addr = src_dma_blk->dma + src_offset;
        dma_node[index].dst_addr = dst_dma_mem_node->dma_node.dma + dst_offset;
        src_dma_size = ka_base_min_t(u64, src_dma_blk->sz - src_offset, left_size);
        dst_dma_size = ka_base_min_t(u64, dst_dma_mem_node->dma_node.size - dst_offset, left_size);
        dma_node[index].size = ka_base_min_t(u64, src_dma_size, dst_dma_size);
        dma_node[index].loc_passid = (u32)attr->loc_passid;
        _queue_chan_dma_update_index(src_dma_size, dst_dma_size, &src_dma_index, &dst_dma_index);
        _queue_chan_dma_update_offset(src_dma_size, dst_dma_size, &src_offset, &dst_offset);
        left_size -= dma_node[index].size;
    }
    *real_num += index;
    if (left_size != 0) {
        queue_err("Pack dma node fail. (left_size=%llu; i=%llu; srv_va=0x%pK; src_index=%llu "
            "dst_va=0x%pK; dst_index=%llu; passid=%d)\n",
            left_size, index, (void *)(uintptr_t)src_chan_dma->dma_list.va, src_dma_index,
            (void *)(uintptr_t)dst_va_mem_node->va_node.va, dst_dma_index, attr->loc_passid);
        return -ENODEV;
    }
    return 0;
}

typedef int (*queue_chan_dma_node_pack_t)(struct queue_chan_dma_node_attr *attr,
    struct devdrv_dma_node *dma_node, u64 num, u64 *real_num);

static int _queue_chan_copy(struct queue_chan *que_chan, enum devdrv_dma_direction dir)
{
    struct queue_chan_dma_node_attr dma_node_attr = {.dir = dir};
    u64 mem_node_index, total_blks_num, total_mem_node_num;
    struct devdrv_dma_node *dma_node = NULL;
    queue_chan_dma_node_pack_t func;
    u64 real_blks_num = 0;
    unsigned long stamp;
    u32 chan_dma_index;
    int ret;

    if ((que_chan->remote_dma_blk_num == 0) && (que_chan->local_dma_blk_num == 0)) {
        /* ctx_addr == 0 or ctx_size == 0, should return 0 */
        return 0;
    }

    total_blks_num = que_chan->remote_dma_blk_num + que_chan->local_dma_blk_num;
    if (ka_unlikely((total_blks_num == 0) || (total_blks_num < que_chan->remote_dma_blk_num) ||
        (total_blks_num < que_chan->local_dma_blk_num))) {
        queue_err("Total dma blk num invalid. (remote_dma_blk_num=%llu; local_dma_blk_num=%llu)\n",
            que_chan->remote_dma_blk_num, que_chan->local_dma_blk_num);
        return -EOVERFLOW;
    }

    dma_node = queue_chan_dma_node_create(que_chan);
    if (dma_node == NULL) {
        queue_err("Dma node create fail. (total_blks_num=%llu)\n", total_blks_num);
        return -ENOMEM;
    }

    func = (dir == DEVDRV_DMA_HOST_TO_DEVICE) ? queue_chan_dma_node_pack_remote_to_local :
        queue_chan_dma_node_pack_local_to_remote;

    /* Validity has been checked before mem node create */
    total_mem_node_num = que_chan->remote_total_va_num +  que_chan->remote_total_dma_blk_num;

    stamp = ka_jiffies;
    queue_chan_dma_node_attr_pack(que_chan, &dma_node_attr);

    for (chan_dma_index = 0, mem_node_index = 0; (chan_dma_index < que_chan->local_va_num) &&
        (mem_node_index < total_mem_node_num); chan_dma_index++) {
        struct queue_chan_dma *chan_dma = &que_chan->local_chan_dma[chan_dma_index];
        struct queue_chan_mem_node *mem_node = &que_chan->remote_mem_node[mem_node_index];
        dma_node_attr.chan_dma = chan_dma;
        dma_node_attr.mem_node = mem_node;
        ret = func(&dma_node_attr, &dma_node[real_blks_num], total_blks_num - real_blks_num,
            &real_blks_num);
        if (ka_unlikely(ret != 0)) {
            queue_err("Dma node pack fail. (ret=%d; dir=%d; chan_dma_index=%u; mem_node_index=%llu)\n",
                ret, dir, chan_dma_index, mem_node_index);
            goto out;
        }
        mem_node_index += mem_node->va_node.blks_num + 1; /* jump to next QUEUE_CHAN_MEM_NODE_VA mem node */
        queue_try_cond_resched(&stamp);
    }

    ret = (real_blks_num != 0) ? queue_dma_sync_link_copy(que_chan->attr.devid, dma_node, real_blks_num) : -EFAULT;
    if (ka_unlikely(ret != 0)) {
        queue_err("Dma dma_sync_link_copy fail. (ret=%d; dir=%d; passid=%d)\n", ret, dir, que_chan->attr.loc_passid);
    }
out:
    queue_chan_dma_node_destroy(dma_node);
    return ret;
}

/* Only device can call */
int queue_chan_copy(struct queue_chan *que_chan, enum devdrv_dma_direction dir)
{
    return _queue_chan_copy(que_chan, dir);
}

static int _queue_chan_copy_addr_add(struct queue_chan *que_chan, u64 va, u64 len, u64 ctx_addr, u64 ctx_len)
{
    u64 mem_node_num = que_chan->remote_total_va_num + que_chan->remote_total_dma_blk_num;
    u64 prev_chan_dma_va = va, prev_chan_dma_len = 0;
    unsigned long stamp = ka_jiffies;
    u64 remain_len = len;
    int ret = -ENODEV;
    u64 i;

    for (i = 0; i < mem_node_num;) {
        struct queue_chan_mem_node *mem_node = &que_chan->remote_mem_node[i];
        struct queue_chan_iovec iovec;

        if (ka_unlikely(mem_node->mem_node_type != QUEUE_CHAN_MEM_NODE_VA)) {
            queue_err("Mem node type invalid. (i=%llu; type=%d)\n", i, mem_node->mem_node_type);
            return -ENODEV;
        }

        if (i == 0) {
            iovec.va = (mem_node->va_node.va == 0) ? 0 : ctx_addr;
            iovec.len = mem_node->va_node.len;
        } else {
            iovec.va = (prev_chan_dma_va + prev_chan_dma_len);
            iovec.len = ka_base_min_t(u64, remain_len, mem_node->va_node.len);
            remain_len -= iovec.len;
            prev_chan_dma_va = iovec.va;
            prev_chan_dma_len = iovec.len;
        }
        iovec.dma_flag = (mem_node->va_node.blks_num == 0) ? false : true;
        ret = queue_chan_iovec_add(que_chan, &iovec);
        if (ret != 0) {
            return ret;
        }
        if (remain_len == 0) {
            break;
        }
        i += mem_node->va_node.blks_num + 1; /* jump to next QUEUE_CHAN_MEM_NODE_VA mem node */
        queue_try_cond_resched(&stamp);
    }
    return ret;
}

static int _queue_chan_copy_addr_check(struct queue_chan *que_chan, u32 op, u64 len, u64 ctx_len)
{
    u64 remote_ctx_size = 0;
    int ret;

    if (ka_unlikely((que_chan->remote_va_num < QUEUE_CHAN_MIN_VA_NUM) ||
        (que_chan->remote_va_num > QUEUE_MAX_VA_NUM))) {
        queue_err("Remote va num invalid. (remote_va_num=%u)\n", que_chan->remote_va_num);
        return -EINVAL;
    }

    if (ka_unlikely((que_chan->attr.remote_page_size != QUEUE_CHAN_PAGE_SIZE_4K) &&
        (que_chan->attr.remote_page_size != QUEUE_CHAN_PAGE_SIZE_64K))) {
        queue_err("Remote page size invalid. (remote_page_size=%u)\n", que_chan->attr.remote_page_size);
        return -EINVAL;
    }

    ret = _queue_chan_get_ctx_size(que_chan, QUEUE_DMA_REMOTE, &remote_ctx_size);
    if (ret != 0) {
        queue_err("Get ctx size fail. (ret=%d)\n", ret);
        return ret;
    }
    if (remote_ctx_size > ctx_len) {
        queue_err("Invalid copy size. (remote_ctx_size=%llu; ctx_len=%llu)\n", remote_ctx_size, ctx_len);
        return -EINVAL;
    }
    if (que_chan->attr.memory_type == QUEUE_BUFF) {
        u64 remote_total_va_size = _queue_chan_get_total_va_size(que_chan, QUEUE_DMA_REMOTE);
        if (remote_total_va_size < remote_ctx_size) {
            queue_err("Total va size invalid. (remote_total_va_size=%llu; remote_ctx_size=%llu)\n",
                remote_total_va_size, remote_ctx_size);
            return -EINVAL;
        }
        remote_total_va_size -= remote_ctx_size;
        if ((op == QUEUE_ENQUEUE_FLAG) && (remote_total_va_size != len)) {
            queue_err("Enque va size invalid. (remote_total_va_size=%llu; len=%llu)\n", remote_total_va_size, len);
            return -EINVAL;
        }
        if ((op == QUEUE_DEQUEUE_FLAG) && (remote_total_va_size < len)) {
            queue_err("Deque va size invalid. (remote_total_va_size=%llu; len=%llu)\n", remote_total_va_size, len);
            return -EINVAL;
        }
    }
    return 0;
}

int queue_chan_copy_addr_add(struct queue_chan *que_chan, struct queue_chan_copy_addr_attr *attr)
{
    int ret = _queue_chan_copy_addr_check(que_chan, attr->op, attr->len, attr->ctx_len);
    if (ret == 0) {
        ret = _queue_chan_copy_addr_add(que_chan, attr->va, attr->len, attr->ctx_addr, attr->ctx_len);
    }
    return ret;
}

int queue_chan_wait(struct queue_chan *que_chan, int timeout)
{
    return ka_task_down_timeout(&que_chan->tx_complete, (long)ka_system_msecs_to_jiffies((unsigned int)timeout));
}

void queue_chan_wake_up(struct queue_chan *que_chan)
{
    ka_task_up(&que_chan->tx_complete);
}

int queue_chan_get_iovec_size(struct queue_chan *que_chan, enum queue_dma_side side, u64 *size)
{
    u64 total_size, ctx_size = KA_ULLONG_MAX;
    int ret;

    ret = _queue_chan_get_ctx_size(que_chan, side, &ctx_size);
    if (ret != 0) {
        return ret;
    }
    total_size = _queue_chan_get_total_va_size(que_chan, side);
    if (total_size < ctx_size) {
        queue_err("Va size invalid. (total_size=%llu; ctx_size=%llu)\n", total_size, ctx_size);
        return -EFAULT;
    }
    total_size -= ctx_size;
    if (size != NULL) {
        *size = total_size;
    }
    return 0;
}

int queue_chan_get_iovec_num(struct queue_chan *que_chan, enum queue_dma_side side, u32 *num)
{
    u32 iovec_num = _queue_chan_get_va_num(que_chan, side);
    if (iovec_num == 0) {
        queue_err("Iovec num invalid.\n");
        return -EFAULT;
    }
    if (num != NULL) {
        *num = iovec_num - 1;
    }
    return 0;
}

