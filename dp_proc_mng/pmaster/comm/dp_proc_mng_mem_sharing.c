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
 *
 * Description:
 * Author: huawei
 * Create: 2025-9-9
 */
#include <linux/types.h>
#include <linux/sched.h>
#include "ascend_hal_define.h"
#include "devdrv_interface.h"
#include "dbl/ka_memory_query.h"
#include "dbl/uda.h"
#include "ka_list_pub.h"
#include "ka_task_pub.h"
#include "ka_memory_pub.h"
#include "ka_system_pub.h"
#include "drv_mem_sharing.h"
#include "dp_proc_mng_log.h"
#include "dp_proc_mng_mem_sharing.h"
#include "dp_proc_mng_alloc_interface.h"

#define DP_PROC_MNG_MEM_SHARING_MAX_SIZE 4096  /* 4KB */
#define DP_PROC_MNG_MEM_SHARING_MAX_NUM 1
#define DP_PROC_UDEV_MAX_NUM 1124
#define DP_PROC_WAKEUP_TIMEINTERVAL 500 /* 0.5s */

struct dp_mem_sharing_node {
    ka_list_head_t node;
    u64 sharing_pa;
    u64 dma_va;
    struct dp_proc_mng_ctx proc_info;
    struct dp_proc_mng_mem_sharing mem_shr;
};

struct dp_mem_sharing_mng {
    u32 devid;
    u32 mem_sharing_num;
    ka_mutex_t mem_mutex;
    ka_list_head_t mem_list_head;
};
struct dp_mem_sharing_mng *g_mem_sharing_mng[DP_PROC_UDEV_MAX_NUM] = { 0 };

static int dp_proc_mng_addr_dma_map(u32 devid, u64 paddr, u64 size, u64 *dma_va)
{
    ka_dma_addr_t dma_addr;
    ka_device_t *dev = NULL;

    dev = devdrv_get_pci_dev_by_devid(devid);
    if (dev == NULL) {
#ifndef EMU_ST
        dp_proc_mng_drv_err("Get dev failed. (devid=%u)\n", devid);
        return -ENODEV;
#endif
    }

    dma_addr = devdrv_dma_map_page(dev, ka_mm_pfn_to_page(KA_MM_PFN_DOWN(paddr)), 0, size, DMA_BIDIRECTIONAL);
    if (ka_mm_dma_mapping_error(dev, dma_addr)) {
#ifndef EMU_ST
        dp_proc_mng_drv_err("Dma_map_page failed. (devid=%u; error=%d; size=%llu)\n",
            devid, ka_mm_dma_mapping_error(dev, dma_addr), size);
        return -ENOMEM;
#endif
    }

    *dma_va = (u64)dma_addr;
    return 0;
}

static int dp_proc_mng_addr_dma_unmap(u32 devid, u64 dma_va, u64 size)
{
    ka_device_t *dev = NULL;

    dev = devdrv_get_pci_dev_by_devid(devid);
    if (dev == NULL) {
#ifndef EMU_ST
        dp_proc_mng_drv_err("Get dev failed. (devid=%u)\n", devid);
        return -ENODEV;
#endif
    }

    devdrv_dma_unmap_page(dev, dma_va, size, DMA_BIDIRECTIONAL);

    return 0;
}

static void dp_sharing_mem_cond_resched(u32 *pre_stamp, u32 time)
{
    u32 timeinterval;

    timeinterval = ka_system_jiffies_to_msecs(jiffies - *pre_stamp);
    if ((timeinterval > time) && !ka_base_in_atomic()) {
        ka_task_cond_resched();
        *pre_stamp = (u32)ka_jiffies;
    }
}

static bool dp_sharing_mem_node_check(struct dp_mem_sharing_mng *sharing_mng,
    struct dp_proc_mng_ctx *proc_ctx, struct dp_proc_mng_mem_sharing *mem_sharing)
{
    struct dp_mem_sharing_node *mem_node = NULL;
    struct dp_mem_sharing_node *tmp_node = NULL;
    u32 stamp = (u32)ka_jiffies;

    ka_list_for_each_entry_safe(mem_node, tmp_node, &sharing_mng->mem_list_head, node) {
        if ((proc_ctx->unique_id == mem_node->proc_info.unique_id) && (proc_ctx->pid == mem_node->proc_info.pid) &&
            (mem_node->mem_shr.ptr == mem_sharing->ptr)) {
            dp_proc_mng_drv_err("Mem have shared. (devid=%u; unique_id=%lld; pid=%d; side=%u)\n",
                sharing_mng->devid, proc_ctx->unique_id, proc_ctx->pid, mem_sharing->side);
            return false;
        }
        dp_sharing_mem_cond_resched(&stamp, DP_PROC_WAKEUP_TIMEINTERVAL);
    }

    return true;
}

static struct dp_mem_sharing_node* dp_sharing_mem_node_find(struct dp_mem_sharing_mng *sharing_mng,
    struct dp_proc_mng_ctx *proc_ctx, struct dp_proc_mng_mem_sharing *mem_sharing)
{
    struct dp_mem_sharing_node *mem_node = NULL;
    struct dp_mem_sharing_node *tmp_node = NULL;
    u32 stamp = (u32)ka_jiffies;

    ka_list_for_each_entry_safe(mem_node, tmp_node, &sharing_mng->mem_list_head, node) {
        if ((proc_ctx->unique_id == mem_node->proc_info.unique_id) && (proc_ctx->pid == mem_node->proc_info.pid) &&
            (mem_node->mem_shr.ptr == mem_sharing->ptr) && (mem_node->mem_shr.size == mem_sharing->size) &&
            (mem_node->mem_shr.side == mem_sharing->side) && (mem_node->mem_shr.accessor == mem_sharing->accessor) &&
            (mem_node->mem_shr.pg_prot == mem_sharing->pg_prot)) {
            return mem_node;
        }
        dp_sharing_mem_cond_resched(&stamp, DP_PROC_WAKEUP_TIMEINTERVAL);
    }
    dp_proc_mng_drv_err("Invalid sharing mem. (devid=%u; size=0x%llx; side=%u; accessor=%u; pg_prot=%u)\n",
        sharing_mng->devid, mem_sharing->size, mem_sharing->side, mem_sharing->accessor, mem_sharing->pg_prot);
    return NULL;
}

static int dp_sharing_mem_node_add(struct dp_mem_sharing_mng *sharing_mng,
    struct dp_proc_mng_ctx *proc_ctx, struct dp_proc_mng_mem_sharing *mem_sharing, u64 sharing_pa, u64 dma_va)
{
    struct dp_mem_sharing_node *mem_node = NULL;

    mem_node = (struct dp_mem_sharing_node *)dp_proc_vzalloc(sizeof(struct dp_mem_sharing_node));
    if (mem_node == NULL) {
#ifndef EMU_ST
        dp_proc_mng_drv_err("Alloc mem failed. (devid=%u)\n", sharing_mng->devid);
        return -ENOMEM;
#endif
    }

    (void)memcpy_s(&mem_node->proc_info, sizeof(struct dp_proc_mng_ctx), proc_ctx, sizeof(struct dp_proc_mng_ctx));
    (void)memcpy_s(&mem_node->mem_shr, sizeof(struct dp_proc_mng_mem_sharing),
        mem_sharing, sizeof(struct dp_proc_mng_mem_sharing));
    mem_node->sharing_pa = sharing_pa;
    mem_node->dma_va = dma_va;
    ka_list_add_tail(&mem_node->node, &sharing_mng->mem_list_head);
    sharing_mng->mem_sharing_num++;

    return 0;
}

static void dp_sharing_mem_node_delete(struct dp_mem_sharing_mng *sharing_mng,
    struct dp_proc_mng_ctx *proc_ctx, struct dp_proc_mng_mem_sharing *mem_sharing)
{
    struct dp_mem_sharing_node *mem_node = NULL;
    struct dp_mem_sharing_node *tmp_node = NULL;
    u32 stamp = (u32)ka_jiffies;

    ka_list_for_each_entry_safe(mem_node, tmp_node, &sharing_mng->mem_list_head, node) {
        if ((proc_ctx->unique_id == mem_node->proc_info.unique_id) && (proc_ctx->pid == mem_node->proc_info.pid) &&
            (mem_node->mem_shr.ptr == mem_sharing->ptr)) {
            ka_list_del(&mem_node->node);
            dp_proc_vfree(mem_node);
            sharing_mng->mem_sharing_num--;
        }
        dp_sharing_mem_cond_resched(&stamp, DP_PROC_WAKEUP_TIMEINTERVAL);
    }
}

static void dp_sharing_mem_recycle_mem_node(struct dp_mem_sharing_mng *sharing_mng,
    struct dp_proc_mng_ctx *proc_ctx)
{
    struct dp_mem_sharing_node *mem_node = NULL;
    struct dp_mem_sharing_node *tmp_node = NULL;
    bool is_need_delete = false;
    u32 stamp = (u32)ka_jiffies;

    ka_task_mutex_lock(&sharing_mng->mem_mutex);
    ka_list_for_each_entry_safe(mem_node, tmp_node, &sharing_mng->mem_list_head, node) {
        is_need_delete = false;
        if (proc_ctx != NULL) {
            if ((proc_ctx->unique_id == mem_node->proc_info.unique_id) && (proc_ctx->pid == mem_node->proc_info.pid)) {
                is_need_delete = true;
            }
        } else {
            is_need_delete = true;
        }

        if (is_need_delete == true) {
            (void)dp_proc_mng_addr_dma_unmap(sharing_mng->devid, mem_node->dma_va, mem_node->mem_shr.size);
            ka_list_del(&mem_node->node);
            dp_proc_vfree(mem_node);
            sharing_mng->mem_sharing_num--;
        }
        dp_sharing_mem_cond_resched(&stamp, DP_PROC_WAKEUP_TIMEINTERVAL);
    }
    ka_task_mutex_unlock(&sharing_mng->mem_mutex);
}

static void dp_mem_sharing_mng_uninit(void)
{
    u32 devid;

    for (devid = 0; devid < DP_PROC_UDEV_MAX_NUM; devid++) {
        if (g_mem_sharing_mng[devid] != NULL) {
            dp_sharing_mem_recycle_mem_node(g_mem_sharing_mng[devid], NULL);
            ka_task_mutex_destroy(&g_mem_sharing_mng[devid]->mem_mutex);
            dp_proc_vfree(g_mem_sharing_mng[devid]);
            g_mem_sharing_mng[devid] = NULL;
        }
    }

    return;
}

static int dp_mem_sharing_mng_init(void)
{
    struct dp_mem_sharing_mng *sharing_mng = NULL;
    u32 devid;
    int ret;

    for (devid = 0; devid < DP_PROC_UDEV_MAX_NUM; devid++) {
        sharing_mng = (struct dp_mem_sharing_mng *)dp_proc_vzalloc(sizeof(struct dp_mem_sharing_mng));
        if (sharing_mng == NULL) {
            dp_proc_mng_drv_err("Alloc mem failed. (devid=%u)\n", devid);
            ret = -ENOMEM;
            goto error;
        }

        sharing_mng->devid = devid;
        sharing_mng->mem_sharing_num = 0;
        ka_task_mutex_init(&sharing_mng->mem_mutex);
        KA_INIT_LIST_HEAD(&sharing_mng->mem_list_head);

        g_mem_sharing_mng[devid] = sharing_mng;
    }

    return 0;
error:
    dp_mem_sharing_mng_uninit();

    return ret;
}

static int dp_proc_mng_mem_get_pa_list(u32 devid, u64 addr, u64 size, u64 *pa_list)
{
    u32 pa_num = 1; /* continuous physical memory */
    return hal_kernel_get_mem_pa_list(devid, current->tgid, addr, size, pa_num, pa_list);
}

static int dp_proc_mng_mem_put_pa_list(u32 devid, u64 addr, u64 size, u64 *pa_list)
{
    u32 pa_num = 1; /* continuous physical memory */
    return hal_kernel_put_mem_pa_list(devid, current->tgid, addr, size, pa_num, pa_list);
}

static int (*const dp_proc_mng_mem_get_func[ACCESSOR_MAX])(u32 devid, u64 addr, u64 size, u64 *pa_list) = {
    [TS_ACCESSOR] = dp_proc_mng_mem_get_pa_list,
};

static int (*const dp_proc_mng_mem_put_func[ACCESSOR_MAX])(u32 devid, u64 addr, u64 size, u64 *pa_list) = {
    [TS_ACCESSOR] = dp_proc_mng_mem_put_pa_list,
};

static mem_sharing_func g_mem_sharing_func[ACCESSOR_MAX];

#ifndef EMU_ST
void dp_proc_mng_mem_sharing_register(mem_sharing_func handle, accessMember_t accessor)
{
    if ((accessor >= 0) && (accessor < ACCESSOR_MAX)) {
        g_mem_sharing_func[accessor] = handle;
        dp_proc_mng_drv_info("Register mem dispatch func success. (accessor=%d)\n", accessor);
    }
}
EXPORT_SYMBOL(dp_proc_mng_mem_sharing_register);

void dp_proc_mng_mem_sharing_unregister(accessMember_t accessor)
{
    if ((accessor >= 0) && (accessor < ACCESSOR_MAX)) {
        g_mem_sharing_func[accessor] = NULL;
        dp_proc_mng_drv_info("Unregister mem dispatch func success. (accessor=%d)\n", accessor);
    }
}
EXPORT_SYMBOL(dp_proc_mng_mem_sharing_unregister);
#endif

static int dp_proc_mng_mem_sharing_func_proc(struct dp_mem_sharing_mng *sharing_mng,
    struct dp_proc_mng_mem_sharing *mem_sharing, u64 convert_addr)
{
    u32 devid = sharing_mng->devid;
    int ret;

#ifndef EMU_ST
    if (g_mem_sharing_func[mem_sharing->accessor] == NULL) {
        dp_proc_mng_drv_err("Not register. (devid=%u; accessor=%u)\n", devid, mem_sharing->accessor);
        return -ENODEV;
    }

    ret = g_mem_sharing_func[mem_sharing->accessor](devid, convert_addr, mem_sharing->size);
    if (ret != 0) {
        dp_proc_mng_drv_err("Failed to dispatch. (ret=%d; devid=%u; accessor=%u; len=%llu)\n",
            ret, devid, mem_sharing->accessor, mem_sharing->size);
        return ret;
    }
#endif

    return 0;
}

static int dp_proc_mng_mem_sharing_enable_proc(struct dp_mem_sharing_mng *sharing_mng,
    struct dp_proc_mng_ctx *proc_ctx, struct dp_proc_mng_mem_sharing *mem_sharing)
{
    u32 devid = sharing_mng->devid;
    u32 id = (mem_sharing->side == MEM_HOST_SIDE) ? 0 : devid;
    u64 sharing_pa, dma_va;
    int ret;

    if (dp_sharing_mem_node_check(sharing_mng, proc_ctx, mem_sharing) == false) {
        return -EINVAL;
    }

    ret = dp_proc_mng_mem_get_func[mem_sharing->accessor](id, (u64)(uintptr_t)mem_sharing->ptr,
        mem_sharing->size, &sharing_pa);
    if (ret != 0) {
        dp_proc_mng_drv_err("Failed to get addr. (ret=%d; devid=%u; accessor=%u)\n",
            ret, id, mem_sharing->accessor);
        return ret;
    }

    ret = dp_proc_mng_addr_dma_map(devid, sharing_pa, mem_sharing->size, &dma_va);
    if (ret != 0) {
#ifndef EMU_ST
        dp_proc_mng_drv_err("Failed to update addr. (ret=%d; devid=%u; accessor=%u)\n",
            ret, devid, mem_sharing->accessor);
        goto err_addr_put;
#endif
    }

    ret = dp_proc_mng_mem_sharing_func_proc(sharing_mng, mem_sharing, dma_va);
    if (ret != 0) {
        dp_proc_mng_drv_err("Sharing func proc failed. (ret=%d; devid=%u; accessor=%u)\n",
            ret, devid, mem_sharing->accessor);
        goto err_dma_unmap;
    }

    ret = dp_sharing_mem_node_add(sharing_mng, proc_ctx, mem_sharing, sharing_pa, dma_va);
    if (ret != 0) {
        goto error_func;
    }

    return 0;

error_func:
    (void)dp_proc_mng_mem_sharing_func_proc(sharing_mng, mem_sharing, 0);
err_dma_unmap:
    dp_proc_mng_addr_dma_unmap(devid, dma_va, mem_sharing->size);
err_addr_put:
    (void)dp_proc_mng_mem_put_func[mem_sharing->accessor](id, (u64)(uintptr_t)mem_sharing->ptr,
        mem_sharing->size, &sharing_pa);

    return ret;
}

static int dp_proc_mng_mem_sharing_disable_proc(struct dp_mem_sharing_mng *sharing_mng,
    struct dp_proc_mng_ctx *proc_ctx, struct dp_proc_mng_mem_sharing *mem_sharing)
{
    u32 devid = sharing_mng->devid;
    u32 id = (mem_sharing->side == MEM_HOST_SIDE) ? 0 : devid;
    struct dp_mem_sharing_node *sharing_node = NULL;
    int ret;

    sharing_node = dp_sharing_mem_node_find(sharing_mng, proc_ctx, mem_sharing);
    if (sharing_node == NULL) {
        dp_proc_mng_drv_err("Find node failed. (devid=%u)\n", sharing_mng->devid);
        return -EINVAL;
    }

    ret = dp_proc_mng_mem_sharing_func_proc(sharing_mng, mem_sharing, 0);
    if (ret != 0) {
        dp_proc_mng_drv_err("Sharing func proc failed. (ret=%d; devid=%u; accessor=%u)\n",
            ret, devid, mem_sharing->accessor);
        return ret;
    }

    (void)dp_proc_mng_addr_dma_unmap(devid, sharing_node->dma_va, mem_sharing->size);
    (void)dp_proc_mng_mem_put_func[mem_sharing->accessor](id, (u64)(uintptr_t)mem_sharing->ptr,
        mem_sharing->size, &sharing_node->sharing_pa);
    dp_sharing_mem_node_delete(sharing_mng, proc_ctx, mem_sharing);

    return 0;
}

static int dp_proc_mng_mem_sharing_proc(u32 devid, struct dp_proc_mng_ctx *proc_ctx,
    struct dp_proc_mng_mem_sharing *mem_sharing)
{
    struct dp_mem_sharing_mng *sharing_mng = g_mem_sharing_mng[devid];
    int ret = 0;

    if (sharing_mng == NULL) {
#ifndef EMU_ST
        dp_proc_mng_drv_err("Invalid para. (devid=%u)\n", devid);
        return -EINVAL;
#endif
    }

    ka_task_mutex_lock(&sharing_mng->mem_mutex);
    if (mem_sharing->enable_flag == 0) {
        ret = dp_proc_mng_mem_sharing_enable_proc(sharing_mng, proc_ctx, mem_sharing);
    } else {
        ret = dp_proc_mng_mem_sharing_disable_proc(sharing_mng, proc_ctx, mem_sharing);
    }
    ka_task_mutex_unlock(&sharing_mng->mem_mutex);

    dp_proc_mng_drv_debug("Sharing result. (devid=%u; flag=%u; size=0x%llx; side=%u; accessor=%u; pg_prot=%u; ret=%d)\n",
        devid, mem_sharing->enable_flag, mem_sharing->size, mem_sharing->side,
        mem_sharing->accessor, mem_sharing->pg_prot, ret);

    return ret;
}

static int dp_proc_mng_ioctl_mem_sharing_para_check(u32 devid, struct dp_proc_mng_mem_sharing *mem_sharing)
{
    if ((mem_sharing->ptr == NULL) || (mem_sharing->size == 0) || (((u64)(mem_sharing->ptr) & (PAGE_SIZE - 1)) != 0) ||
        (mem_sharing->size > DP_PROC_MNG_MEM_SHARING_MAX_SIZE)) {
        dp_proc_mng_drv_err("Invalid addr or size. (size=%llu)\n", mem_sharing->size);
        return -EINVAL;
    }

    if ((mem_sharing->accessor < 0) || (mem_sharing->accessor >= ACCESSOR_MAX)) {
        dp_proc_mng_drv_err("Invalid memory accessor. (accessor=%d)\n", mem_sharing->accessor);
        return EINVAL;
    }

    if ((mem_sharing->enable_flag != 0) && (mem_sharing->enable_flag != 1)) {
        dp_proc_mng_drv_err("Invalid enable flag. (enable_flag=%d)\n", mem_sharing->enable_flag);
        return -EINVAL;
    }

    if (mem_sharing->side != MEM_HOST_SIDE) {
        dp_proc_mng_drv_err("Invalid memory side. (side=%d)\n", mem_sharing->side);
        return -EINVAL;
    }

    if (!uda_is_phy_dev(devid)) {
#ifndef EMU_ST
        return -ENOTSUPP;
#endif
    }

    return 0;
}

int dp_proc_mng_ioctl_mem_sharing(struct file *file, struct dp_proc_mng_ioctl_arg *arg)
{
    struct dp_proc_mng_ctx *proc_ctx = ka_fs_get_file_private_data(file);
    struct dp_proc_mng_mem_sharing *mem_sharing = &arg->data.mem_sharing_para;
    u32 devid = arg->head.devid;
    int ret;

    if (proc_ctx == NULL) {
#ifndef EMU_ST
        dp_proc_mng_drv_err("Invalid proc ctx.\n");
        return -EINVAL;
#endif
    }

    ret = dp_proc_mng_ioctl_mem_sharing_para_check(devid, mem_sharing);
    if (ret != 0) {
        dp_proc_mng_drv_err("Check para failed. (ret=%d)\n", ret);
        return ret;
    }

    return dp_proc_mng_mem_sharing_proc(devid, proc_ctx, mem_sharing);
}

int dp_proc_mng_exit_mem_sharing(struct dp_proc_mng_ctx *proc_ctx)
{
    u32 stamp = (u32)ka_jiffies;
    u32 devid;

    for (devid = 0; devid < DP_PROC_UDEV_MAX_NUM; devid++) {
        struct dp_mem_sharing_mng *sharing_mng = g_mem_sharing_mng[devid];
        if (sharing_mng == NULL) {
            continue;
        }

        dp_sharing_mem_recycle_mem_node(sharing_mng, proc_ctx);

        dp_sharing_mem_cond_resched(&stamp, DP_PROC_WAKEUP_TIMEINTERVAL);
    }

    return 0;
}

int dp_mem_mng_init(void)
{
    return dp_mem_sharing_mng_init();
}

void dp_mem_mng_uninit(void)
{
    dp_mem_sharing_mng_uninit();
}
