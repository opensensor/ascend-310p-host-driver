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
#include "ka_base_pub.h"
#include "ka_memory_pub.h"
#include "ka_system_pub.h"
#include "ka_task_pub.h"

#include "devmm_mem_alloc_interface.h"

#include "svm_log.h"
#include "devmm_common.h"
#include "svm_rbtree.h"
#include "svm_ref_server_occupier.h"

static u64 rb_handle_of_ref_server_occupier(ka_rb_node_t *node)
{
    struct devmm_ref_server_occupier *occupier = ka_base_rb_entry(node, struct devmm_ref_server_occupier, node);
    return (u64)occupier->sdid;
}

static struct devmm_ref_server_occupier *devmm_ref_server_occupier_create(
    struct devmm_ref_server_occupier_mng *mng, u32 sdid)
{
    struct devmm_ref_server_occupier *occupier = NULL;
    int ret;

    occupier = devmm_kvzalloc_ex(sizeof(struct devmm_ref_server_occupier), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (occupier == NULL) {
        devmm_drv_err("Kzalloc failed.\n");
        return NULL;
    }

    occupier->sdid = sdid;
    ka_base_atomic64_set(&occupier->occupy_num, 0);
    KA_BASE_RB_CLEAR_NODE(&occupier->node);

    ret = devmm_rb_insert(&mng->root, &occupier->node, rb_handle_of_ref_server_occupier);
    if (ret != 0) {
        devmm_drv_err("Insert fail. (ret=%d; sdid=%u)\n", ret, sdid);
        devmm_kvfree_ex(occupier);
    }

    return occupier;
}

static void devmm_ref_server_occupier_destroy(struct devmm_ref_server_occupier_mng *mng,
    struct devmm_ref_server_occupier *occupier)
{
    int ret;

    ret = devmm_rb_erase(&mng->root, &occupier->node);
    if (ret == 0) {
        devmm_kvfree_ex(occupier);
    }
}

static struct devmm_ref_server_occupier *devmm_get_ref_server_occupier(
    struct devmm_ref_server_occupier_mng *mng, u32 sdid)
{
    struct devmm_ref_server_occupier *occupier = NULL;
    ka_rb_node_t *node = NULL;

    node = devmm_rb_search(&mng->root, (u64)sdid, rb_handle_of_ref_server_occupier);
    if (node != NULL) {
        occupier = ka_base_rb_entry(node, struct devmm_ref_server_occupier, node);
    }

    return occupier;
}

int devmm_ref_server_occupier_add(struct devmm_ref_server_occupier_mng *mng, u32 sdid)
{
    struct devmm_ref_server_occupier *occupier = NULL;

    ka_task_down_write(&mng->rw_sem);
    occupier = devmm_get_ref_server_occupier(mng, sdid);
    if (occupier == NULL) {
        occupier = devmm_ref_server_occupier_create(mng, sdid);
        if (occupier == NULL) {
            devmm_drv_err("Create occupier failed. (sdid=%u)\n", sdid);
#ifndef EMU_ST
            ka_task_up_write(&mng->rw_sem);
            return -ENOMEM;
#endif
        }
    }

    ka_base_atomic64_inc(&occupier->occupy_num);
    ka_task_up_write(&mng->rw_sem);
    return 0;
}

int devmm_ref_server_occupier_del(struct devmm_ref_server_occupier_mng *mng, u32 sdid)
{
    struct devmm_ref_server_occupier *occupier = NULL;

    ka_task_down_write(&mng->rw_sem);
    occupier = devmm_get_ref_server_occupier(mng, sdid);
    if (occupier == NULL) {
        devmm_drv_err("Get occupier failed. (sdid=%u)\n", sdid);
#ifndef EMU_ST
        ka_task_up_write(&mng->rw_sem);
        return -EINVAL;
#endif
    }

    ka_base_atomic64_dec(&occupier->occupy_num);
    ka_task_up_write(&mng->rw_sem);
    return 0;
}
#ifndef EMU_ST
int devmm_for_each_ref_server_occupier(struct devmm_ref_server_occupier_mng *mng,
    int (*func)(struct devmm_ref_server_occupier *occupier, void *priv), void *priv)
{
    struct devmm_ref_server_occupier *occupier = NULL;
    struct devmm_ref_server_occupier *tmp = NULL;
    u32 stamp = (u32)ka_jiffies;
    int ret;

    ka_task_down_write(&mng->rw_sem);
    rbtree_postorder_for_each_entry_safe(occupier, tmp, &mng->root, node) {
        devmm_try_cond_resched(&stamp);
        ret = func(occupier, priv);
        if (ret != 0) {
            ka_task_up_write(&mng->rw_sem);
            return ret;
        }
    }
    ka_task_up_write(&mng->rw_sem);

    return 0;
}
#endif
void devmm_ref_server_occupier_mng_init(struct devmm_ref_server_occupier_mng *mng)
{
    mng->root = KA_RB_ROOT;
    ka_task_init_rwsem(&mng->rw_sem);
}

void devmm_ref_server_occupier_mng_uninit(struct devmm_ref_server_occupier_mng *mng)
{
    struct devmm_ref_server_occupier *occupier = NULL;
    struct devmm_ref_server_occupier *tmp = NULL;
    u32 stamp = (u32)ka_jiffies;

    ka_task_down_write(&mng->rw_sem);
    rbtree_postorder_for_each_entry_safe(occupier, tmp, &mng->root, node) {
        devmm_try_cond_resched(&stamp);
        devmm_ref_server_occupier_destroy(mng, occupier);
    }
    ka_task_up_write(&mng->rw_sem);
}
