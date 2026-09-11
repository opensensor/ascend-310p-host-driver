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
 * Create: 2024-9-4
 */
#include <linux/types.h>
#include <linux/proc_fs.h>

#include "ascend_hal_define.h"
#include "svm_proc_mng.h"
#include "svm_master_proc_mng.h"
#include "svm_mem_stats.h"

#ifdef CONFIG_PROC_FS
#define SVM_MASTER_MAX_SVM_PROC_NUM         1024
#define SVM_MEM_STATS_TITLE "\nMem stats(Bytes):\nDevid   Mem_type                Module_name     Module_id       "     \
    "Current_alloced_size    Alloced_peak_size       Alloc_cnt       Free_cnt\n"
SVM_DECLARE_MODULE_NAME(svm_module_name);

static struct svm_mem_stats *devmm_get_mem_stats_mng(struct devmm_svm_process *svm_proc,
    struct svm_mem_stats_type *type, u32 logic_id)
{
    struct devmm_svm_proc_master *master_data = (struct devmm_svm_proc_master *)svm_proc->priv_data;
    struct svm_mem_stats *kva = (struct svm_mem_stats *)master_data->mem_stats_va_mng[logic_id].kva;

    if (kva == NULL) {
        return NULL;
    }
    return (kva + (type->mem_val * MEM_STATS_MAX_PAGE_TYPE + type->page_type) *
        MEM_STATS_MAX_PHY_MEMTYPE + type->phy_memtype);
}

#define DEVMM_MEM_STATS_SHOW(seq, mem_val, page_type, phy_memtype, devid, fmt, ...) do {                                \
    if (mem_val != MEM_DEV_VAL) {                                                                                       \
        ka_fs_seq_printf(seq, "NA      %-24s"fmt, svm_get_mem_type_str(mem_val, page_type, phy_memtype), ##__VA_ARGS__);      \
    } else {                                                                                                            \
        ka_fs_seq_printf(seq, "dev%-5d%-24s"fmt, devid, svm_get_mem_type_str(mem_val, page_type, phy_memtype), ##__VA_ARGS__);\
    }                                                                                                                   \
} while (0)

static void devmm_mem_stats_show(ka_seq_file_t *seq, struct devmm_svm_process *svm_proc,
    struct svm_mem_stats_type *type, u32 logic_id)
{
    struct svm_mem_stats *mem_stats = NULL;
    u32 module_id;

    mem_stats = devmm_get_mem_stats_mng(svm_proc, type, logic_id);
    if (mem_stats == NULL) {
        return;
    }

    for (module_id = 0; module_id < SVM_MAX_MODULE_ID; module_id++) {
        if (mem_stats->alloced_peak_size[module_id] == 0) {
            continue;
        }
        if (seq != NULL) {
            DEVMM_MEM_STATS_SHOW(seq, type->mem_val, type->page_type, type->phy_memtype, logic_id,
                "%-16s%-16u%-24llu%-24llu%-16llu%-16llu\n", SVM_GET_MODULE_NAME(svm_module_name, module_id),
                module_id, mem_stats->current_alloced_size[module_id], mem_stats->alloced_peak_size[module_id],
                mem_stats->alloc_cnt[module_id], mem_stats->free_cnt[module_id]);
        } else {
            devmm_drv_run_info("%d dev%u %s %s %u %llu %llu %llu %llu\n", svm_proc->process_id.hostpid, logic_id,
                svm_get_mem_type_str(type->mem_val, type->page_type, type->phy_memtype),
                SVM_GET_MODULE_NAME(svm_module_name, module_id),
                module_id, mem_stats->current_alloced_size[module_id], mem_stats->alloced_peak_size[module_id],
                mem_stats->alloc_cnt[module_id], mem_stats->free_cnt[module_id]);
        }
    }
}

void devmm_task_mem_stats_show(ka_seq_file_t *seq)
{
    u32 page_type, phy_memtype, logic_id;
    struct svm_mem_stats_type type;
    u32 stamp = (u32)ka_jiffies;

    ka_fs_seq_printf(seq, SVM_MEM_STATS_TITLE);

    svm_mem_stats_type_pack(&type, MEM_HOST_VAL, 0, 0);
    devmm_mem_stats_show(seq, (struct devmm_svm_process *)seq->private, &type, 0);

    for (page_type = 0; page_type < MEM_STATS_MAX_PAGE_TYPE; page_type++) {
        for (phy_memtype = 0; phy_memtype < MEM_STATS_MAX_PHY_MEMTYPE; phy_memtype++) {
            svm_mem_stats_type_pack(&type, MEM_DEV_VAL, page_type, phy_memtype);
            for (logic_id = 0; logic_id < SVM_MAX_AGENT_NUM; logic_id++) {
                devmm_mem_stats_show(seq, (struct devmm_svm_process *)seq->private, &type, logic_id);
            }
            devmm_try_cond_resched(&stamp);
        }
    }
}

static void devmm_dev_mem_stats_show(ka_seq_file_t *seq, u32 logic_id)
{
    u32 svm_proc_num = SVM_MASTER_MAX_SVM_PROC_NUM;
    struct devmm_svm_process **svm_procs = NULL;
    struct svm_mem_stats_type type;
    u32 page_type, phy_memtype, i;
    u32 stamp = (u32)ka_jiffies;

    svm_procs = devmm_kvzalloc(svm_proc_num * (sizeof(struct devmm_svm_process *)));
    if (svm_procs == NULL) {
        return;
    }

    devmm_svm_procs_get(svm_procs, &svm_proc_num);
    for (i = 0; i < svm_proc_num; i++) {
        if (seq != NULL) {
            ka_fs_seq_printf(seq, "Hostpid=%d\n", svm_procs[i]->process_id.hostpid);
        }
        for (page_type = 0; page_type < MEM_STATS_MAX_PAGE_TYPE; page_type++) {
            for (phy_memtype = 0; phy_memtype < MEM_STATS_MAX_PHY_MEMTYPE; phy_memtype++) {
                svm_mem_stats_type_pack(&type, MEM_DEV_VAL, page_type, phy_memtype);
                devmm_mem_stats_show(seq, svm_procs[i], &type, logic_id);
            }
        }
        devmm_svm_proc_put(svm_procs[i]);
        devmm_try_cond_resched(&stamp);
    }
    devmm_kvfree(svm_procs);
}

int devmm_dev_mem_stats_procfs_show(ka_seq_file_t *seq, void *offset)
{
    u32 logic_id = (u32)(uintptr_t)seq->private;

#ifndef EMU_ST
    if (devmm_thread_is_run_in_docker()) {
        return 0;
    }
#endif

    ka_fs_seq_printf(seq, SVM_MEM_STATS_TITLE);
    devmm_dev_mem_stats_show(seq, logic_id);
    return 0;
}

void devmm_dev_mem_stats_log_show(u32 logic_id)
{
    devmm_drv_run_info("Hostpid Devid Mem_type Module_name Module_id Current_alloced_size(Bytes) "
        "Alloced_peak_size(Bytes) Alloc_cnt Free_cnt\n");
    devmm_dev_mem_stats_show(NULL, logic_id);
}

static void _devmm_mem_stats_va_map(struct devmm_svm_proc_master *master_data, u32 logic_id, u64 va)
{
    ka_page_t **pages = NULL;
    void *ptr = NULL;
    u64 page_num;
    int ret;

    if (KA_DRIVER_IS_ALIGNED(va, KA_MM_PAGE_SIZE) == false) {
        devmm_drv_debug("Va is not aligned. (va=0x%llx)\n", va);
        return;
    }

    page_num = ka_base_round_up(MEM_STATS_MNG_SIZE, KA_MM_PAGE_SIZE) / KA_MM_PAGE_SIZE;
    pages = (ka_page_t **)devmm_kvalloc(page_num * sizeof(ka_page_t *), 0);
    if (pages == NULL) {
        devmm_drv_debug("Alloc pages fail. (va=0x%llx; num=%llu)\n", va, page_num);
        return;
    }

    ret = devmm_pin_user_pages_fast(va, page_num, 1, pages);
    if (ret != 0) {
        devmm_drv_debug("Get user pages fail. (ret=%d; va=0x%llx; num=%llu)\n", ret, va, page_num);
        goto free_page;
    }

    ptr = ka_mm_vmap(pages, page_num, 0, KA_PAGE_KERNEL);
    if (ptr == NULL) {
        devmm_drv_debug("vmap fail. (va=0x%llx; num=%llu)\n", va, page_num);
        goto unpin_page;
    }

    master_data->mem_stats_va_mng[logic_id].pages = pages;
    master_data->mem_stats_va_mng[logic_id].kva = ptr;
    devmm_drv_debug("Va map. (logic_id=%u; va=0x%llx; num=%llu)\n", logic_id, va, page_num);
    return;

unpin_page:
    devmm_unpin_user_pages(pages, page_num, page_num);
free_page:
    devmm_kvfree(pages);
    return;
}

void devmm_mem_stats_va_map(struct devmm_svm_process *svm_proc, u32 logic_id, u64 va)
{
    struct devmm_svm_proc_master *master_data = (struct devmm_svm_proc_master *)svm_proc->priv_data;

    if (master_data->mem_stats_va_mng[logic_id].kva != NULL) {
        return;
    }

    ka_task_down(&master_data->mem_stats_va_mng[logic_id].sem);
    if (master_data->mem_stats_va_mng[logic_id].kva == NULL) {
        _devmm_mem_stats_va_map(master_data, logic_id, va);
    }
    ka_task_up(&master_data->mem_stats_va_mng[logic_id].sem);
}

void devmm_mem_stats_va_unmap(struct devmm_svm_process *svm_proc)
{
    struct devmm_svm_proc_master *master_data = (struct devmm_svm_proc_master *)svm_proc->priv_data;
    u32 logic_id;

    for (logic_id = 0; logic_id < SVM_MAX_AGENT_NUM; logic_id++) {
        ka_task_down(&master_data->mem_stats_va_mng[logic_id].sem);
        if (master_data->mem_stats_va_mng[logic_id].kva != NULL) {
            u64 page_num = ka_base_round_up(MEM_STATS_MNG_SIZE, KA_MM_PAGE_SIZE) / KA_MM_PAGE_SIZE;

            devmm_drv_debug("Va unmap. (logic_id=%u; num=%llu)\n", logic_id, page_num);
            vunmap(master_data->mem_stats_va_mng[logic_id].kva);
            devmm_unpin_user_pages(master_data->mem_stats_va_mng[logic_id].pages, page_num, page_num);
            devmm_kvfree(master_data->mem_stats_va_mng[logic_id].pages);
            master_data->mem_stats_va_mng[logic_id].kva = NULL;
            master_data->mem_stats_va_mng[logic_id].pages = NULL;
        }
        ka_task_up(&master_data->mem_stats_va_mng[logic_id].sem);
    }
}

#else
void devmm_dev_mem_stats_log_show(u32 logic_id)
{
}

void devmm_mem_stats_va_map(struct devmm_svm_process *svm_proc, u32 logic_id, u64 va)
{
}

void devmm_mem_stats_va_unmap(struct devmm_svm_process *svm_proc)
{
}

#endif
