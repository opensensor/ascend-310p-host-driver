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
#ifndef SVM_MEM_MNG_H
#define SVM_MEM_MNG_H

#include "ka_common_pub.h"
#include "ka_memory_pub.h"

#include "svm_cmd.h"

/*
 * page property define for devmm_make_pgprot
 * bit0: mem readonly
 */
#define DEVMM_MEM_READONLY_BIT 0

#define DEVMM_PAGE_READONLY_FLG (1UL << DEVMM_MEM_READONLY_BIT)

typedef ka_page_t *(*devmm_alloc_page_func)(int, u32, u32);
typedef int (*devmm_mem_split_inc_test_func)(u32 devid,
    u32 vfid, int nid, u64 page_num, int alloc_flag);

struct page_map_info {
    u32 devid;
    u32 vfid;
    u32 mem_type;
    u32 heap_type;
    int nid;
    u8 is_continuty;
    u8 is_svm;
    u8 page_prot;
    u8 is_giant_page;
    u64 va;
    u64 page_num;
    ka_page_t **inpages;
};

struct devmm_mmap_para {
    u32 seg_num;
    struct devmm_mmap_addr_seg segs[DEVMM_MAX_VMA_NUM];
};

/* If you want to add pgprot_cfg type, follow the format no_* */
struct devmm_pgprot_cfg_info {
    bool no_pte_user;
    bool no_cache;
};

ka_page_t *devmm_pa_to_page(u64 paddr);
void devmm_print_nodes_info(u32 devid, u32 vfid, u32 mem_type);
bool devmm_pa_is_remote_addr(u64 pa);

ka_pgprot_t devmm_make_pgprot(unsigned int flg, bool is_nocache);
ka_pgprot_t devmm_make_pgprot_ex(unsigned int flg, struct devmm_pgprot_cfg_info cfg_info);

ka_pgprot_t devmm_make_remote_pgprot(u32 flg);
ka_pgprot_t devmm_make_remote_pgprot_ex(u32 flg, struct devmm_pgprot_cfg_info cfg_info);

ka_pgprot_t devmm_make_nocache_pgprot(u32 flg);
ka_pgprot_t devmm_make_readonly_pgprot(ka_pgprot_t prot);
ka_pgprot_t devmm_make_reserved_pgprot(u32 addr_type);
bool devmm_is_readonly_mem(u32 page_prot);
int devmm_get_svm_pages_with_lock(void *svm_proc, u64 va, u64 num, ka_page_t **pages);
ka_vm_area_struct_t *devmm_find_vma_from_mm(ka_mm_struct_t *mm, u64 vaddr);
ka_vm_area_struct_t *devmm_find_vma_proc(ka_mm_struct_t *mm, ka_vm_area_struct_t *vma[],
    u32 vma_num, u64 vaddr);
ka_vm_area_struct_t *_devmm_find_vma_proc(ka_vm_area_struct_t *vma[], u32 vma_num, u64 vaddr);
u32 devmm_get_svm_vma_index(u64 vaddr, u32 vma_num);
void devmm_set_proc_vma(ka_mm_struct_t *mm, ka_vm_area_struct_t *vma[], u32 vma_num);
void devmm_init_dev_set_mmap_para(struct devmm_mmap_para *mmap_para);
void devmm_remove_vma_wirte_flag(ka_vm_area_struct_t *vma);
u32 devmm_get_normal_vma_num(void);
int devmm_get_user_pages_remote(ka_task_struct_t *tsk, ka_mm_struct_t *mm,
    u64 va, int write, u32 num, ka_page_t **pages);
int devmm_check_and_set_svm_static_reserve_vma(void *svm_proc, ka_vm_area_struct_t **svm_vma);
int devmm_check_and_set_custom_svm_vma(void *svm_proc, ka_vm_area_struct_t **svm_vma);
u64 devmm_kpg_size(ka_page_t *pg);
bool devmm_is_giant_page(ka_page_t **pages, u64 pg_num);
bool devmm_is_svm_vma_magic(void *check_magic);

#endif /* SVM_MEM_MNG_H */
