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
 * Create: 2023-02-06
 */

#include <linux/slab.h>
#include <linux/kref.h>
#include <linux/spinlock_types.h>
#include <linux/preempt.h>

#include "devdrv_interface.h"

#include "svm_proc_mng.h"
#include "svm_srcu_work.h"
#include "svm_dev_res_mng.h"
#include "svm_master_proc_mng.h"
#include "svm_master_convert.h"
#include "svm_task_dev_res_mng.h"
#include "svm_dma_prepare_pool.h"
#include "svm_master_dma_desc_mng.h"

struct devmm_dma_desc_node_info {
    struct svm_id_inst id_inst;
    struct DMA_ADDR dma_addr;
    u64 src_va;
    u64 dst_va;
    u64 size;

    u32 key;
    u32 subkey;

    int host_pid;
};

struct devmm_dma_desc_node {
    struct devmm_svm_process *svm_proc;

    ka_rb_node_t task_node;
    u64 rb_handle;

    ka_kref_t ref;

    struct devmm_dma_desc_node_info info;
};

static inline struct devmm_dma_desc_node_rb_info *devmm_dma_desc_get_rb_info(
    struct devmm_svm_proc_master *master_data, u32 key)
{
    return &master_data->dma_desc_rb_info[key % DMA_DESC_RB_INFO_CNT];
}

void devmm_dma_desc_stats_info_print(struct devmm_svm_process *svm_proc)
{
    struct devmm_svm_proc_master *master_data = (struct devmm_svm_proc_master *)svm_proc->priv_data;
    u64 node_num = 0, node_peak_num = 0;
    u32 i;

    for (i = 0; i < DMA_DESC_RB_INFO_CNT; i++) {
        struct devmm_dma_desc_node_rb_info *rb_info = &master_data->dma_desc_rb_info[i];
        node_num += rb_info->node_num;
        node_peak_num += rb_info->node_peak_num;
    }
    devmm_drv_info("Dma desc stats info. (node_num=%llu; peak_node_num=%llu)\n",
        node_num, node_peak_num);
}

static int devmm_dma_desc_res_create(struct devmm_svm_process *svm_proc,
    struct svm_dma_desc_addr_info *addr_info, struct devmm_copy_res **res)
{
    struct devmm_mem_convrt_addr_para convert_para = {0};
    struct devmm_copy_res *tmp = NULL;
    void *fd = NULL;
    int ret;

    convert_para.pSrc = addr_info->src_va;
    convert_para.pDst = addr_info->dst_va;
    convert_para.len = addr_info->size;
    convert_para.direction = DEVMM_COPY_INVILED_DIRECTION;
    /* direction is h2d and host mem is read-only, if need_write is true will cause devmm_get_user_pages to fail */
    convert_para.need_write = false;
    ret = devmm_convert_one_addr(svm_proc, &convert_para);
    if (ret != 0) {
        devmm_drv_err("Convert one addr failed.\n");
        return ret;
    }

    tmp = (struct devmm_copy_res *)convert_para.dmaAddr.phyAddr.priv;
    fd = devmm_dma_prepare_get_from_pool((u32)tmp->dev_id, tmp->dma_node_num, &tmp->dma_prepare);
    if (fd != NULL) {
#ifndef EMU_ST
        ret = devdrv_dma_fill_desc_of_sq((u32)tmp->dev_id, tmp->dma_prepare, tmp->dma_node, tmp->dma_node_num, DEVDRV_DMA_DESC_FILL_FINISH);
#endif
        if (ret != 0) {
            devmm_drv_err("Devdrv_dma_fill_desc_of_sq failed. (ret=%d)\n", ret);
            devmm_dma_prepare_put_to_pool((u32)tmp->dev_id, fd);
            devmm_destroy_one_addr(tmp);
            return ret;
        }
        tmp->dma_prepare_pool_fd = fd;
        *res = tmp;
        return 0;
    }
    tmp->dma_prepare = devdrv_dma_link_prepare((u32)tmp->dev_id, DEVDRV_DMA_DATA_TRAFFIC,
        tmp->dma_node, tmp->dma_node_num, DEVDRV_DMA_DESC_FILL_FINISH);
    if (tmp->dma_prepare == NULL) {
        devmm_drv_err("Dma_link_prepare alloc failed.\n");
        devmm_destroy_one_addr(tmp);
        return -ENOMEM;
    }

    *res = tmp;
    return 0;
}

static void devmm_dma_prepare_destroy_srcu_work(u64 *arg, u64 arg_size)
{
    struct devdrv_dma_prepare **dam_parpare = (struct devdrv_dma_prepare **)arg;

    devdrv_dma_link_free(*dam_parpare);
}

static void devmm_dma_desc_res_destroy(struct devmm_svm_process *svm_proc, struct devmm_copy_res *res)
{
    if (res->dma_prepare_pool_fd != NULL) {
        devmm_dma_prepare_put_to_pool(res->dev_id, res->dma_prepare_pool_fd);
        res->dma_prepare_pool_fd = NULL;
    } else {
        if (in_softirq()) {
            devmm_srcu_subwork_add(&svm_proc->srcu_work, DEVMM_SRCU_SUBWORK_ENSURE_EXEC_TYPE,
                    devmm_dma_prepare_destroy_srcu_work, (u64 *)&res->dma_prepare, sizeof(struct devdrv_dma_prepare));
        } else {
            devdrv_dma_link_free(res->dma_prepare);
        }
    }
    devmm_destroy_one_addr(res);
}

static inline u64 keys_to_rb_handle(u32 key, u32 subkey)
{
    return (((u64)key << 32) | (u64)subkey);  /* high 32 is key, low 32 is subkey */
}

static u64 rb_handle_of_dma_desc_node(ka_rb_node_t *node)
{
    struct devmm_dma_desc_node *tmp = ka_base_rb_entry(node, struct devmm_dma_desc_node, task_node);

    return tmp->rb_handle;
}

static void devmm_dma_desc_node_erase(struct devmm_dma_desc_node_rb_info *rb_info,
    struct devmm_dma_desc_node *node)
{
    (void)devmm_rb_erase(&rb_info->root, &node->task_node);
    rb_info->node_num--;
}

static int devmm_dma_desc_node_insert(struct devmm_dma_desc_node_rb_info *rb_info,
    struct devmm_dma_desc_node *node)
{
    int ret;

    ret = devmm_rb_insert(&rb_info->root, &node->task_node, rb_handle_of_dma_desc_node);
    if (ret == 0) {
        rb_info->node_num++;
        rb_info->node_peak_num = (rb_info->node_num > rb_info->node_peak_num) ?
            rb_info->node_num : rb_info->node_peak_num;
    }
    return ret;
}

static int devmm_dma_desc_node_create(struct devmm_svm_process *svm_proc, struct devmm_dma_desc_node_info *info)
{
    struct devmm_svm_proc_master *master_data = (struct devmm_svm_proc_master *)svm_proc->priv_data;
    struct devmm_dma_desc_node_rb_info *rb_info = devmm_dma_desc_get_rb_info(master_data, info->key);
    struct devmm_dma_desc_node *node = NULL;
    int ret;

    node = devmm_kzalloc_ex(sizeof(struct devmm_dma_desc_node), __KA_GFP_ACCOUNT | KA_GFP_ATOMIC);
    if (node == NULL) {
        devmm_drv_err("Kzalloc failed.\n");
        return -ENOMEM;
    }

    node->svm_proc = svm_proc;
    node->info = *info;
    ka_base_kref_init(&node->ref);

    node->rb_handle = keys_to_rb_handle(node->info.key, node->info.subkey);
    KA_BASE_RB_CLEAR_NODE(&node->task_node);

    ka_task_spin_lock_bh(&rb_info->spinlock);
    ret = devmm_dma_desc_node_insert(rb_info, node);
    ka_task_spin_unlock_bh(&rb_info->spinlock);
    if (ret != 0) {
        devmm_drv_err("Insert fail. (key=%u; subkey=%u)\n", info->key, info->subkey);
        devmm_kfree_ex(node);
    }
    return ret;
}

static void devmm_dma_desc_node_release(ka_kref_t *kref)
{
    struct devmm_dma_desc_node *node = ka_container_of(kref, struct devmm_dma_desc_node, ref);

    devmm_kfree_ex(node);
}

static void devmm_dma_desc_node_destroy(struct devmm_dma_desc_node *node)
{
    devmm_dma_desc_res_destroy(node->svm_proc, (struct devmm_copy_res *)node->info.dma_addr.phyAddr.priv);

    ka_base_kref_put(&node->ref, devmm_dma_desc_node_release);
}

static struct devmm_dma_desc_node *devmm_dma_desc_node_erase_by_handle(struct devmm_svm_process *svm_proc,
    u32 subkey, u32 key)
{
    struct devmm_svm_proc_master *master_info = (struct devmm_svm_proc_master *)svm_proc->priv_data;
    struct devmm_dma_desc_node_rb_info *rb_info = devmm_dma_desc_get_rb_info(master_info, key);
    struct devmm_dma_desc_node *dma_desc_node = NULL;
    ka_rb_node_t *node = NULL;

    ka_task_spin_lock_bh(&rb_info->spinlock);
    node = devmm_rb_search(&rb_info->root, keys_to_rb_handle(key, subkey), rb_handle_of_dma_desc_node);
    if (node != NULL) {
        dma_desc_node = ka_base_rb_entry(node, struct devmm_dma_desc_node, task_node);
        devmm_dma_desc_node_erase(rb_info, dma_desc_node);
    }
    ka_task_spin_unlock_bh(&rb_info->spinlock);

    return dma_desc_node;
}

static struct devmm_dma_desc_node *devmm_dma_desc_node_erase_one_by_key(
    struct devmm_svm_process *svm_proc, u32 key)
{
    struct devmm_svm_proc_master *master_info = (struct devmm_svm_proc_master *)svm_proc->priv_data;
    struct devmm_dma_desc_node_rb_info *rb_info = devmm_dma_desc_get_rb_info(master_info, key);
    struct devmm_dma_desc_node *pos = NULL;
    struct devmm_dma_desc_node *tmp = NULL;

    ka_task_spin_lock_bh(&rb_info->spinlock);
    rbtree_postorder_for_each_entry_safe(pos, tmp, &rb_info->root, task_node) {
        if (pos->info.key == key) {
            devmm_dma_desc_node_erase(rb_info, pos);
            ka_task_spin_unlock_bh(&rb_info->spinlock);
            return pos;
        }
    }
    ka_task_spin_unlock_bh(&rb_info->spinlock);

    return NULL;
}

static void devmm_dma_desc_node_info_pack(struct svm_dma_desc_addr_info *addr_info,
    struct svm_dma_desc_handle *handle, struct devmm_copy_res *res, struct devmm_dma_desc_node_info *info)
{
    svm_id_inst_pack(&info->id_inst, res->dev_id, 0);

    info->dma_addr.phyAddr.priv = (void *)res;
    info->dma_addr.phyAddr.src = (void *)(uintptr_t)res->dma_prepare->sq_dma_addr;
    info->dma_addr.phyAddr.dst = (void *)(uintptr_t)res->dma_prepare->cq_dma_addr;
    info->dma_addr.phyAddr.len = res->dma_node_alloc_num;
    info->dma_addr.phyAddr.flag = 1;

    info->src_va = addr_info->src_va;
    info->dst_va = addr_info->dst_va;
    info->size = addr_info->size;

    info->key = handle->key;
    info->subkey = handle->subkey;

    info->host_pid = handle->pid;
}

static int devmm_dma_desc_create(struct devmm_svm_process *svm_proc, struct svm_dma_desc_addr_info *addr_info,
    struct svm_dma_desc_handle *handle, struct svm_dma_desc *dma_desc)
{
    struct devmm_dma_desc_node_info info = {{0}};
    struct devmm_copy_res *res = NULL;
    int ret;

    ret = devmm_dma_desc_res_create(svm_proc, addr_info, &res);
    if (ret != 0) {
        return ret;
    }
    dma_desc->sq_addr = (void *)(uintptr_t)res->dma_prepare->sq_dma_addr;
    dma_desc->sq_tail = res->dma_node_num;

    devmm_dma_desc_node_info_pack(addr_info, handle, res, &info);
    ret = devmm_dma_desc_node_create(svm_proc, &info);
    if (ret != 0) {
        devmm_dma_desc_res_destroy(svm_proc, res);
        return ret;
    }
    /*
     * res cannot be accessed here.
     * Because res may be released by hal_kernel_svm_dma_desc_destroy if the contents of the handle are consistent.
     */
    devmm_drv_debug("Dma desc info. (sq_addr=0x%llx; sq_tail=%u)\n", (u64)dma_desc->sq_addr, dma_desc->sq_tail);
    return 0;
}

static void devmm_dma_desc_destroy_batch(struct devmm_svm_process *svm_proc, u32 key)
{
    struct devmm_dma_desc_node *node = NULL;
    u64 start_time = 0, end_time = 0;
    u32 stamp = (u32)ka_jiffies;
    u32 num = 0;

    start_time = (u64)ka_system_ktime_to_ms(ka_system_ktime_get_boottime());
    while (1) {
        node = devmm_dma_desc_node_erase_one_by_key(svm_proc, key);
        if (node == NULL) {
            break;
        }

        devmm_dma_desc_node_destroy(node);
        devmm_try_cond_resched(&stamp);
        num++;
    }
    end_time = (u64)ka_system_ktime_to_ms(ka_system_ktime_get_boottime());

    devmm_drv_debug("Destroy dma_desc info. (num=%u; cost_time=%llu ms)\n", num, end_time - start_time);
}

static void devmm_dma_desc_destroy_one(struct devmm_svm_process *svm_proc, u32 key, u32 subkey)
{
    struct devmm_dma_desc_node *node = NULL;

    node = devmm_dma_desc_node_erase_by_handle(svm_proc, subkey, key);
    if (node == NULL) {
        /* If stream destroy and cq concurrent call, may occur this issue. */
#ifndef EMU_ST
        devmm_drv_info("Key is invalid or node has been destroyed. (pid=%d; key=%u; subkey=%u)\n",
            svm_proc->process_id.hostpid, key, subkey);
#endif
        return;
    }
    devmm_dma_desc_node_destroy(node);
}

static bool is_destroy_one_dma_desc(u32 subkey)
{
    return (subkey != SVM_DMA_DESC_INVALID_SUB_KEY);
}

static void devmm_dma_desc_destroy(struct devmm_svm_process *svm_proc, u32 key, u32 subkey)
{
    if (is_destroy_one_dma_desc(subkey)) {
        devmm_dma_desc_destroy_one(svm_proc, key, subkey);
    } else {
        devmm_dma_desc_destroy_batch(svm_proc, key);
    }
}

static ka_rb_node_t *_devmm_erase_one_rb_node(struct devmm_dma_desc_node_rb_info *rb_info)
{
    ka_rb_node_t *node = NULL;

    node = devmm_rb_erase_one_node(&rb_info->root, NULL);
    if (node != NULL) {
        rb_info->node_num--;
    }
    return node;
}

static struct devmm_dma_desc_node *_devmm_erase_one_dma_desc_node(struct devmm_dma_desc_node_rb_info *rb_info)
{
    ka_rb_node_t *node = NULL;

    ka_task_spin_lock_bh(&rb_info->spinlock);
    node = _devmm_erase_one_rb_node(rb_info);
    ka_task_spin_unlock_bh(&rb_info->spinlock);

    return ((node == NULL) ? NULL : ka_base_rb_entry(node, struct devmm_dma_desc_node, task_node));
}

static struct devmm_dma_desc_node *devmm_erase_one_dma_desc_node(struct devmm_svm_proc_master *master_data)
{
    struct devmm_dma_desc_node *node = NULL;
    u32 i;

    for (i = 0; i < DMA_DESC_RB_INFO_CNT; i++) {
        struct devmm_dma_desc_node_rb_info *rb_info = &master_data->dma_desc_rb_info[i];
        node = _devmm_erase_one_dma_desc_node(rb_info);
        if (node != NULL) {
            break;
        }
    }
    return node;
}

void devmm_dma_desc_nodes_destroy_by_task_release(struct devmm_svm_process *svm_proc)
{
    struct devmm_svm_proc_master *master_data = (struct devmm_svm_proc_master *)svm_proc->priv_data;
    struct devmm_dma_desc_node *node = NULL;
    u32 stamp = (u32)ka_jiffies;
    u32 num = 0;

    while (1) {
        node = devmm_erase_one_dma_desc_node(master_data);
        if (node == NULL) {
            break;
        }

        num++;
        devmm_dma_desc_node_destroy(node);
        devmm_try_cond_resched(&stamp);
    }

    if (num != 0) {
        devmm_drv_info("Destroy dma_desc nodes info. (destroyed_num=%u)\n", num);
    }
}

/* If return -ESRCH, tsagent will check and not print err. */
int hal_kernel_svm_dma_desc_create(struct svm_dma_desc_addr_info *addr_info,
    struct svm_dma_desc_handle *handle, struct svm_dma_desc *dma_desc)
{
    struct devmm_svm_process_id process_id = {0};
    struct devmm_svm_process *svm_proc = NULL;
    int ret;

    if ((addr_info == NULL) || (handle == NULL) || (dma_desc == NULL)) {
#ifndef EMU_ST
        return -EINVAL;
#endif
    }

    process_id.hostpid = handle->pid;
    ka_task_might_sleep();

    if (handle->subkey == SVM_DMA_DESC_INVALID_SUB_KEY) {
        return -EINVAL;
    }

    svm_proc = devmm_svm_proc_get_by_process_id(&process_id);
    if (svm_proc == NULL) {
        return -ESRCH;
    }

    ka_task_down_read(&svm_proc->heap_sem);
    ret = devmm_dma_desc_create(svm_proc, addr_info, handle, dma_desc);
    ka_task_up_read(&svm_proc->heap_sem);
    devmm_svm_proc_put(svm_proc);
    return ret;
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_svm_dma_desc_create);

/* destroy will be called in tasklet */
void hal_kernel_svm_dma_desc_destroy(struct svm_dma_desc_handle *handle)
{
    struct devmm_svm_process_id process_id = {0};
    struct devmm_svm_process *svm_proc = NULL;

    if (handle == NULL) {
#ifndef EMU_ST
        return;
#endif
    }

    process_id.hostpid = handle->pid;
    svm_proc = devmm_svm_proc_get_by_process_id(&process_id);
    if (svm_proc == NULL) {
        return;
    }

    devmm_dma_desc_destroy(svm_proc, handle->key, handle->subkey);
    devmm_svm_proc_put(svm_proc);
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_svm_dma_desc_destroy);

