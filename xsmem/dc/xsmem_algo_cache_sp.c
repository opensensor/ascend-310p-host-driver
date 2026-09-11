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

#define pr_fmt(fmt) "XSMEM_CACHE_SP: <%s:%d> " fmt, __func__, __LINE__

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kallsyms.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/spinlock.h>
#include <linux/rwlock_types.h>
#include <linux/list_sort.h>

#include "kernel_version_adapt.h"
#include "xsmem_framework_log.h"
#include "xsmem_framework.h"
#include "ascend_kernel_hal.h"
#include "xsmem_algo_vma.h"
#include "xsmem_algo_sp.h"

#define XSMEM_MAX_DEV_NUM   4U

struct cache_sp_info {
    struct rb_node cache_node;  /* for free */
    struct list_head list_node;      /* for alloc */

    unsigned long memflag;
    unsigned long addr;
    unsigned long total_size;
    atomic64_t alloc_size;
    unsigned long alloc_max_size;
    void *vma_ctrl;
};

struct cache_dev_info {
    unsigned int dev_id;
    unsigned int cache_cnt;
    unsigned long total_size;
    atomic64_t alloc_size;
    atomic64_t ref;
    struct rw_semaphore cache_sem;

    struct rb_root cache_rbtree;
    struct list_head list_head;
};

struct mem_stat {
    atomic64_t alloced_size;
    atomic64_t blk_cnt;
};

struct cache_sp_ctrl {
    void *sp_ctrl;
    struct rw_semaphore dev_sem;
    struct cache_dev_info *dev_info[XSMEM_MAX_DEV_NUM];
    struct mem_stat os_info;
};

static int cache_dev_create(struct cache_sp_ctrl *cache_ctrl, unsigned int devid)
{
    struct cache_dev_info *cache_dev = NULL;

    down_write(&cache_ctrl->dev_sem);
    cache_dev = cache_ctrl->dev_info[devid];
    if (cache_dev != NULL) {
        up_write(&cache_ctrl->dev_sem);
        return 0;
    }

    cache_dev = xsmem_drv_kvzalloc(sizeof(struct cache_dev_info), GFP_KERNEL | __GFP_ACCOUNT);
    if (cache_dev == NULL) {
        up_write(&cache_ctrl->dev_sem);
        xsmem_err("Alloc memory for cache_dev_info failed.\n");
        return -ENOMEM;
    }
    atomic64_set(&cache_dev->ref, 1);
    cache_dev->dev_id = devid;
    cache_dev->cache_cnt = 0;
    cache_dev->total_size = 0;
    atomic64_set(&cache_dev->alloc_size, 0);
    cache_dev->cache_rbtree = RB_ROOT;
    INIT_LIST_HEAD(&cache_dev->list_head);
    init_rwsem(&cache_dev->cache_sem);

    cache_ctrl->dev_info[devid] = cache_dev;
    up_write(&cache_ctrl->dev_sem);
    return 0;
}

static void cache_info_destroy(struct cache_sp_ctrl *cache_ctrl, struct cache_sp_info *cache_info)
{
    xsmem_info("Cache sp destroy. (addr=0x%pK; total_size=%lu)\n",
        (void *)(uintptr_t)cache_info->addr, cache_info->total_size);
    (void)vma_inst_destroy(cache_info->vma_ctrl);
    (void)sp_algo_free(cache_ctrl->sp_ctrl, cache_info->addr);
    xsmem_drv_kvfree(cache_info);
}

static void cache_sp_rb_erase(struct cache_sp_ctrl *cache_ctrl, struct cache_dev_info *cache_dev)
{
    struct cache_sp_info *cache_info = NULL;
    struct rb_node *node = NULL;

    node = rb_first(&cache_dev->cache_rbtree);
    while (node != NULL) {
        cache_info = rb_entry(node, struct cache_sp_info, cache_node);
        node = rb_next(node);
        rb_erase(&cache_info->cache_node, &cache_dev->cache_rbtree);
        RB_CLEAR_NODE(&cache_info->cache_node);
        list_del(&cache_info->list_node);

        cache_info_destroy(cache_ctrl, cache_info);
    }
}

static void cache_sp_destroy_dev(struct cache_sp_ctrl *cache_ctrl, struct cache_dev_info *cache_dev)
{
    down_write(&cache_dev->cache_sem);
    cache_sp_rb_erase(cache_ctrl, cache_dev);
    up_write(&cache_dev->cache_sem);
    xsmem_drv_kvfree(cache_dev);
}

static int cache_dev_destroy(struct cache_sp_ctrl *cache_ctrl, unsigned int devid)
{
    struct cache_dev_info *cache_dev = NULL;
    long ref_cnt;

    down_write(&cache_ctrl->dev_sem);
    cache_dev = cache_ctrl->dev_info[devid];
    if (cache_dev == NULL) {
        up_write(&cache_ctrl->dev_sem);
        xsmem_err("Cache dev has been released or not created. (devid=%u)\n", devid);
        return -ENXIO;
    }

    if (atomic64_read(&cache_dev->alloc_size) != 0) {
        up_write(&cache_ctrl->dev_sem);
        return -EBUSY;
    }

    ref_cnt = atomic64_read(&cache_dev->ref);
    if (ref_cnt != 1) {
        up_write(&cache_ctrl->dev_sem);
        xsmem_err("Ref is not zero. (devid=%u; ref=%ld)\n", devid, ref_cnt);
        return -ETXTBSY;
    }

    atomic64_dec(&cache_dev->ref);
    cache_sp_destroy_dev(cache_ctrl, cache_dev);
    cache_ctrl->dev_info[devid] = NULL;
    up_write(&cache_ctrl->dev_sem);
    return 0;
}

static struct cache_dev_info *cache_dev_get(struct cache_sp_ctrl *cache_ctrl, unsigned int devid)
{
    struct cache_dev_info *cache_dev = NULL;

    down_read(&cache_ctrl->dev_sem);
    cache_dev = cache_ctrl->dev_info[devid];
    if (cache_dev != NULL) {
        atomic64_inc(&cache_dev->ref);
    }
    up_read(&cache_ctrl->dev_sem);

    return cache_dev;
}

static void cache_dev_put(struct cache_sp_ctrl *cache_ctrl, unsigned int devid)
{
    struct cache_dev_info *cache_dev = NULL;

    down_read(&cache_ctrl->dev_sem);
    cache_dev = cache_ctrl->dev_info[devid];
    if (cache_dev != NULL) {
        atomic64_dec(&cache_dev->ref);
    }
    up_read(&cache_ctrl->dev_sem);
}

static int cache_sp_algo_pool_init(struct xsm_pool *xp, struct xsm_reg_arg *arg)
{
    struct cache_sp_ctrl *cache_ctrl = NULL;
    void *sp_ctrl = NULL;

    cache_ctrl = xsmem_drv_kvzalloc(sizeof(struct cache_sp_ctrl), GFP_KERNEL | __GFP_ACCOUNT);
    if (cache_ctrl == NULL) {
        xsmem_err("Alloc memory for cache_ctrl failed.\n");
        return -ENOMEM;
    }

    sp_ctrl = sp_inst_create();
    if (sp_ctrl == NULL) {
        xsmem_drv_kvfree(cache_ctrl);
        return -EFAULT;
    }

    cache_ctrl->sp_ctrl = sp_ctrl;
    init_rwsem(&cache_ctrl->dev_sem);
    xp->private = cache_ctrl;
    return 0;
}

static void cache_sp_destroy_all_dev(struct cache_sp_ctrl *cache_ctrl)
{
    struct cache_dev_info *cache_dev = NULL;
    unsigned int devid;

    for (devid = 0; devid < XSMEM_MAX_DEV_NUM; devid++) {
        cache_dev = cache_ctrl->dev_info[devid];
        if (cache_dev == NULL) {
            continue;
        }

        cache_sp_destroy_dev(cache_ctrl, cache_dev);
        cache_ctrl->dev_info[devid] = NULL;
    }
}

static int cache_sp_algo_pool_free(struct xsm_pool *xp)
{
    struct cache_sp_ctrl *cache_ctrl = (struct cache_sp_ctrl *)xp->private;

    cache_sp_destroy_all_dev(cache_ctrl);
    sp_inst_destroy(cache_ctrl->sp_ctrl);
    xsmem_drv_kvfree(cache_ctrl);
    xp->private = NULL;
    return 0;
}

static int cache_sp_rb_insert(struct cache_dev_info *cache_dev, struct cache_sp_info *cache_info)
{
    struct rb_node **cur_node = NULL;
    struct rb_node *parent = NULL;

    cur_node = &(cache_dev->cache_rbtree.rb_node);
    /* Figure out where to put new node */
    while (*cur_node) {
        struct cache_sp_info *this = rb_entry(*cur_node, struct cache_sp_info, cache_node);
        parent = *cur_node;
        if (cache_info->addr < this->addr) {
            cur_node = &((*cur_node)->rb_left);
        } else if (cache_info->addr > this->addr) {
            cur_node = &((*cur_node)->rb_right);
        } else {
            return -EINVAL;
        }
    }

    /* Add new node and rebalance tree. */
    rb_link_node(&cache_info->cache_node, parent, cur_node);
    rb_insert_color(&cache_info->cache_node, &cache_dev->cache_rbtree);

    return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
static int cmp_condition(void *priv, const struct list_head *a, const struct list_head *b)
#else
static int cmp_condition(void *priv, struct list_head *a, struct list_head *b)
#endif
{
    struct cache_sp_info *a_cache = NULL;
    struct cache_sp_info *b_cache = NULL;

    a_cache = list_entry(a, struct cache_sp_info, list_node);
    b_cache = list_entry(b, struct cache_sp_info, list_node);
    /* Priority 1: alloc_max_size, Priority 2: total_size */
    if (a_cache->alloc_max_size == b_cache->alloc_max_size) {
        return a_cache->total_size - b_cache->total_size;
    } else {
        return a_cache->alloc_max_size - b_cache->alloc_max_size;
    }
}

static void cache_sp_list_insert(struct cache_dev_info *cache_dev, struct cache_sp_info *cache_info)
{
    list_add_tail(&cache_info->list_node, &cache_dev->list_head);
    /* sort from smallest to largest */
    list_sort(NULL, &cache_dev->list_head, cmp_condition);
}

static int cache_info_create(struct cache_sp_ctrl *cache_ctrl, struct cache_dev_info *cache_dev,
    struct xsm_cache_create_arg *arg)
{
    unsigned long flag = (unsigned long)arg->memFlag;
    struct cache_sp_info *cache_info = NULL;
    void *vma_ctrl = NULL;
    int ret;

    cache_info = xsmem_drv_kvzalloc(sizeof(struct cache_sp_info), GFP_KERNEL | __GFP_ACCOUNT);
    if (cache_info == NULL) {
        xsmem_err("Alloc memory for cache_dev_info failed.\n");
        return -ENOMEM;
    }

    sp_set_device_id_to_flags(arg->devId, &flag);
    ret = sp_algo_alloc(cache_ctrl->sp_ctrl, arg->memSize, flag, &cache_info->addr,
        &cache_info->total_size);
    if (ret != 0) {
        goto algo_alloc_fail;
    }
    vma_ctrl = vma_inst_create(cache_info->total_size);
    if (vma_ctrl == NULL) {
        ret = -EINVAL;
        goto inst_create_fail;
    }
    cache_info->memflag = flag;
    cache_info->alloc_max_size = (arg->alloc_max_size == 0) ?
        cache_info->total_size : arg->alloc_max_size;
    cache_info->vma_ctrl = vma_ctrl;
    atomic64_set(&cache_info->alloc_size, 0);

    down_write(&cache_dev->cache_sem);
    ret = cache_sp_rb_insert(cache_dev, cache_info);
    if (ret != 0) {
        up_write(&cache_dev->cache_sem);
        xsmem_err("Insert same addr. (addr=0x%pK)\n", (void *)(uintptr_t)cache_info->addr);
        goto insert_fail;
    }
    cache_sp_list_insert(cache_dev, cache_info);

    cache_dev->cache_cnt++;
    cache_dev->total_size += cache_info->total_size;
    up_write(&cache_dev->cache_sem);

    xsmem_info("Cache sp create. (devid=%u; alloc_size=%llu; memFlag=0x%lx; addr=0x%pK; size=%lu; "
        "alloc_max_size=%llu)\n", arg->devId, arg->memSize, flag, (void *)(uintptr_t)cache_info->addr,
        cache_info->total_size, arg->alloc_max_size);
    return 0;

insert_fail:
    (void)vma_inst_destroy(cache_info->vma_ctrl);
inst_create_fail:
    sp_algo_free(cache_ctrl->sp_ctrl, cache_info->addr);
algo_alloc_fail:
    xsmem_drv_kvfree(cache_info);
    return ret;
}

static int cache_sp_create_para_check(struct xsm_cache_create_arg *cache_arg)
{
    if (cache_arg->devId >= XSMEM_MAX_DEV_NUM) {
        xsmem_err("Invalid dev id. (dev_id=%u)\n", cache_arg->devId);
        return -ENODEV;
    }
    if (cache_arg->memSize == 0) {
        xsmem_err("Invalid memSize. (memSize=%llu)\n", cache_arg->memSize);
        return -EINVAL;
    }
    if (cache_arg->alloc_max_size > cache_arg->memSize) {
        xsmem_err("Invalid alloc_max_size. (alloc_max_size=%llu; memSize=%llu)\n",
            cache_arg->alloc_max_size, cache_arg->memSize);
        return -EINVAL;
    }
    if ((cache_arg->memFlag != BUFF_SP_NORMAL) && (cache_arg->memFlag != BUFF_SP_HUGEPAGE_ONLY) &&
        (cache_arg->memFlag != (BUFF_SP_NORMAL | BUFF_SP_DVPP)) &&
        (cache_arg->memFlag != (BUFF_SP_HUGEPAGE_ONLY | BUFF_SP_DVPP))) {
        xsmem_err("Invalid memflag. (memFlag=%u)\n", cache_arg->memFlag);
        return -EINVAL;
    }
    return 0;
}

static int cache_sp_algo_cache_create(struct xsm_pool *xp, struct xsm_cache_create_arg *arg)
{
    struct cache_sp_ctrl *cache_ctrl = (struct cache_sp_ctrl *)xp->private;
    struct cache_dev_info *cache_dev = NULL;
    int ret;

    ret = cache_sp_create_para_check(arg);
    if (ret != 0) {
        return ret;
    }

    ret = cache_dev_create(cache_ctrl, arg->devId);
    if (ret != 0) {
        return ret;
    }

    cache_dev = cache_dev_get(cache_ctrl, arg->devId);
    if (cache_dev == NULL) {
        xsmem_err("Cache dev has been released. (dev_id=%u)\n", arg->devId);
        return -ENXIO;
    }

    ret = cache_info_create(cache_ctrl, cache_dev, arg);
    cache_dev_put(cache_ctrl, arg->devId);
    return ret;
}

static int cache_sp_algo_cache_destroy(struct xsm_pool *xp, struct xsm_cache_destroy_arg *arg)
{
    struct cache_sp_ctrl *cache_ctrl = (struct cache_sp_ctrl *)xp->private;

    if (arg->devId >= XSMEM_MAX_DEV_NUM) {
        xsmem_err("Invalid dev id. (dev_id=%u)\n", arg->devId);
        return -ENODEV;
    }

    return cache_dev_destroy(cache_ctrl, arg->devId);
}

static unsigned int cache_sp_query_one_node(unsigned int cache_total, unsigned int cache_index,
    struct cache_sp_info *cache_info, GrpQueryGroupAddrInfo *cache_buff)
{
    if (cache_index < cache_total) {
        cache_buff[cache_index].addr = cache_info->addr;
        cache_buff[cache_index].size = cache_info->total_size;
        xsmem_debug("Cache sp query. (cache_index=%u; mem_flag=0x%lx; addr=0x%pK; size=%llu)\n",
            cache_index, cache_info->memflag, (void *)(uintptr_t)cache_buff[cache_index].addr, cache_buff[cache_index].size);
        cache_index++;
    }
    return cache_index;
}

static int cache_sp_query(struct cache_dev_info *cache_dev, GrpQueryGroupAddrInfo *cache_buff,
    unsigned int *cache_cnt)
{
    struct cache_sp_info *cache_info = NULL;
    struct rb_node *node = NULL;
    unsigned int cache_total = *cache_cnt;
    unsigned int cache_index = 0;

    down_read(&cache_dev->cache_sem);
    node = rb_first(&cache_dev->cache_rbtree);
    while (node != NULL) {
        cache_info = rb_entry(node, struct cache_sp_info, cache_node);

        cache_index = cache_sp_query_one_node(cache_total, cache_index, cache_info, cache_buff);

        node = rb_next(node);
    }
    up_read(&cache_dev->cache_sem);

    *cache_cnt = cache_index;
    return 0;
}

static int cache_sp_algo_cache_query(struct xsm_pool *xp, unsigned int dev_id,
    GrpQueryGroupAddrInfo *cache_buff, unsigned int *cache_cnt)
{
    struct cache_sp_ctrl *cache_ctrl = (struct cache_sp_ctrl *)xp->private;
    struct cache_dev_info *cache_dev = NULL;
    int ret;

    if (dev_id >= XSMEM_MAX_DEV_NUM) {
        xsmem_err("Invalid dev id. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    cache_dev = cache_dev_get(cache_ctrl, dev_id);
    if (cache_dev == NULL) {
        xsmem_err("Cache dev has been released or not created. (dev_id=%u)\n", dev_id);
        return -ENXIO;
    }
    ret = cache_sp_query(cache_dev, cache_buff, cache_cnt);
    cache_dev_put(cache_ctrl, dev_id);

    return ret;
}

static int cache_sp_algo_pool_perm_add(struct xsm_pool *xp, int pid, unsigned long prop)
{
    struct cache_sp_ctrl *cache_ctrl = (struct cache_sp_ctrl *)xp->private;
    return sp_perm_add(cache_ctrl->sp_ctrl, pid, prop);
}

#define cache_sp_algo_print_adapt(seq, fmt, ...)  do { \
    if (seq == NULL) { \
        pr_notice(fmt, ##__VA_ARGS__); \
    } else { \
        seq_printf(seq, fmt, ##__VA_ARGS__); \
    } \
} while (0)

static void cache_sp_algo_dev_cache_show(struct cache_dev_info *cache_dev, struct seq_file *seq)
{
    struct cache_sp_info *cache_info = NULL;
    struct rb_node *node = NULL;

    cache_sp_algo_print_adapt(seq,
        "    Device all cache show. (devid=%u; cache_cnt=%u; total_size=%lu; alloc_size=%lu)\n", cache_dev->dev_id,
        cache_dev->cache_cnt, cache_dev->total_size, (unsigned long)atomic64_read(&cache_dev->alloc_size));

    down_read(&cache_dev->cache_sem);
    node = rb_first(&cache_dev->cache_rbtree);
    while (node != NULL) {
        cache_info = rb_entry(node, struct cache_sp_info, cache_node);
        if (seq != NULL) {
            seq_printf(seq, "    One cache in device info show, memflag %lu: \n", cache_info->memflag);
        } else {
            pr_notice("    One cache in device info show. (memflag=%lu; total_size=%lu; alloc_size=%lu)\n",
                cache_info->memflag, cache_info->total_size, (unsigned long)atomic64_read(&cache_info->alloc_size));
        }
        vma_algo_show(cache_info->vma_ctrl, seq);
        node = rb_next(node);
    }
    up_read(&cache_dev->cache_sem);

    cache_sp_algo_print_adapt(seq, "\n");

    return;
}

static void cache_sp_algo_cache_info_show(struct cache_sp_ctrl *cache_ctrl, struct seq_file *seq)
{
    struct cache_dev_info *cache_dev = NULL;
    u32 dev_id;

    cache_sp_algo_print_adapt(seq, "    ---------cache info show start-------------\n");
    for (dev_id = 0; dev_id < XSMEM_MAX_DEV_NUM; dev_id++) {
        cache_dev = cache_dev_get(cache_ctrl, dev_id);
        if (cache_dev != NULL) {
            cache_sp_algo_dev_cache_show(cache_dev, seq);
            cache_dev_put(cache_ctrl, dev_id);
        }
    }
    cache_sp_algo_print_adapt(seq, "    ---------cache info show end-------------\n");
}

static void cache_sp_algo_os_info_show(struct cache_sp_ctrl *cache_ctrl, struct seq_file *seq)
{
    unsigned long alloced_size = atomic64_read(&cache_ctrl->os_info.alloced_size);
    unsigned long blk_cnt = atomic64_read(&cache_ctrl->os_info.blk_cnt);

    cache_sp_algo_print_adapt(seq, "    ---------os info show start-------------\n");
    cache_sp_algo_print_adapt(seq, "    Os info show. (alloc_size=%lu; alloc_block_cnt=%lu)\n",
        alloced_size, blk_cnt);
    cache_sp_algo_print_adapt(seq, "    ---------os info show end-------------\n");
}

static void cache_sp_algo_pool_show(struct xsm_pool *xp, struct seq_file *seq)
{
    struct cache_sp_ctrl *cache_ctrl = (struct cache_sp_ctrl *)xp->private;

    cache_sp_algo_os_info_show(cache_ctrl, seq);
    cache_sp_algo_cache_info_show(cache_ctrl, seq);
}

static int cache_sp_blk_alloc(struct cache_dev_info *cache_dev, struct xsm_block *blk)
{
    struct cache_sp_info *cache_info = NULL;
    int ret = -ENOMEM;

    down_read(&cache_dev->cache_sem);
    list_for_each_entry(cache_info, &cache_dev->list_head, list_node) {
        if (blk->alloc_size > cache_info->alloc_max_size) {
            continue;
        }
        ret = vma_algo_alloc(cache_info->vma_ctrl, blk->alloc_size, &blk->offset, &blk->real_size);
        if (ret == 0) {
            atomic64_add(blk->real_size, &cache_dev->alloc_size);
            atomic64_add(blk->real_size, &cache_info->alloc_size);
            blk->offset += cache_info->addr;
            break;
        }
    }
    up_read(&cache_dev->cache_sem);

    if (ret != 0) {
        cache_sp_algo_dev_cache_show(cache_dev, NULL);
    }
    return ret;
}

static int cache_sp_algo_alloc_from_cache(struct cache_sp_ctrl *cache_ctrl, struct xsm_block *blk)
{
    struct cache_dev_info *cache_dev = NULL;
    unsigned int devid;
    int ret;

    devid = sp_get_device_id_from_flags(blk->flag);
    if (devid >= XSMEM_MAX_DEV_NUM) {
        xsmem_err("Invalid dev id. (dev_id=%u; flag=0x%lx)\n", devid, blk->flag);
        return -ENODEV;
    }

    cache_dev = cache_dev_get(cache_ctrl, devid);
    if (cache_dev == NULL) {
        xsmem_err("Cache dev has been released or not created. (devid=%u)\n", devid);
        return -ENXIO;
    }

    ret = cache_sp_blk_alloc(cache_dev, blk);
    cache_dev_put(cache_ctrl, devid);
    return ret;
}

static int cache_sp_algo_alloc_from_os(struct cache_sp_ctrl *cache_ctrl, struct xsm_block *blk)
{
    int ret;

    ret = sp_algo_alloc(cache_ctrl->sp_ctrl, blk->alloc_size, blk->flag, &blk->offset, &blk->real_size);
    if (ret == 0) {
        atomic64_add(blk->real_size, &cache_ctrl->os_info.alloced_size);
        atomic64_inc(&cache_ctrl->os_info.blk_cnt);
    } else {
        cache_sp_algo_os_info_show(cache_ctrl, NULL);
    }
    return ret;
}

static int cache_sp_algo_block_alloc(struct xsm_pool *xp, struct xsm_block *blk)
{
    struct cache_sp_ctrl *cache_ctrl = (struct cache_sp_ctrl *)xp->private;

    return blk_is_alloced_from_os(blk) ? cache_sp_algo_alloc_from_os(cache_ctrl, blk) :
        cache_sp_algo_alloc_from_cache(cache_ctrl, blk);
}

static struct cache_sp_info *cache_sp_rb_search(struct cache_dev_info *cache_dev, unsigned long addr)
{
    struct cache_sp_info *cache_info = NULL;
    struct rb_node *node = NULL;

    node = cache_dev->cache_rbtree.rb_node;
    while (node != NULL) {
        cache_info = rb_entry(node, struct cache_sp_info, cache_node);
        if (addr < cache_info->addr) {
            node = node->rb_left;
        } else if (addr >= (cache_info->addr + cache_info->total_size)) {
            node = node->rb_right;
        } else {
            return cache_info;
        }
    }
    return NULL;
}

static int cache_sp_blk_free(struct cache_sp_info *cache_info, struct xsm_block *blk)
{
    unsigned long blk_start = blk->offset - cache_info->addr;
    return vma_algo_free(cache_info->vma_ctrl, blk_start, blk->real_size);
}

static int _cache_sp_algo_free_to_cache(struct cache_dev_info *cache_dev, struct xsm_block *blk)
{
    struct cache_sp_info *cache_info = NULL;
    int ret;

    down_read(&cache_dev->cache_sem);
    cache_info = cache_sp_rb_search(cache_dev, blk->offset);
    if (cache_info == NULL) {
        up_read(&cache_dev->cache_sem);
        xsmem_err("Vaddr is not alloced, please check addr. (addr=0x%pK)\n", (void *)(uintptr_t)blk->offset);
        return -ENXIO;
    }

    ret = cache_sp_blk_free(cache_info, blk);
    if (ret == 0) {
        atomic64_sub(blk->real_size, &cache_dev->alloc_size);
        atomic64_sub(blk->real_size, &cache_info->alloc_size);
    }
    up_read(&cache_dev->cache_sem);

    return ret;
}

static int cache_sp_algo_free_to_cache(struct cache_sp_ctrl *cache_ctrl, struct xsm_block *blk)
{
    struct cache_dev_info *cache_dev = NULL;
    unsigned int devid;
    int ret;

    devid = sp_get_device_id_from_flags(blk->flag);
    cache_dev = cache_dev_get(cache_ctrl, devid);
    if (cache_dev == NULL) {
        xsmem_err("Cache dev has been released. (devid=%u)\n", devid);
        return -ENXIO;
    }

    ret = _cache_sp_algo_free_to_cache(cache_dev, blk);
    cache_dev_put(cache_ctrl, devid);
    return ret;
}

static int cache_sp_algo_free_to_os(struct cache_sp_ctrl *cache_ctrl, struct xsm_block *blk)
{
    int ret;

    ret = sp_algo_free(cache_ctrl->sp_ctrl, blk->offset);
    if (ret == 0) {
        atomic64_sub(blk->real_size, &cache_ctrl->os_info.alloced_size);
        atomic64_dec(&cache_ctrl->os_info.blk_cnt);
    }
    return ret;
}

static int cache_sp_algo_block_free(struct xsm_pool *xp, struct xsm_block *blk)
{
    struct cache_sp_ctrl *cache_ctrl = (struct cache_sp_ctrl *)xp->private;

    return blk_is_alloced_from_os(blk) ? cache_sp_algo_free_to_os(cache_ctrl, blk) :
        cache_sp_algo_free_to_cache(cache_ctrl, blk);
}

static int cache_sp_algo_va_check(struct xsm_pool *xp, unsigned long va, int *result)
{
    *result = sp_algo_is_sharepool_addr(va);
    return 0;
}

static struct xsm_pool_algo cache_sp_algo = {
    .num = XSMEM_ALGO_CACHE_SP,
    .name = "cache_sp_algo",
    .xsm_pool_init = cache_sp_algo_pool_init,
    .xsm_pool_free = cache_sp_algo_pool_free,
    .xsm_pool_cache_create = cache_sp_algo_cache_create,
    .xsm_pool_cache_destroy = cache_sp_algo_cache_destroy,
    .xsm_pool_cache_query = cache_sp_algo_cache_query,
    .xsm_pool_perm_add = cache_sp_algo_pool_perm_add,
    .xsm_pool_va_check = cache_sp_algo_va_check,
    .xsm_block_alloc = cache_sp_algo_block_alloc,
    .xsm_block_free = cache_sp_algo_block_free,
    .xsm_pool_show = cache_sp_algo_pool_show,
};

struct xsm_pool_algo *xsm_get_cache_sp_algo(void)
{
    return &cache_sp_algo;
}
