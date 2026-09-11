/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2022. All rights reserved.
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
#include <linux/mm.h>
#include <linux/mm_types.h>
#include "svm_log.h"
#include "svm_mem_mng.h"
#include "devmm_common.h"
#include "devmm_proc_info.h"
#include "kernel_version_adapt.h"

#ifdef HOST_AGENT
#include "svm_agent_mem_mng.h"
#endif

ka_pgprot_t devmm_make_pgprot(unsigned int flg, bool is_nocache)
{
    u64 prot_val = ka_pgprot_val(KA_PAGE_SHARED);

    return __pgprot(prot_val);
}

ka_pgprot_t devmm_make_pgprot_ex(u32 flg, struct devmm_pgprot_cfg_info cfg_info)
{
    u64 prot_val = ka_pgprot_val(KA_PAGE_SHARED);

    return __pgprot(prot_val);
}

bool devmm_pa_is_remote_addr(u64 pa)
{
    return false;
}

void devmm_print_nodes_info(u32 devid, u32 vfid, u32 mem_type)
{
    struct ka_sysinfo chuck_info = {0};

    ka_si_meminfo(&chuck_info);

    devmm_drv_info("Physical memory chunk page num. (nid=%d; totalram=%ld; freeram=%ld; sharedram=%ld)\n",
                   0, chuck_info.totalram, chuck_info.freeram, chuck_info.sharedram);
}

#ifndef HOST_AGENT
int devmm_get_svm_pages_with_lock(void *svm_proc, u64 va, u64 num, ka_page_t **pages)
{
    struct devmm_svm_process *svm_process = (struct devmm_svm_process *)svm_proc;
    ka_vm_area_struct_t *vma = NULL;
    u64 size = num << KA_MM_PAGE_SHIFT;
    int ret;

    vma = devmm_find_vma(svm_process, va);
    if (vma == NULL) {
        devmm_drv_err("Get vma failed. (va=0x%llx)\n", va);
        return -EINVAL;
    }

    ret = devmm_inc_page_ref(svm_process, va, size);
    if (ret != 0) {
        return -ENOENT;
    }

    ret = devmm_get_svm_pages(vma, va, num, pages);
    devmm_dec_page_ref(svm_process, va, size);

    return ret;
}
#endif

#ifdef HOST_AGENT
u32 devmm_extract_memtype_from_bitmap(u32 bitmap)
{
    return DEVMM_DDR_MEM;
}
#endif

u32 devmm_get_svm_vma_index(u64 vaddr, u32 vma_num)
{
    return 0;
}

void devmm_set_proc_vma(ka_mm_struct_t *mm, ka_vm_area_struct_t *vma[], u32 vma_num)
{
    ka_vm_area_struct_t *tmp_vma = NULL;
    u64 vma_addr;

    vma_addr = DEVMM_SVM_MEM_START;
    tmp_vma = devmm_find_vma_from_mm(mm, vma_addr);
    if (tmp_vma == NULL) {
        return;
    }
    vma[0] = tmp_vma;

    return;
}

ka_vm_area_struct_t *devmm_find_vma_custom(struct devmm_svm_process *svm_proc, u32 idx, u64 vaddr)
{
    return NULL;
}

ka_mm_struct_t *devmm_custom_mm_get(ka_pid_t custom_pid)
{
    return NULL;
}

void devmm_custom_mm_put(ka_mm_struct_t *custom_mm)
{
}

void devmm_remove_vma_wirte_flag(ka_vm_area_struct_t *vma)
{
    return;
}

void devmm_free_ptes_in_range(struct devmm_svm_process *svm_proc, u64 start, u64 size)
{
    return;
}

void devmm_init_dev_set_mmap_para(struct devmm_mmap_para *mmap_para)
{
    mmap_para->seg_num = 0;
    mmap_para->segs[mmap_para->seg_num].va = DEVMM_SVM_MEM_START;
    mmap_para->segs[mmap_para->seg_num++].size = DEVMM_SVM_MEM_SIZE;

    /* devmm_set_host_uva_range set host uva mmap_para */
}

