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

#include <linux/types.h>
#include <linux/hugetlb.h>

#include "svm_cmd.h"
#include "devmm_proc_info.h"
#include "devmm_channel.h"
#include "devmm_common.h"
#include "svm_master_dev_capability.h"
#include "devmm_page_cache.h"

struct devmm_dev_page_node {
    ka_list_head_t list;
    u64 va;
    u32 node_sz;  /* pa sizes save this node */
    u32 blk_sz;   /* pa page size */
    u32 blk_num;  /* pa blk nums save this node */
    struct devmm_addr_block blks[];
};

struct devmm_search_para {
    u64 start_addr;
    u64 end_addr;
    u32 addr_type;
    u32 blks_num;
    struct devmm_dma_block *blks;
};

void devmm_init_dev_pages_cache_inner(struct devmm_svm_process *svm_pro)
{
    struct devmm_dev_pages_cache *dev_pages_head = NULL;
    u32 i, j;

    for (i = 0; i < SVM_MAX_AGENT_NUM; i++) {
        if (svm_pro->dev_pages_head[i] != NULL) {
            continue;
        }
        dev_pages_head = devmm_kzalloc_ex(sizeof(struct devmm_dev_pages_cache), KA_GFP_KERNEL);
        svm_pro->dev_pages_head[i] = dev_pages_head;
        if (dev_pages_head != NULL) {
            dev_pages_head->ref = 1;
            ka_task_init_rwsem(&dev_pages_head->lock);
            for (j = 0; j < DEVMM_PAGE_CACHE_LIST_NUM; j++) {
                KA_INIT_LIST_HEAD(&dev_pages_head->head[j]);
            }
            for (j = 0; j < DEVMM_HUGE_PAGE_CACHE_LIST_NUM; j++) {
                KA_INIT_LIST_HEAD(&dev_pages_head->huge_head[j]);
            }
        }
        /* dev_pages_head not key way */
    }

    return;
}

void devmm_init_dev_pages_cache(struct devmm_svm_process *svm_proc)
{
    devmm_init_dev_pages_cache_inner(svm_proc);
}

STATIC struct devmm_dev_pages_cache *devmm_get_dev_pages_head(struct devmm_svm_process *svm_pro, u32 devid)
{
    struct devmm_dev_pages_cache *dev_pages_head = NULL;

    if (devid >= SVM_MAX_AGENT_NUM) {
        return NULL;
    }

    ka_task_mutex_lock(&svm_pro->proc_lock);
    dev_pages_head = svm_pro->dev_pages_head[devid];
    if (dev_pages_head == NULL) {
        ka_task_mutex_unlock(&svm_pro->proc_lock);
        return NULL;
    }

    dev_pages_head->ref++;
    ka_task_mutex_unlock(&svm_pro->proc_lock);

    return dev_pages_head;
}

STATIC void devmm_put_dev_pages_head(struct devmm_svm_process *svm_pro, u32 devid)
{
    struct devmm_dev_pages_cache *dev_pages_head = NULL;

    if (devid >= SVM_MAX_AGENT_NUM) {
        return;
    }

    ka_task_mutex_lock(&svm_pro->proc_lock);
    dev_pages_head = svm_pro->dev_pages_head[devid];
    if (dev_pages_head == NULL) {
        ka_task_mutex_unlock(&svm_pro->proc_lock);
        return;
    }

    dev_pages_head->ref--;
    if (dev_pages_head->ref <= 0) {
        svm_pro->dev_pages_head[devid] = NULL;
        devmm_kfree_ex(dev_pages_head);
    }
    ka_task_mutex_unlock(&svm_pro->proc_lock);

    return;
}

void devmm_destroy_dev_pages_cache_inner(struct devmm_svm_process *svm_pro, u32 devid)
{
    struct devmm_dev_pages_cache *dev_pages_head = NULL;
    struct devmm_dev_page_node *node = NULL;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;
    u32 stamp = (u32)ka_jiffies;
    u32 j;

    dev_pages_head = devmm_get_dev_pages_head(svm_pro, devid);
    if (dev_pages_head == NULL) {
        return;
    }
    ka_task_down_write(&dev_pages_head->lock);
    for (j = 0; j < DEVMM_PAGE_CACHE_LIST_NUM; j++) {
        ka_list_for_each_safe(pos, n, &dev_pages_head->head[j]) {
            node = ka_list_entry(pos, struct devmm_dev_page_node, list);
            ka_list_del(&node->list);
            devmm_kfree_ex(node);
            node = NULL;
            devmm_try_cond_resched(&stamp);
        }
    }
    for (j = 0; j < DEVMM_HUGE_PAGE_CACHE_LIST_NUM; j++) {
        ka_list_for_each_safe(pos, n, &dev_pages_head->huge_head[j]) {
            node = ka_list_entry(pos, struct devmm_dev_page_node, list);
            ka_list_del(&node->list);
            devmm_kfree_ex(node);
            node = NULL;
            devmm_try_cond_resched(&stamp);
        }
    }
    ka_task_up_write(&dev_pages_head->lock);
    devmm_put_dev_pages_head(svm_pro, devid);
}

void devmm_destroy_dev_pages_cache(struct devmm_svm_process *svm_proc, u32 devid)
{
    devmm_destroy_dev_pages_cache_inner(svm_proc, devid);
}

void devmm_destroy_pages_cache_inner(struct devmm_svm_process *svm_proc)
{
    u32 i, stamp;

    stamp = (u32)ka_jiffies;
    for (i = 0; i < SVM_MAX_AGENT_NUM; i++) {
        devmm_destroy_dev_pages_cache(svm_proc, i);
        /* devmm_init_svm_process we init dev_pages_head.ref = 1, so release it here when process is exited */
        devmm_put_dev_pages_head(svm_proc, i);
        devmm_try_cond_resched(&stamp);
    }
}

void devmm_destroy_pages_cache(struct devmm_svm_process *svm_proc)
{
    devmm_destroy_pages_cache_inner(svm_proc);
}

STATIC u32 devmm_get_dev_pages_head_idx(u32 page_size, u64 va)
{
    u32 cache_node_shift = (page_size == devmm_svm->device_hpage_size) ?
            DEVMM_HUGE_PAGE_CACHE_NODE_SHIFT : DEVMM_PAGE_CACHE_NODE_SHIFT;
    u32 cache_node_list_num = (page_size == devmm_svm->device_hpage_size) ?
            DEVMM_HUGE_PAGE_CACHE_LIST_NUM : DEVMM_PAGE_CACHE_LIST_NUM;

    return (u32)((va >> cache_node_shift) & (cache_node_list_num - 1));
}

STATIC u32 devmm_get_dev_pages_idx(struct devmm_dev_page_node *node, u64 va)
{
    return (u32)((va & (node->node_sz - 1)) / node->blk_sz);
}


STATIC struct devmm_dev_page_node *devmm_create_page_node(
    u32 page_size, struct devmm_dev_pages_cache *dev_pages_head, u64 va)
{
    struct devmm_dev_page_node *node = NULL;
    ka_list_head_t *head = NULL;
    u32 idx, blk_num, blk_sz;

    blk_sz = page_size;
    blk_num = DEVMM_PAGE_CACHE_BLK_NUM;
    head = (page_size == devmm_svm->device_hpage_size) ? dev_pages_head->huge_head : dev_pages_head->head;
    node = devmm_kzalloc_ex(sizeof(struct devmm_dev_page_node) + sizeof(struct devmm_addr_block) * blk_num,
        KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (node) {
        KA_INIT_LIST_HEAD(&node->list);
        node->node_sz = blk_num * blk_sz;
        node->va = ka_base_round_down(va, node->node_sz);
        node->blk_num = blk_num;
        node->blk_sz  = blk_sz;  /* page size */
        idx = devmm_get_dev_pages_head_idx(page_size, va);
        devmm_drv_debug("Enter. (va=0x%llx; idx=%u)\n", va, idx);
        ka_list_add(&node->list, &head[idx]);
    }

    return node;
}

STATIC u32 devmm_free_page_node(u64 va, u32 page_num, struct devmm_dev_page_node *node, bool reuse)
{
    u32 page_idx, free_num;
    u64 j;

    if (reuse) {
        page_idx = devmm_get_dev_pages_idx(node, va);
        free_num = node->blk_num - page_idx;
        free_num = ka_base_min(page_num, free_num);
        for (j = 0; j < free_num; j++) {
            node->blks[page_idx + j].dma_addr = 0;
            node->blks[page_idx + j].phy_addr = 0;
        }
    } else {
        free_num = node->blk_num;
        ka_list_del(&node->list);
        devmm_kfree_ex(node);
        node = NULL;
    }
    return free_num;
}

STATIC struct devmm_dev_page_node *devmm_get_page_node_by_va(u32 page_size,
    struct devmm_dev_pages_cache *dev_pages_head, u64 va)
{
    u32 idx = devmm_get_dev_pages_head_idx(page_size, va);
    struct devmm_dev_page_node *node = NULL;
    ka_list_head_t *head = NULL;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;

    head = (page_size == devmm_svm->device_hpage_size) ? dev_pages_head->huge_head : dev_pages_head->head;
    ka_list_for_each_safe(pos, n, &head[idx]) {
        node = ka_list_entry(pos, struct devmm_dev_page_node, list);
        if ((va >= node->va) && (va < (node->va + node->node_sz))) {
            return node;
        }
    }

    return NULL;
}

void devmm_free_pages_cache_inner(struct devmm_svm_process *svm_process,
    u32 devid, u32 page_num, u32 page_size, u64 va, bool reuse)
{
    struct devmm_dev_pages_cache *dev_pages_head = NULL;
    struct devmm_dev_page_node *node = NULL;
    u32 blk_sz, freed_num, blk_num;
    u64 size, offset, freed_size, aligned_va;
    u32 stamp = (u32)ka_jiffies;

    dev_pages_head = devmm_get_dev_pages_head(svm_process, devid);
    if (dev_pages_head == NULL) {
        return;
    }
    size = (u64)page_size * page_num;
    blk_sz = (page_size == devmm_svm->device_hpage_size) ? devmm_svm->device_hpage_size : devmm_svm->device_page_size;
    blk_num = (u32)(size / blk_sz);
    aligned_va = ka_base_round_down(va, page_size);
    ka_task_down_write(&dev_pages_head->lock);
    for (offset = 0, freed_num = 0, freed_size = 0; offset < size;
        offset += freed_size, aligned_va += freed_size, blk_num -= freed_num) {
        node = devmm_get_page_node_by_va(page_size, dev_pages_head, aligned_va);
        if (node) {
            freed_num = devmm_free_page_node(aligned_va, blk_num, node, reuse);
            freed_size = (u64)freed_num * blk_sz;
        } else {
            freed_num = DEVMM_PAGE_CACHE_BLK_NUM;
            freed_size = (u64)blk_sz * DEVMM_PAGE_CACHE_BLK_NUM;
        }

        if (freed_num >= blk_num) {
            break;
        }
        devmm_try_cond_resched(&stamp);
    }
    ka_task_up_write(&dev_pages_head->lock);
    devmm_put_dev_pages_head(svm_process, devid);

    return;
}

void devmm_free_pages_cache(struct devmm_svm_process *svm_proc,
    u32 devid, u32 page_num, u32 page_size, u64 va, bool reuse)
{
    devmm_free_pages_cache_inner(svm_proc, devid, page_num, page_size, va, reuse);
}

STATIC void devmm_set_pa_first(struct devmm_dev_pages_cache *dev_pages_head, u64 va, u32 page_size)
{
    struct devmm_dev_page_node *node = NULL;
    u32 page_idx;

    node = devmm_get_page_node_by_va(page_size, dev_pages_head, va);
    if (node == NULL) {
        return;
    }

    page_idx = devmm_get_dev_pages_idx(node, va);
    if ((node->blks[page_idx].phy_addr & DEVMM_PA_VALID) == 0) {
        return;
    }

    node->blks[page_idx].dma_addr |= DEVMM_PA_FIRST;
    node->blks[page_idx].phy_addr |= DEVMM_PA_FIRST;
}

STATIC void devmm_set_dma_phy_addr_to_node(struct devmm_pages_cache_info *info,
    u64 query_pages_blk_index, struct devmm_dev_page_node *node, u64 node_blk_index)
{
    node->blks[node_blk_index].dma_addr = info->blks[query_pages_blk_index].dma_addr | DEVMM_PA_VALID;
    node->blks[node_blk_index].phy_addr = info->blks[query_pages_blk_index].phy_addr | DEVMM_PA_VALID;
}

STATIC void devmm_insert_pa_info_to_node(struct devmm_dev_pages_cache *dev_pages_head,
    struct devmm_pages_cache_info *info)
{
    struct devmm_dev_page_node *node = NULL;
    u32 stamp = (u32)ka_jiffies;
    u64 i, j, insert_num;
    u64 va = info->va;
    u32 page_idx;

    for (i = 0, j = 0; i < info->pg_num; i += (u32)j) {
        node = devmm_get_page_node_by_va((u32)info->pg_size, dev_pages_head, va);
        if (node == NULL) {
            node = devmm_create_page_node((u32)info->pg_size, dev_pages_head, va);
        }

        if (node != NULL) {
            page_idx = devmm_get_dev_pages_idx(node, va);
            insert_num = ka_base_min((info->pg_num - i), (u64)(node->blk_num - page_idx));
            for (j = 0; j < insert_num; j++) {
                devmm_set_dma_phy_addr_to_node(info, i + j, node, page_idx + j);
                devmm_drv_debug("Enter. (va=0x%llx; num=%llu; i=%llu; j=%llu; page_idx=%u; psize=%llu; blk_sz=%u)\n",
                    va + j * node->blk_sz, info->pg_num, i, j, page_idx, info->pg_size, node->blk_sz);
            }
            va += j * node->blk_sz;
        } else {
            break;
        }
        devmm_try_cond_resched(&stamp);
    }
}

void devmm_insert_pages_cache(struct devmm_svm_process *svm_process,
    struct devmm_chan_page_query_ack *query_pages, u32 devid)
{
    struct devmm_pages_cache_info info = {.va = query_pages->va, .pg_num = query_pages->num,
        .pg_size = query_pages->page_size, .blks = query_pages->blks};

    devmm_pages_cache_set(svm_process, devid, &info);
}

void devmm_pages_cache_set(struct devmm_svm_process *svm_proc, u32 logical_devid, struct devmm_pages_cache_info *info)
{
    struct devmm_dev_pages_cache *dev_pages_head = NULL;

    dev_pages_head = devmm_get_dev_pages_head(svm_proc, logical_devid);
    if (dev_pages_head == NULL) {
        return;
    }

    ka_task_down_write(&dev_pages_head->lock);
    devmm_insert_pa_info_to_node(dev_pages_head, info);
    devmm_set_pa_first(dev_pages_head, info->va, (u32)info->pg_size);
    ka_task_up_write(&dev_pages_head->lock);
    devmm_put_dev_pages_head(svm_proc, logical_devid);
}

STATIC u64 devmm_fill_dma_node(struct devmm_dev_page_node *node, struct devmm_search_para *search_para)
{
    struct devmm_dma_block *blks = search_para->blks;
    u64 va = search_para->start_addr;
    u64 end_va = search_para->end_addr;
    u64 offset, real_blk_size, fill_size;
    u32 i, page_idx;

    page_idx = devmm_get_dev_pages_idx(node, va);
    fill_size = 0;

    for (i = 0; (page_idx < node->blk_num) && (i < search_para->blks_num) && (va < end_va); i++, page_idx++) {
        if ((node->blks[page_idx].phy_addr & DEVMM_PA_VALID) != 0) {
            blks[i].pa = (search_para->addr_type == DEVMM_ADDR_TYPE_DMA) ?
               node->blks[page_idx].dma_addr : node->blks[page_idx].phy_addr;
            blks[i].pa = blks[i].pa & (~DEVMM_PA_MASK);
            blks[i].sz = node->blk_sz;
            blks[i].ssid = 0;
            offset = va & (node->blk_sz - 1); /* node->blk_sz is page size, count page offset */
            real_blk_size = node->blk_sz - offset;
            fill_size += real_blk_size;
            va += real_blk_size;
        } else {
            break;
        }
    }
    search_para->blks_num = i;

    return fill_size;
}

bool devmm_find_pages_cache(struct devmm_svm_process *svm_process, struct devmm_page_query_arg query_arg,
    struct devmm_dma_block *blks, u32 *num)
{
    struct devmm_dev_pages_cache *dev_pages_head = NULL;
    struct devmm_dev_page_node *node = NULL;
    struct devmm_search_para search_para;
    u64 offset, fill_size;
    u32 blks_start, idx;
    bool success = true;

    if (blks == NULL) {
        return false;
    }
    devmm_drv_debug("Enter. (va=0x%llx; size=%llu; page_insert_dev_id=%u addr_type=%u)\n",
        query_arg.va, query_arg.size, query_arg.page_insert_dev_id, query_arg.addr_type);

    dev_pages_head = devmm_get_dev_pages_head(svm_process, query_arg.page_insert_dev_id);
    if (dev_pages_head == NULL) {
        return false;
    }

    search_para.start_addr = query_arg.va;
    search_para.blks_num = *num;
    search_para.end_addr = query_arg.va + query_arg.size;
    search_para.addr_type = query_arg.addr_type;

    ka_task_down_read(&dev_pages_head->lock);
    for (blks_start = 0, offset = 0; offset < query_arg.size;) {
        fill_size = 0;
        node = devmm_get_page_node_by_va(query_arg.page_size, dev_pages_head, search_para.start_addr);
        if (node != NULL) {
            search_para.blks = &blks[blks_start];
            fill_size = devmm_fill_dma_node(node, &search_para);
            search_para.start_addr += fill_size;
            offset += fill_size;
            blks_start += search_para.blks_num;
            search_para.blks_num = *num - search_para.blks_num;
        }
        if (fill_size == 0) {
            success = false;
            break;
        }
    }
    ka_task_up_read(&dev_pages_head->lock);

    if (success) {
        for (*num = 0, idx = 0; idx < blks_start; idx++) {
            devmm_merg_blk(blks, idx, num);
        }
    }
    devmm_put_dev_pages_head(svm_process, query_arg.page_insert_dev_id);
    devmm_drv_debug("Enter. (va=0x%llx; size=%llu; page_insert_dev_id=%u; blks_start=%d; num=%d; success=%d).\n",
        query_arg.va, query_arg.size, query_arg.page_insert_dev_id, blks_start, *num, success);
    return success;
}

#ifndef EMU_ST
int devmm_find_dma_addr_cache(struct devmm_svm_process *svm_process,
    u32 logic_id, u64 va, u32 page_size, u64 *dma_addr)
{
    struct devmm_dev_pages_cache *dev_pages_head = NULL;
    struct devmm_dev_page_node *node = NULL;
    u32 page_idx;
    int ret = 0;

    dev_pages_head = devmm_get_dev_pages_head(svm_process, logic_id);
    if (dev_pages_head == NULL) {
        return -EINVAL;
    }

    ka_task_down_read(&dev_pages_head->lock);

    node = devmm_get_page_node_by_va(page_size, dev_pages_head, va);
    if (node == NULL) {
        ret = -EINVAL;
        goto OUT;
    }

    page_idx = devmm_get_dev_pages_idx(node, va);
    if (((node->blks[page_idx].dma_addr & DEVMM_PA_VALID) == 0) ||
        ((node->blks[page_idx].dma_addr & (~DEVMM_PA_MASK)) == 0)) {
        ret = -EINVAL;
        goto OUT;
    }

    *dma_addr = (node->blks[page_idx].dma_addr & (~DEVMM_PA_MASK)) +
        (va - (node->va + (u64)node->blk_sz * (u64)page_idx));

OUT:
    ka_task_up_read(&dev_pages_head->lock);
    devmm_put_dev_pages_head(svm_process, logic_id);

    return ret;
}

int hal_kernel_svm_dev_va_to_dma_addr(int hostpid, u32 logical_devid, u64 va, u64 *dma_addr)
{
    struct devmm_svm_process_id process_id = {.hostpid = hostpid, .devid = 0, .vfid = 0};
    struct devmm_svm_process *svm_proc = NULL;
    int ret;

    if ((va == 0) || (dma_addr == NULL)) {
        devmm_drv_err("Input param is invalid. (va=0x%llx)\n", va);
        return -EINVAL;
    }

    svm_proc = devmm_svm_proc_get_by_process_id(&process_id);
    if (svm_proc == NULL) {
        devmm_drv_err("Find svm_proc failed. (hostpid=%d)\n", hostpid);
        return -ESRCH;
    }

    ret = devmm_find_dma_addr_cache(svm_proc, logical_devid, va, devmm_svm->device_page_size, dma_addr);
    if (ret != 0) {
        ret = devmm_find_dma_addr_cache(svm_proc, logical_devid, va, devmm_svm->device_hpage_size, dma_addr);
        if (ret != 0) {
            devmm_svm_proc_put(svm_proc);
            return ret;
        }
    }
    devmm_svm_proc_put(svm_proc);
    return ret;
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_svm_dev_va_to_dma_addr);
#endif

int devmm_find_pa_cache(struct devmm_svm_process *svm_process, u32 logic_id, u64 va, u32 page_size, u64 *pa)
{
    struct devmm_dev_pages_cache *dev_pages_head = NULL;
    struct devmm_dev_page_node *node = NULL;
    u32 page_idx;
    int ret = 0;

    dev_pages_head = devmm_get_dev_pages_head(svm_process, logic_id);
    if (dev_pages_head == NULL) {
        return -EINVAL;
    }

    ka_task_down_read(&dev_pages_head->lock);

    node = devmm_get_page_node_by_va(page_size, dev_pages_head, va);
    if (node == NULL) {
        ret = -EINVAL;
        goto OUT;
    }

    page_idx = devmm_get_dev_pages_idx(node, va);
    if ((node->blks[page_idx].phy_addr & DEVMM_PA_VALID) == 0) {
        ret = -EINVAL;
        goto OUT;
    }

    *pa = (node->blks[page_idx].phy_addr & (~DEVMM_PA_MASK)) +
        (va - (node->va + (u64)node->blk_sz * (u64)page_idx));

OUT:
    ka_task_up_read(&dev_pages_head->lock);
    devmm_put_dev_pages_head(svm_process, logic_id);

    return ret;
}

u64 devmm_get_continuty_len_after_dev_va(struct devmm_svm_process *svm_proc,
    u32 logic_id, u64 va, u32 page_size)
{
#ifndef DEVMM_UT
    struct devmm_dev_pages_cache *dev_pages_head = NULL;
    struct devmm_dev_page_node *node = NULL;
    u64 max_num = MAX_CONTINUTY_PHYS_SIZE / page_size;
    u64 continuty_size = 0;
    u64 tmp_size = 0;
    u64 tmp_va, pre_pa;
    u64 pa = 0;
    u32 page_idx;
    int i;

    dev_pages_head = devmm_get_dev_pages_head(svm_proc, logic_id);
    if (dev_pages_head == NULL) {
        return 0;
    }

    ka_task_down_read(&dev_pages_head->lock);
    tmp_va = ka_base_round_down(va, page_size);
    for (i = 0; i < max_num; i++) {
        int is_first_pa, is_continuty;
        node = devmm_get_page_node_by_va(page_size, dev_pages_head, tmp_va);
        if (node == NULL) {
            break;
        }
        page_idx = devmm_get_dev_pages_idx(node, tmp_va);
        if ((node->blks[page_idx].phy_addr & DEVMM_PA_VALID) == 0) {
            break;
        }
        pre_pa = pa;
        pa = node->blks[page_idx].phy_addr & (~DEVMM_PA_MASK);
        is_first_pa = (node->blks[page_idx].phy_addr & DEVMM_PA_FIRST);
        is_continuty = ((pre_pa + page_size) == pa);
        if ((i != 0) && ((is_first_pa != 0) || (is_continuty == 0))) {
            break;
        }

        tmp_va += page_size;
        tmp_size += page_size;
    }
    ka_task_up_read(&dev_pages_head->lock);
    devmm_put_dev_pages_head(svm_proc, logic_id);
    continuty_size = (i == 0) ? 0 : (tmp_size - (va - ka_base_round_down(va, page_size)));
    return continuty_size;
#else
    return page_size;
#endif
}

