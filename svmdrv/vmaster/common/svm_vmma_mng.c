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
 * Create: 2023-08-21
 */

#include "svm_rbtree.h"
#include "devmm_adapt.h"
#include "devmm_common.h"
#include "svm_mem_map.h"
#include "devmm_mem_alloc_interface.h"
#include "svm_vmma_mng.h"

int devmm_vmma_mng_init(struct devmm_vmma_mng *mng, u64 va, u64 size)
{
    mng->va = va;
    mng->size = size;
    ka_task_init_rwsem(&mng->rw_sem);
    mng->root = KA_RB_ROOT;

    return 0;
}

static void _devmm_vmma_get(struct devmm_vmma_struct *vmma)
{
    ka_base_kref_get(&vmma->ref);
}

static void devmm_vmma_release(ka_kref_t *kref)
{
    struct devmm_vmma_struct *vmma = ka_container_of(kref, struct devmm_vmma_struct, ref);

    devmm_kvfree_ex(vmma);
}

static void devmm_rb_handle_of_vmma_struct(ka_rb_node_t *rbnode, struct rb_range_handle *range_handle)
{
    struct devmm_vmma_struct *vmma = ka_base_rb_entry(rbnode, struct devmm_vmma_struct, rbnode);

    range_handle->start = vmma->info.va;
    range_handle->end = vmma->info.va + vmma->info.size - 1;
}

struct devmm_vmma_struct *devmm_vmma_get(struct devmm_vmma_mng *mng, u64 va)
{
    struct rb_range_handle handle = {.start = va, .end = va};
    struct devmm_vmma_struct *vmma = NULL;
    ka_rb_node_t *rbnode = NULL;

    ka_task_down_read(&mng->rw_sem);
    if ((va >= mng->va) && (va < (mng->va + mng->size))) {
        rbnode = devmm_rb_search_by_range(&mng->root, &handle, devmm_rb_handle_of_vmma_struct);
        if (rbnode != NULL) {
            vmma = ka_base_rb_entry(rbnode, struct devmm_vmma_struct, rbnode);
            _devmm_vmma_get(vmma);
        }
    }
    ka_task_up_read(&mng->rw_sem);

    return vmma;
}

void devmm_vmma_put(struct devmm_vmma_struct *vmma)
{
    ka_base_kref_put(&vmma->ref, devmm_vmma_release);
}

static void devmm_vmma_erase(struct devmm_vmma_mng *mng, struct devmm_vmma_struct *vmma)
{
    ka_task_down_write(&mng->rw_sem);
    (void)devmm_rb_erase(&mng->root, &vmma->rbnode);
    ka_task_up_write(&mng->rw_sem);
}

static int devmm_vmma_insert(struct devmm_vmma_mng *mng, struct devmm_vmma_struct *vmma)
{
    int ret;

    ka_task_down_write(&mng->rw_sem);
    if ((vmma->info.va < mng->va) || (vmma->info.size > (mng->va + mng->size - vmma->info.va))) {
        devmm_drv_err("Out of vmma mng range. (mng->va=0x%llx; mng->size=%llu; va=0x%llx; size=%llu)\n",
            mng->va, mng->size, vmma->info.va, vmma->info.size);
        ka_task_up_write(&mng->rw_sem);
        return -EINVAL;
    }

    ret = devmm_rb_insert_by_range(&mng->root, &vmma->rbnode, devmm_rb_handle_of_vmma_struct);
    ka_task_up_write(&mng->rw_sem);
    return ret;
}

int devmm_vmma_create(struct devmm_vmma_mng *mng, struct devmm_vmma_info *info)
{
    struct devmm_vmma_struct *vmma = NULL;
    int ret;

    vmma = devmm_kvzalloc_ex(sizeof(struct devmm_vmma_struct), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (vmma == NULL) {
        devmm_drv_err("Kvzalloc failed.\n");
        return -ENOMEM;
    }

    ka_base_kref_init(&vmma->ref);
    ka_task_init_rwsem(&vmma->rw_sem);
    ka_task_mutex_init(&vmma->mutex);
    vmma->info = *info;

    ret = devmm_vmma_insert(mng, vmma);
    if (ret != 0) {
        devmm_kvfree_ex(vmma);
    }

    return ret;
}

void devmm_vmma_destroy(struct devmm_vmma_mng *mng, struct devmm_vmma_struct *vmma)
{
    devmm_vmma_erase(mng, vmma);
    devmm_vmma_put(vmma);
}

void devmm_vmmas_destroy(struct devmm_svm_process *svm_proc, struct devmm_vmma_mng *mng)
{
    struct devmm_vmma_struct *vmma = NULL;
    ka_rb_node_t *rbnode = NULL;
    u32 stamp = (u32)ka_jiffies;

    while (1) {
        ka_task_down_write(&mng->rw_sem);
        rbnode = devmm_rb_erase_one_node(&mng->root, NULL);
        ka_task_up_write(&mng->rw_sem);
        if (rbnode == NULL) {
            break;
        }

        vmma = ka_base_rb_entry(rbnode, struct devmm_vmma_struct, rbnode);
        if (vmma->info.side == DEVMM_SIDE_TYPE) {
            devmm_mem_unmap(svm_proc, &vmma->info);
        } else { /* for dev mem access_map host, host recycle */
            devmm_access_munmap_all(svm_proc, vmma);
        }
        devmm_vmma_put(vmma);
        devmm_try_cond_resched(&stamp);
    }
}

int devmm_vmma_exclusive_set(struct devmm_vmma_struct *vmma)
{
    if (ka_task_down_write_trylock(&vmma->rw_sem) == 0) {
        devmm_drv_err("Addr is occupied, should release occupied before unmap. (va=0x%llx; size=%llu)\n",
            vmma->info.va, vmma->info.size);
        return -EBUSY;
    }
    return 0;
}

void devmm_vmma_exclusive_clear(struct devmm_vmma_struct *vmma)
{
    ka_task_up_write(&vmma->rw_sem);
}

static int devmm_vmma_occupy_inc(struct devmm_vmma_struct *vmma)
{
    if (ka_task_down_read_trylock(&vmma->rw_sem) == 0) {
        devmm_drv_err("Addr is unmapping, shouldn't be concurrent to occupy. (va=0x%llx; size=%llu)\n",
            vmma->info.va, vmma->info.size);
        return -EFAULT;
    }
    return 0;
}

static void devmm_vmma_occupy_dec(struct devmm_vmma_struct *vmma)
{
    ka_task_up_read(&vmma->rw_sem);
}

void devmm_vmmas_occupy_dec(struct devmm_vmma_mng *mng, u64 va, u64 size)
{
    struct devmm_vmma_struct *vmma = NULL;
    u64 tmp_size;

    for (tmp_size = 0; tmp_size < size;) {
        vmma = devmm_vmma_get(mng, va + tmp_size);
        if (vmma == NULL) {
            return;
        }

        tmp_size += vmma->info.size;
        devmm_vmma_occupy_dec(vmma);
        devmm_vmma_put(vmma);
    }
}

int devmm_vmmas_occupy_inc(struct devmm_vmma_mng *mng, u64 va, u64 size)
{
    struct devmm_vmma_struct *vmma = NULL;
    u64 tmp_size;
    int ret;

    for (tmp_size = 0; tmp_size < size;) {
        vmma = devmm_vmma_get(mng, va + tmp_size);
        if (vmma == NULL) {
            devmm_vmmas_occupy_dec(mng, va, tmp_size);
            devmm_drv_err("Invalid addr range. (va=0x%llx; mng->va=0x%llx; mng->size=%llu)\n",
                va + tmp_size, mng->va, mng->size);
            return -EINVAL;
        }

        ret = devmm_vmma_occupy_inc(vmma);
        if (ret != 0) {
            devmm_vmma_put(vmma);
            devmm_vmmas_occupy_dec(mng, va, tmp_size);
            return ret;
        }

        tmp_size += vmma->info.size;
        devmm_vmma_put(vmma);
    }

    return 0;
}

bool devmm_addr_range_is_in_vmma(struct devmm_vmma_mng *mng, u64 va, u64 size)
{
    struct devmm_vmma_struct *vmma = NULL;
    u64 tmp_size;

    for (tmp_size = 0; tmp_size < size;) {
        vmma = devmm_vmma_get(mng, va + tmp_size);
        if (vmma == NULL) {
            return false;
        }

        tmp_size += vmma->info.size;
        devmm_vmma_put(vmma);
    }

    return true;
}
