/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Description:
 * Author: huawei
 * Create: 2024-7-20
 */

#include <linux/types.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/gfp.h>
#include <linux/sched.h>
#include <linux/delay.h>

#include "ascend_hal_define.h"
#include "ka_rbtree.h"
#include "ka_memory.h"
#include "ka_module_init.h"
#include "ka_memory_mng.h"

#define KA_SUB_MODULE_TYPE_MASK ((1U << KA_MODULE_TYPE_BIT) - 1)
/* MODULE_TYPE : (16~31)bit of module_id */
#define ka_get_module_type(module_id) (module_id >> KA_MODULE_TYPE_BIT)
/* KA_SUB_MODULE_TYPE : (0~15)bit of module_id */
#define ka_get_sub_module_type(module_id) (module_id & KA_SUB_MODULE_TYPE_MASK)

struct ka_mem_stats {
    atomic64_t cur_alloc_times;
    atomic64_t cur_free_times;
    atomic64_t cur_alloc_size[KA_SUB_MODULE_TYPE_MAX];
    atomic64_t peak_alloc_size[KA_SUB_MODULE_TYPE_MAX];
};

struct ka_module_mem_mng {
    struct ka_mem_stats mem_stats;
    struct rb_root rbroot;
    spinlock_t rb_spinlock;
};

struct ka_mem_mng {
    struct ka_module_mem_mng mem_mng[HAL_MODULE_TYPE_MAX];
    bool is_enable_mem_record;
};

struct ka_mem_node {
    struct rb_node node;
    size_t size;
    unsigned long va;
};

STATIC struct ka_mem_mng g_ka_mem_mng;

STATIC inline struct ka_module_mem_mng *ka_get_mem_mng_node(unsigned int module_id)
{
    return &(g_ka_mem_mng.mem_mng[ka_get_module_type(module_id)]);
}

STATIC inline struct ka_mem_stats *ka_get_mem_stats(unsigned int module_type)
{
    return &(g_ka_mem_mng.mem_mng[module_type].mem_stats);
}

STATIC void ka_mem_size_record_clear(void)
{
    int i, j;

    for (i = 0; i < HAL_MODULE_TYPE_MAX; i++) {
        for (j = 0; j < KA_SUB_MODULE_TYPE_MAX; j++) {
            struct ka_mem_stats *mem_stats = ka_get_mem_stats(i);
            atomic64_set(&mem_stats->cur_alloc_size[j], 0);
            atomic64_set(&mem_stats->peak_alloc_size[j], 0);
        }
    }
}

STATIC char *ka_module_type_to_str(uint32_t module_type)
{
    STATIC char *module_str[HAL_MODULE_TYPE_MAX] = {
            [HAL_MODULE_TYPE_VNIC] = "VNIC",
            [HAL_MODULE_TYPE_HDC] = "HDC",
            [HAL_MODULE_TYPE_DEVMM] = "DEVMM",
            [HAL_MODULE_TYPE_DEV_MANAGER] = "DEV_MANAGER",
            [HAL_MODULE_TYPE_DMP] = "DMP",
            [HAL_MODULE_TYPE_FAULT] = "FAULT",
            [HAL_MODULE_TYPE_UPGRADE] = "UPGRADE",
            [HAL_MODULE_TYPE_PROCESS_MON] = "PROCESS_MON",
            [HAL_MODULE_TYPE_LOG] = "LOG",
            [HAL_MODULE_TYPE_PROF] = "PROF",
            [HAL_MODULE_TYPE_DVPP] = "DVPP",
            [HAL_MODULE_TYPE_PCIE] = "PCIE",
            [HAL_MODULE_TYPE_IPC] = "IPC",
            [HAL_MODULE_TYPE_TS_DRIVER] = "TS_DRIVER",
            [HAL_MODULE_TYPE_SAFETY_ISLAND] = "SAFETY_ISLAND",
            [HAL_MODULE_TYPE_BSP] = "BSP",
            [HAL_MODULE_TYPE_USB] = "USB",
            [HAL_MODULE_TYPE_NET] = "NET",
            [HAL_MODULE_TYPE_EVENT_SCHEDULE] = "EVENT_SCHEDULE",
            [HAL_MODULE_TYPE_BUF_MANAGER] = "BUF_MANAGER",
            [HAL_MODULE_TYPE_QUEUE_MANAGER] = "QUEUE_MANAGER",
            [HAL_MODULE_TYPE_DP_PROC_MNG] = "DP_PROC_MNG",
            [HAL_MODULE_TYPE_BBOX] = "BBOX",
            [HAL_MODULE_TYPE_VMNG] = "VMNG",
            [HAL_MODULE_TYPE_COMMON] = "COMMON",
    };

    return module_str[module_type];
}

STATIC void ka_mem_node_free(struct ka_mem_node *mem_node)
{
    kfree(mem_node);
}

STATIC void ka_mem_node_release(struct rb_node *node)
{
    struct ka_mem_node *mem_node = rb_entry(node, struct ka_mem_node, node);

    ka_mem_node_free(mem_node);
}

#define KA_WAKEUP_TIMEINTERVAL 500  /* 0.5S */
STATIC void ka_try_cond_resched(unsigned long *pre_stamp)
{
    unsigned int timeinterval;

    timeinterval = jiffies_to_msecs(jiffies - *pre_stamp);
    if (timeinterval > KA_WAKEUP_TIMEINTERVAL) {
        cond_resched();
        *pre_stamp = (unsigned long)jiffies;
    }
}

STATIC void ka_destory_all_mem_node(void)
{
    unsigned long stamp = (unsigned long)jiffies;
    int i;

    for (i = 0; i < HAL_MODULE_TYPE_MAX; i++) {
        struct ka_module_mem_mng *mem_mng = ka_get_mem_mng_node(i);
        struct rb_node *node = NULL;
        unsigned long flags;
        while (1) {
            spin_lock_irqsave(&mem_mng->rb_spinlock, flags);
            node = ka_rb_erase_one_node(&mem_mng->rbroot);
            spin_unlock_irqrestore(&mem_mng->rb_spinlock, flags);
            if (node == NULL) {
                break;
            }
            ka_mem_node_release(node);
            ka_try_cond_resched(&stamp);
        }
    }
}

STATIC void ka_mem_size_record_reset(void)
{
    ka_mem_size_record_clear();
    ka_destory_all_mem_node();
}

void ka_mem_record_status_reset(bool is_enable)
{
    g_ka_mem_mng.is_enable_mem_record = is_enable;
    ka_mem_size_record_reset();
}

bool ka_is_enable_mem_record(void)
{
    return g_ka_mem_mng.is_enable_mem_record;
}

int ka_mem_stats_show(struct seq_file *seq, void *offset)
{
    unsigned int i, j;

    for (i = 0; i < HAL_MODULE_TYPE_MAX; i++) {
        struct ka_mem_stats *mem_stats = ka_get_mem_stats(i);
        if (atomic64_read(&mem_stats->cur_alloc_times) != 0) {
            seq_printf(seq, "Ka_mem_alloc_times_stats. (module_name=%s; cur_alloc_times=%lu; cur_free_times=%lu)\n",
                ka_module_type_to_str(i), (unsigned long)atomic64_read(&mem_stats->cur_alloc_times),
                (unsigned long)atomic64_read(&mem_stats->cur_free_times));
        }
        if (g_ka_mem_mng.is_enable_mem_record) {
            for (j = 0; j < KA_SUB_MODULE_TYPE_MAX; j++) {
                if (atomic64_read(&mem_stats->peak_alloc_size[j]) != 0) {
                    seq_printf(seq, "Ka_mem_alloc_size_stats(bytes). (module_name=%s; sub_module_type=%u; cur_size=%lu; peak_size=%lu)\n",
                        ka_module_type_to_str(i), j, (unsigned long)atomic64_read(&mem_stats->cur_alloc_size[j]),
                        (unsigned long)atomic64_read(&mem_stats->peak_alloc_size[j]));
                }
            }
        }
    }
    return 0;
}

STATIC void ka_mng_node_init(void)
{
    int i;

    for (i = 0; i < HAL_MODULE_TYPE_MAX; i++) {
        spin_lock_init(&(g_ka_mem_mng.mem_mng[i].rb_spinlock));
        g_ka_mem_mng.mem_mng[i].rbroot = RB_ROOT;
    }
}

void ka_mem_mng_uninit(void)
{
    ka_mem_size_record_reset();
}

void ka_mem_mng_init(void)
{
    ka_mng_node_init();
    g_ka_mem_mng.is_enable_mem_record = false;
}

STATIC struct ka_mem_node *ka_mem_node_alloc(size_t size, unsigned long va)
{
    struct ka_mem_node *mem_node = NULL;

    mem_node = (struct ka_mem_node *)kmalloc(sizeof(struct ka_mem_node), GFP_ATOMIC | __GFP_ACCOUNT);
    if (mem_node == NULL) {
        return NULL;
    }
    RB_CLEAR_NODE(&mem_node->node);
    mem_node->size = size;
    mem_node->va = va;

    return mem_node;
}

#define KA_MAX_RETRY_TIMES 3
STATIC void ka_mem_alloc_size_inc(unsigned int module_id, size_t size, unsigned long va)
{
    struct ka_mem_stats *mem_stats = ka_get_mem_stats(ka_get_module_type(module_id));
    unsigned int sub_type = ka_get_sub_module_type(module_id);
    unsigned long tmp;
    int retry = 0;

    tmp = (unsigned long)atomic64_add_return(size, &mem_stats->cur_alloc_size[sub_type]);
    while (retry < KA_MAX_RETRY_TIMES) {
        unsigned long peak_size = (unsigned long)atomic64_read(&mem_stats->peak_alloc_size[sub_type]);
        if (tmp <= peak_size) {
            break;
        }
        if ((unsigned long)atomic64_cmpxchg(&mem_stats->peak_alloc_size[sub_type], peak_size, tmp) == peak_size) {
            break;
        }
        retry++;
    }

    ka_debug("Size_inc_record. (va=0x%lx; sub_type=%u; cur_size=%lu bytes; peak_size=%lu bytes; cur_alloc_times=%lu; cur_free_times=%lu)\n",
        va, sub_type, (unsigned long)atomic64_read(&mem_stats->cur_alloc_size[sub_type]),
        (unsigned long)atomic64_read(&mem_stats->peak_alloc_size[sub_type]),
        (unsigned long)atomic64_read(&mem_stats->cur_alloc_times),
        (unsigned long)atomic64_read(&mem_stats->cur_free_times));
}

STATIC unsigned long rb_handle_of_mem_stats_node(struct rb_node *node)
{
    struct ka_mem_node *mem_node = rb_entry(node, struct ka_mem_node, node);

    return mem_node->va;
}

STATIC int ka_mem_node_insert(struct ka_mem_node *mem_node, unsigned int module_id)
{
    struct ka_module_mem_mng *module_mng = ka_get_mem_mng_node(module_id);
    unsigned long flags;
    int ret;

    spin_lock_irqsave(&module_mng->rb_spinlock, flags);
    ret = ka_rb_insert(&module_mng->rbroot, &mem_node->node, rb_handle_of_mem_stats_node);
    spin_unlock_irqrestore(&module_mng->rb_spinlock, flags);

    return ret;
}

STATIC void ka_mem_node_create(unsigned int module_id, size_t size, unsigned long va)
{
    struct ka_mem_node *mem_node = NULL;
    int ret;

    mem_node = ka_mem_node_alloc(size, va);
    if (mem_node == NULL) {
        return;
    }
    ret = ka_mem_node_insert(mem_node, module_id);
    if (ret != 0) {
        ka_mem_node_free(mem_node);
        return;
    }
    ka_mem_alloc_size_inc(module_id, size, va);
}

STATIC void ka_mem_alloc_size_dec(unsigned int module_id, size_t size, unsigned long va)
{
    struct ka_mem_stats *mem_stats = ka_get_mem_stats(ka_get_module_type(module_id));
    unsigned int sub_type = ka_get_sub_module_type(module_id);

    atomic64_sub(size, &mem_stats->cur_alloc_size[sub_type]);

    ka_debug("Size_dec_record. (va=0x%lx; sub_type=%u; cur_size=%lu bytes; peak_size=%lu bytes; cur_alloc_times=%lu; cur_free_times=%lu)\n",
        va, sub_type, (unsigned long)atomic64_read(&mem_stats->cur_alloc_size[sub_type]),
        (unsigned long)atomic64_read(&mem_stats->peak_alloc_size[sub_type]),
        (unsigned long)atomic64_read(&mem_stats->cur_alloc_times),
        (unsigned long)atomic64_read(&mem_stats->cur_free_times));
}

STATIC void ka_mem_node_destroy(unsigned int module_id, struct ka_mem_node *mem_node, unsigned long va)
{
    ka_mem_alloc_size_dec(module_id, mem_node->size, va);
    ka_mem_node_free(mem_node);
}

STATIC inline void ka_mem_alloc_times_inc(unsigned int module_id)
{
    atomic64_inc(&ka_get_mem_stats(ka_get_module_type(module_id))->cur_alloc_times);
}

STATIC inline void ka_mem_free_times_inc(unsigned int module_id)
{
    atomic64_inc(&ka_get_mem_stats(ka_get_module_type(module_id))->cur_free_times);
}

void ka_mem_alloc_stat_add(unsigned int module_id, size_t size, unsigned long va)
{
    if (va == 0) {
        return;
    }
    if (unlikely(g_ka_mem_mng.is_enable_mem_record)) {
        ka_mem_node_create(module_id, size, va);
    }
    ka_mem_alloc_times_inc(module_id);
}

STATIC struct ka_mem_node *ka_mem_rbnode_search_and_erase(unsigned int module_id, unsigned long va)
{
    struct ka_module_mem_mng *module_mng = ka_get_mem_mng_node(module_id);
    struct rb_node *node = NULL;
    unsigned long flags;

    spin_lock_irqsave(&module_mng->rb_spinlock, flags);
    node = ka_rb_search(&module_mng->rbroot, va, rb_handle_of_mem_stats_node);
    if (node != NULL) {
        ka_rb_erase(&module_mng->rbroot, node);
    }
    spin_unlock_irqrestore(&module_mng->rb_spinlock, flags);
    if (node == NULL) {
        return NULL;
    }
    return rb_entry(node, struct ka_mem_node, node);
}

void ka_mem_alloc_stat_del(unsigned long va, unsigned int module_id)
{
    if (unlikely(g_ka_mem_mng.is_enable_mem_record)) {
        struct ka_mem_node *mem_node = ka_mem_rbnode_search_and_erase(module_id, va);
        if (mem_node != NULL) {
            ka_mem_node_destroy(module_id, mem_node, va);
        }
    }
    ka_mem_free_times_inc(module_id);
}

