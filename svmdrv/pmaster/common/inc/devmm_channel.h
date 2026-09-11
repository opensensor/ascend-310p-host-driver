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

#ifndef DEVMM_CHANNEL_H
#define DEVMM_CHANNEL_H
#include "svm_cmd.h"
#include "devmm_proc_info.h"
#include "svm_heap_mng.h"
#include "svm_dma_map.h"
#include "svm_mem_map.h"

#define DEVMM_CHANNEL_MASK 0x00000FFF

enum {
    DEVMM_CHAN_EX_PGINFO_H2D_ID = 0,
    DEVMM_CHAN_SETUP_DEVICE_H2D = 1,
    DEVMM_CHAN_CLOSE_DEVICE_H2D = 2,
    DEVMM_CHAN_PAGE_QUERY_H2D_ID = 3,
    DEVMM_CHAN_PAGE_CREATE_H2D_ID = 4,
    DEVMM_CHAN_PAGE_CREATE_QUERY_H2D_ID = 5,
    DEVMM_CHAN_PAGE_P2P_CREATE_H2D_ID = 6,
    DEVMM_CHAN_PAGE_FAULT_P2P_ID = 7,
    DEVMM_CHAN_FREE_PAGES_H2D_ID = 8,
    DEVMM_CHAN_MEMSET8_H2D_ID = 9,
    DEVMM_CHAN_ADVISE_CACHE_PERSIST_H2D_ID = 10,
    DEVMM_CHAN_MEM_DFX_QUERY_H2D_ID = 11,
    DEVMM_CHAN_PAGE_FAULT_H2D_ID = 12,
    DEVMM_CHAN_MAP_DEV_RESERVE_H2D_ID = 13,
    DEVMM_CHAN_PAGE_FAULT_D2H_ID = 14,
    DEVMM_CHAN_QUERY_VAFLGS_D2H_ID = 15,
    DEVMM_CHAN_UPDATE_HEAP_H2D_ID = 16,
    DEVMM_CHAN_QUERY_MEMINFO_H2D_ID = 17,
    DEVMM_CHAN_MEMCPY_D2D_ID = 18,
    DEVMM_CHAN_REMOTE_MAP_ID = 19,
    DEVMM_CHAN_REMOTE_UNMAP_ID = 20,
    DEVMM_CHAN_SHM_GET_PAGES_D2H_ID = 21,
    DEVMM_CHAN_SHM_PUT_PAGES_D2H_ID = 22,
    DEVMM_CHAN_CHECK_VA_D2H_ID = 23,
    DEVMM_CHAN_PROCESS_STATUS_REPORT_D2H_ID = 24,
    DEVMM_CHAN_QUERY_PROCESS_STATUS_H2D_ID = 25,
    DEVMM_CHAN_MEM_CREATE_H2D_ID = 26,
    DEVMM_CHAN_MEM_RELEASE_H2D_ID = 27,
    DEVMM_CHAN_MEM_MAP_H2D_ID = 28,
    DEVMM_CHAN_MEM_UNMAP_H2D_ID = 29,
    DEVMM_CHAN_MEM_EXPORT_H2D_ID = 30,
    DEVMM_CHAN_MEM_IMPORT_H2D_ID = 31,
    DEVMM_CHAN_SHARE_MEM_RELEASE_H2D_ID = 32,
    DEVMM_CHAN_SHARE_MEM_MEMSET_H2D_ID = 33,
    DEVMM_CHAN_TARGET_ADDR_P2P_ID = 34,
    DEVMM_CHAN_SVM_MEM_REPAIR_H2D_ID = 35,
    DEVMM_CHAN_NO_SVM_MEM_REPAIR_H2D_ID = 36,
    DEVMM_CHAN_GET_BLK_INFO_H2D_ID = 37,
    DEVMM_CHAN_MAX_ID
};

enum {
    DEVMM_DMA,
    DEVMM_NON_DMA
};

struct devmm_chan_handlers_st {
    int (*const chan_msg_processes)(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap,
        void *msg, u32 *ack_len);
    u32 msg_size;
    u32 extend_size;
    u32 msg_bitmap;
};

extern struct devmm_chan_handlers_st devmm_channel_msg_processes[DEVMM_CHAN_MAX_ID];
extern struct devmm_chan_handlers_st devmm_channel_p2p_msg_processes[DEVMM_CHAN_MAX_ID];

struct devmm_chan_msg_head {
    struct devmm_svm_process_id process_id;
    u16 msg_id;
    u16 logical_devid;
    /* master to agent: dst agent id;
     * agent to master: src agent id;
     * agent to agent: dst agent id;
     */
    u16 dev_id;
    u16 vfid;
    short result;
    u16 extend_num;
    u32 res; /* used as dst hostpid in p2p copy */
};

struct devmm_chan_addr_head {
    struct devmm_chan_msg_head head;
    u64 va;
};

enum {
    DEVMM_EXCHANGE_DDR_SIZE,
    DEVMM_EXCHANGE_HBM_SIZE,
    DEVMM_EXCHANGE_MAX_MEM_TYPE
};

struct devmm_chan_exchange_pginfo {
    struct devmm_chan_msg_head head;
    u32 host_page_shift;
    u32 host_hpage_shift;
    u32 device_page_shift;
    u32 device_hpage_shift;
    u32 cluster_id;
    u32 ts_shm_block_num;
    u32 ts_shm_data_num;
    u32 ts_shm_support_bar_write;
    u32 ts_shm_block_id[DEVMM_MAX_BLOCK_NUM];
    u64 ts_shm_dma_addr[DEVMM_MAX_BLOCK_NUM];
    u64 ts_shm_addr[DEVMM_MAX_BLOCK_NUM];
    u64 dev_mem[DEVMM_EXCHANGE_MAX_MEM_TYPE];
    u64 dev_mem_p2p[DEVMM_EXCHANGE_MAX_MEM_TYPE];
    struct devmm_device_capability device_capability;
    u64 host_uva_start;
};

struct devmm_chan_phy_block {
    unsigned long pa;
    unsigned int sz;
};

#define DEVMM_PAGE_NUM_PER_FAULT 514
struct devmm_chan_page_fault {
    struct devmm_chan_msg_head head;
    u64 va;
    u32 num;
    struct devmm_chan_phy_block blks[DEVMM_PAGE_NUM_PER_FAULT];
};
#define DEVMM_BLKNUM_ADD_NUM 2
#define DEVMM_CHUNK_PAGE_SHIFT 12
#define DEVMM_HUGE_PAGE_SHIFT 21
#define DEVMM_SIZE_TO_PAGE_NUM(size, page_size) (((size) / (page_size)) + DEVMM_BLKNUM_ADD_NUM)
#define DEVMM_SIZE_TO_PAGE_MAX_NUM(size, page_size) (((size) / (page_size)) + DEVMM_BLKNUM_ADD_NUM)
#define DEVMM_SIZE_TO_HUGEPAGE_MAX_NUM(size) (((size) >> DEVMM_HUGE_PAGE_SHIFT) + DEVMM_BLKNUM_ADD_NUM)
#define DEVMM_BLKNUM_TO_DMANODE_MAX_NUM(blk_num) ((blk_num) * 2 + DEVMM_BLKNUM_ADD_NUM)
#define DEVMM_VA_SIZE_TO_PAGE_NUM(va, sz, pgsz) ((ka_base_round_up((va) + (sz), pgsz) - ka_base_round_down(va, pgsz)) / (pgsz))
#ifdef CFG_SOC_PLATFORM_ESL_FPGA
#define DEVMM_PAGE_NUM_PER_MSG 32ULL /* for fpga scene; normal page 512K per msg, huge page 256M per msg */
#else
#define DEVMM_PAGE_NUM_PER_MSG 3072ULL /* the size must be align to 64; normal page 12M per msg, huge page 6G per msg */
#endif
#define DEVMM_MEMSET_SIZE_PER_MSG (1ULL << 24)    // 16M
#ifdef CFG_SOC_PLATFORM_ESL_FPGA
#define DEVMM_MEMSET8D_SIZE_PER_MSG (1ULL << 24)  // 16M
#else
#define DEVMM_MEMSET8D_SIZE_PER_MSG (1ULL << 29)  // 512M
#endif

#ifdef CFG_SOC_PLATFORM_ESL_FPGA
#define DEVMM_FREE_SECTION_NUM 128
#else
#define DEVMM_FREE_SECTION_NUM 8192
#endif
#define DEVMM_PA_VALID 0x1ul
#define DEVMM_PA_FIRST 0x2ul
/* emu st malloc just last 4bit is 0 */
#ifndef EMU_ST
#define DEVMM_PA_MASK 0xfful
#else
#define DEVMM_PA_MASK 0xful
#endif
#define DEVMM_ADDR_TYPE_PHY 0
#define DEVMM_ADDR_TYPE_DMA 1

/*
 * one msg has a byte to mantain msg status
 *    bit4~bit31: reserve
 *    bit3: msg write lock
 *    bit2: first data is va, like struct devmm_chan_addr_head
 *    bit1: svm_process exit return ok
 *    bit0: don't need svm_process, msg proc fun pass null
 */
#define DEVMM_MSG_NOT_NEED_SVM_PROC_BIT 0
#define DEVMM_MSG_RETURN_OK_BIT 1
#define DEVMM_MSG_GET_HEAP_BIT 2
#define DEVMM_MSG_WRITE_LOCK_BIT 3
#define DEVMM_MSG_ADD_CGROUP_BIT 4

/* MEROS FOR MSG_FLAG* */
#define DEVMM_MSG_NOT_NEED_PROC_MASK (1UL << DEVMM_MSG_NOT_NEED_SVM_PROC_BIT)
#define DEVMM_MSG_RETURN_OK_MASK (1UL << DEVMM_MSG_RETURN_OK_BIT)
#define DEVMM_MSG_GET_HEAP_MASK (1UL << DEVMM_MSG_GET_HEAP_BIT)
#define DEVMM_MSG_WRITE_LOCK_MASK (1UL << DEVMM_MSG_WRITE_LOCK_BIT)
#define DEVMM_MSG_ADD_CGROUP_MASK (1UL << DEVMM_MSG_ADD_CGROUP_BIT)

struct devmm_chan_page_query {
    struct devmm_chan_msg_head head;
    u64 va;
    u64 size;
    u32 bitmap;
    u32 shr_page_num;
};

struct devmm_chan_query_phy_blk {
    u64 dma_addr; /* dma addr */
    u64 phy_addr; /* phy addr */
};

struct devmm_chan_page_query_ack {
    struct devmm_chan_msg_head head;
    u64 va;
    u64 size;
    u32 addr_type;
    u32 bitmap;
    u32 num;
    u32 page_size;
    u32 is_giant_page;

    u32 p2p_owner_sdid;
    u64 p2p_owner_va;
    u64 mem_map_route;
    struct devmm_svm_process_id p2p_owner_process_id;
    struct devmm_chan_query_phy_blk blks[];
};

struct devmm_node_info {
    u64 total_normal_size;
    u64 free_normal_size;
    u64 total_huge_size;
    u64 free_huge_size;
};

struct devmm_chan_query_mem_dfx {
    struct devmm_chan_msg_head head;
    struct devmm_node_info node_info[DEVMM_MAX_NUMA_NUM_OF_PER_DEV];
    u32 node_index[DEVMM_MAX_NUMA_NUM_OF_PER_DEV];
    u32 node_num;
    u32 mem_type;
    u64 used_page_cnt;
    u64 used_hpage_cnt;
};

struct devmm_chan_advise_cache_persist {
    struct devmm_chan_msg_head head;
    u64 va;
    u64 count;
};

struct devmm_chan_target_blk {
    u64 target_addr;
};

struct devmm_chan_target_query_ack {
    struct devmm_chan_msg_head head;
    u64 va;
    u64 size;
    u32 num;
    int addr_type;
    struct devmm_chan_target_blk blks[];
};

struct devmm_target_blk {
    u64 target_addr;
    struct devmm_dma_blk dma_blk;
};

/* small than (DEVMM_IPC_POD_MSG_DATA_LEN(512-16) -
   struct devmm_chan_target_blk_query_msg) / sizeof(struct devmm_target_blk) */
#define SVM_CS_HOST_BLK_MAX_NUM 16

struct devmm_chan_target_blk_query_msg {
    int share_sdid;
    int share_devid;
    int share_id;
    int addr_type;
    u32 dma_saved;
    u32 num;
    u32 offset;
    struct devmm_target_blk blk[];
};

struct devmm_chan_target_blk_query {
    struct devmm_chan_msg_head head;
    struct devmm_chan_target_blk_query_msg msg;
};

struct devmm_chan_free_pages {
    struct devmm_chan_msg_head head;
    u64 va;
    u64 real_size;  /* page aligned */
};

struct devmm_chan_memset {
    struct devmm_chan_msg_head head;
    u64 dst;
    u64 value;
    u32 count;
};

#define DEVMM_CHAN_MAX_HEAP_INFO_NUM 128
struct devmm_chan_heap_info {
    u32 heap_idx;
    u32 heap_type;
    u32 heap_sub_type;
    u64 heap_size;
};

#define DEVMM_POLLING_CMD_CREATE 0xEFEF0001
#define DEVMM_POLLING_CMD_UPDATE_HEAP 0xEFEF0004

struct devmm_chan_setup_device {
    struct devmm_chan_msg_head head;
    u32 cmd;
    ka_pid_t devpid;         /* agent return */
    int ssid;           /* agent return */
    u32 logic_devid;
    u32 heap_cnt;
    struct devmm_chan_heap_info heap_info[];
};

struct devmm_chan_device_meminfo {
    struct devmm_chan_msg_head head;
    u32 mem_type;
    u64 normal_free_size;
    u64 normal_total_size;
    u64 huge_free_size;
    u64 huge_total_size;
    u64 giant_free_size;
    u64 giant_total_size;
};

struct devmm_chan_close_device {
    struct devmm_chan_msg_head head;
    u32 cmd;
    ka_pid_t devpid;
};

struct devmm_chan_update_heap {
    struct devmm_chan_msg_head head;
    struct devmm_update_heap_para cmd;
};

struct devmm_chan_check_va {
    struct devmm_chan_msg_head head;
    u64 check_va;
    u64 pre_start_va; /* alloced memory before check_va */
    u64 pre_end_va;
    u64 post_start_va; /* alloced memory after check_va */
    u64 post_end_va;
    u64 bitmap;
};

struct devmm_chan_proc_abort {
    struct devmm_chan_msg_head head;
    u32 status;
    u32 dma_status;
};

struct devmm_chan_memcpy_d2d {
    struct devmm_chan_msg_head head;
    u64 dst;
    u64 src;
    u64 size;
};

struct devmm_chan_remote_map {
    struct devmm_chan_msg_head head;
    u64 src_va;
    u32 access;
    u64 size;
    u64 dst_va;
    u32 page_size;
    u32 map_type;
    u32 proc_type;
    u64 src_pa[];
};

struct devmm_chan_remote_unmap {
    struct devmm_chan_msg_head head;
    u64 src_va;
    u64 dst_va;
    u64 size;
    u32 map_type;
    u32 proc_type;
};

struct devmm_chan_shm_getput_pages_d2h {
    struct devmm_chan_msg_head head;
    u64 dev_va;
    u64 size;
};

struct devmm_chan_agent_proc_exiting_d2h {
    struct devmm_chan_msg_head head;
};

struct devmm_chan_map_dev_reserve {
    struct devmm_chan_msg_head head;
    u32 addr_type;  /* l2buff or c2c_ctrl */
    u64 va;
    u64 len;
};

struct devmm_chan_process_status {
    struct devmm_chan_msg_head head;
    processStatus_t pid_status;
    bool status_occur;
    u32 pid;
    u32 tid;
};

struct devmm_chan_mem_create {
    struct devmm_chan_msg_head head;
    u32 is_create_to_new_blk;

    int id;
    u32 module_id;

    u32 pg_type;
    u32 mem_type;

    u32 total_pg_num;
    u32 to_create_pg_num;
};

struct devmm_chan_mem_release {
    struct devmm_chan_msg_head head;

    int id;
    u32 free_type;

    u64 to_free_pg_num;
};

struct devmm_chan_mem_map {
    struct devmm_chan_msg_head head;
    u64 va;
    u64 size;

    int phy_addr_blk_id;
    u32 pg_type;
    u64 offset_pg_num;

    u64 dma_blk_id;             /* Agent will return, to get dma addr quickly */
    u64 dma_blk_pg_id;          /* Agent will return, to get dma addr quickly */
    u32 get_next_dma_blk_pg_id;

    u32 module_id;              /* No actual use, just for handle verify */
    u64 phy_addr_blk_pg_num;    /* No actual use, just for handle verify */
    struct devmm_chan_query_phy_blk blks[];
};

struct devmm_chan_mem_unmap {
    struct devmm_chan_msg_head head;
    u64 va;
};

struct devmm_chan_mem_export {
    struct devmm_chan_msg_head head;
    int id;

    int share_id;
    u64 pg_num;
    u32 module_id;
    u32 side;
    u32 pg_type;
    u32 mem_type;
};

struct devmm_chan_mem_import {
    struct devmm_chan_msg_head head;
    int side;
    int share_id;
    u32 share_sdid;
    u32 host_did;

    u32 module_id;
    u32 pg_type;
    u32 mem_type;
    u32 total_pg_num;
    u32 to_create_pg_num;
    u32 is_create_to_new_blk;

    int id;
};

struct devmm_chan_share_mem_memset {
    struct devmm_chan_msg_head head;
    u64 va_offset;
    u64 value;
    u32 count;
    int share_id;
};

struct devmm_chan_mem_repair {
    struct devmm_chan_msg_head head;
    u64 addr;
    u64 len;
    u8 need_cache_update; /* dev -> host */
    u8 is_giant_page; /* dev -> host, update 512 cache phy blk */
    u32 bitmap;
    struct devmm_chan_query_phy_blk blk;
};

#define DEVMM_P2P_PAGE_MAX_NUM_QUERY_MSG 32

int devmm_chan_msg_dispatch(void *msg, u32 in_data_len, u32 out_data_len, u32 *ack_len,
    const struct devmm_chan_handlers_st *msg_process);

int devmm_host_dev_init(u32 dev_id, u32 vfid);
void devmm_host_dev_uninit(u32 dev_id);
int devmm_notify_device_close_process(struct devmm_svm_process *svm_pro,
    u32 logical_devid, u32 phy_devid, u32 vfid);
int devmm_chan_send_msg_free_pages(struct devmm_chan_free_pages *free_info, struct devmm_svm_heap *heap,
                                   struct devmm_svm_process *svm_proc, int shared_flag, u32 free_self);
void devmm_chan_set_host_device_page_size(void);
void devmm_merg_pa_by_num(u64 *pas, u32 num, u32 pgsz, u32 *merg_szlist, u32 *merg_num);
void devmm_svm_free_share_page_msg(struct devmm_svm_process *svm_process, struct devmm_svm_heap *heap,
                                   unsigned long start, u64 real_size, u32 *page_bitmap);
void devmm_merg_blk(struct devmm_dma_block *blks, u32 idx, u32 *merg_idx);
void devmm_merg_phy_blk(struct devmm_chan_phy_block *blks, u32 blks_idx, u32 *merg_idx);
int devmm_chan_page_fault_d2h_process_dma_copy(struct devmm_chan_page_fault *fault_msg, u64 *pas,
    u32 *szs, u32 num);
int devmm_init_convert_addr_mng(u32 dev_id, struct devmm_chan_exchange_pginfo *info);
void devmm_uninit_convert_addr_mng(u32 dev_id);
int devmm_dev_page_fault_get_vaflgs(struct devmm_svm_process *svm_process, struct devmm_svm_heap *heap,
    struct devmm_chan_page_query *flg_msg);
int devmm_chan_update_msg_logic_id(struct devmm_svm_process *svm_proc, struct devmm_chan_msg_head *msg_head);

typedef int (*svm_host_agent_msg_send_handle)(int agent_id, void *msg, unsigned int len, unsigned int out_len);
void devmm_register_host_agent_msg_send_handle(svm_host_agent_msg_send_handle func);
int devmm_host_chan_msg_recv(void *msg, unsigned int len, unsigned int out_len);

/* host s2s msg */
struct devmm_ipc_pod_msg_head {
    u32 devid;
    u32 cmdtype;
    u16 valid;  /* validity judgement, 0x5A5A is valide */
    s16 result; /* process result from rp, zero for succ, non zero for fail */
    u32 rsv;
};

/* will define stack variable, witch must be small than 1024 configed in makefile */
#define DEVMM_IPC_POD_MSG_TOTAL_LEN       512 /* cannot modify the len, may cause compatibility issue */
#define DEVMM_IPC_POD_MSG_DATA_LEN        (DEVMM_IPC_POD_MSG_TOTAL_LEN - sizeof(struct devmm_ipc_pod_msg_head))

#define DEVMM_IPC_POD_MSG_SEND_MAGIC      0x5A5A
#define DEVMM_IPC_POD_MSG_RCV_MAGIC       0xA5A5

struct devmm_ipc_pod_msg_data {
    struct devmm_ipc_pod_msg_head header;
    char payload[DEVMM_IPC_POD_MSG_DATA_LEN];
};

enum devmm_ipc_pod_msg_cmd {
    DEVMM_IPC_POD_MSG_SET_PID,
    DEVMM_IPC_POD_MSG_DESTROY,
    DEVMM_IPC_POD_MSG_MEM_REPAIR,
    DEVMM_GET_SHARE_INFO,
    DEVMM_PUT_SHARE_INFO,
    DEVMM_GET_BLK_INFO,
    DEVMM_IPC_POD_MSG_MAX
};

#endif /* __DEVMM_CHANNEL_H__ */
