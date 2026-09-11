/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
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
 * Create: 2022-08-13
 */

#define pr_fmt(fmt) "XSMEM_VMA: <%s:%d> " fmt, __func__, __LINE__

#include "xsmem_framework_log.h"
#include "multi_rbtree.h"
#include "xsmem_framework.h"
#include "xsmem_algo_vma.h"
#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_define.h"
#include "ka_errno_pub.h"
#include "ka_dfx_pub.h"
#include "ka_fs_pub.h"
#include "ka_memory_pub.h"
#include "ka_driver_pub.h"

#define XSMEM_VMA_ALIGN KA_MM_PAGE_SIZE

struct vma_ctrl {
    unsigned long       total_size;
    unsigned long       free_size;
    unsigned long       alloc_node_cnt;

    ka_rb_root_t      idle_size_tree;
    ka_rb_root_t      idle_va_tree;
    ka_mutex_t mutex;
};

struct vma_node_data {
    unsigned long       start;
    unsigned long       size;
};

struct vma_node {
    struct multi_rb_node      va_node;
    struct multi_rb_node      size_node;

    struct vma_node_data       data;
};

static struct vma_node *vma_node_get_from_idle_size_tree(u64 size, ka_rb_root_t *idle_size_tree)
{
    struct multi_rb_node *node = NULL;

    node = multi_rbtree_get(idle_size_tree, size);
    if (node != NULL) {
        return ka_base_rb_entry(node, struct vma_node, size_node);
    }

    node = multi_rbtree_get_upper_bound(idle_size_tree, size);
    if (node == NULL) {
        /* can not find upper bound node with the key */
        return NULL;
    }
    return ka_base_rb_entry(node, struct vma_node, size_node);
}

static void vma_erase_idle_va_tree(struct vma_node *node, ka_rb_root_t *idle_va_tree)
{
    multi_rbtree_erase(idle_va_tree, &node->va_node);
}

static void vma_erase_idle_size_tree(struct vma_node *node, ka_rb_root_t *idle_size_tree)
{
    multi_rbtree_erase(idle_size_tree, &node->size_node);
}

static void vma_insert_idle_va_tree(struct vma_node *node, ka_rb_root_t *idle_va_tree)
{
    multi_rbtree_insert(idle_va_tree, &node->va_node, node->data.start);
}

static void vma_insert_idle_size_tree(struct vma_node *node, ka_rb_root_t *idle_size_tree)
{
    multi_rbtree_insert(idle_size_tree, &node->size_node, node->data.size);
}

static void vma_update_idle_size_tree(ka_rb_root_t *idle_size_tree,
    struct vma_node *node, unsigned long size)
{
    vma_erase_idle_size_tree(node, idle_size_tree);
    node->data.size += size;
    vma_insert_idle_size_tree(node, idle_size_tree);
}

static void vma_update_idle_va_size_tree(ka_rb_root_t *idle_va_tree, ka_rb_root_t *idle_size_tree,
    struct vma_node *node, unsigned long size)
{
    vma_erase_idle_size_tree(node, idle_size_tree);
    vma_erase_idle_va_tree(node, idle_va_tree);
    node->data.start -= size;
    node->data.size += size;
    vma_insert_idle_size_tree(node, idle_size_tree);
    vma_insert_idle_va_tree(node, idle_va_tree);
}

void *vma_inst_create(unsigned long pool_size)
{
    struct vma_ctrl *ctrl = NULL;
    struct vma_node *node = NULL;

    if ((KA_MM_IS_ALIGNED(pool_size, XSMEM_VMA_ALIGN) == 0) || (pool_size == 0)) {
        xsmem_err("Input xsm_pool size invalid. (pool_size=%lx)\n", pool_size);
        return NULL;
    }

    ctrl = xsmem_drv_kmalloc(sizeof(struct vma_ctrl), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (ka_unlikely(ctrl == NULL)) {
        return NULL;
    }

    node = xsmem_drv_kvzalloc(sizeof(struct vma_node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (ka_unlikely(node == NULL)) {
        xsmem_drv_kfree(ctrl);
        return NULL;
    }

    ctrl->total_size = pool_size;
    ctrl->free_size = pool_size;
    ctrl->alloc_node_cnt = 0;
    ctrl->idle_va_tree = KA_RB_ROOT;
    ctrl->idle_size_tree = KA_RB_ROOT;
    ka_task_mutex_init(&ctrl->mutex);

    node->data.start = 0;
    node->data.size = pool_size;
    vma_insert_idle_va_tree(node, &ctrl->idle_va_tree);
    vma_insert_idle_size_tree(node, &ctrl->idle_size_tree);
    return (void *)ctrl;
}

static int vma_algo_pool_init(struct xsm_pool *xp, struct xsm_reg_arg *arg)
{
    void *ctrl = NULL;

    ctrl = vma_inst_create(arg->pool_size);
    if (ctrl == NULL) {
        return -EINVAL;
    }

    xp->private = ctrl;
    return 0;
}

int vma_inst_destroy(void *vma_ctrl)
{
    struct vma_ctrl *ctrl = (struct vma_ctrl *)vma_ctrl;
    struct multi_rb_node *tmp = multi_rbtree_get_node_from_rb_node(ctrl->idle_va_tree.rb_node);
    struct vma_node *node = ka_base_rb_entry(tmp, struct vma_node, va_node);

    if ((ctrl->total_size != ctrl->free_size) || (ctrl->total_size != node->data.size)) {
        ka_dfx_pr_notice("Not all node free. (total_size=%lu; free_size=%lu; node_size=%lu)\n",
            ctrl->total_size, ctrl->free_size, node->data.size);
        vma_algo_show(vma_ctrl, NULL);
    }

    ka_task_mutex_destroy(&ctrl->mutex);
    xsmem_drv_kvfree(node);
    xsmem_drv_kfree(ctrl);
    return 0;
}

static int vma_algo_pool_free(struct xsm_pool *xp)
{
    vma_inst_destroy(xp->private);
    xp->private = NULL;
    return 0;
}

static int vma_alloc_block(ka_rb_root_t *idle_size_tree, ka_rb_root_t *idle_va_tree,
    unsigned long size, unsigned long *start)
{
    struct vma_node *node = NULL;

    node = vma_node_get_from_idle_size_tree(size, idle_size_tree);
    if (node == NULL) {
        return -ENOMEM;
    }

    *start = node->data.start;
    vma_erase_idle_size_tree(node, idle_size_tree);
    vma_erase_idle_va_tree(node, idle_va_tree);
    if (node->data.size == size) {
        xsmem_drv_kvfree(node);
    } else {
        node->data.start += size;
        node->data.size -= size;
        vma_insert_idle_size_tree(node, idle_size_tree);
        vma_insert_idle_va_tree(node, idle_va_tree);
    }

    return 0;
}

int vma_algo_alloc(void *vma_ctrl, unsigned long alloc_size,
    unsigned long *addr, unsigned long *real_size)
{
    struct vma_ctrl *ctrl = (struct vma_ctrl *)vma_ctrl;
    unsigned long real_size_tmp, offset;
    int ret;

    if (alloc_size == 0) {
        xsmem_err("alloc size %lx invalid\n", alloc_size);
        return -EINVAL;
    }

    real_size_tmp = KA_DRIVER_ALIGN(alloc_size, XSMEM_VMA_ALIGN);
    if (real_size_tmp < alloc_size) {
        return -EOVERFLOW;
    }

    ka_task_mutex_lock(&ctrl->mutex);
    ka_dfx_pr_debug("Alloc info. (alloc_size=%lu; real_size=%lu; free_size=%lu)\n",
        alloc_size, real_size_tmp, ctrl->free_size);
    if (real_size_tmp > ctrl->free_size) {
        ka_dfx_pr_notice("Can not alloc. (alloc_size=%lu; real_size=%lu; free_size=%lu)\n",
            alloc_size, real_size_tmp, ctrl->free_size);
        ka_task_mutex_unlock(&ctrl->mutex);
        return -ENOSPC;
    }

    ret = vma_alloc_block(&ctrl->idle_size_tree, &ctrl->idle_va_tree, real_size_tmp, &offset);
    if (ret == 0) {
        ctrl->free_size -= real_size_tmp;
        ctrl->alloc_node_cnt++;
        *real_size = real_size_tmp;
        *addr = offset;
        ka_dfx_pr_debug("Alloc success. (alloc_size=%lu; real_size=%lu; free_size=%lu; start=0x%pK)\n",
            alloc_size, real_size_tmp, ctrl->free_size, (void *)(uintptr_t)offset);
    }
    ka_task_mutex_unlock(&ctrl->mutex);
    return ret;
}

static int vma_algo_block_alloc(struct xsm_pool *xp, struct xsm_block *blk)
{
    return vma_algo_alloc(xp->private, blk->alloc_size, &blk->offset, &blk->real_size);
}

static struct vma_node *prev_vma_node(ka_rb_node_t *parent, ka_rb_node_t **link)
{
    struct multi_rb_node *tmp = NULL;

    if (parent == NULL) {
        return NULL;
    }

    if (link == ka_base_get_rb_node_right_addr(parent)) {
        tmp = multi_rbtree_get_node_from_rb_node(parent);
    } else {
        /* the prev node should always exist */
        tmp = multi_rbtree_get_node_from_rb_node(ka_base_rb_prev(parent));
    }
    return ka_base_rb_entry(tmp, struct vma_node, va_node);
}

static struct vma_node *next_vma_node(ka_rb_node_t *parent, ka_rb_node_t **link)
{
    struct multi_rb_node *tmp = NULL;

    if (parent == NULL) {
        return NULL;
    }

    if (link == ka_base_get_rb_node_left_addr(parent)) {
        tmp = multi_rbtree_get_node_from_rb_node(parent);
    } else {
        /* the next node should always exist */
        tmp = multi_rbtree_get_node_from_rb_node(ka_base_rb_next(parent));
    }
    return ka_base_rb_entry(tmp, struct vma_node, va_node);
}

/* two intervals overlapped ? [l1, h1), [l2, h2) */
#define is_overlap(l1, h1, l2, h2) (((l1) < (h2)) && ((l2) < (h1)))

static int vma_free_block(ka_rb_root_t *idle_va_tree, ka_rb_root_t *idle_size_tree,
    unsigned long start, unsigned long size)
{
    ka_rb_node_t **link = ka_base_get_rb_root_node_addr(idle_va_tree);
    ka_rb_node_t *parent = NULL;
    struct vma_node *node = NULL;
    struct vma_node *prev = NULL;
    bool merged = false;

    while (*link != NULL) {
        struct multi_rb_node *tmp = NULL;

        parent = *link;
        tmp = multi_rbtree_get_node_from_rb_node(*link);
        node = ka_base_rb_entry(tmp, struct vma_node, va_node);
        if (start < node->data.start) {
            link = ka_base_get_rb_node_left_addr(parent);
        } else if (start > node->data.start) {
            link = ka_base_get_rb_node_right_addr(parent);
        } else {
            xsmem_err("Double free. (start=0x%pK; size=%lu)\n", (void *)(uintptr_t)start, size);
            return -EFAULT;
        }
    }

    prev = prev_vma_node(parent, link);
    if (prev != NULL) {
        if (prev->data.start + prev->data.size == start) {
            vma_update_idle_size_tree(idle_size_tree, prev, size);
            merged = true;
        } else if (is_overlap(start, start + size, prev->data.start, prev->data.start + prev->data.size)) {
            xsmem_err("Prev-node overlap detected. (start=0x%pK; size=%lu; prev_start=0x%pK; prev_size=0x%lx)\n",
                (void *)(uintptr_t)start, size, (void *)(uintptr_t)prev->data.start, prev->data.size);
            return -EINVAL;
        }
    }

    node = next_vma_node(parent, link);
    if (node != NULL) {
        if (start + size == node->data.start) {
            if (merged) {
                vma_erase_idle_size_tree(node, idle_size_tree);
                vma_erase_idle_va_tree(node, idle_va_tree);
                vma_update_idle_size_tree(idle_size_tree, prev, node->data.size);
                xsmem_drv_kvfree(node);
            } else {
                vma_update_idle_va_size_tree(idle_va_tree, idle_size_tree, node, size);
            }
            merged = true;
        } else if (is_overlap(start, start + size, node->data.start, node->data.start + node->data.size)) {
            xsmem_err("Next-node overlap detected. (start=0x%pK; size=%lu; next_start=0x%pK; next_size=0x%lx)\n",
                (void *)(uintptr_t)start, size, (void *)(uintptr_t)node->data.start, node->data.size);
            return -EINVAL;
        }
    }

    if (merged == false) {
        node = xsmem_drv_kvzalloc(sizeof(struct vma_node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
        if (ka_unlikely(node == NULL)) {
            xsmem_err("Alloc vma_node failed. (start=0x%pK; size=%lu)\n", (void *)(uintptr_t)start, size);
            return -ENOMEM;
        }
        node->data.start = start;
        node->data.size = size;
        vma_insert_idle_size_tree(node, idle_size_tree);
        vma_insert_idle_va_tree(node, idle_va_tree);
    }
    return 0;
}

int vma_algo_free(void *vma_ctrl, unsigned long addr, unsigned long real_size)
{
    struct vma_ctrl *ctrl = (struct vma_ctrl *)vma_ctrl;
    int ret;

    ka_task_mutex_lock(&ctrl->mutex);
    ret = vma_free_block(&ctrl->idle_va_tree, &ctrl->idle_size_tree, addr, real_size);
    if (ret == 0) {
        ctrl->alloc_node_cnt--;
        ctrl->free_size += real_size;
    }
    ka_dfx_pr_debug("Free info. (addr=0x%pK; real_size=%lu; ret=%d)\n", (void *)(uintptr_t)addr, real_size, ret);
    ka_task_mutex_unlock(&ctrl->mutex);
    return ret;
}

static int vma_algo_block_free(struct xsm_pool *xp, struct xsm_block *blk)
{
    return vma_algo_free(xp->private, blk->offset, blk->real_size);
}

void vma_algo_show(void *vma_ctrl, ka_seq_file_t *seq)
{
    struct vma_ctrl *ctrl = (struct vma_ctrl *)vma_ctrl;
    ka_rb_node_t *rbtree_node = NULL;
    int vma_free_block_cnt = 0;

    if (seq != NULL) {
        ka_fs_seq_printf(seq, "    total_size=%lu, free_size=%lu, alloc_node_cnt=%ld\n",
                ctrl->total_size, ctrl->free_size, ctrl->alloc_node_cnt);
    } else {
        ka_dfx_pr_notice("total_size=%lu, free_size=%lu, alloc_node_cnt=%ld\n",
            ctrl->total_size, ctrl->free_size, ctrl->alloc_node_cnt);
    }

    ka_task_mutex_lock(&ctrl->mutex);
    rbtree_node = ka_base_rb_first(&ctrl->idle_va_tree);
    while (rbtree_node != NULL) {
        struct multi_rb_node *tmp = multi_rbtree_get_node_from_rb_node(rbtree_node);
        struct vma_node *node = ka_base_rb_entry(tmp, struct vma_node, va_node);

        vma_free_block_cnt++;
        if (seq != NULL) {
            ka_fs_seq_printf(seq, "        free_block_%d:size=%lu start=0x%pK\n", vma_free_block_cnt,
                node->data.size, (void *)(uintptr_t)node->data.start);
        } else {
            ka_dfx_pr_notice("free_block_%d:size=%lu start=0x%pK\n", vma_free_block_cnt, node->data.size, (void *)(uintptr_t)node->data.start);
        }
        rbtree_node = ka_base_rb_next(rbtree_node);
    }
    ka_task_mutex_unlock(&ctrl->mutex);
}

static void vma_algo_pool_show(struct xsm_pool *xp, ka_seq_file_t *seq)
{
    vma_algo_show(xp->private, seq);
}

static struct xsm_pool_algo vma_algo = {
    .num = XSMEM_ALGO_VMA,
    .name = "vma_algo",
    .xsm_pool_init = vma_algo_pool_init,
    .xsm_pool_free = vma_algo_pool_free,
    .xsm_pool_show = vma_algo_pool_show,
    .xsm_block_alloc = vma_algo_block_alloc,
    .xsm_block_free = vma_algo_block_free,
};

struct xsm_pool_algo *xsm_get_vma_algo(void)
{
    return &vma_algo;
}