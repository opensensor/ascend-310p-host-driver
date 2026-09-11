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

#ifndef DEVMM_COMMON_H
#define DEVMM_COMMON_H

#include "ka_base_pub.h"
#include "ka_common_pub.h"
#include "ka_memory_pub.h"
#include "ka_task_pub.h"
#include "ka_net_pub.h"
#include "ka_list_pub.h"
#include "ka_fs_pub.h"
#include "ka_driver_pub.h"
#include "ka_errno_pub.h"
#include "ka_system_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_compiler_pub.h"
#include "ka_hashtable_pub.h"

#include "devmm_proc_info.h"
#include "svm_proc_gfp.h"
#include "devmm_mem_alloc_interface.h"

#ifndef __KA_GFP_ACCOUNT

#ifdef __KA_GFP_KMEMCG
#define __KA_GFP_ACCOUNT __KA_GFP_KMEMCG /* for linux version 3.10 */
#endif

#ifdef __KA_GFP_NOACCOUNT
#define __KA_GFP_ACCOUNT 0 /* for linux version 4.1 */
#endif

#endif

#define DEVMM_BITS_PER_CHAR 8
#define DEVMM_PAGENUM_PER_HPAGE (KA_HPAGE_SIZE / KA_MM_PAGE_SIZE)

#define DEVMM_WAKEUP_TIMEINTERVAL 500 /* 0.5s */

#define DEVMM_SVM_MMU_MIN_SLEEP 50
#define DEVMM_SVM_MMU_MAX_SLEEP 60

#define SVM_DEV_INST_MAX_NUM (SVM_MAX_AGENT_NUM * DEVMM_MAX_VF_NUM)

#define SVM_MASTER_HUGE_PAGE_SIZE 0x200000ULL  /* 0x200000 master use 2M huge page size */
#define SVM_MASTER_GIANT_PAGE_SIZE 0x40000000ULL    /* 0x40000000 master use 1G giant page size */

#if KA_MAX_NUMNODES > 256
#define SVM_MASTER_NUMA_MAX     256
#else
#define SVM_MASTER_NUMA_MAX     KA_MAX_NUMNODES
#endif

#define DEVMM_S2S_HOST_GLOBAL_BASE_OFFSET           0x800000000000   /* 128TB */
#define DEVMM_S2S_HOST_NODE_NUM                     4
#ifdef EMU_ST
#define DEVMM_S2S_HOST_NODE_MEM_SIZE                0x40000000 /* 1G */
#else
#define DEVMM_S2S_HOST_NODE_MEM_SIZE                0xaa80000000 /* 682G */
#endif
#define DEVMM_S2S_HOST_SERVER_MEM_SIZE              (DEVMM_S2S_HOST_NODE_MEM_SIZE * DEVMM_S2S_HOST_NODE_NUM)

static inline u64 devmm_get_s2s_host_node_start_addr(u32 server_id, u32 node_id)
{
    return (DEVMM_S2S_HOST_GLOBAL_BASE_OFFSET +
        DEVMM_S2S_HOST_SERVER_MEM_SIZE * server_id + DEVMM_S2S_HOST_NODE_MEM_SIZE * node_id);
}

static inline u64 devmm_get_host_node_local_addr(u32 node_id)
{
    u64 mem_node_start[DEVMM_S2S_HOST_NODE_NUM] = {
        0x29580000000, /* node 0 map addr 0x29580000000 */
        0xa9580000000, /* node 1 map addr 0xa9580000000 */
        0x129580000000, /* node 2 map addr 0x129580000000 */
        0x1a9580000000 /* node 3 map addr 0x1a9580000000 */
    };

    return mem_node_start[node_id];
}

static inline u32 devmm_get_host_node_id_by_addr(u64 addr)
{
    u32 i;

    for (i = 0; i < DEVMM_S2S_HOST_NODE_NUM; i++) {
        u64 node_local_addr = devmm_get_host_node_local_addr(i);
        if ((addr >= node_local_addr) && (addr < (node_local_addr + DEVMM_S2S_HOST_NODE_MEM_SIZE))) {
            return i;
        }
    }

    return DEVMM_S2S_HOST_NODE_NUM;
}

static inline int devmm_get_s2s_host_global_addr(u32 server_id, u64 addr, u64 *global_addr)
{
    u32 node_id = 0;

#ifndef EMU_ST
    node_id = devmm_get_host_node_id_by_addr(addr);
    if (node_id >= DEVMM_S2S_HOST_NODE_NUM) {
        return -EINVAL;
    }
#endif

    *global_addr = devmm_get_s2s_host_node_start_addr(server_id, node_id) +
        (addr - devmm_get_host_node_local_addr(node_id));

    return 0;
}

struct svm_id_inst {
    u32 devid;
    u32 vfid;
};

static inline void svm_id_inst_pack(struct svm_id_inst *inst, u32 devid, u32 vfid)
{
    inst->devid = devid;
    inst->vfid = vfid;
}

static inline u32 svm_id_inst_to_dev_inst(struct svm_id_inst *id_inst)
{
    return (id_inst->devid * DEVMM_MAX_VF_NUM + id_inst->vfid);
}

static inline bool devmm_heap_subtype_is_matched(u32 heap_subtype, u32 mask)
{
    return ((mask & (1ul << heap_subtype)) != 0) ? true : false;
}
#ifndef EMU_ST
static inline bool devmm_pages_is_continue(ka_page_t *pre_page, ka_page_t *post_page)
{
    return (ka_mm_page_to_pfn(pre_page) + 1) == ka_mm_page_to_pfn(post_page);
}
#else
static inline bool devmm_pages_is_continue(ka_page_t *pre_page, ka_page_t *post_page)
{
    return false;
}
#endif
static inline void devmm_isb(void)
{
#if defined(__arm__) || defined(__aarch64__)
    isb();
#endif
}

extern bool g_devmm_true;
extern bool g_devmm_false;

u32 devmm_get_max_page_num_of_per_msg(u32 *bitmap);
void devmm_try_usleep_by_time(u32 *pre_stamp, u32 time);
void devmm_try_cond_resched(u32 *pre_stamp);
void devmm_try_cond_resched_by_time(u32 *pre_stamp, u32 time);
bool devmm_smmu_is_opening(void);
int devmm_pin_user_pages_fast(u64 va, u64 total_num, int write, ka_page_t **pages);
void devmm_unpin_user_pages(ka_page_t **pages, u64 page_num, u64 unpin_num);
void devmm_put_user_pages(ka_page_t **pages, u64 page_num, u64 unpin_num);
void devmm_pin_user_pages(ka_page_t **pages, u64 page_num);
u64 devmm_get_pagecount_by_size(u64 vptr, u64 sizes, u32 page_size);
int devmm_check_va_add_size_by_heap(struct devmm_svm_heap *heap, u64 va, u64 size);
bool devmm_check_input_heap_info(struct devmm_svm_process *svm_pro,
    struct devmm_update_heap_para *cmd, u32 devid);
ssize_t devmm_read_file(ka_file_t *fp, char *dst_addr, size_t fsize, loff_t *pos);
char *devmm_read_line(ka_file_t *fp, loff_t *offset, char *buf, u32 buf_len);
void *devmm_kvalloc(u64 size, ka_gfp_t flags);
void *devmm_kvzalloc(u64 size);
void devmm_kvfree(const void *ptr);
void devmm_set_heap_used_status(struct devmm_svm_heap *heap, u64 va, u64 size);
ka_vm_area_struct_t *devmm_find_vma(struct devmm_svm_process *svm_proc, u64 vaddr);
ka_vm_area_struct_t *devmm_find_vma_custom(struct devmm_svm_process *svm_proc, u32 idx, u64 vaddr);
ka_mm_struct_t *devmm_custom_mm_get(ka_pid_t custom_pid);
void devmm_custom_mm_put(ka_mm_struct_t *custom_mm);

int devmm_update_heap_info(struct devmm_svm_process *svm_process, struct devmm_update_heap_para *cmd,
    struct devmm_svm_heap *free_heap);
void devmm_free_heap_struct(struct devmm_svm_process *svm_process, u32 heap_idx, u32 heap_num);
void devmm_clear_svm_heap_struct(struct devmm_svm_process *svm_process, u32 heap_idx, u32 heap_num);
bool devmm_wait_svm_heap_unoccupied(struct devmm_svm_process *svm_process, struct devmm_svm_heap *heap);
struct devmm_svm_heap *devmm_get_heap_by_idx(struct devmm_svm_process *svm_proc, u32 heap_idx);
struct devmm_svm_heap *devmm_get_heap_by_va(struct devmm_svm_process *svm_proc, u64 vaddr);
bool devmm_check_heap_is_entity(struct devmm_svm_heap *heap);
int devmm_shm_inform_host_get_pages(struct devmm_svm_process_id *process_id, u64 va, u64 size);
int devmm_shm_inform_host_put_pages(struct devmm_svm_process_id *process_id, u64 va, u64 size);
void devmm_dev_fault_flag_set(u32 *flag, u32 shift, u32 wide, u32 value);
u32 devmm_dev_fault_flag_get(u32 flag, u32 shift, u32 wide);
bool devmm_is_pcie_connect(u32 dev_id);
bool devmm_is_hccs_connect(u32 dev_id);
bool devmm_is_host_agent(u32 agent_id);
bool devmm_is_dma_link_abnormal(u32 dev_id);
const char *devmm_get_chrdev_name(void);
void devmm_set_proc_vma(ka_mm_struct_t *mm, ka_vm_area_struct_t *vma[], u32 vma_num);
int devmm_get_svm_pages(ka_vm_area_struct_t *vma, u64 va, u64 num, ka_page_t **pages);

void devmm_svm_mem_enable(struct devmm_svm_process *svm_proc);
void devmm_svm_mem_disable(struct devmm_svm_process *svm_proc);
bool devmm_svm_mem_is_enable(struct devmm_svm_process *svm_proc);

void devmm_phy_addr_attr_pack(struct devmm_svm_process *svm_proc, u32 pg_type, u32 mem_type, bool is_continuous,
    struct devmm_phy_addr_attr *attr);
u64 devmm_get_tgid_start_time(void);
#ifdef EMU_ST
ka_vm_area_struct_t *devmm_find_vma_from_mm(ka_mm_struct_t *mm, u64 vaddr);
#endif
extern int uda_devid_to_udevid_ex(u32 devid, u32 *udevid);
extern u32 uda_get_host_id(void); /* todo: delete it later, inlcude uda.h report error */
bool devmm_palist_is_continuous(u64 *pa, u32 num, u32 page_size);
bool devmm_palist_is_specify_continuous(u64 *pa, u32 page_size, u32 total_num, u32 min_con_num);
void devmm_update_devids(struct devmm_devid *devids, u32 logical_devid, u32 devid, u32 vfid);
int devmm_sdma_page_kernel_memzero(ka_page_t *pg, u64 page_size);
#endif /* __DEVMM_COMMON_H__ */
