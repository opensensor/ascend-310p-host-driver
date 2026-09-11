/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
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
 * Create: 2024-11-9
 */
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/semaphore.h>
#include <linux/kthread.h>

#include "ka_task_pub.h"

#include "devmm_common.h"
#include "svm_hot_reset.h"
#include "devmm_mem_alloc_interface.h"
#include "svm_master_dev_capability.h"
#include "svm_dma_prepare_pool.h"

#define DEVMM_DMA_PREPARE_POOL_LIST_NUM 4

struct devmm_dma_prepare_pool_cfg {
    u32 dma_node_num;
    u32 idle_down_thres;
    u32 idle_up_thres;
    u32 max_cnt;
    u32 default_cnt;
};

struct devmm_dma_prepare_node {
    struct devdrv_dma_prepare dma_prepare;
    ka_list_head_t list;
    u32 dma_node_num;
};

struct devmm_dma_prepare_pool {
    struct devmm_dma_prepare_pool_cfg cfg;

    ka_list_head_t head;
    ka_spinlock_t lock;

    u32 idle_node_cnt;
    u32 sum_node_cnt;
};

static struct devmm_dma_prepare_pool g_pool[DEVMM_MAX_PHY_DEVICE_NUM][DEVMM_DMA_PREPARE_POOL_LIST_NUM] = {{{{0}}}};
static ka_semaphore_t g_loop_sema[DEVMM_MAX_PHY_DEVICE_NUM];
static bool g_thread_inited[DEVMM_MAX_PHY_DEVICE_NUM] = {false};
static bool g_thread_stop_signal[DEVMM_MAX_PHY_DEVICE_NUM] = {false};

static struct devmm_dma_prepare_node *devmm_dma_prepare_node_create(u32 devid, u32 dma_node_num)
{
    struct devmm_dma_prepare_node *node = NULL;
    int ret;

    node = (struct devmm_dma_prepare_node *)devmm_kvzalloc_ex(sizeof(struct devmm_dma_prepare_node),
        KA_GFP_KERNEL | __KA_GFP_ACCOUNT); /* Dma_prepare need to be initialized as empty */
    if (node == NULL) {
        devmm_drv_err("Kzalloc node failed. (devid=%d)\n", devid);
        return NULL;
    }

    node->dma_node_num = dma_node_num;
    node->dma_prepare.devid = devid;
    ret = devdrv_dma_prepare_alloc_sq_addr(devid, dma_node_num, &node->dma_prepare);
    if (ret != 0) {
        devmm_drv_err("Devmm_dma_link_prepare_addr_alloc failed. (devid=%u)\n", devid);
        devmm_kvfree_ex(node);
        return NULL;
    }
    KA_INIT_LIST_HEAD(&node->list);

    return node;
}

static int devmm_dma_prepare_node_add(u32 devid, struct devmm_dma_prepare_pool *pool)
{
    struct devmm_dma_prepare_node *node = NULL;

    if (g_thread_stop_signal[devid]) {
#ifndef EMU_ST
        return -EHOSTDOWN;
#endif
    }

    node = devmm_dma_prepare_node_create(devid, pool->cfg.dma_node_num);
    if (node == NULL) {
        return -ENOMEM;
    }

    ka_task_spin_lock_bh(&pool->lock);
    ka_list_add(&node->list, &pool->head);
    pool->sum_node_cnt++;
    pool->idle_node_cnt++;
    ka_task_spin_unlock_bh(&pool->lock);

    devmm_drv_debug("Devmm_dma_prepare_node_add. (devid=%u; dma_node_num=%u)\n", devid, pool->cfg.dma_node_num);
    return 0;
}

void devmm_dma_prepare_node_release(struct devmm_dma_prepare_node *node)
{
    devdrv_dma_prepare_free_sq_addr(node->dma_prepare.devid, &node->dma_prepare);
    devmm_kvfree_ex(node);
}

static void devmm_dma_prepare_node_delete(u32 devid, struct devmm_dma_prepare_pool *pool)
{
    struct devmm_dma_prepare_node *node = NULL;

    ka_task_spin_lock_bh(&pool->lock);
    if (ka_list_empty(&pool->head) != 0) {
        ka_task_spin_unlock_bh(&pool->lock);
        return;
    }

    node = ka_list_last_entry(&pool->head, struct devmm_dma_prepare_node, list);
    ka_list_del_init(&node->list);
    pool->sum_node_cnt--;
    pool->idle_node_cnt--;
    ka_task_spin_unlock_bh(&pool->lock);

    devmm_dma_prepare_node_release(node);

    devmm_drv_debug("Devmm_dma_prepare_node_delete. (devid=%u; dma_node_num=%u)\n", devid, pool->cfg.dma_node_num);
}

static int devmm_dma_prepare_pool_dynamic_mng(u32 devid, struct devmm_dma_prepare_pool *pool)
{
    u32 idle_node_cnt = pool->idle_node_cnt;
    u32 sum_node_cnt = pool->sum_node_cnt;
    u32 add_count, sub_count, i;
    u32 stamp = (u32)ka_jiffies;
    int ret;

    if ((idle_node_cnt < pool->cfg.idle_down_thres) && (sum_node_cnt < pool->cfg.max_cnt)) {
        add_count = ((pool->cfg.default_cnt - idle_node_cnt) <= (pool->cfg.max_cnt - sum_node_cnt)) ?
            (pool->cfg.default_cnt - idle_node_cnt) : (pool->cfg.max_cnt - sum_node_cnt);

        for (i = 0; i < add_count; ++i) {
            ret = devmm_dma_prepare_node_add(devid, pool);
            if (ret != 0) {
                return ret;
            }
            devmm_try_cond_resched(&stamp);
        }
    } else if (idle_node_cnt > pool->cfg.idle_up_thres) {
        sub_count = idle_node_cnt - pool->cfg.default_cnt;

        for (i = 0; i < sub_count; ++i) {
            devmm_dma_prepare_node_delete(devid, pool);
            devmm_try_cond_resched(&stamp);
        }
    }

    return 0;
}

#define DEVMM_DMA_PREPARE_LOOP_TIMEOUT (60 * KA_HZ)
static int devmm_dma_prepare_pool_loop_task(void *arg)
{
    u32 devid = (u32)(uintptr_t)arg;
    u32 stamp = (u32)ka_jiffies;
    int ret, tmp;
    u32 i;

    devmm_drv_info("Devmm dma prepare pool loop start. (devid=%u)\n", devid);

    g_thread_inited[devid] = true;
    while (g_thread_stop_signal[devid] == false) {
        for (i = 0; i < DEVMM_DMA_PREPARE_POOL_LIST_NUM; ++i) {
            ret = devmm_dma_prepare_pool_dynamic_mng(devid, &g_pool[devid][i]);
            if (ret != 0) {
                devmm_drv_err_if((ret != -EHOSTDOWN), "Devmm dma prepare pool loop abnormal exit. (devid=%u)\n", devid);
                g_thread_inited[devid] = false;
#ifndef EMU_ST
                devmm_dma_prepare_pool_uninit(devid);
#endif
                return ret;
            }
            devmm_try_cond_resched(&stamp);
        }
#ifndef EMU_ST
        tmp = ka_task_down_interruptible(&g_loop_sema[devid]);
#endif
        devmm_try_cond_resched(&stamp);
    }
    g_thread_inited[devid] = false;
    devmm_drv_info("Devmm dma prepare pool loop loop end. (devid=%u)\n", devid);
    return 0;
}

void devmm_dma_prepare_pool_init(u32 devid)
{
    struct devmm_dma_prepare_pool_cfg cfg[DEVMM_DMA_PREPARE_POOL_LIST_NUM] = {
        {.dma_node_num = 16, .idle_down_thres = 1000, .idle_up_thres = 2000, .max_cnt = 3000, .default_cnt = 1500},
        {.dma_node_num = 128, .idle_down_thres = 200, .idle_up_thres = 400, .max_cnt = 600, .default_cnt = 300},
        {.dma_node_num = 512, .idle_down_thres = 100, .idle_up_thres = 200, .max_cnt = 300, .default_cnt = 150},
        /* Runtime ensure async_copy_size less than 64M of dma_desc_create, so the max dma_node_num set 64M/4K. */
        {.dma_node_num = 16384, .idle_down_thres = 8, .idle_up_thres = 16, .max_cnt = 24, .default_cnt = 12}
    };
    ka_task_struct_t *task = NULL;
    u32 i;

    if (devmm_dev_capability_support_dma_prepare_pool(devid) == false) {
        return;
    }

    for (i = 0; i < DEVMM_DMA_PREPARE_POOL_LIST_NUM; ++i) {
        g_pool[devid][i] = (struct devmm_dma_prepare_pool){{0}};
        KA_INIT_LIST_HEAD(&g_pool[devid][i].head);
        ka_task_spin_lock_init(&g_pool[devid][i].lock);
        g_pool[devid][i].cfg = cfg[i];
    }

    ka_task_sema_init(&g_loop_sema[devid], 0);
    task = ka_task_kthread_create(devmm_dma_prepare_pool_loop_task, (void *)(uintptr_t)devid, "devmm_pool_loop_task_%u", devid);
    if (KA_IS_ERR_OR_NULL(task)) {
        devmm_drv_err("ka_task_kthread_create failed. (devid=%u)\n", devid);
    } else {
        (void)ka_task_wake_up_process(task);
    }
}

#define DEVMM_DMA_PREPARE_POOL_UNINIT_WAIT_MSECONDS (100 * 1000)
void devmm_dma_prepare_pool_uninit(u32 devid)
{
    u32 stamp = (u32)ka_jiffies;
    u32 mseconds = 0;
    u32 i;

    if (devmm_dev_capability_support_dma_prepare_pool(devid) == false) {
        return;
    }

    g_thread_stop_signal[devid] = true;
    ka_task_up(&g_loop_sema[devid]);
    while (mseconds < DEVMM_DMA_PREPARE_POOL_UNINIT_WAIT_MSECONDS) {
        u32 idle_node_cnt = 0, sum_node_cnt = 0;

        ka_system_msleep(1);
        mseconds++;
        if (g_thread_inited[devid]) {
            continue;
        }

#ifndef EMU_ST
        if (devmm_is_active_reboot_status()) {
            devmm_drv_info("Is active reboot status, no wait and no recycle. (devid=%u)\n", devid);
            return;
        }
#endif

        for (i = 0; i < DEVMM_DMA_PREPARE_POOL_LIST_NUM; ++i) {
            idle_node_cnt += g_pool[devid][i].idle_node_cnt;
            sum_node_cnt += g_pool[devid][i].sum_node_cnt;
        }

        if (idle_node_cnt == sum_node_cnt) {
            break;
        }
    }

    if (mseconds >= DEVMM_DMA_PREPARE_POOL_UNINIT_WAIT_MSECONDS) {
        devmm_drv_err("devmm_dma_prepare_pool_uninit failed. (devid=%u; is_thread_inited=%d)\n", devid, g_thread_inited[devid]);
    }

    for (i = 0; i < DEVMM_DMA_PREPARE_POOL_LIST_NUM; ++i) {
        while (ka_list_empty(&g_pool[devid][i].head) == 0) {
            devmm_dma_prepare_node_delete(devid, &g_pool[devid][i]);
            devmm_try_cond_resched(&stamp);
        }
    }
}

void *devmm_dma_prepare_get_from_pool(u32 devid, u32 dma_node_num, struct devdrv_dma_prepare **dma_prepare)
{
    struct devmm_dma_prepare_pool *pool = NULL;
    struct devmm_dma_prepare_node *node = NULL;
    u32 i;

    if ((devmm_dev_capability_support_dma_prepare_pool(devid) == false) || (g_thread_inited[devid] == false)) {
        return NULL;
    }

    for (i = 0; i < DEVMM_DMA_PREPARE_POOL_LIST_NUM; ++i) {
        if (dma_node_num <= g_pool[devid][i].cfg.dma_node_num) {
            pool = &g_pool[devid][i];
            break;
        }
    }

    if (ka_unlikely(pool == NULL)) {
        devmm_drv_warn("Pool is NULL. (devid=%u; dma_node_num=%u)\n", devid, dma_node_num);
        return NULL;
    }

    ka_task_spin_lock_bh(&pool->lock);
    if (ka_list_empty(&pool->head) != 0) {
        ka_task_spin_unlock_bh(&pool->lock);
        ka_task_up(&g_loop_sema[devid]);
        return NULL;
    }

    node = ka_list_first_entry(&pool->head, struct devmm_dma_prepare_node, list);
    ka_list_del_init(&node->list);
    pool->idle_node_cnt--;
    ka_task_spin_unlock_bh(&pool->lock);
    if (pool->idle_node_cnt < pool->cfg.idle_down_thres) {
        ka_task_up(&g_loop_sema[devid]);
    }

    *dma_prepare = &node->dma_prepare;
    devmm_drv_debug("devmm_dma_prepare_pool_get. (devid=%u; dma_node_num=%u; real_dma_node_num=%u)\n",
        devid, pool->cfg.dma_node_num, dma_node_num);
    return (void *)node;
}

void devmm_dma_prepare_put_to_pool(u32 devid, void *fd)
{
    struct devmm_dma_prepare_node *node = (struct devmm_dma_prepare_node *)fd;
    struct devmm_dma_prepare_pool *pool = NULL;
    u32 i;

    for (i = 0; i < DEVMM_DMA_PREPARE_POOL_LIST_NUM; ++i) {
        if (node->dma_node_num == g_pool[devid][i].cfg.dma_node_num) {
            pool = &g_pool[devid][i];
            break;
        }
    }

    if (ka_unlikely(pool == NULL)) {
        devmm_drv_warn("List is NULL. (devid=%u)\n", devid);
        devmm_dma_prepare_node_release(node);
        return;
    }

    ka_task_spin_lock_bh(&pool->lock);
    ka_list_add(&node->list, &pool->head);
    pool->idle_node_cnt++;
    ka_task_spin_unlock_bh(&pool->lock);
    if (pool->idle_node_cnt > pool->cfg.idle_up_thres) {
        ka_task_up(&g_loop_sema[devid]);
    }
    devmm_drv_debug("devmm_dma_prepare_pool_put. (devid=%u; dma_node_num=%u)\n", devid, pool->cfg.dma_node_num);
    return;
}
