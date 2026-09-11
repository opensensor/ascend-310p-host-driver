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

#ifndef DEVMM_PROC_INFO_H
#define DEVMM_PROC_INFO_H

#include <linux/mmu_notifier.h>
#include <linux/hugetlb.h>
#include <linux/export.h>

#include "ka_base_pub.h"
#include "ka_memory_pub.h"
#include "ka_task_pub.h"
#include "ka_net_pub.h"
#include "ka_common_pub.h"
#include "ka_list_pub.h"
#include "ka_fs_pub.h"
#include "ka_hashtable_pub.h"
#include "ka_driver_pub.h"
#include "ka_ioctl_pub.h"
#include "ka_barrier_pub.h"

#include "svm_cmd.h"
#include "devdrv_interface.h"
#include "svm_log.h"
#include "devmm_adapt.h"
#include "devmm_addr_mng.h"
#include "svm_mem_mng.h"
#include "svm_dynamic_addr.h"
#include "svm_res_idr.h"
#include "svm_srcu_work.h"
#include "svm_page_cnt_stats.h"
#include "svm_gfp.h"
#include "svm_define.h"
#ifdef HOST_AGENT
#include <securec.h>
#endif

#define DEVMM_DEVICE_AUTHORITY 0440

#if ((defined CFG_BUILD_DEBUG) && (!defined EXPORT_SYMBOL_UNRELEASE))
#define EXPORT_SYMBOL_UNRELEASE(symbol) KA_EXPORT_SYMBOL_GPL(symbol)
#elif (!defined EXPORT_SYMBOL_UNRELEASE)
#define EXPORT_SYMBOL_UNRELEASE(symbol)
#endif

#define DEVMM_FAULT_OK VM_FAULT_NOPAGE

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 16, 0)
typedef int vm_fault_t;
#endif

/*
 * linux kernel < 3.11 not defined KA_VM_FAULT_SIGSEGV,
 * euler LINUX_VERSION_CODE 3.10 defined KA_VM_FAULT_SIGSEGV
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
#define DEVMM_FAULT_ERROR KA_VM_FAULT_SIGSEGV
#else
#ifdef KA_VM_FAULT_SIGSEGV
#define DEVMM_FAULT_ERROR KA_VM_FAULT_SIGSEGV
#else
#define DEVMM_FAULT_ERROR KA_VM_FAULT_SIGBUS
#endif
#endif

#define devmm_pin_page(page) ka_mm_get_page(page)
#define devmm_unpin_page(page) ka_mm_put_page(page)

#define DEVMM_SETUP_INVAL_PID (-1) /* to CREATE_WAIT */

#define DEVMM_DEV_CLOSE_WAITTIME_MIN 500000 /* us */
#define DEVMM_DEV_CLOSE_WAITTIME_MAX 600000 /* us */
#define DEVMM_DEV_CLOSE_TIMES 1000          /* 500s */

#define PXD_JUDGE(pxd) (((pxd) == NULL) || (pxd##_none(*(pxd##_t *)(pxd)) != 0) || \
    (pxd##_bad(*(pxd##_t *)(pxd)) != 0))
#define PMD_JUDGE(pmd) (((pmd) == NULL) || (pmd_none(*(ka_pmd_t *)(pmd)) != 0) || \
    (pmd_bad(*(ka_pmd_t *)(pmd)) != 0))

#if defined(__arm__) || defined(__aarch64__)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0)
#define PMD_HUGE(pmd) (((pmd) != NULL) && (pmd_val(*(ka_pmd_t *)(pmd)) != 0) && \
    ((pmd_val(*(ka_pmd_t *)(pmd)) & PMD_TABLE_BIT) == 0))
#define PUD_GIANT(pud) (((pud) != NULL) && (pud_val(*(pud_t *)(pud)) != 0) && \
    ((pud_val(*(pud_t *)(pud)) & PUD_TABLE_BIT) == 0))
#else
#define PMD_HUGE(pmd) (((pmd) != NULL) && (pmd_none(*(ka_pmd_t *)(pmd)) == 0) && \
    (pte_huge(*(ka_pte_t *)(pmd)) != 0))
#define PUD_GIANT(pud) (((pud) != NULL) && (pud_none(*(pud_t *)(pud)) == 0) && \
    (pte_huge(*(ka_pte_t *)(pud)) != 0))
#endif
#else
#define PMD_HUGE(pmd) 0
#define PUD_GIANT(pud) 0
#endif

#define HEAP_USED_PER_MASK_SIZE (1UL << 30)   /* 1G */

#define DEVMM_PREFETCH_COPY_NUM 1024ul /* Multiple of huge page size  */

#define DEVMM_COPY_END_BLK_LEN 0x400000ul /* 4M */

/* multiplexing svm_heap_mng.h bitmap bit2 */
#define DEVMM_PAGE_NOSYNC_BIT 2

/* MEROS FOR SYNC PROCESS* */
#define DEVMM_PAGE_NOSYNC_FLG (1UL << DEVMM_PAGE_NOSYNC_BIT)

/* DFX: direction of check va */
#define DEVMM_PRE_ALLOCED_FLAG 1
#define DEVMM_POST_ALLOCED_FLAG 0

#define DEVMM_COMMON_PARA_VA_NUM MEM_REPAIR_MAX_CNT /* at least 2 */

struct devmm_pa_info_para {
    u64 vaddr;
    u64 *paddrs;
    u64 pa_num;
    u64 page_size;
};

struct devmm_ioctl_addr_info {
    u32 num;
    u32 cmd_id;
    u64 va[DEVMM_COMMON_PARA_VA_NUM];
    u64 size[DEVMM_COMMON_PARA_VA_NUM];
    struct devmm_svm_heap *heap[DEVMM_COMMON_PARA_VA_NUM];
    struct devmm_heap_ref *ref[DEVMM_COMMON_PARA_VA_NUM];
};

enum devmm_trans_way_type {
    SDMA = 0x0,
    PCIE_DMA
};

struct devmm_page_query_arg {
    struct devmm_svm_process_id process_id;
    u32 dev_id;
    u32 logical_devid;
    u64 va;
    u64 size;
    u64 offset;
    u32 page_size;
    u32 msg_id;
    u32 addr_type;
    u32 bitmap;
    bool is_giant_page;
    u32 page_insert_dev_id; /* in pm query, this is pm dev id; in vm query, this is vm dev id */
    u32 p2p_owner_sdid;
    u64 p2p_owner_va;
    u64 mem_map_route;
    struct devmm_svm_process_id p2p_owner_process_id;
};

struct devmm_pa_list_info {
    u32 pin_flg;
    u32 write;
    unsigned long *palist;
    ka_page_t **pages;
    u32 *szlist;
};

struct devmm_p2p_page_query_arg {
    struct devmm_svm_process_id process_id;
    u32 dev_id;
    u32 p2p_owner_sdid;
    int is_self_system;
    unsigned long va;
    unsigned long page_size;
};

struct devmm_page_cnt {
    ka_atomic64_t alloc_page_cnt;
    ka_atomic64_t free_page_cnt;
    ka_atomic64_t alloc_hugepage_cnt;
    ka_atomic64_t free_hugepage_cnt;
};

struct devmm_deviceinfo {
    ka_pid_t devpid;
    int ssid;
};

struct devmm_setupdevice {
    u32 dev_setup_map;   /* use for user thread to set ai cpu para */
    u32 dev_setuped_map; /* use for user thread flaged ai cpu fun para seted */
    ka_semaphore_t setup_sema;
    u32 already_got_mm;
};

#define DEVMM_P2P_FAULT_PAGE_MAX_NUM 32

struct devmm_p2p_pg_info {
    u32 pg_num;
    u64 pa[DEVMM_P2P_FAULT_PAGE_MAX_NUM];
};

struct devmm_huge_pg_info {
    ka_page_t *hpage;
};

struct devmm_fault_info {
    unsigned long fault_addr;
    struct devmm_svm_heap *heap;
    struct devmm_huge_pg_info huge_page_info;
    struct devmm_p2p_pg_info p2p_page_info;
};

#define DEVMM_SVM_MAX_AICORE_NUM 32
struct devmm_fault_err {
    u64 fault_addr;
    u32 fault_cnt;
};

#define DEVMM_MAX_IPC_NODE_LIST_NUM 32

struct devmm_proc_ipc_node_head {
    ka_list_head_t create_head[DEVMM_MAX_IPC_NODE_LIST_NUM];
    ka_list_head_t open_head[DEVMM_MAX_IPC_NODE_LIST_NUM];
    ka_mutex_t node_mutex;
    u32 node_cnt;
};
#define DEVMM_PAGE_CACHE_LIST_NUM   256
#define DEVMM_HUGE_PAGE_CACHE_LIST_NUM   8
#define DEVMM_PAGE_CACHE_BLK_NUM    512 /* chuck page 2M, huge page 1G */
#define DEVMM_PAGE_CACHE_NODE_SHIFT 21  /* 2M SHIFT count by DEVMM_PAGE_CACHE_BLK_NUM */
#define DEVMM_HUGE_PAGE_CACHE_NODE_SHIFT 30   /* 1G SHIFT count by DEVMM_PAGE_CACHE_BLK_NUM */

struct devmm_dev_pages_cache {
    int ref;
    ka_rw_semaphore_t lock;
    ka_list_head_t head[DEVMM_PAGE_CACHE_LIST_NUM];
    ka_list_head_t huge_head[DEVMM_HUGE_PAGE_CACHE_LIST_NUM];
};

struct devmm_addr_block {
    u64 dma_addr; /* bit0: valid or not. dma addr */
    u64 phy_addr; /* phy addr */
};

#define DEVMM_CONVERT_RES_HLIST_NUM   0x20
#define DEVMM_CONVERT_RES_LIST_SHIFT 5  /* 32 1<<5 */
struct devmm_pm_convert_res {
    ka_atomic64_t convert_id;      /* for vm safety check */
    ka_mutex_t hlist_mutex;
    ka_hlist_head_t hlist[DEVMM_CONVERT_RES_HLIST_NUM];
};

struct devmm_pa_list {
    u32 pin_flg;
    u32 pa_num;
    u64 palist[];
};

struct devmm_pa_batch {
    struct devmm_pa_batch *next;
    struct devmm_pa_list pa_list;
};

struct devmm_pa_node {
    ka_rb_node_t pa_node;
    u64 offset;
    u64 status;
    u64 src;
    u64 dst;
    u64 len;
    struct devmm_pa_batch *pa_batch;
};

struct devmm_vm_host_pa_list {
    ka_rb_root_t pa_rbtree;
    ka_mutex_t rbtree_mutex;
};
#define DEVMM_MAX_TXATU 8
#define CONFIG_TXATU_MAX_LEN 0x10000000000 /* 1T */

#define DEVMM_SHM_HUGE_PAGE DEVMM_HEAP_HUGE_PAGE
#define DEVMM_SHM_CHUNK_PAGE DEVMM_HEAP_CHUNK_PAGE
#define DEVMM_SHM_MIXED_PAGE 0
#define DEVMM_GET_2M_PAGE_NUM 512u      /* the normal page_num of 2M */
enum {
    DEVMM_SMMU_STATUS_UNINIT,
    DEVMM_SMMU_STATUS_OPENING,
    DEVMM_SMMU_STATUS_CLOSEING
};
struct devmm_txatu_info {
    u64 addr;
    u64 txatu_flag;
};

struct devmm_dma_info {
    ka_dma_addr_t dma_addr;
    u32 dma_size;
};

struct devmm_shm_node {
    ka_list_head_t list;
    u64 src_va;
    u64 dst_va;
    u64 size;
    u64 page_num;
    u64 ref;
    u32 map_type;
    u32 proc_type;
    u32 dev_id;
    u32 logical_devid;
    u32 vfid;
    u32 dma_cnt;
    bool src_va_is_pfn_map;
    bool src_pa_is_ram;
    struct devmm_dma_info *dma_info;
    ka_page_t **pages;
    u64 *pa_list;
};

struct devmm_shm_head {
    ka_list_head_t list;
};

struct devmm_shm_pro_node {
    ka_list_head_t list;
    u32 hostpid;
    struct devmm_shm_head shm_head;
    struct devmm_txatu_info txatu_info[DEVMM_MAX_TXATU];
};

struct devmm_shm_process_head {
    ka_list_head_t head;
    ka_mutex_t node_lock;
};

struct devmm_shm_dev_frag_node {
    ka_list_head_t frag_entry;
    u64 dev_va;
    u64 size;
    u32 page_size;
};

struct devmm_shm_dev_node {
    ka_list_head_t shm_entry;
    ka_list_head_t frag_list_head;
    u64 free_va;                    /* the start dev_va which have't been released */
    u64 released_size;              /* the size which have been released */
    u64 mmap_size;
    u64 mmap_va;
    u64 aligned_dst_va;
    u64 size;
    u32 map_type;
    u32 flag;
    u32 devid;
    bool svsp_remap;                /* svsp means Shadow Virtual Space of Process,
                                       true: when HOST_MEM_MAP_DEV_PCIE_TH */
    ka_page_t **pages;
    u64 page_index;
};

/* proc_status */
#define DEVMM_SVM_THREAD_EXITING   0x2   /* svm proc exiting, above pcie msg */
#define DEVMM_SVM_THREAD_WAIT_EXIT 0x4   /* svm proc wait exit, wait pcie msg proc over */

#define DEVMM_SVM_PROC_ABORT_STATE (DEVMM_SVM_THREAD_EXITING | DEVMM_SVM_THREAD_WAIT_EXIT)

#define DEVMM_CUSTOM_PROCESS_NUM  1
#define DEVMM_CUSTOM_IDLE         0
#define DEVMM_CUSTOM_USED         1
#define DEVMM_CUSTOM_CP_EXIT      2    /* CP proc exit, but CUSTOM proc not exit */

struct devmm_svm_process;
struct devmm_custom_process {
    ka_pid_t custom_pid;
    u32 idx;
    int status;
    u32 vma_num;
    ka_vm_area_struct_t *vma[DEVMM_MAX_VMA_NUM];
    ka_mm_struct_t *mm;
    struct devmm_svm_process *aicpu_proc;
#ifndef ADAPT_KP_OS_FOR_EMU_TEST
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
    ka_mmu_notifier_t *notifier;
#else
    ka_mmu_notifier_t notifier;
#endif
#endif
    ka_mutex_t proc_lock;
};

struct svm_proc_id {
    struct devmm_svm_process_id proc_id;
    ka_list_head_t proc_list;
};

struct devmm_proc_states_info {
    ka_rw_semaphore_t rw_sem;
    ka_atomic64_t oom_ref;
    u32 state[STATUS_MAX];
};

struct devmm_phy_addr_blk_mng {
    ka_idr_t idr;
    int id_start;
    int id_end;
    ka_rw_semaphore_t rw_sem;
};

struct devmm_svm_process {
    u32 proc_idx;
    u32 inited;
    u32 normal_exited; /* recv close-msg from host, will set this flag */
    bool is_enable_svm_mem;
    bool is_enable_host_giant_page;
    struct devmm_svm_process_id process_id;
    ka_list_head_t proc_id_head;

    int ssid;
    u32 docker_id;
    ka_pid_t devpid;
    volatile u32 proc_status;  /* process status */
    volatile u32 msg_processing; /* Number of messages currently being processed */
    volatile u32 other_proc_occupying; /* Number of processes who are using current processes. */
    volatile u32 notifier_reg_flag; /* struct mmu_notifier regist or not */
    int dvpp_split_flag;
    ka_atomic_t ref;    /* Optimized into kref later */

    u32 release_work_cnt;
    u32 release_work_timeout;
    u32 vma_num;
    u32 max_heap_use;
    u32 phy_devid[SVM_MAX_AGENT_NUM];
    u32 vfid[SVM_MAX_AGENT_NUM];

    u32 real_phy_devid[SVM_MAX_AGENT_NUM]; /* Real physical devid id. Agent devid is ignored  */
    /*
     * start_addr:mapped start addr of this process
     * brk_addr:used add,we used range start_addr~brk_addr
     * brk addr=start_addr+heap_size*heap_cnt
     * end_addr:mapped end addr of this process
     */
    unsigned long start_addr;
    unsigned long end_addr;
    unsigned long host_pin_start_addr;
    unsigned long host_pin_end_addr;
    u64 alloced_heap_size;

    struct devmm_deviceinfo deviceinfo[SVM_MAX_AGENT_NUM];
    struct devmm_setupdevice setup_dev;
    struct devmm_proc_ipc_node_head ipc_node;
    ka_mm_struct_t *mm;
    ka_task_struct_t *tsk;
    ka_vm_area_struct_t *vma[DEVMM_MAX_VMA_NUM];
    struct svm_da_info da_info;

    struct devmm_custom_process custom[DEVMM_CUSTOM_PROCESS_NUM];
    ka_delayed_work_t release_work;
    ka_delayed_work_t mmput_work;

    u64 device_fault_printf; /* smmu page faults always retry, printf too much */
    struct devmm_fault_err fault_err[DEVMM_SVM_MAX_AICORE_NUM];
#ifndef ADAPT_KP_OS_FOR_EMU_TEST
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
    ka_mmu_notifier_t *notifier;
#else
    ka_mmu_notifier_t notifier;
#endif
#endif
    ka_rw_semaphore_t host_fault_sem; /* free use read lock, fault use wirte lock */
    ka_semaphore_t fault_sem;
    ka_semaphore_t huge_fault_sem;
    ka_semaphore_t p2p_fault_sem; /* p2p fault and memory release process are exclusive, avoid pa is freed */
    ka_rw_semaphore_t bitmap_sem;
    ka_rw_semaphore_t ioctl_rwsem;
    ka_rw_semaphore_t msg_chan_sem;
    ka_rw_semaphore_t heap_sem; /* free use write lock, fault use read lock */
    struct devmm_svm_heap *heaps[DEVMM_MAX_HEAP_NUM];
    struct devmm_svm_heap *host_pin_heap;
    struct devmm_pm_convert_res convert_res;
    struct devmm_vm_host_pa_list host_pa_list;
    ka_mutex_t proc_lock;
    struct devmm_dev_pages_cache *dev_pages_head[SVM_MAX_AGENT_NUM];

    /* remote map */
    ka_semaphore_t register_sem[HOST_REGISTER_MAX_TPYE]; /* lock shm_pro_node */
    struct devmm_shm_pro_node *shm_pro_node[HOST_REGISTER_MAX_TPYE];
    struct devmm_shm_process_head shm_dev_head[HOST_REGISTER_MAX_TPYE];

    ka_mutex_t mem_node_mutex;
    struct devmm_addr_mng addr_mng;

    ka_proc_dir_entry_t *task_entry;

    struct devmm_proc_states_info proc_states_info[SVM_UAGENT_MAX_NUM];

    struct devmm_srcu_work srcu_work;
    struct devmm_page_cnt_stats pg_cnt_stats;

    struct devmm_phy_addr_blk_mng phy_addr_blk_mng;

    void *priv_data;
};

extern ka_mmu_notifier_ops_t devmm_process_mmu_notifier;

struct devmm_dma_block {
    unsigned long pa;       /* pa is aligned by page_size */
    u64 sz;                 /* sz is aligned by page_size */
    ka_page_t *page;
    ka_dma_addr_t dma;
    int ssid;
    bool pg_is_get;
};

struct devmm_dma_copy_task {
    u32 cpy_ret;
    u32 task_id;
    u32 task_mode;
    u32 submit_num;
    ka_atomic_t finish_num;
    ka_atomic_t occupy_num;
    u32 dev_id;
    u64 src;
    u64 dst;
    u64 size;
    volatile u64 async_status;
    u32 submit_status;
};

/*
 * convert:              set   state is   IDLE
 * sumbit:               trans state from IDLE           to      COPYING
 * wait:                 trans state from COPYING        to      IDLE
 * destroy:              trans state from IDLE           to      FREEING
 * async destroy submit: trans state from IDLE           to      PREPARE_FREE
 * async destroy:        trans state from PREPARE_FREE   to      FREEING
 */
#define CONVERT_NODE_IDLE               1
#define CONVERT_NODE_PREPARE_SUBMIT     2
#define CONVERT_NODE_COPYING            3
#define CONVERT_NODE_WAITING            4
#define CONVERT_NODE_PREPARE_FREE       5
#define CONVERT_NODE_SUBRES_RECYCLED    6
#define CONVERT_NODE_FREEING            7

struct devmm_alloc_numa_info {
    u64 free_size;
    u64 threshold;
    bool enable_threshold;
};

struct devmm_device_info {
    ka_atomic_t devicechipnum;
    u32 devicefirstchipid;
    u64 host_ddr;
    ka_atomic64_t total_ddr;
    ka_atomic64_t total_hbm;
    u32 devicechipid[DEVMM_MAX_DEVICE_NUM];
    u32 cluster_id[DEVMM_MAX_DEVICE_NUM];
    u64 ddr_size[DEVMM_MAX_DEVICE_NUM][DEVMM_MAX_VF_NUM];
    u64 ddr_hugepage_size[DEVMM_MAX_DEVICE_NUM][DEVMM_MAX_VF_NUM];
    u64 p2p_ddr_size[DEVMM_MAX_DEVICE_NUM][DEVMM_MAX_VF_NUM];
    u64 p2p_ddr_hugepage_size[DEVMM_MAX_DEVICE_NUM][DEVMM_MAX_VF_NUM];
    ka_atomic64_t free_mem_size[DEVMM_MAX_DEVICE_NUM][DEVMM_MAX_NUMA_NUM_OF_PER_DEV][DEVMM_MAX_VF_NUM];
    u64 hbm_size[DEVMM_MAX_DEVICE_NUM][DEVMM_MAX_VF_NUM];
    ka_atomic64_t free_mem_hugepage_size[DEVMM_MAX_DEVICE_NUM][DEVMM_MAX_NUMA_NUM_OF_PER_DEV][DEVMM_MAX_VF_NUM];
    u64 hbm_hugepage_size[DEVMM_MAX_DEVICE_NUM][DEVMM_MAX_VF_NUM];
    u64 p2p_hbm_size[DEVMM_MAX_DEVICE_NUM][DEVMM_MAX_VF_NUM];
    u64 p2p_hbm_hugepage_size[DEVMM_MAX_DEVICE_NUM][DEVMM_MAX_VF_NUM];
    u64 ts_ddr_size[DEVMM_MAX_DEVICE_NUM][DEVMM_MAX_VF_NUM];
    u64 ts_ddr_hugepage_size[DEVMM_MAX_DEVICE_NUM][DEVMM_MAX_VF_NUM];
    struct devmm_alloc_numa_info alloc_numa_info[SVM_UAGENT_MAX_NUM][DEVMM_MAX_NUMA_NUM_OF_PER_DEV];
};

struct devmm_svm_statistics {
    ka_atomic64_t fault_cnt;
    ka_atomic64_t p2p_fault_cnt;
    ka_atomic64_t page_cnt;
    ka_atomic64_t page_lens;
    ka_atomic64_t send_msg_cnt;
    ka_atomic64_t recv_msg_cnt;
    ka_atomic64_t copy_dirs_cnt[DEVMM_COPY_LEN_CNT4];
    ka_atomic64_t copy_lens_cnt[DEVMM_COPY_LEN_CNT32];
    ka_atomic64_t vir_addr_cnt;
    ka_atomic64_t vir_addr_lens;

    ka_atomic64_t page_alloc_cnt;
    ka_atomic64_t page_free_cnt;
    ka_atomic64_t huge_page_alloc_cnt;
    ka_atomic64_t huge_page_free_cnt;

    ka_atomic64_t page_map_cnt;
    ka_atomic64_t page_unmap_cnt;
    ka_atomic64_t huge_page_map_cnt;
    ka_atomic64_t huge_page_unmap_cnt;

    ka_atomic64_t send_p2p_msg_cnt;
    ka_atomic64_t recv_p2p_msg_cnt;

    ka_atomic64_t p2p_page_map_cnt;
    ka_atomic64_t p2p_page_unmap_cnt;
    ka_atomic64_t p2p_huge_page_map_cnt;
    ka_atomic64_t p2p_huge_page_unmap_cnt;
};

#define DEVMM_SHM_TS_SIZE_MINI_V1 0x3200000
#define DEVMM_SHM_DATA_NUM_MINI_V1 (DEVMM_SHM_TS_SIZE_MINI_V1 / sizeof(struct devmm_share_memory_data))
#define DEVMM_SHM_TS_SIZE_MINI_V2 0x1900000
#define DEVMM_SHM_DATA_NUM_MINI_V2 (DEVMM_SHM_TS_SIZE_MINI_V2 / sizeof(struct devmm_share_memory_data))
#define DEVMM_SHM_TS_SIZE_CLOUD_V1 0x1e00000
#define DEVMM_SHM_DATA_NUM_CLOUD_V1 (DEVMM_SHM_TS_SIZE_CLOUD_V1 / sizeof(struct devmm_share_memory_data))
#define DEVMM_MAX_SHM_TS_SIZE 0x3200000
#define DEVMM_MAX_SHM_DATA_NUM (DEVMM_MAX_SHM_TS_SIZE / sizeof(struct devmm_share_memory_data))

#define DEVMM_SHM_BLOCK_NUM_MINI_V1 1
#define DEVMM_SHM_BLOCK_NUM_MINI_V2 1
#define DEVMM_SHM_BLOCK_NUM_CLOUD_V1 15
#define DEVMM_MAX_BLOCK_NUM 16

#define DEVMM_SHM_TS_NOT_MAP_BAR 0
#define DEVMM_SHM_TS_DYNAMIC_MAP_BAR 1
#define DEVMM_SHM_TS_RESERVE_MAP_BAR 2

struct devmm_share_memory_mng {
    u64 va;
    u16 id;
    u8 vfid;
    u8 data_type;
    u32 host_pid;
};

struct devmm_share_memory_head {
    u64 block_addr[DEVMM_MAX_BLOCK_NUM];
    u64 block_dma_addr[DEVMM_MAX_BLOCK_NUM];
    u32 block_id[DEVMM_MAX_BLOCK_NUM];
    u32 support_bar_write;
    u32 total_block_num;
    u32 total_data_num;
    u32 free_index;
    u32 recycle_index;
    u32 core_num[DEVMM_MAX_VF_NUM];
    u32 total_core_num[DEVMM_MAX_VF_NUM];
    u32 vdev_total_data_num[DEVMM_MAX_VF_NUM];
    u32 vdev_free_data_num[DEVMM_MAX_VF_NUM];
    u64 vdev_total_convert_len[DEVMM_MAX_VF_NUM];
    u64 vdev_free_convert_len[DEVMM_MAX_VF_NUM];
    struct devmm_share_memory_mng *share_memory_mng;
    ka_mutex_t mutex;
};

struct devmm_pm_convert_ids {
    ka_atomic_t vdev_free_id_num[DEVMM_MAX_VF_NUM];
};

#define DEVMM_SUPPORT_OFFSET_SECURITY_MASK 0x1
#define DEVMM_CONVERT_SUPPORT_OFFSET_MASK 0x2

struct devmm_device_capability {
    u64 dvpp_memsize;
    u32 svm_offset_num;
    u32 feature_phycial_address;
    u32 feature_bar_mem;
    u32 feature_pcie_th;
    u32 feature_dev_read_only;
    u32 feature_pcie_dma_support_sva;
    u32 feature_dev_mem_map_host;
    u32 feature_bar_huge_mem;
    u64 double_pgtable_offset;
    u32 feature_giant_page;
    u32 feature_aic_reg_map;
    u32 feature_remote_mmap;
    u32 feature_shmem_repair;
    u32 feature_host_mem_map_cap;
    u32 feature_mem_host_uva;
};

struct devmm_svm_dev {
    ka_cdev_t char_dev;
    unsigned int dev_no;
    ka_device_t *dev;

    /* svm page size,means svm mgmt's page size. 64k */
    u32 svm_page_size;
    u32 svm_page_shift;
    u32 huge_page_size;  // device

    /* device page size,means device page size, 64K. */
    u32 host_page_size;
    u32 host_hpage_size;
    u32 device_page_size;
    u32 device_hpage_size;

    u32 host_page_shift;
    u32 host_hpage_shift;
    u32 device_page_shift;
    u32 device_hpage_shift;

    u32 host_hpage2device_hpage_order;
    u32 host_page2device_hpage_order;
    u32 host_page2device_page_order;

    u32 page_size_inited;

    u32 smmu_status;
    struct devmm_mmap_para mmap_para;

    u64 total_convert_len[SVM_MAX_AGENT_NUM];          /* total_len pre dev can convert */
    ka_rw_semaphore_t convert_sem;

    struct devmm_device_info device_info;

    ka_atomic_t ipcnode_sq;
    ka_mutex_t proc_lock;

    struct devmm_shm_process_head shm_pro_head[HOST_REGISTER_MAX_TPYE];
    struct devmm_svm_statistics stat;

    struct devmm_share_memory_head pa_info[SVM_MAX_AGENT_NUM];    /* for master */
    struct devmm_pm_convert_ids convert_ids[SVM_MAX_AGENT_NUM];   /* in VM, limit the convert times */
    struct devmm_device_capability dev_capability[SVM_MAX_AGENT_NUM];

    struct devmm_phy_addr_blk_mng share_phy_addr_blk_mng[DEVMM_MAX_AGENTMM_DEVICE_NUM];

    ka_mutex_t setup_lock;
};

struct devmm_copy_side {
    struct devmm_register_dma_node *register_dma_node;
    struct devmm_dma_block *blks;
    unsigned int blks_num; /* alloc mem num */
    unsigned int num; /* actual num after merge pa */
    unsigned long blk_page_size; /* page size */
    int side_type;
};

struct devmm_copy_res {
    struct devmm_copy_res *next;
    struct devdrv_dma_prepare *dma_prepare;
    struct devmm_copy_side from;
    struct devmm_copy_side to;
    struct devdrv_dma_node *dma_node;
    struct devmm_svm_process *svm_pro;
    struct devmm_dma_copy_task *copy_task;
    void *dma_prepare_pool_fd;
    u64 src_va;
    u64 dst_va;
    u64 spitch;
    u64 dpitch;
    u64 cpy_len;    /* width */
    u64 height;
    u64 fixed_size;
    u32 dma_node_alloc_num; /* 1: make dmanode list */
                            /* 2: used for record all res's dma_node_num */
    u32 dma_node_num;
    int copy_direction;
    int pin_flg;
    int dev_id;      /* which device's dma is used */
    int dst_dev_id;
    bool vm_copy_flag;
    int cpy_mode;
    u32 vm_id;      /* process vm_id */
    int fid;
    u32 task_id;
    u64 byte_count;
};

struct devmm_memory_attributes {
    bool is_local_host;
    bool is_host_pin;
    bool is_svm;
    bool is_svm_huge;
    bool is_svm_host;
    bool is_svm_host_agent;
    bool is_svm_device;
    bool is_svm_non_page;
    bool is_svm_continuty;
    bool is_svm_readonly;
    bool is_svm_remote_maped;
    bool is_locked_host;
    bool is_locked_device;
    bool is_local_device;
    bool is_ipc_open;
    bool copy_use_va;
    bool is_reserve_addr;
    bool is_mem_export;
    bool is_mem_import;
    u32 bitmap;
    u32 logical_devid;
    u32 devid;
    u32 vfid;
    u32 page_size;
    u32 host_page_size;
    u64 heap_size;
    u32 granularity_size;
    u64 va;
    int ssid;
    u32 mem_share_devid;
    int mem_share_id;
    bool is_svm_dev_readonly;
};

struct devmm_translate_info {
    u32 logical_devid;
    u32 dev_id;
    u32 vfid;
    u32 page_size;
    u32 page_insert_dev_id;
    u64 va;
    u64 alloced_va;
    u64 alloced_size;
    bool is_svm_continuty;
    bool is_vm_translate;
};

struct devmm_mem_copy_convrt_para {
    u64 dst;
    u64 src;
    size_t count;
    u32 direction;
    u32 blk_size;
    u32 task_id;
    u32 dev_id;
    u32 vfid;
    u32 seq;
    u32 task_mode; /* convert, cpy async, cpy sync */
    int task_query_id;
    bool last_seq_flag;
    bool create_msg;
    bool is_2d;
    bool need_write;
    struct devmm_dma_copy_task *copy_task;
    struct devmm_copy_res *res;
    bool is_memcpy_batch;
    bool is_memcpy_batch_first_addr;
};

#define DEVMM_DATA_SIZE 40
#pragma pack(1)
struct devmm_share_memory_data {
    u32 image_word;
    u16 id;
    u16 vm_id;
    int host_pid;
    u16 data_type;
    u16 vfid;
    char data[DEVMM_DATA_SIZE];
    u64 va;
};
#pragma pack()

struct devmm_free_dev_mem_info {
    u32 first_devshared_index;
    u32 last_devshared_index;
    u32 first_devmmaped_index;
    u32 last_devmapped_index;
    u32 devmapped_flag;
    u32 shared_flag;
    u32 free_flag;
};

extern struct devmm_svm_dev *devmm_svm;

/* cmd_flag define */
/* proc lock */
#define DEVMM_CMD_NOLOCK 0x01 /* ioctl sem, 1:no sem, 0:need sem and see bit1 */
#define DEVMM_CMD_WLOCK 0x02  /* read sem or write sem, 0:read sem, 1:write lock; */
/* device id convert flag */
#define DEVMM_CONVERT_ID 0x04 /* convert phy id, 0:not need convert, 1:need convert */
/* va ref for multi thread */
#define DEVMM_OPER_REF 0x08    /* 0 no va ref; 1 va ref */
#define DEVMM_ADD_REF (0x10 | DEVMM_OPER_REF) /* va ref ++ */
#define DEVMM_SUB_REF (0x20 | DEVMM_OPER_REF) /* va ref -- */
#define DEVMM_ADD_SUB_REF (DEVMM_ADD_REF | DEVMM_SUB_REF)
#define DEVMM_HAS_MUTIL_ADDR 0x80   /* 0 has one va; 1 has mutil va */
#define DEVMM_IS_FREE (0x100 | DEVMM_OPER_REF)  /* is free api */
#define DEVMM_IS_MALLOC (0x200 | DEVMM_OPER_REF) /* is malloc api */
#define DEVMM_IS_ADVISE (0x400 | DEVMM_OPER_REF)  /* is adivse api */
#define DEVMM_OPS_SUCCESS_SUB_REF (0x800 | DEVMM_OPER_REF) /* va ref -- */
#define DEVMM_HUGE_PAGE_SIZE    0x200000U  /* 2MB page */

/* vdev not support flag */
#define DEVMM_CMD_NOT_SURPORT_VDEV 0x1000
/* host agent not support flag */
#define DEVMM_CMD_NOT_SURPORT_HOST_AGENT 0x4000
#define DEVMM_CMD_SURPORT_HOST_ID 0x8000

extern int (*const devmm_ioctl_file_arg_handlers[DEVMM_SVM_CMD_MAX_CMD])
    (ka_file_t *file, struct devmm_ioctl_arg *arg);

struct devmm_ioctl_handlers_st {
    int (*ioctl_handler)(struct devmm_svm_process *, struct devmm_ioctl_arg *);
    /* detailed in cmd_flag define */
    u32 cmd_flag;
};

#define DEVMM_ACCESS_H2D_PAGE_NUM 512ULL
#define DEVMM_PAGE_WRITE 1

extern struct devmm_ioctl_handlers_st devmm_ioctl_handlers[DEVMM_SVM_CMD_MAX_CMD];

#define devmm_svm_pageshift2pagesize(pgshift) ((u32)1 << (pgshift))

#define devmm_is_page_size_inited() (devmm_svm->page_size_inited == 1)
#define devmm_svm_get_host_pgsf() (devmm_svm->host_page_shift)
#define devmm_svm_get_device_pgsf() (devmm_svm->device_page_shift)
#define devmm_svm_get_device_hpgsf() (devmm_svm->device_hpage_shift)
#define devmm_svm_set_host_pgsf(pgshift) (devmm_svm->host_page_shift = (pgshift))
#define devmm_svm_set_host_hpgsf(pgshift) (devmm_svm->host_hpage_shift = (pgshift))

#define devmm_svm_set_device_pgsf(pgshift) (devmm_svm->device_page_shift = (pgshift))
#define devmm_svm_set_device_hpgsf(pgshift) (devmm_svm->device_hpage_shift = (pgshift))
#define devmm_svm_set_device_hugepg(pgsize) (devmm_svm->huge_page_size = (pgsize))

#define devmm_svm_stat_fault_inc() (ka_base_atomic64_inc(&devmm_svm->stat.fault_cnt))
#define devmm_svm_stat_p2p_fault_inc() (ka_base_atomic64_inc(&devmm_svm->stat.p2p_fault_cnt))

#define devmm_svm_stat_copy_inc(dir, len)               \
    (ka_base_atomic64_inc(&devmm_svm->stat.copy_dirs_cnt[dir]))

#define devmm_svm_stat_send_inc() (ka_base_atomic64_inc(&devmm_svm->stat.send_msg_cnt))
#define devmm_svm_stat_recv_inc() (ka_base_atomic64_inc(&devmm_svm->stat.recv_msg_cnt))
#define devmm_svm_stat_page_inc(len) \
    (ka_base_atomic64_add(len, &devmm_svm->stat.page_lens), ka_base_atomic64_inc(&devmm_svm->stat.page_cnt))
#define devmm_svm_stat_page_dec(len) \
    (ka_base_atomic64_sub(len, &devmm_svm->stat.page_lens), ka_base_atomic64_dec(&devmm_svm->stat.page_cnt))

#define devmm_svm_stat_p2p_send_inc() (ka_base_atomic64_inc(&devmm_svm->stat.send_p2p_msg_cnt))
#define devmm_svm_stat_p2p_recv_inc() (ka_base_atomic64_inc(&devmm_svm->stat.recv_p2p_msg_cnt))

#define devmm_svm_stat_vir_page_inc(len) \
    (ka_base_atomic64_add(len, &devmm_svm->stat.vir_addr_lens), ka_base_atomic64_inc(&devmm_svm->stat.vir_addr_cnt))
#define devmm_svm_stat_vir_page_dec(len) \
    (ka_base_atomic64_sub(len, &devmm_svm->stat.vir_addr_lens), ka_base_atomic64_dec(&devmm_svm->stat.vir_addr_cnt))
#define devmm_get_current_pid() (ka_task_get_current_tgid())

#define devmm_svm_stat_pg_alloc_inc() (ka_base_atomic64_inc(&devmm_svm->stat.page_alloc_cnt))
#define devmm_svm_stat_pg_free_inc() (ka_base_atomic64_inc(&devmm_svm->stat.page_free_cnt))
#define devmm_svm_stat_huge_alloc_inc() (ka_base_atomic64_inc(&devmm_svm->stat.huge_page_alloc_cnt))
#define devmm_svm_stat_huge_free_inc() (ka_base_atomic64_inc(&devmm_svm->stat.huge_page_free_cnt))

#define devmm_svm_stat_pg_map_inc() (ka_base_atomic64_inc(&devmm_svm->stat.page_map_cnt))
#define devmm_svm_stat_pg_unmap_inc() (ka_base_atomic64_inc(&devmm_svm->stat.page_unmap_cnt))
#define devmm_svm_stat_huge_map_inc() (ka_base_atomic64_inc(&devmm_svm->stat.huge_page_map_cnt))
#define devmm_svm_stat_huge_unmap_inc() (ka_base_atomic64_inc(&devmm_svm->stat.huge_page_unmap_cnt))

#define devmm_svm_stat_p2p_map_inc() (ka_base_atomic64_inc(&devmm_svm->stat.p2p_page_map_cnt))
#define devmm_svm_stat_p2p_unmap_inc() (ka_base_atomic64_inc(&devmm_svm->stat.p2p_page_unmap_cnt))
#define devmm_svm_stat_p2p_huge_map_inc() (ka_base_atomic64_inc(&devmm_svm->stat.p2p_huge_page_map_cnt))
#define devmm_svm_stat_p2p_huge_unmap_inc() (ka_base_atomic64_inc(&devmm_svm->stat.p2p_huge_page_unmap_cnt))

#define devmm_host_hugepage_fault_adjust_order() (devmm_svm->host_page2device_hpage_order)
#define devmm_host_to_dev_hpage_adjust_num() (1ul << devmm_svm->host_page2device_hpage_order)
#define devmm_device_page_adjust_order() (devmm_svm->host_page2device_page_order)
#define devmm_device_page_adjust_num() (1ul << devmm_svm->host_page2device_page_order)
#define devmm_device_hpage_adjust_order() (devmm_svm->host_hpage2device_hpage_order)
#define devmm_device_hpage_adjust_num() (1ul << devmm_svm->host_hpage2device_hpage_order)

enum devmm_page_convert_type {
    DEVMM_UNPIN_PAGES = 0,
    DEVMM_PIN_PAGES = 1,
    DEVMM_DEVICE_PAGES = 2,
    DEVMM_USER_PIN_PAGES = 3
};

extern int devdrv_manager_container_logical_id_to_physical_id(u32 logical_dev_id, u32 *physical_dev_id, u32 *vfid);
u32 devmm_get_phyid_devid_from_svm_process(struct devmm_svm_process *svm_process, u32 logic_id);
u32 devmm_get_vfid_from_svm_process(struct devmm_svm_process *svm_process, u32 logic_id);
void devmm_set_phyid_devid_to_svm_process(struct devmm_svm_process *svm_process, u32 logic_id, u32 phyid);
void devmm_set_vfid_to_svm_process(struct devmm_svm_process *svm_process, u32 logic_id, u32 vfid);
u32 devmm_page_bitmap_get_phy_devid(struct devmm_svm_process *svm_process, u32 *bitmap);
u32 devmm_page_bitmap_get_vfid(struct devmm_svm_process *svm_process, u32 *bitmap);
struct devmm_heap_ref *devmm_find_first_page_ref(struct devmm_svm_process *svm_process, u64 va, u32 ref_flag);
int devmm_svm_other_proc_occupy_get_lock(struct devmm_svm_process *svm_proc);
void devmm_svm_other_proc_occupy_put_lock(struct devmm_svm_process *svm_proc);
int devmm_svm_other_proc_occupy_num_add(struct devmm_svm_process *svm_proc);
void devmm_svm_other_proc_occupy_num_sub(struct devmm_svm_process *svm_proc);

int devmm_inc_page_ref(struct devmm_svm_process *svm_proc, u64 va, u64 size);
void devmm_dec_page_ref(struct devmm_svm_process *svm_proc, u64 va, u64 size);

ka_device_t *devmm_device_get_by_devid(u32 dev_id);
void devmm_device_put_by_devid(u32 dev_id);

struct devmm_svm_heap *devmm_svm_get_heap(struct devmm_svm_process *svm_process, unsigned long va);
struct devmm_svm_heap *devmm_svm_heap_get(struct devmm_svm_process *svm_proc, unsigned long va);
void devmm_svm_heap_put(struct devmm_svm_heap *heap);

int devmm_va_to_pfn(const ka_vm_area_struct_t *vma, u64 va, u64 *pfn, u64 *kpg_size);
int devmm_va_to_pa(const ka_vm_area_struct_t *vma, u64 va, u64 *pa);
int devmm_get_va_to_pa(const ka_vm_area_struct_t *vma, u32 page_type, u64 va, u64 *pa);
void devmm_unmap_pages(struct devmm_svm_process *svm_proc, u64 vaddr, u64 page_num);
bool devmm_va_is_not_svm_process_addr(const struct devmm_svm_process *svm_process, unsigned long va);
bool devmm_is_static_reserve_addr(struct devmm_svm_process *svm_proc, u64 va);
int devmm_insert_pages_to_vma(ka_vm_area_struct_t *vma, u64 va,
    u64 page_num, ka_page_t **inpages, u32 pgprot);
int devmm_insert_pages_to_vma_custom(ka_vm_area_struct_t *vma, u64 va,
    u64 page_num, ka_page_t **inpages, u32 pgprot);
int devmm_insert_pages_to_vma_owner(ka_vm_area_struct_t *vma, u64 va,
    u64 page_num, ka_page_t **inpages, ka_pgprot_t vm_page_prot);
int devmm_pages_remap_owner(struct devmm_svm_process *svm_proc, u64 va, u64 page_num,
    ka_page_t **inpages, u32 page_prot);
int devmm_pages_remap(struct devmm_svm_process *svm_proc, u64 va, u64 page_num,
    ka_page_t **inpages, u32 page_prot);
int devmm_remap_pages(struct devmm_svm_process *svm_proc, u64 va,
    ka_page_t **pages, u64 pg_num, u32 pg_type);
void devmm_zap_pages(struct devmm_svm_process *svm_proc, u64 va, u64 pg_num, u32 pg_type);
int devmm_va_to_palist(const ka_vm_area_struct_t *vma, u64 va, u64 sz, u64 *pa, u32 *num);
int devmm_insert_normal_pages(struct page_map_info *page_map_info, struct devmm_svm_process *svm_proc);
int devmm_dev_is_self_system(unsigned int dev_id);
int devmm_txatu_target_to_base(u32 to_devid, u32 from_devid, phys_addr_t target_addr, phys_addr_t *base_addr);
void devmm_zap_vma_ptes(ka_vm_area_struct_t *vma, unsigned long vaddr, unsigned long size);
ka_pmd_t *devmm_get_va_to_pmd(const ka_vm_area_struct_t *vma, unsigned long va);
void *devmm_get_pte(const ka_vm_area_struct_t *vma, u64 va, u64 *kpg_size);
int devmm_va_to_pmd(const ka_vm_area_struct_t *vma, unsigned long va, int huge_flag, ka_pmd_t **tem_pmd);
void devmm_init_dev_private(struct devmm_svm_dev *dev, ka_file_operations_t *svm_fops);
void devmm_uninit_dev_private(struct devmm_svm_dev *dev);
void devmm_notifier_release_private(struct devmm_svm_process *svm_proc);
int devmm_svm_proc_and_heap_get(struct devmm_svm_process_id *process_id, u64 va,
    struct devmm_svm_process **svm_proc, struct devmm_svm_heap **heap);
void devmm_svm_proc_and_heap_put(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap);

void devmm_svm_ioctl_lock(struct devmm_svm_process *svm_proc, u32 lock_flag);
void devmm_svm_ioctl_unlock(struct devmm_svm_process *svm_proc, u32 lock_flag);
void devmm_get_svm_id(struct devmm_svm_process *svm_process, u32 *bitmap,
    u32 *logic_id, u32 *phy_id, u32 *vfid);
int devmm_fill_svm_id(struct devmm_devid *svm_id, u32 logic_id, u32 phy_id, u32 vfid);
u32 devmm_svm_va_to_devid(struct devmm_svm_process *svm_proc, unsigned long va);
int devmm_container_vir_to_phs_devid(u32 virtual_devid, u32 *physical_devid, u32 *vfid);
int devmm_convert_id_from_vir_to_phy(struct devmm_svm_process *process,
    struct devmm_ioctl_arg *buffer, u32 cmd_flag);
int devmm_check_cmd_support(u32 cmd_flag);
void devmm_dev_fault_flag_set(u32 *flag, u32 shift, u32 wide, u32 value);
u32 devmm_dev_fault_flag_get(u32 flag, u32 shift, u32 wide);
void devmm_destory_heap_mem(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap);
int devmm_svm_davinci_module_init(const ka_file_operations_t *ops);
void devmm_svm_davinci_module_uninit(void);
int devmm_svm_dev_init(ka_file_operations_t *ops);
void devmm_svm_dev_destory(void);
bool devmm_svm_can_release_private(struct devmm_svm_process *svm_proc);
void devmm_svm_release_private_proc(struct devmm_svm_process *svm_proc);
void devmm_unmap_pages_owner(struct devmm_svm_process *svm_proc, u64 vaddr, u64 num);
int devmm_remap_huge_pages(struct devmm_svm_process *svm_proc, u64 va, ka_page_t **hpages, u64 pg_num, u32 pg_prot);
void devmm_zap_huge_pages(struct devmm_svm_process *svm_proc, u64 va, u64 page_num);
int devmm_remap_giant_pages(struct devmm_svm_process *svm_proc,
    u64 va, ka_page_t **hpages, u64 pg_num, u32 pg_prot, bool has_interval);
void devmm_zap_giant_pages(struct devmm_svm_process *svm_proc, u64 va, u64 page_num);
void devmm_zap_normal_pages(struct devmm_svm_process *svm_proc, u64 va, u64 page_num);
int devmm_ioctl_dispatch(struct devmm_svm_process *svm_proc, u32 cmd_id, u32 cmd_flag,
    struct devmm_ioctl_arg *buffer);
void devmm_proc_debug_info_print(struct devmm_svm_process *svm_proc);

/* host */
u32 *devmm_get_alloced_va_fst_page_bitmap_with_heap(struct devmm_svm_heap *heap, u64 va);
u32 *devmm_get_alloced_va_fst_page_bitmap(struct devmm_svm_process *svm_proc, u64 va);
int devmm_get_alloced_va(struct devmm_svm_process *svm_proc, u64 va, u64 *alloced_va);
int devmm_get_alloced_size(struct devmm_svm_process *svm_proc, u64 va, u64 *alloced_size);
u32 *devmm_get_page_bitmap(struct devmm_svm_process *svm_process, u64 va);
u32 *devmm_get_page_bitmap_with_heap(struct devmm_svm_heap *heap, u64 va);
void devmm_svm_set_mapped_with_heap(struct devmm_svm_process *svm_process, unsigned long va, size_t size,
    u32 devid, struct devmm_svm_heap *heap);
void devmm_svm_clear_mapped_with_heap(struct devmm_svm_process *svm_process, unsigned long va, size_t size,
    u32 devid, struct devmm_svm_heap *heap);
int devmm_query_page_by_msg(struct devmm_svm_process *svm_proc, struct devmm_page_query_arg query_arg,
    struct devmm_dma_block *blks, u32 *num);
int devmm_page_create_query_msg(struct devmm_svm_process *svm_pro, struct devmm_page_query_arg query_arg,
    struct devmm_dma_block *blks, u32 *num);
int devmm_p2p_page_create_msg(struct devmm_svm_process *svm_pro, struct devmm_page_query_arg query_arg,
    struct devmm_dma_block *blks, u32 *num);
int devmm_chan_query_meminfo_h2d(struct devmm_svm_process *svm_pro, struct devmm_svm_heap *heap,
    void *msg, u32 *ack_len);
int devmm_page_fault_h2d_sync(struct devmm_devid svm_id, ka_page_t **pages, unsigned long va, u32 adjust_order,
                              const struct devmm_svm_heap *heap);
void devmm_print_pre_alloced_va(struct devmm_svm_process *svm_process, u64 va);
int devmm_check_alloced_va(struct devmm_svm_process *svm_process, u64 va, u64 *start_va, u64 *end_va, u32 direction);
u32 devmm_get_adjust_order_by_heap(struct devmm_svm_heap *heap);
int devmm_get_memory_attributes(struct devmm_svm_process *svm_proc, u64 addr,
    struct devmm_memory_attributes *attr);
int devmm_get_svm_mem_attrs(struct devmm_svm_process *svm_proc, u64 addr, struct devmm_memory_attributes *attr);
void devmm_get_local_host_mem_attrs(struct devmm_svm_process *svm_proc, u64 addr,
    struct devmm_memory_attributes *attr);
int devmm_get_local_dev_mem_attrs(struct devmm_svm_process *svm_proc, u64 addr, u64 size, u32 logical_devid,
    struct devmm_memory_attributes *attr);
int devmm_check_status_va_info(struct devmm_svm_process *svm_process, u64 va, u64 count);
void devmm_destroy_dev_pages_cache(struct devmm_svm_process *svm_proc, u32 devid);
bool devmm_dev_is_same_system(u32 src_devid, u32 dst_devid);
void devmm_notify_wait_device_close_process(struct devmm_svm_process *svm_proc,
    u32 logical_devid, u32 phy_devid, u32 vfid);
int devmm_insert_host_page_range(struct devmm_svm_process *svm_pro, u64 dst,
    u64 byte_count, struct devmm_memory_attributes *fst_attr);
bool devmm_is_master(struct devmm_memory_attributes *attr);
int devmm_alloc_host_range(struct devmm_svm_process *svm_proc, u64 va, u64 page_num, enum devmm_page_type page_type);
bool devmm_acquire_aligned_addr_and_cnt(u64 address, u64 byte_count, int is_svm_huge,
    u64 *aligned_down_addr, u64 *aligned_count);
u32 devmm_get_logic_id_by_phy_id(struct devmm_svm_process *svm_proc, u32 devid, u32 vfid);
int devmm_get_alloced_va_with_heap(struct devmm_svm_heap *heap, u64 va, u64 *alloced_va);
void devmm_destory_all_heap_by_proc(struct devmm_svm_process *svm_pro);
int devmm_init_process_notice_pm(struct devmm_svm_process *svm_proc);
int devmm_release_process_notice_pm(struct devmm_svm_process *svm_proc);
int devmm_page_fault_get_va_ref(struct devmm_svm_process *svm_proc, u64 va);
void devmm_page_fault_put_va_ref(struct devmm_svm_process *svm_proc, u64 va);
int devmm_ioctl_query_process_status(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
int devmm_chan_report_process_status_d2h(
    struct devmm_svm_process *svm_process, struct devmm_svm_heap *heap, void *msg, u32 *ack_len);
bool devmm_check_is_translate(struct devmm_svm_process *svm_pro, u64 va, u32 *page_bitmap,
    u32 page_size, u64 page_num);

/* host end
 * device
 */
void devmm_unmap_page_from_vma_owner(struct devmm_svm_process *svm_proc,
    ka_vm_area_struct_t *vma, u64 vaddr, u64 num);

void devmm_svm_setup_vma_ops(ka_vm_area_struct_t *vma);
void devmm_free_ptes_in_range(struct devmm_svm_process *svm_proc, u64 start, u64 size);
int devmm_va_to_pa_range(const ka_vm_area_struct_t *vma, u64 va, u64 num, u64 *pas);
bool devmm_is_device_agent(struct devmm_memory_attributes *attr);
int devmm_notify_deviceprocess(struct devmm_svm_process *svm_proc);
bool devmm_current_is_vdev(void);
void devmm_unmap_page_from_vma_custom(struct devmm_svm_process *svm_proc,
    ka_vm_area_struct_t *vma, u64 vaddr, u64 num);
int devmm_chan_query_process_status_h2d(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap,
    void *msg, u32 *ack_len);
int _devmm_insert_virt_range(struct devmm_svm_process *svm_proc, u32 pg_type, u64 vaddr,
    u64 *paddr, u32 pg_num);

int devmm_ioctl_handler_register(int cmd, struct devmm_ioctl_handlers_st hander);
void devmm_clear_page_ref_after_ioctl(struct devmm_svm_process *svm_proc,
    u32 cmd_flag, int ret, struct devmm_ioctl_addr_info *addr_info);
int devmm_set_page_ref_before_ioctl(struct devmm_svm_process *svm_proc, u32 cmd_flag,
    struct devmm_ioctl_addr_info *addr_info);

bool devmm_va_is_support_sdma_kernel_clear(struct devmm_svm_process *svm_proc, u64 va);
void devmm_sdma_kernel_mem_clear(struct devmm_phy_addr_attr *attr, int ssid, struct devmm_pa_info_para *pa_info);
bool devmm_is_mdev_vm_boot_mode(u32 devid);
/* extern end */
#endif /* __DEVMM_PROC_INFO_H__ */
