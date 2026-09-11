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
 */
#include <linux/types.h>

#include "devmm_proc_info.h"
#include "devmm_common.h"
#include "svm_rbtree.h"
#include "ka_base_pub.h"
#include "svm_dynamic_addr.h"

struct svm_da_node_info {
    ka_rb_node_t rbnode;

    u64 va;
    u64 size;
    ka_vm_area_struct_t *vma;
    ka_vm_area_struct_t *custom_vma;

    u32 status;
    u32 first_heap_idx;
    u32 last_heap_idx;
    struct devmm_svm_heap *heap;
};

void svm_da_init(struct devmm_svm_process *svm_proc)
{
    struct svm_da_info *da_info = &svm_proc->da_info;

    da_info->rbtree = KA_RB_ROOT;
    ka_task_rwlock_init(&da_info->rbtree_rwlock);
    ka_task_init_rwsem(&da_info->rwsem);
}

void svm_use_da(struct devmm_svm_process *svm_proc)
{
    if (svm_proc != NULL) {
        struct svm_da_info *da_info = &svm_proc->da_info;
        ka_task_down_read(&da_info->rwsem);
    }
}

void svm_unuse_da(struct devmm_svm_process *svm_proc)
{
    if (svm_proc != NULL) {
        struct svm_da_info *da_info = &svm_proc->da_info;
        ka_task_up_read(&da_info->rwsem);
    }
}

#define SVM_OCUUPY_MAX_CNT 1209600000ull /* 7 days */
void svm_occupy_da(struct devmm_svm_process *svm_proc)
{
    if (svm_proc != NULL) {
        struct svm_da_info *da_info = &svm_proc->da_info;
        u64 i;

        for (i = 0; i < SVM_OCUUPY_MAX_CNT; i++) {
            /* down_write_trylock: aviod p2p msg + mmap dead lock */
            if (ka_task_down_write_trylock(&da_info->rwsem) != 0) {
                break;
            }
#ifndef EMU_ST
            ka_system_usleep_range(500, 600); /* 500~600us */
#endif
        }
    }
}

void svm_release_da(struct devmm_svm_process *svm_proc)
{
    if (svm_proc != NULL) {
        struct svm_da_info *da_info = &svm_proc->da_info;
        ka_task_up_write(&da_info->rwsem);
    }
}

void svm_da_recycle(struct devmm_svm_process *svm_proc)
{
    struct svm_da_info *da_info = &svm_proc->da_info;
    struct svm_da_node_info *node = NULL, *tmp = NULL;

	rbtree_postorder_for_each_entry_safe(node, tmp, &da_info->rbtree, rbnode) {
        (void)devmm_rb_erase(&da_info->rbtree, &node->rbnode);
        devmm_kvfree(node);
	}
}

static void svm_da_rb_range_handle(ka_rb_node_t *rbnode, struct rb_range_handle *range_handle)
{
    struct svm_da_node_info *node = ka_base_rb_entry(rbnode, struct svm_da_node_info, rbnode);

    range_handle->start = node->va;
    range_handle->end = node->va + node->size - 1;
}

static struct svm_da_node_info *svm_da_node_find(struct svm_da_info *da_info, u64 va, u64 size)
{
    struct svm_da_node_info *node = NULL;
    ka_rb_node_t *rbnode = NULL;
    struct rb_range_handle handle;

    handle.start = va;
    handle.end = va + size - 1;

    rbnode = devmm_rb_search_by_range(&da_info->rbtree, &handle, svm_da_rb_range_handle);
    if (rbnode != NULL) {
        node = ka_base_rb_entry(rbnode, struct svm_da_node_info, rbnode);
    }

    return node;
}

static void svm_da_info_rbtree_lock(struct svm_da_info *da_info, bool is_occupy)
{
    if (is_occupy) {
        ka_task_write_lock_bh(&da_info->rbtree_rwlock);
    } else {
        ka_task_read_lock_bh(&da_info->rbtree_rwlock);
    }
}

static void svm_da_info_rbtree_unlock(struct svm_da_info *da_info, bool is_occupy)
{
    if (is_occupy) {
        ka_task_write_unlock_bh(&da_info->rbtree_rwlock);
    } else {
        ka_task_read_unlock_bh(&da_info->rbtree_rwlock);
    }
}

int svm_da_add_addr(struct devmm_svm_process *svm_proc, u64 va, u64 size, ka_vm_area_struct_t *vma)
{
    struct svm_da_info *da_info = &svm_proc->da_info;
    struct svm_da_node_info *node = NULL;
    int ret;

    if (va < DEVMM_MAX_DYN_ALLOC_BASE) {
        devmm_drv_err("Invalid vma. (vm_start=0x%llx)\n", va);
        return -EINVAL;
    }

    node = (struct svm_da_node_info *)devmm_kvzalloc(sizeof(*node));
    if (node == NULL) {
        devmm_drv_err("Kvzalloc node failed. (va=0x%llx)\n", va);
        return -ENOMEM;
    }

    node->va = va;
    node->size = size;
    node->status = DEVMM_DA_STATUS_NONE;
    node->first_heap_idx = (u32)((va - DEVMM_SVM_MEM_START) / DEVMM_HEAP_SIZE);
    node->last_heap_idx = (u32)(node->first_heap_idx + size / DEVMM_HEAP_SIZE - 1);
    node->heap = NULL;
    node->vma = vma;
    KA_BASE_RB_CLEAR_NODE(&node->rbnode);

    svm_da_info_rbtree_lock(da_info, true);
    ret = devmm_rb_insert_by_range(&da_info->rbtree, &node->rbnode, svm_da_rb_range_handle);
    svm_da_info_rbtree_unlock(da_info, true);
    if (ret != 0) {
        devmm_drv_err("Instert failed. (va=0x%llx; size=0x%llx)\n", node->va, node->size);
        devmm_kvfree(node);
        return ret;
    }

    return 0;
}

int svm_da_del_addr(struct devmm_svm_process *svm_proc, u64 va, u64 size)
{
    struct svm_da_info *da_info = &svm_proc->da_info;
    struct svm_da_node_info *node = NULL;

    svm_da_info_rbtree_lock(da_info, true);
    node = svm_da_node_find(da_info, va, size);
    if (node != NULL) {
        (void)devmm_rb_erase(&da_info->rbtree, &node->rbnode);
    }
    svm_da_info_rbtree_unlock(da_info, true);

    if (node != NULL) {
        devmm_kvfree(node);
    }

    return (node != NULL) ? 0 : -EINVAL;
}

bool svm_is_da_addr(struct devmm_svm_process *svm_proc, u64 va, u64 size)
{
    struct svm_da_info *da_info = &svm_proc->da_info;
    struct svm_da_node_info *node = NULL;

    svm_da_info_rbtree_lock(da_info, false);
    node = svm_da_node_find(da_info, va, size);
    svm_da_info_rbtree_unlock(da_info, false);

    return (node != NULL);
}

bool svm_is_da_match(struct devmm_svm_process *svm_proc, u64 va, u64 size)
{
    struct svm_da_info *da_info = &svm_proc->da_info;
    struct svm_da_node_info *node = NULL;
    bool match = false;

    svm_da_info_rbtree_lock(da_info, false);
    node = svm_da_node_find(da_info, va, size);
    if (node != NULL) {
        if ((node->va == va) && (node->size == size)) {
            match = true;
        }
    }
    svm_da_info_rbtree_unlock(da_info, false);

    return match;
}

void svm_set_da_status(struct devmm_svm_process *svm_proc, u64 va, u64 size, u32 status)
{
    struct svm_da_info *da_info = &svm_proc->da_info;
    struct svm_da_node_info *node = NULL;

    svm_da_info_rbtree_lock(da_info, true);
    node = svm_da_node_find(da_info, va, size);
    if (node != NULL) {
        node->status = status;
    }
    svm_da_info_rbtree_unlock(da_info, true);
}

#ifndef EMU_ST
bool svm_is_da_unmap(struct devmm_svm_process *svm_proc, u64 va, u64 size)
{
    struct svm_da_info *da_info = &svm_proc->da_info;
    struct svm_da_node_info *node = NULL;
    bool unmap_flag = false;

    svm_da_info_rbtree_lock(da_info, false);
    node = svm_da_node_find(da_info, va, size);
    if ((node != NULL) && (node->status == DEVMM_DA_STATUS_UNMAPING)) {
        unmap_flag = true;
    }
    svm_da_info_rbtree_unlock(da_info, false);
    return unmap_flag;
}
#endif

ka_vm_area_struct_t *svm_da_query_vma(struct devmm_svm_process *svm_proc, u64 va)
{
    struct svm_da_info *da_info = &svm_proc->da_info;
    struct svm_da_node_info *node = NULL;

    svm_da_info_rbtree_lock(da_info, false);
    node = svm_da_node_find(da_info, va, 1);
    svm_da_info_rbtree_unlock(da_info, false);

    return (node != NULL) ? node->vma : NULL;
}

int svm_da_set_custom_vma_nolock(struct devmm_svm_process *svm_proc, u64 va, ka_vm_area_struct_t *vma)
{
    struct svm_da_info *da_info = &svm_proc->da_info;
    struct svm_da_node_info *node = NULL;

    node = svm_da_node_find(da_info, va, 1);
    if (node != NULL) {
        node->custom_vma = vma;
    }

    return (node != NULL) ? 0 : -EINVAL;
}

int svm_da_set_custom_vma(struct devmm_svm_process *svm_proc, u64 va, ka_vm_area_struct_t *vma)
{
#ifndef EMU_ST
    struct svm_da_info *da_info = &svm_proc->da_info;
    int ret;

    svm_da_info_rbtree_lock(da_info, true);
    ret = svm_da_set_custom_vma_nolock(svm_proc, va, vma);
    svm_da_info_rbtree_unlock(da_info, true);

    return ret;
#endif
}

ka_vm_area_struct_t *svm_da_query_custom_vma(struct devmm_svm_process *svm_proc, u64 va)
{
    struct svm_da_info *da_info = &svm_proc->da_info;
    struct svm_da_node_info *node = NULL;

    svm_da_info_rbtree_lock(da_info, false);
    node = svm_da_node_find(da_info, va, 1);
    svm_da_info_rbtree_unlock(da_info, false);

    return (node != NULL) ? node->custom_vma : NULL;
}

u32 svm_da_query_addr_num(struct devmm_svm_process *svm_proc)
{
    struct svm_da_info *da_info = &svm_proc->da_info;
    struct svm_da_node_info *node = NULL, *tmp = NULL;
    u32 addr_num = 0;

    svm_da_info_rbtree_lock(da_info, false);

    rbtree_postorder_for_each_entry_safe(node, tmp, &da_info->rbtree, rbnode) {
        addr_num++;
    }

    svm_da_info_rbtree_unlock(da_info, false);

    return addr_num;
}

int svm_da_for_each_addr(struct devmm_svm_process *svm_proc, bool is_occupy,
    int (*func)(struct devmm_svm_process *svm_proc, u64 va, u64 size, void *priv), void *priv)
{
    struct svm_da_info *da_info = &svm_proc->da_info;
    struct svm_da_node_info *node = NULL, *tmp = NULL;

    svm_da_info_rbtree_lock(da_info, is_occupy);

    rbtree_postorder_for_each_entry_safe(node, tmp, &da_info->rbtree, rbnode) {
        int ret = func(svm_proc, node->va, node->size, priv);
        if (ret != 0) {
            svm_da_info_rbtree_unlock(da_info, is_occupy);
            return ret;
        }
    }

    svm_da_info_rbtree_unlock(da_info, is_occupy);

    return 0;
}

static void _svm_set_da_heap(struct svm_da_info *da_info, u32 heap_idx, struct devmm_svm_heap *heap)
{
    struct svm_da_node_info *node = NULL, *tmp = NULL;

	rbtree_postorder_for_each_entry_safe(node, tmp, &da_info->rbtree, rbnode) {
        if (heap_idx == node->first_heap_idx) {
            node->heap = heap;
            return;
        }
	}

    devmm_drv_err("Invalid heap index. (heap_idx=%u)\n", heap_idx);
}

void svm_set_da_heap(struct devmm_svm_process *svm_proc, u32 heap_idx, struct devmm_svm_heap *heap)
{
    struct svm_da_info *da_info = &svm_proc->da_info;

    svm_da_info_rbtree_lock(da_info, true);
    _svm_set_da_heap(da_info, heap_idx, heap);
    svm_da_info_rbtree_unlock(da_info, true);
}

static struct devmm_svm_heap *_svm_get_da_heap_by_idx(struct svm_da_info *da_info, u32 heap_idx)
{
    struct svm_da_node_info *node = NULL, *tmp = NULL;

	rbtree_postorder_for_each_entry_safe(node, tmp, &da_info->rbtree, rbnode) {
        if ((heap_idx >= node->first_heap_idx) && (heap_idx <= node->last_heap_idx)) {
            return node->heap;
        }
	}

    return NULL;
}

struct devmm_svm_heap *svm_get_da_heap_by_idx(struct devmm_svm_process *svm_proc, u32 heap_idx)
{
    struct svm_da_info *da_info = &svm_proc->da_info;
    struct devmm_svm_heap *heap = NULL;

    svm_da_info_rbtree_lock(da_info, false);
    heap = _svm_get_da_heap_by_idx(da_info, heap_idx);
    svm_da_info_rbtree_unlock(da_info, false);

    return heap;
}
