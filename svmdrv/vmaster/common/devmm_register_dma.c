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
 * Create: 2024-04-25
 */

#include <linux/types.h>

#include "svm_cmd.h"
#include "devmm_proc_info.h"
#include "svm_heap_mng.h"
#include "devmm_common.h"
#include "svm_rbtree.h"
#include "devmm_proc_mem_copy.h"
#include "devmm_channel.h"
#include "devmm_register_dma.h"

static void devmm_register_dma_rb_range_handle(ka_rb_node_t *rbnode, struct rb_range_handle *range_handle)
{
    struct devmm_register_dma_node *node =
        ka_base_rb_entry(rbnode, struct devmm_register_dma_node, rbnode);

    range_handle->start = node->src_va;
    range_handle->end = node->src_va + node->src_size - 1;
}

struct devmm_register_dma_node *devmm_register_dma_node_get(struct devmm_register_dma_mng *mng,
    u64 addr, u64 size)
{
    struct devmm_register_dma_node *node = NULL;
    ka_rb_node_t *rbnode = NULL;
    struct rb_range_handle handle;

    handle.start = addr;
    handle.end = handle.start + size - 1;

    ka_task_read_lock_bh(&mng->rbtree_rwlock);
    rbnode = devmm_rb_search_by_range(&mng->rbtree, &handle,
        devmm_register_dma_rb_range_handle);
    if (rbnode != NULL) {
        node = ka_base_rb_entry(rbnode, struct devmm_register_dma_node, rbnode);
        ka_base_kref_get(&node->ref);
    }
    ka_task_read_unlock_bh(&mng->rbtree_rwlock);
    return node;
}

static void devmm_destory_registered_dma_node_res(struct devmm_register_dma_node *node)
{
    int pin_flg = devmm_va_is_in_svm_range(node->align_va) ? DEVMM_PIN_PAGES : DEVMM_USER_PIN_PAGES;
    struct devmm_copy_side side;

    side.blks = node->blks;
    side.blks_num = node->blks_num;
    side.num = node->num;
    devmm_pa_node_list_dma_unmap(node->devid, &side);
    devmm_clear_host_pa_node_list(pin_flg, &side);
    devmm_kvfree(node->blks);
}

static void devmm_destory_register_dma_node(struct devmm_register_dma_node *node)
{
    u64 va = node->src_va;
    u64 size = node->src_size;
    u32 devid = node->devid;
    u32 ref = ka_base_kref_read(&node->ref);

    devmm_destory_registered_dma_node_res(node);
    devmm_kvfree(node);
    devmm_drv_debug("Destory dma node. (va=0x%llx; size=%llu; dev_id=%d; kef=%u)\n", va, size, devid, ref);
}

/* might call in tasklet of dma desc destroy, don't sleep */
static void devmm_register_dma_node_release(ka_kref_t *kref)
{
    struct devmm_register_dma_node *node = ka_container_of(kref, struct devmm_register_dma_node, ref);

    devmm_destory_register_dma_node(node);
}

void devmm_register_dma_node_put(struct devmm_register_dma_node *node)
{
    ka_base_kref_put(&node->ref, devmm_register_dma_node_release);
}

static int devmm_insert_register_dma_node(struct devmm_register_dma_mng *mng,
    struct devmm_register_dma_node *node)
{
    int ret;

    ka_task_write_lock_bh(&mng->rbtree_rwlock);
    ret = devmm_rb_insert_by_range(&mng->rbtree, &node->rbnode, devmm_register_dma_rb_range_handle);
    ka_task_write_unlock_bh(&mng->rbtree_rwlock);
    if (ret != 0) {
        devmm_drv_err("Insert register dma node fail. (va=0x%llx; size=%llu; devid=%u)\n",
            node->src_va, node->src_size, node->devid);
        return ret;
    }
    return 0;
}

static int devmm_erase_register_dma_node(struct devmm_register_dma_mng *mng,
    struct devmm_register_dma_node *node)
{
    int ret;

    ka_task_write_lock_bh(&mng->rbtree_rwlock);
    ret = devmm_rb_erase(&mng->rbtree, &node->rbnode);
    ka_task_write_unlock_bh(&mng->rbtree_rwlock);
    if (ret != 0) {
        devmm_drv_err("Erase register dma node fail. (va=0x%llx; size=%llu; devid=%u)\n",
            node->src_va, node->src_size, node->devid);
        return ret;
    }
    return 0;
}

void devmm_destory_register_dma_mng_by_devid(struct devmm_svm_process *svm_proc, u32 devid)
{
    struct devmm_svm_proc_master *master_data = (struct devmm_svm_proc_master *)svm_proc->priv_data;
    struct devmm_register_dma_mng *mng = &master_data->register_dma_mng[devid];
    struct devmm_register_dma_node *node = NULL;
    ka_rb_node_t *rbnode = NULL;
    u32 stamp = (u32)ka_jiffies;
    u32 num = 0;

    while (1) {
        ka_task_write_lock_bh(&mng->rbtree_rwlock);
        rbnode = devmm_rb_erase_one_node(&mng->rbtree, NULL);
        ka_task_write_unlock_bh(&mng->rbtree_rwlock);
        if (rbnode == NULL) {
            break;
        }

        num++;
        node = ka_base_rb_entry(rbnode, struct devmm_register_dma_node, rbnode);
        devmm_register_dma_node_put(node);
        devmm_try_cond_resched(&stamp);
    }

    if (num != 0) {
        devmm_drv_debug("Destroy register dma nodes info. (destroyed_num=%u)\n", num);
    }
}

void devmm_destory_register_dma_mng(struct devmm_svm_process *svm_proc)
{
    u32 stamp = (u32)ka_jiffies;
    u32 i;

    for (i = 0; i < DEVMM_MAX_DEVICE_NUM; i++) {
        devmm_destory_register_dma_mng_by_devid(svm_proc, i);
        devmm_try_cond_resched(&stamp);
    }
}

static int devmm_get_host_addr_pa_list(struct devmm_svm_process *svm_proc, u32 devid,
    u64 va, u64 size,  struct devmm_copy_side *side)
{
    u32 i, merg_idx, host_flag;
    int write = 1;
    int ret;

    ret = devmm_get_host_phy_mach_flag(devid, &host_flag);
    if (ret != 0) {
        return ret;
    }

    if (devmm_va_is_in_svm_range(va)) {
        ret = devmm_svm_addr_pa_list_get(svm_proc, va, size, side);
        if (ret != 0) {
            return ret;
        }
    } else {
        ret = devmm_get_non_svm_addr_pa_list(svm_proc, va, size, side, write);
        if (ret != 0) {
            return ret;
        }
    }

    if (devmm_is_hccs_vm_scene(devid, host_flag)) {
        ret = devmm_vm_pa_blks_to_pm_pa_blks(devid, side->blks, side->num, side->blks);
        if (ret != 0) {
            return ret;
        }
    }

    for (merg_idx = 0, i = 0; i < side->num; i++) {
        devmm_merg_blk(side->blks, i, &merg_idx);
    }
    side->num = merg_idx;

    return 0;
}

static int devmm_register_dma_node_set_dma_phy_addr(struct devmm_svm_process *svm_proc,
    struct devmm_register_dma_node *node, u64 *num)
{
    int pin_flg = devmm_va_is_in_svm_range(node->align_va) ? DEVMM_PIN_PAGES : DEVMM_USER_PIN_PAGES;
    struct devmm_copy_side side;
    int ret;

    side.blks = node->blks;
    side.blks_num = node->blks_num;
    side.num = node->blks_num;
    side.blk_page_size = KA_MM_PAGE_SIZE;
    ret = devmm_get_host_addr_pa_list(svm_proc, node->devid, node->align_va, node->align_size, &side);
    if (ret != 0) {
        devmm_drv_err("Get host addr pa list failed. (va=0x%llx; size=%llu)\n", node->align_va, node->align_size);
        return ret;
    }

    ret = devmm_pa_node_list_dma_map(node->devid, &side);
    if (ret != 0) {
        devmm_clear_host_pa_node_list(pin_flg, &side);
        devmm_drv_err("Dma map failed. (dev_id=%u; va=0x%llx)\n", node->devid, node->align_va);
        return ret;
    }

    *num = (u64)side.num;
    return 0;
}

static int devmm_set_register_dma_node(struct devmm_svm_process *svm_proc, u64 vaddr, u64 size,
    u32 devid, struct devmm_register_dma_node *node)
{
    struct devmm_dma_block *blks = NULL;
    u64 blks_num;
    int ret;

    blks_num = devmm_get_pagecount_by_size(vaddr, size, KA_MM_PAGE_SIZE);
    blks = (struct devmm_dma_block *)devmm_kvzalloc(sizeof(struct devmm_dma_block) * blks_num);
    if (blks == NULL) {
        devmm_drv_err("Kvzalloc node blks buf failed. (blks_num=%llu)\n", blks_num);
        return -ENOMEM;
    }

    node->src_va = vaddr;
    node->align_va = ka_base_round_down(vaddr, KA_MM_PAGE_SIZE);
    node->src_size = size;
    node->align_size = ka_base_round_up(size + (vaddr - node->align_va), KA_MM_PAGE_SIZE);
    node->devid = devid;
    node->blks = blks;
    node->blks_num = blks_num;
    ka_base_kref_init(&node->ref);
    KA_BASE_RB_CLEAR_NODE(&node->rbnode);
    ret = devmm_register_dma_node_set_dma_phy_addr(svm_proc, node, &node->num);
    if (ret != 0) {
        devmm_drv_err("Register dma node set dma phy addr failed. (va=0x%llx; size=%llu; devid=%u)\n",
            node->align_va, node->align_size, node->devid);
        devmm_kvfree(blks);
        return ret;
    }

    return 0;
}

static int devmm_create_register_dma_node(struct devmm_svm_process *svm_proc,
    u64 va, u64 size, u32 devid, struct devmm_register_dma_node **out_node)
{
    struct devmm_register_dma_node *node = NULL;
    int ret;

    node = devmm_kvalloc(sizeof(struct devmm_register_dma_node), 0);
    if (node == NULL) {
        devmm_drv_err("Kzalloc node buf failed.\n");
        return -ENOMEM;
    }

    ret = devmm_set_register_dma_node(svm_proc, va, size, devid, node);
    if (ret != 0) {
        devmm_kvfree(node);
        return ret;
    }

    *out_node = node;
    return 0;
}

static int devmm_register_dma_para_check(struct devmm_svm_process *svm_proc, u64 va, u64 size)
{
    struct devmm_svm_heap *heap = NULL;

    if ((va == 0) || (size == 0) || (size > DEVMM_MAX_MAPPED_RANGE)) {
        devmm_drv_err("Src_va is zero or size is invalid. (src_va=0x%llx; size=%llu)\n", va, size);
        return -EINVAL;
    }

    if ((devmm_va_is_in_svm_range(va) == false) && devmm_va_is_in_svm_range(va + size)) {
        devmm_drv_err("Start_va and end_va have cross between svm and non svm. (src_va=0x%llx; size=%llu)\n", va, size);
        return -EINVAL;   
    }

    if (devmm_va_is_in_svm_range(va)) {
        heap = devmm_svm_get_heap(svm_proc, va);
        if (heap == NULL) {
            devmm_drv_err("devmm_svm_get_heap failed. (va=0x%llx; size=%llu)\n", va, size);
            return -EINVAL;
        }
        return devmm_check_va_add_size_by_heap(heap, va, size);
    }

    return 0;
}

int devmm_ioctl_register_dma(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_svm_proc_master *master_data = (struct devmm_svm_proc_master *)svm_proc->priv_data;
    struct devmm_register_dma_para *para = &arg->data.register_dma_para;
    struct devmm_register_dma_node *node = NULL;
    u32 devid = arg->head.devid;
    int ret;

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 19, 0)
    if (devmm_va_is_in_svm_range(para->vaddr) == false) {
        devmm_drv_run_info("Devmm register os malloc va to dma is not support in Linux versions below 5.19."
            "(va=0x%llx; size=0x%llx; devid=%u)\n", para->vaddr, para->size, devid);
        return -EOPNOTSUPP;
    }
#endif

    ret = devmm_register_dma_para_check(svm_proc, para->vaddr, para->size);
    if (ret != 0) {
        devmm_drv_err("Devmm register dma para check failed. (va=0x%llx; size=0x%llx; devid=%u)\n",
            para->vaddr, para->size, devid);
        return ret;
    }

    ret = devmm_create_register_dma_node(svm_proc, para->vaddr, para->size, devid, &node);
    if (ret != 0) {
        devmm_drv_err("Devmm create register dma node failed. (va=0x%llx; size=0x%llx; devid=%u)\n",
            para->vaddr, para->size, devid);
        return ret;
    }

    ret = devmm_insert_register_dma_node(&master_data->register_dma_mng[devid], node);
    if (ret != 0) {
        devmm_destory_register_dma_node(node);
        return ret;
    }

    devmm_drv_debug("Insert register dma node success. (va=0x%llx; size=%llu; dev_id=%d)\n",
        para->vaddr, para->size, devid);
    return 0;
}

int devmm_ioctl_unregister_dma(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_svm_proc_master *master_data = (struct devmm_svm_proc_master *)svm_proc->priv_data;
    struct devmm_unregister_dma_para *para = &arg->data.unregister_dma_para;
    struct devmm_register_dma_node *node = NULL;
    u32 devid = arg->head.devid;
    int ret;

    node = devmm_register_dma_node_get(&master_data->register_dma_mng[devid], para->vaddr, 1);
    if (node == NULL) {
        return -EFAULT;
    }

    if (para->vaddr != node->src_va) {
        devmm_drv_err("Va is not the register src va. (va=0x%llx; node->srv_va=0x%llx)\n",
            para->vaddr, node->src_va);
        devmm_register_dma_node_put(node);
        return -EFAULT;
    }

    devmm_drv_debug("Unregister dma node. (va=0x%llx; size=%llu; dev_id=%d; ref=%u)\n",
        node->src_va, node->src_size, node->devid, ka_base_kref_read(&node->ref));
    ret = devmm_erase_register_dma_node(&master_data->register_dma_mng[devid], node);
    if (ret != 0) {
        devmm_register_dma_node_put(node);
        return ret;
    }
    devmm_register_dma_node_put(node);
    devmm_register_dma_node_put(node);

    return 0;
}

