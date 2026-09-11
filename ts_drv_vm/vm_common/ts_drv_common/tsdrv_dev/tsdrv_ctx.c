/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2023. All rights reserved.
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
#include <linux/rbtree.h>
#include <linux/list.h>
#include <linux/delay.h>
#ifdef AOS_LLVM_BUILD
#include <linux/mm.h>
#include <linux/share_pool.h>
#endif

#include "tsdrv_ctx.h"
#include "tsdrv_device.h"
#include "devdrv_cbctrl.h"
#include "devdrv_cqsq.h"
#include "devdrv_common.h"
#include "tsdrv_interface.h"
#include "tsdrv_common.h"

/* mirror ctx status set by svm, it's defined in devmm_dev.h */
#define DEVMM_SVM_NORMAL_EXITED_FLAG    0xEEEEEEEEU
#define DEVMM_SVM_ABNORMAL_EXITED_FLAG  0U
#ifndef AOS_LLVM_BUILD
/* Remap is temporarily stored here. later create a tsdrv_proc file and store both mmap and remap in the file. */
int tsdrv_check_va_range(struct tsdrv_ctx *ctx, struct vm_area_struct *vma,
    unsigned long addr, unsigned long size)
{
    unsigned long end = addr + KA_MM_PAGE_ALIGN(size);

    if (ka_mm_get_vm_flags(vma) & KA_VM_HUGETLB) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Input pararmeter is error. (addr=0x%pK; vm_flags=0x%lx)\n",
            (void *)(uintptr_t)addr, ka_mm_get_vm_flags(vma));
        return -EINVAL;
#endif
    }

    if (vma->vm_private_data != ctx) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Input pararmeter is not match. (addr=0x%pK vma=%pK ctx=%pK)\n",
            (void *)(uintptr_t)addr, vma, ctx);
        return -EINVAL;
#endif
    }

#ifndef TSDRV_UT
    if (addr & (KA_MM_PAGE_SIZE - 1)) {
        TSDRV_PRINT_ERR("Input pararmeter is error. (addr=0x%pK)\n", (void *)(uintptr_t)addr);
        return -EINVAL;
    }
#endif /* TSDRV_UT */

    if ((addr < vma->vm_start) || (addr > vma->vm_end) || (end > vma->vm_end) || (addr >= end)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Input pararmeter is error. (addr=0x%pK; len=0x%lx; vm_start=0x%pK; vm_end=0x%pK)\n",
            (void *)(uintptr_t)addr, size, (void *)(uintptr_t)vma->vm_start, (void *)(uintptr_t)vma->vm_end);
        return -EINVAL;
#endif
    }

    return 0;
}

int tsdrv_check_va_map(struct vm_area_struct *vma, unsigned long addr, unsigned long size)
{
    unsigned long end = addr + KA_MM_PAGE_ALIGN(size);
    unsigned long va_check;
    unsigned long pfn;

    for (va_check = addr; va_check < end; va_check += KA_MM_PAGE_SIZE) {
        if (ka_mm_follow_pfn(vma, va_check, &pfn) == 0) {
            TSDRV_PRINT_ERR("Check va is map. (addr=0x%pK; size=%lx; va_check=0x%pK)\n",
                (void *)(uintptr_t)addr, size, (void *)(uintptr_t)va_check);
            return -EINVAL;
        }
    }

    return 0;
}

int tsdrv_check_va_unmap(struct vm_area_struct *vma, unsigned long addr, unsigned long size)
{
    unsigned long end = addr + KA_MM_PAGE_ALIGN(size);
    unsigned long va_check;
    unsigned long pfn;

    for (va_check = addr; va_check < end; va_check += KA_MM_PAGE_SIZE) {
#ifndef TSDRV_UT
        if (ka_mm_follow_pfn(vma, va_check, &pfn) != 0) {
            TSDRV_PRINT_ERR("Check va is unmap. (addr=0x%pK; size=%lx; va_check=0x%pK)\n",
                (void *)(uintptr_t)addr, size, (void *)(uintptr_t)va_check);
            return -EINVAL;
        }
#endif
    }

    return 0;
}

static int tsdrv_get_remap_phy_addr(struct tsdrv_ctx *ctx, u32 tsId,
    struct tsdrv_mem_map_para *map_para, struct tsdrv_phy_addr_get *info)
{
    if (map_para->id_type == TSDRV_SQ_ID) {
        return tsdrv_get_sq_mem_phy_addr(ctx, tsId, map_para->id, map_para->sub_type, info);
    } else {
        return tsdrv_get_cq_mem_phy_addr(ctx, tsId, map_para->id, map_para->sub_type, info);
    }
}

static int tsdrv_get_remap_prot(struct tsdrv_ctx *ctx, u32 tsId, struct tsdrv_mem_map_para *map_para,
    ka_pgprot_t vm_page_prot, ka_pgprot_t *prot)
{
    if (map_para->id_type == TSDRV_SQ_ID) {
        return tsdrv_get_sq_mem_map_prot(ctx, tsId, map_para->sub_type, vm_page_prot, prot);
    } else {
        return tsdrv_get_cq_mem_map_prot(ctx, tsId, map_para, vm_page_prot, prot);
    }
}

void tsdrv_zap_vma_ptes(struct vm_area_struct *vma, unsigned long vaddr, size_t size)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 18, 0)
    int ret;
    ret = ka_mm_zap_vma_ptes(vma, vaddr, KA_MM_PAGE_ALIGN(size));
    if (ret != 0) {
        TSDRV_PRINT_WARN("zap_vma_ptes fail, va=0x%pK, ret=%d, vma=%pK, vm_flags=0x%lx.\n",
            (void *)(uintptr_t)vaddr, ret, vma, ka_mm_get_vm_flags(vma));
    }
#else
    ka_mm_zap_vma_ptes(vma, vaddr, KA_MM_PAGE_ALIGN(size));
#endif
}

static int tsdrv_remap_pfn(struct tsdrv_ctx *ctx, u32 tsId,
    struct tsdrv_mem_map_para *map_para, struct vm_area_struct *vma)
{
    struct tsdrv_phy_addr_get info;
    unsigned long va, pfn, len, align_len;
    ka_pgprot_t prot;
    int ret;

    ret = tsdrv_get_remap_prot(ctx, tsId, map_para, vma->vm_page_prot, &prot);
    if (ret != 0) {
        return ret;
    }

    info.offset = 0;
    align_len = KA_MM_PAGE_ALIGN(map_para->len);
    info.len = align_len;
    info.paddr_len = 0;

    do {
        info.offset += info.paddr_len;
        info.len -= info.paddr_len;

        ret = tsdrv_get_remap_phy_addr(ctx, tsId, map_para, &info);
        if (ret != 0) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("Get phy addr failed. (id_type=%u; sub_type=%u; offset=%u; total_len=%lu)\n",
                map_para->id_type, map_para->sub_type, info.offset, map_para->len);
            goto error;
#endif
        }

        va = map_para->va + info.offset;
        len = info.paddr_len;
        pfn = __phys_to_pfn(info.paddr);

        ret = ka_mm_remap_pfn_range(vma, va, pfn, len, prot);
        if (ret != 0) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("Remap va failed. (vma_start=0x%pK; end=0x%pK; addr=0x%pK; len=%lx; offset=%u)\n",
                (void *)(uintptr_t)vma->vm_start, (void *)(uintptr_t)vma->vm_end, (void *)(uintptr_t)va,
                len, info.offset);
            goto error;
#endif
        }
    } while ((info.offset + info.paddr_len) < align_len);

    return 0;
#ifndef TSDRV_UT
error:
    if (info.offset > 0) {
        tsdrv_zap_vma_ptes(vma, map_para->va, info.offset);
    }
    return ret;
#endif
}

#ifdef CFG_TSDRV_VM_ENV
static int tsdrv_vm_flag_check(struct tsdrv_ctx *ctx, struct vm_area_struct *vma,
    struct tsdrv_mem_map_para *map_para)
{
    if (map_para->sub_type == MEM_MAP_SUBTYPE_SHM) {
        if (ka_mm_get_vm_flags(vma) & KA_VM_WRITE) {
            TSDRV_PRINT_ERR("Invalid vm_flags. (vm_flags=0x%lx)\n", ka_mm_get_vm_flags(vma));
            return -ENODEV;
        }
    }
    return 0;
}
#else /* !CFG_TSDRV_VM_ENV */
static int tsdrv_vm_flag_check(struct tsdrv_ctx *ctx, struct vm_area_struct *vma,
    struct tsdrv_mem_map_para *map_para)
{
    if (map_para->sub_type == MEM_MAP_SUBTYPE_SHM) {
        if (!tsdrv_is_pm_ctx(ctx)) {
            if ((ka_mm_get_vm_flags(vma) & KA_VM_WRITE) != 0) {
                TSDRV_PRINT_ERR("Invalid vm_flags. (vm_flags=0x%lx)\n", ka_mm_get_vm_flags(vma));
                return -ENODEV;
            }
        }
    }
    return 0;
}
#endif /* CFG_TSDRV_VM_ENV */

int tsdrv_remap_va(struct tsdrv_ctx *ctx, u32 tsId, struct tsdrv_mem_map_para *map_para)
{
    ka_vm_area_struct_t *vma = NULL;
    unsigned long va, len;
    int ret;

    va = map_para->va;
    len = map_para->len;

    ka_task_down_write(ka_mm_get_mmap_sem(current->mm));

    vma = ka_mm_find_vma(current->mm, va);
    if (vma == NULL) {
#ifndef TSDRV_UT
        ka_task_up_write(ka_mm_get_mmap_sem(current->mm));
        TSDRV_PRINT_ERR("Find vma failed. (va=0x%pK)\n", (void *)(uintptr_t)va);
        return -EINVAL;
#endif
    }
    ret = tsdrv_vm_flag_check(ctx, vma, map_para);
    if (ret != 0) {
#ifndef TSDRV_UT
        ka_task_up_write(ka_mm_get_mmap_sem(current->mm));
        TSDRV_PRINT_ERR("vma_flag check fail.\n");
        return ret;
#endif
    }

    ret = tsdrv_check_va_range(ctx, vma, va, len);
    if (ret != 0) {
#ifndef TSDRV_UT
        ka_task_up_write(ka_mm_get_mmap_sem(current->mm));
        TSDRV_PRINT_ERR("Check va range failed. (va=0x%pK; len=%lx)\n", (void *)(uintptr_t)va, len);
        return ret;
#endif
    }

    ret = tsdrv_check_va_map(vma, va, len);
    if (ret != 0) {
#ifndef TSDRV_UT
        ka_task_up_write(ka_mm_get_mmap_sem(current->mm));
        return ret;
#endif
    }

    ret = tsdrv_remap_pfn(ctx, tsId, map_para, vma);
    ka_task_up_write(ka_mm_get_mmap_sem(current->mm));
    TSDRV_PRINT_DEBUG("Remap va success. (va=0x%lx; len=0x%lx; id_type=%u; subtype=%u; side=%u)\n", va, len,
        map_para->id_type, map_para->sub_type, map_para->addr_side);

    return ret;
}

int tsdrv_unmap_va(struct tsdrv_ctx *ctx, u32 tsId, struct tsdrv_mem_unmap_para *unmap_para)
{
    struct vm_area_struct *vma = NULL;
    unsigned long va, len;
    int ret;

    va = unmap_para->va;
    len = unmap_para->len;

    ka_task_down_write(ka_mm_get_mmap_sem(current->mm));

    vma = ka_mm_find_vma(current->mm, va);
    if (vma == NULL) {
#ifndef TSDRV_UT
        ka_task_up_write(ka_mm_get_mmap_sem(current->mm));
        TSDRV_PRINT_ERR("Find vma failed unmap. (va=0x%lx)\n", va);
        return -EINVAL;
#endif
    }

    ret = tsdrv_check_va_range(ctx, vma, va, len);
    if (ret != 0) {
#ifndef TSDRV_UT
        ka_task_up_write(ka_mm_get_mmap_sem(current->mm));
        TSDRV_PRINT_ERR("Check va range failed. (va=0x%lx; len=%lx)\n", va, len);
        return ret;
#endif
    }

    ret = tsdrv_check_va_unmap(vma, va, len);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Check va unmap failed. (va=0x%lx; len=%lx)\n", va, len);
        ka_task_up_write(ka_mm_get_mmap_sem(current->mm));
        return ret;
#endif
    }

    tsdrv_zap_vma_ptes(vma, va, len);
    ka_task_up_write(ka_mm_get_mmap_sem(current->mm));
    TSDRV_PRINT_DEBUG("Unremap_va succ: (va=0x%lx; len=0x%lx)\n", va, len);

    return 0;
}
#else
int tsdrv_get_user_va(phys_addr_t vaddr, unsigned long size, unsigned long *va)
{
    void *ptr;
    TSDRV_PRINT_DEBUG("mg_sp_make_share_k2u vaddr = 0x%pK. size = %ld \n", vaddr, size);
    ptr = mg_sp_make_share_k2u(vaddr, size, SP_PFN_MAP, 0, SPG_ID_DEFAULT);
    if (KA_IS_ERR_OR_NULL(ptr)) {
        TSDRV_PRINT_ERR("mg_sp_make_share_k2u fail ret = %d.\n", KA_PTR_ERR(ptr));
        return KA_PTR_ERR(ptr);
    }
    *va = (unsigned long)(uintptr_t)ptr;
    return 0;
}

#endif
STATIC int tsdrv_ts_ctx_init(struct tsdrv_ctx *ctx)
{
    struct tsdrv_ts_ctx *ts_ctx  = NULL;
    u32 tsid, i;

    for (tsid = 0; tsid < DEVDRV_MAX_TS_NUM; tsid++) {
        ts_ctx = &ctx->ts_ctx[tsid];
        ts_ctx->tsid = tsid;
        for (i = TSDRV_STREAM_ID; i < TSDRV_MAX_ID; i++) {
            ts_ctx->id_ctx[i].id_num = 0;
            KA_INIT_LIST_HEAD(&ts_ctx->id_ctx[i].id_list);
        }

        ts_ctx->cq_tail_updated = 0;
        ka_task_init_waitqueue_head(&ts_ctx->report_wait);
    }
    return 0;
}

struct tsdrv_ctx *tsdrv_find_ctx(struct tsdrv_dev_ctx *dev_ctx, ka_pid_t tgid)
{
    struct rb_node *node = NULL;

    node = dev_ctx->root.rb_node;
    while (node != NULL) {
        struct tsdrv_ctx *ctx_l = ka_base_rb_entry(node, struct tsdrv_ctx, node);
        if (tgid < ctx_l->tgid) {
            node = node->rb_left;
        } else if (tgid > ctx_l->tgid) {
            node = node->rb_right;
        } else {
            return ctx_l;
        }
    }

    return NULL;
}

STATIC int tsdrv_insert_ctx(struct tsdrv_dev_ctx *dev_ctx, struct tsdrv_ctx *ctx)
{
    struct rb_node **node = &dev_ctx->root.rb_node;
    struct rb_node *parent = NULL;

    /* locked outside */
    while (*node != NULL) {
        struct tsdrv_ctx *ctx_l;
        parent = *node;
        ctx_l = ka_base_rb_entry(parent, struct tsdrv_ctx, node);
        if (ctx->tgid < ctx_l->tgid) {
            node = &(*node)->rb_left;
        } else if (ctx->tgid > ctx_l->tgid) {
            node = &(*node)->rb_right;
        } else {
            return -ENODEV;
        }
    };
    ka_list_add(&ctx->list, &dev_ctx->head);
    ka_base_rb_link_node(&ctx->node, parent, node);
    ka_base_rb_insert_color(&ctx->node, &dev_ctx->root);
    return 0;
}

STATIC void tsdrv_remove_ctx(struct tsdrv_dev_ctx *dev_ctx, struct tsdrv_ctx *ctx)
{
    /* locked outside */
    ka_list_del(&ctx->list);
    ka_base_rb_erase(&ctx->node, &dev_ctx->root);
    KA_BASE_RB_CLEAR_NODE(&ctx->node);
}

void tsdrv_ctx_put(struct tsdrv_dev_ctx *dev_ctx, struct tsdrv_ctx *ctx)
{
#ifndef TSDRV_UT
    enum tsdrv_ctx_status status;

    ka_task_spin_lock(&dev_ctx->dev_ctx_lock);
    ka_task_spin_lock_bh(&ctx->ctx_lock);
    status = (enum tsdrv_ctx_status)ka_base_atomic_read(&ctx->status);
    if (status != TSDRV_CTX_VALID) {
        ka_task_spin_unlock_bh(&ctx->ctx_lock);
        ka_task_spin_unlock(&dev_ctx->dev_ctx_lock);
        TSDRV_PRINT_ERR("invalid ctx status=%d\n", (int)status);
        return;
    }
    tsdrv_remove_ctx(dev_ctx, ctx);
    ctx->pid = TSDRV_INVALID_PID;
    ctx->tgid = TSDRV_INVALID_TGID;
    ka_base_atomic_set(&ctx->status, TSDRV_CTX_INVALID);
    ka_task_spin_unlock_bh(&ctx->ctx_lock);
    dev_ctx->ctx_num--;
    ka_task_spin_unlock(&dev_ctx->dev_ctx_lock);
#endif
}

STATIC int tsdrv_ctx_recycle_wait_finish(struct tsdrv_ctx *ctx)
{
#ifndef TSDRV_UT
#define CUR_MSLEEP_INTERRUPT 10

    u32 timeout = TSDRV_CTX_RECYCLE_TIMEOUT;
    unsigned long msec;

    while (ka_base_atomic_read(&ctx->status) == (int)TSDRV_CTX_RECYCLE) {
        msec = ka_system_msleep_interruptible(CUR_MSLEEP_INTERRUPT);
        if (msec != 0) {
            TSDRV_PRINT_ERR("Process has been interruptible. (tgid=%d)\n", ctx->tgid);
            return -ERESTARTSYS;
        }

        if (timeout == 0) {
            TSDRV_PRINT_ERR("Wait ctx recycle timeout. (tgid=%d)\n", ctx->tgid);
            return -EBUSY;
        }
        timeout--;
    }

    TSDRV_PRINT_INFO("Wait ctx recycle success. (tgid=%d)\n", ctx->tgid);
#endif
    return 0;
}

void tsdrv_ctx_print_alloc_id(u32 devid, u32 fid, u32 tsid, enum tsdrv_id_type type)
{
    struct tsdrv_device *tsdrv_dev = tsdrv_get_dev(devid);
    static int flow_ctrl[TSDRV_MAX_ID] = {0};
    struct tsdrv_ctx *ctx = NULL;
    const int freq = 100;
    u32 ctx_idx;
    u32 id_num;

    if ((flow_ctrl[type]++ % freq) == 0) {
        TSDRV_PRINT_ERR("print id alloc state start\n");
        TSDRV_PRINT_ERR("[tgid:tsid:id_type:id_num]\n");
        for (ctx_idx = 0; ctx_idx < TSDRV_MAX_CTX_NUM; ctx_idx++) {
            ctx = &tsdrv_dev->dev_ctx[fid].ctx[ctx_idx];
            id_num = ctx->ts_ctx[tsid].id_ctx[type].id_num;
            if (id_num != 0) {
                TSDRV_PRINT_ERR("[%d:%u:%d:%u]\n", ctx->tgid, tsid, (int)type, id_num);
            }
        }
        TSDRV_PRINT_ERR("print id alloc state finish\n");
    }

    if (flow_ctrl[type] >= freq) {
        flow_ctrl[type] = 0;
    }
}

STATIC int tsdrv_ctx_recycle_check(struct tsdrv_dev_ctx *dev_ctx, ka_pid_t tgid)
{
    u32 ctx_index;
    struct tsdrv_ctx *ctx = NULL;

    for (ctx_index = 0; ctx_index < TSDRV_MAX_CTX_NUM; ctx_index++) {
        ctx = &dev_ctx->ctx[ctx_index];
        ka_task_spin_lock_bh(&ctx->ctx_lock);
        /* if ctx is recycle error, not need wait. */
        if (ctx->tgid != tgid ||
            ka_base_atomic_read(&ctx->status) == (int)TSDRV_CTX_RECYCLE_ERR) {
            ka_task_spin_unlock_bh(&ctx->ctx_lock);
            continue;
        }
#ifndef TSDRV_UT
        /* if ctx status is valid/run/wait/release, need return open error. */
        if (ka_base_atomic_read(&ctx->status) != (int)TSDRV_CTX_RECYCLE) {
            ka_task_spin_unlock_bh(&ctx->ctx_lock);
            TSDRV_PRINT_ERR("Ctx status is invalid. (tgid=%u; status=%u)\n",
                ctx->tgid, ka_base_atomic_read(&ctx->status));
            return -EBUSY;
        }

        ka_task_spin_unlock_bh(&ctx->ctx_lock);
        return tsdrv_ctx_recycle_wait_finish(ctx);
#endif
    }

    return 0;
}

struct tsdrv_ctx *tsdrv_ctx_get(struct tsdrv_dev_ctx *dev_ctx, ka_pid_t tgid)
{
    struct tsdrv_ctx *ctx = NULL;
    u32 ctx_index;
    int err;

    err = tsdrv_ctx_recycle_check(dev_ctx, tgid);
    if (err != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Check ctx status failed. (tgid=%d)\n", tgid);
        return NULL;
#endif
    }

    ka_task_spin_lock(&dev_ctx->dev_ctx_lock);
    if (unlikely(dev_ctx->ctx_num >= TSDRV_MAX_CTX_NUM)) {
#ifndef TSDRV_UT
        ka_task_spin_unlock(&dev_ctx->dev_ctx_lock);
        TSDRV_PRINT_ERR("ctx_num exceeds max ctx num, ctx_num(%u)\n", dev_ctx->ctx_num);
        return NULL;
#endif
    }
    for (ctx_index = 0; ctx_index < TSDRV_MAX_CTX_NUM; ctx_index++) {
        ctx = &dev_ctx->ctx[ctx_index];
        ka_task_spin_lock_bh(&ctx->ctx_lock);
        if (ka_base_atomic_read(&ctx->status) == (int)TSDRV_CTX_INVALID) {
            ctx->pid = current->pid;
            ctx->tgid = tgid;
            ctx->vm_recycle_flag = 0;
            err = tsdrv_insert_ctx(dev_ctx, ctx);
            if (err != 0) {
#ifndef TSDRV_UT
                goto err_insert;
#endif
            }
            ctx->refcnt = 0;
            ctx->refflag = true;
            ctx->mirror_ctx_status = DEVMM_SVM_NORMAL_EXITED_FLAG;
            ka_base_atomic_set(&ctx->status, TSDRV_CTX_VALID);
            tsdrv_set_ctx_remote_open(ctx, false);
            (void)tsdrv_ts_ctx_init(ctx);
            ka_task_spin_unlock_bh(&ctx->ctx_lock);
            break;
        }
        ka_task_spin_unlock_bh(&ctx->ctx_lock);
    }
    if (unlikely(ctx_index == TSDRV_MAX_CTX_NUM)) {
#ifndef TSDRV_UT
        ka_task_spin_unlock(&dev_ctx->dev_ctx_lock);
        TSDRV_PRINT_ERR("get ctx fail, ctx_num(%u)\n", dev_ctx->ctx_num);
        return NULL;
#endif
    }
    dev_ctx->ctx_num++;
    ka_task_spin_unlock(&dev_ctx->dev_ctx_lock);
    return ctx;
#ifndef TSDRV_UT
err_insert:
    ctx->pid = TSDRV_INVALID_PID;
    ctx->tgid = TSDRV_INVALID_TGID;
    ka_task_spin_unlock_bh(&ctx->ctx_lock);
    ka_task_spin_unlock(&dev_ctx->dev_ctx_lock);
    return NULL;
#endif
}

void tsdrv_set_ctxs_stop_recycle(u32 devid, u32 fid)
{
#ifndef TSDRV_UT
    struct tsdrv_device *ts_dev = tsdrv_get_dev(devid);
    struct tsdrv_dev_ctx *dev_ctx = &ts_dev->dev_ctx[fid];
    enum tsdrv_ctx_status ctx_status;
    struct tsdrv_ctx *ctx = NULL;
    u32 ctx_index;

    ka_task_spin_lock(&dev_ctx->dev_ctx_lock);
    if (unlikely(dev_ctx->ctx_num >= TSDRV_MAX_CTX_NUM)) {
        ka_task_spin_unlock(&dev_ctx->dev_ctx_lock);
        TSDRV_PRINT_ERR("ctx_num exceeds max ctx num, ctx_num(%u)\n", dev_ctx->ctx_num);
        return;
    }
    for (ctx_index = 0; ctx_index < TSDRV_MAX_CTX_NUM; ctx_index++) {
        ctx = &dev_ctx->ctx[ctx_index];
        ctx_status = (enum tsdrv_ctx_status)ka_base_atomic_read(&ctx->status);
        if ((ctx_status == TSDRV_CTX_RECYCLE) || (ctx_status == TSDRV_CTX_RELEASE)) {
            ka_base_atomic_set(&ctx->status, TSDRV_CTX_STOP_RECYCLE);
        }
    }
    ka_task_spin_unlock(&dev_ctx->dev_ctx_lock);
#endif
}

void tsdrv_set_ctx_release(struct tsdrv_ctx *ctx)
{
    struct tsdrv_dev_ctx *dev_ctx = NULL;
    enum tsdrv_ctx_status status;
    u32 ctx_index;
    u32 devid;
    u32 tsnum;

    if (unlikely(ctx == NULL)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("ctx is NULL\n");
        return;
#endif
    }
    if (unlikely(ctx->ctx_index >= TSDRV_MAX_CTX_NUM)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid ctx_index(%u)\n", ctx->ctx_index);
        return;
#endif
    }
    devid = tsdrv_get_devid_by_ctx(ctx);
    tsnum = tsdrv_get_dev_tsnum(devid);

    ctx_index = ctx->ctx_index;
    dev_ctx = ka_container_of(ctx, struct tsdrv_dev_ctx, ctx[ctx_index]);
    if (unlikely(dev_ctx->fid >= TSDRV_MAX_FID_NUM)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid fid(%u)\n", dev_ctx->fid);
        return;
#endif
    }

    ka_task_spin_lock(&dev_ctx->dev_ctx_lock);
    ka_task_spin_lock_bh(&ctx->ctx_lock);
    status = (enum tsdrv_ctx_status)ka_base_atomic_read(&ctx->status);
    if ((status != TSDRV_CTX_RELEASE) && (status != TSDRV_CTX_RECYCLE)) {
#ifndef TSDRV_UT
        ka_task_spin_unlock_bh(&ctx->ctx_lock);
        ka_task_spin_unlock(&dev_ctx->dev_ctx_lock);
        TSDRV_PRINT_ERR("invalid ctx status(%d)\n", (int)status);
        return;
#endif
    }
    tsdrv_remove_ctx(dev_ctx, ctx);

    ctx->pid = TSDRV_INVALID_PID;
    ctx->tgid = TSDRV_INVALID_TGID;
    ka_base_atomic_set(&ctx->status, TSDRV_CTX_INVALID);
    ka_task_spin_unlock_bh(&ctx->ctx_lock);
    dev_ctx->ctx_num--;
    ka_task_spin_unlock(&dev_ctx->dev_ctx_lock);
}

struct tsdrv_ctx *tsdrv_find_one_ctx(struct tsdrv_dev_ctx *dev_ctx)
{
#ifndef TSDRV_UT
    struct rb_node *node = NULL;
    struct tsdrv_ctx *ctx_l = NULL;

    ka_task_spin_lock(&dev_ctx->dev_ctx_lock);
    node = dev_ctx->root.rb_node;
    if (node != NULL) {
        ctx_l = ka_base_rb_entry(node, struct tsdrv_ctx, node);
    }
    ka_task_spin_unlock(&dev_ctx->dev_ctx_lock);
    return ctx_l;
#endif
}

void tsdrv_ctx_recycle(struct tsdrv_dev_ctx *dev_ctx, struct tsdrv_ctx *ctx)
{
    enum tsdrv_ctx_status status;

    ka_task_spin_lock(&dev_ctx->dev_ctx_lock);
    ka_task_spin_lock_bh(&ctx->ctx_lock);
    status = (enum tsdrv_ctx_status)ka_base_atomic_read(&ctx->status);
    if (status != TSDRV_CTX_RELEASE) {
#ifndef TSDRV_UT
        ka_task_spin_unlock_bh(&ctx->ctx_lock);
        ka_task_spin_unlock(&dev_ctx->dev_ctx_lock);
        TSDRV_PRINT_ERR("invalid ctx status(%d)\n", (int)status);
        return;
#endif
    }
    /*
     * when ctx enter recycle state, we should remove the ctx from rb.
     */
    tsdrv_remove_ctx(dev_ctx, ctx);
    ka_base_atomic_set(&ctx->status, TSDRV_CTX_RECYCLE);
    ka_task_spin_unlock_bh(&ctx->ctx_lock);
    ka_task_spin_unlock(&dev_ctx->dev_ctx_lock);
}

void tsdrv_set_ctx_recycle_status(struct tsdrv_dev_ctx *dev_ctx, struct tsdrv_ctx *ctx,
    enum tsdrv_ctx_status status)
{
    enum tsdrv_ctx_status status_tmp;

    ka_task_spin_lock(&dev_ctx->dev_ctx_lock);
    ka_task_spin_lock_bh(&ctx->ctx_lock);
    status_tmp = (enum tsdrv_ctx_status)ka_base_atomic_read(&ctx->status);
    if (status_tmp != TSDRV_CTX_RECYCLE) {
        ka_task_spin_unlock_bh(&ctx->ctx_lock);
        ka_task_spin_unlock(&dev_ctx->dev_ctx_lock);
        TSDRV_PRINT_ERR("invalid ctx status(%d)\n", (int)status_tmp);
        return;
    }

    if (status != TSDRV_CTX_RECYCLE_ERR) {
        ctx->pid = TSDRV_INVALID_PID;
        ctx->tgid = TSDRV_INVALID_TGID;
    }
    dev_ctx->ctx_num--;
    ka_base_atomic_set(&ctx->status, status);
    ka_task_spin_unlock_bh(&ctx->ctx_lock);
    ka_task_spin_unlock(&dev_ctx->dev_ctx_lock);
}

bool tsdrv_ts_ctx_id_res_chk(struct tsdrv_ts_ctx *ts_ctx, ka_pid_t tgid, ka_pid_t pid, int print_flag)
{
    /* event contains hw event and sw event. */
    char *id_name[TSDRV_MAX_ID] = {"stream", "notify", "model", "event", "ipc_event", "sq", "cq", "cmo"};
    enum tsdrv_id_type id_type;
    bool err = false;

    for (id_type = TSDRV_STREAM_ID; id_type < TSDRV_MAX_ID; id_type++) {
        DRV_CHECK_EXP_ACT_DBG(ts_ctx->id_ctx[id_type].id_num != 0, goto leak, "");
    }

    err = devdrv_cbsqcq_need_recycle(ts_ctx);
    if (err == true) {
        goto leak;
    }
    return err;
leak:
    if (print_flag == LEAK_CHK_NO_PRINT) {
        return true;
    }
    TSDRV_PRINT_WARN("tsid(%u) tgid(%d) pid(%d) resource needs to recycle is following:\n", ts_ctx->tsid, tgid, pid);

    for (id_type = TSDRV_STREAM_ID; id_type < TSDRV_MAX_ID; id_type++) {
        TSDRV_PRINT_WARN("id_name-num(%s: %u);\n", id_name[id_type], ts_ctx->id_ctx[id_type].id_num);
    }

    TSDRV_PRINT_WARN("id_name-num(%s: %u).\n", "cbsqcq", ts_ctx->recycle_cbsqcq_num);

    return true;
}

bool tsdrv_ctx_id_res_chk(struct tsdrv_ctx *ctx, int32_t print_flag)
{
    u32 tsid;

    for (tsid = 0; tsid < DEVDRV_MAX_TS_NUM; tsid++) {
        struct tsdrv_ts_ctx *ts_ctx = &ctx->ts_ctx[tsid];
        bool ret = tsdrv_ts_ctx_id_res_chk(ts_ctx, ctx->tgid, ctx->pid, print_flag);
        if (ret == true) {
            return ret;
        }
    }
    return false;
}

void tsdrv_set_mirror_ctx_status(struct tsdrv_ctx *ctx, u32 status)
{
    ka_task_spin_lock_bh(&ctx->ctx_lock);
    ctx->mirror_ctx_status = status;
    ka_task_spin_unlock_bh(&ctx->ctx_lock);
}

u32 tsdrv_get_mirror_ctx_status(struct tsdrv_ctx *ctx)
{
    return ctx->mirror_ctx_status;
}

bool tsdrv_mirror_ctx_abnormal(struct tsdrv_ctx *ctx)
{
    u32 status;

    ka_task_spin_lock_bh(&ctx->ctx_lock);
    status = ctx->mirror_ctx_status;
    ka_task_spin_unlock_bh(&ctx->ctx_lock);
    return status == DEVMM_SVM_ABNORMAL_EXITED_FLAG;
}

STATIC void tsdrv_ctx_init(struct tsdrv_ctx *ctx, u32 ctx_num)
{
    u32 ctx_index;

    for (ctx_index = 0; ctx_index < ctx_num; ctx_index++) {
        ka_base_atomic_set(&ctx[ctx_index].status, TSDRV_CTX_INVALID);
        ctx[ctx_index].ctx_index = ctx_index;
        ctx[ctx_index].tgid = TSDRV_INVALID_TGID;
        ctx[ctx_index].pid = TSDRV_INVALID_PID;
        ctx[ctx_index].thread_bind_irq_num = 0;
        ka_task_spin_lock_init(&ctx[ctx_index].ctx_lock);
        ka_task_mutex_init(&ctx[ctx_index].mutex_lock);
        ctx[ctx_index].vm_recycle_flag = 0;
        ka_task_init_waitqueue_head(&ctx[ctx_index].vm_recycle_work);
    }
}

void tsdrv_dev_ctx_init_per_fid(struct tsdrv_dev_ctx *dev_ctx, u32 fid)
{
    KA_INIT_LIST_HEAD(&dev_ctx[fid].head);
    dev_ctx[fid].root = KA_RB_ROOT;
    dev_ctx[fid].fid = fid;
    dev_ctx[fid].ctx_num = 0;
    ka_task_spin_lock_init(&dev_ctx[fid].dev_ctx_lock);
    tsdrv_ctx_init(dev_ctx[fid].ctx, TSDRV_MAX_CTX_NUM);
}

void tsdrv_dev_ctx_init(struct tsdrv_dev_ctx *dev_ctx, u32 fid_num)
{
    u32 fid;

    for (fid = 0; fid < fid_num; fid++) {
        tsdrv_dev_ctx_init_per_fid(dev_ctx, fid);
    }
}

