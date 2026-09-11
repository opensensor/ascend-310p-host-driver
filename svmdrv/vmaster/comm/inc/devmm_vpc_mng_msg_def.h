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

#ifndef DEVMM_VPC_MNG_MSG_DEF_H
#define DEVMM_VPC_MNG_MSG_DEF_H

#include "devmm_proc_info.h"
#include "devmm_channel.h"

#define DEVMM_VPC_VM_TO_PM 1UL
#define DEVMM_VPC_PM_TO_VM 0UL

enum {
    DEVMM_VPC_SYNC = 0,
    DEVMM_VPC_INIT_PROC,
    DEVMM_VPC_UNINIT_PROC,
    DEVMM_VPC_MNG_CREATE_DEV_PAGE,
    DEVMM_VPC_MNG_DELETE_DEV_PAGE_CACHE,
    DEVMM_VPC_MNG_COPY_MEM,
    DEVMM_VPC_MNG_CONVERT_ADDR,
    DEVMM_VPC_MNG_CONVERT_DMA_ADDR,
    DEVMM_VPC_MNG_DESTROY_ADDR,
    DEVMM_VPC_MNG_TRANSLATE_ADDR,
    DEVMM_VPC_MNG_CLEAR_TRANSLATE_ADDR,
    DEVMM_VPC_MNG_HOST_PAGE_FAULT,
    DEVMM_VPC_MNG_DEVICE_PAGE_FAULT,
    DEVMM_VPC_MNG_AGENT_MEM_MAP,
    DEVMM_VPC_SYNC_VERSION = 99,
    DEVMM_VPC_MNG_MSG_MAX_ID
};

struct devmm_vpc_mng_msg_head {
    struct devmm_svm_process_id process_id;
    u32 msg_id;
    u32 dev_id;
    u32 logical_devid;
    u16 extend_num;
};

struct devmm_vpc_mng_msg_sync_version {
    struct devmm_vpc_mng_msg_head head;
    u32 vm_version;
    u32 pm_version;
};

struct devmm_vpc_mng_msg_sync {
    struct devmm_vpc_mng_msg_head head;
    u32 pm_dev_id;
    u32 pm_fid;
    u32 device_page_shift;
    u32 device_hpage_shift;
    u64 dev_mem[DEVMM_EXCHANGE_MAX_MEM_TYPE];
    u64 dev_mem_p2p[DEVMM_EXCHANGE_MAX_MEM_TYPE];
    u64 dvpp_memsize;
    u8 feature_phycial_address;
    u8 resv1[7];    /* 7:reserved num */
    u64 resv[2];    /* 2:reserved num */

    u64 double_pgtable_offset;
};

struct devmm_vpc_mng_msg_init_process {
    struct devmm_vpc_mng_msg_head head;
    u64 resv[4];    /* 4:reserved num */
};

struct devmm_vpc_mng_msg_create_dev_page {
    struct devmm_vpc_mng_msg_head head;
    u64 va;
    u64 size;
    u64 offset;
    u32 page_size;
    u32 bitmap;
    u64 page_cnt;
    u64 resv[4];    /* 4:reserved num */
};

#define DEL_PAGE_CACHE_TYPE_VA 0
#define DEL_PAGE_CACHE_TYPE_DEV 1
struct devmm_vpc_mng_msg_del_dev_page_cache {
    struct devmm_vpc_mng_msg_head head;
    u32 type;
    u32 rsv;
    u64 va;
    u32 page_num;
    u32 page_size;
    u32 reuse;
    u32 resv[9];    /* 9:reserved num */
};

struct devmm_vpc_mem_copy_convrt_para {
    u64 dst;
    u64 src;
    u64 count;
    u32 direction;
    u32 blk_size;
    u32 create_msg;
    u32 resv[9]; /* 9:reserved num */
};

struct devmm_vpc_memory_attributes {
    u8 is_local_host;
    u8 is_host_pin;
    u8 is_svm;
    u8 is_svm_host;
    u8 is_svm_device;
    u8 is_svm_non_page;
    u8 is_ipc_open;
    u8 copy_use_va;
    u32 bitmap;
    u32 devid;
    u32 vfid;
    u32 page_size;
    u64 va;
    u32 logical_devid;
    u8 is_svm_huge;
    u8 resv1[3];  /* 3:reserved num */
    u32 host_page_size;
    u32 resv2;
    u64 resv3[2]; /* 2:reserved num */
};

struct devmm_vpc_mng_msg_mem_copy {
    struct devmm_vpc_mng_msg_head head;
    struct devmm_vpc_mem_copy_convrt_para para;
    struct devmm_vpc_memory_attributes src_attr;
    struct devmm_vpc_memory_attributes dst_attr;
    struct devmm_pa_list pa_list;
};

struct devmm_vpc_mem_convrt_addr_para {
    u64 pSrc;
    u64 pDst;
    u64 spitch;
    u64 dpitch;
    u64 len;
    u64 height;
    u64 fixed_size;
    struct DMA_ADDR dmaAddr;
    u32 direction;
    u32 dma_node_num;
    u32 virt_id;           /* store logic id to destroy addr */
    u32 destroy_flag;      /* used in virt machine, to destroy the res */
    u32 convert_id;        /* for vm limit convert times */
    u32 resv[9];           /* 9:reserved num */
};

struct devmm_vpc_mng_msg_convert_addr {
    struct devmm_vpc_mng_msg_head head;
    struct devmm_vpc_mem_convrt_addr_para convrt_para;
    struct devmm_vpc_memory_attributes src_attr;
    struct devmm_vpc_memory_attributes dst_attr;
    u32 page_size;
    /* pa_list must be the last one, cann't be changed */
    struct devmm_pa_list pa_list;
};

struct devmm_vpc_mng_msg_convert_dma_addr {
    struct devmm_vpc_mng_msg_head head;
    struct devmm_vpc_mem_convrt_addr_para convrt_para;
};

struct devmm_vpc_mem_desty_addr_para {
    u64 pSrc;  /* pSrc pDst just use in virt machine */
    u64 pDst;
    u64 spitch;
    u64 dpitch;
    u64 len;
    u64 height;
    u64 fixed_size;
    struct DMA_ADDR dmaAddr; /* user to kernel */
    u64 resv[4];             /* 4:reserved num */
};

struct devmm_vpc_mng_msg_destroy_addr {
    struct devmm_vpc_mng_msg_head head;
    struct devmm_vpc_mem_desty_addr_para desty_para;
};

struct devmm_vpc_translate_info {
    u64 va;
    u32 page_size;
    u32 resv[9];    /* 9:reserved num */
};

struct devmm_vpc_mng_msg_translate_addr {
    struct devmm_vpc_mng_msg_head head;
    struct devmm_vpc_translate_info trans;
    u64 pa_offset;
};

struct devmm_vpc_mng_msg_clear_translate_addr {
    struct devmm_vpc_mng_msg_head head;
    u64 va;
    u64 len;
    u64 resv[4];    /* 4:reserved num */
};

#define PAGE_FAULT_STAGE_QUERY 0
#define PAGE_FAULT_STAGE_FREE 1
struct devmm_vpc_mng_msg_page_fault {
    struct devmm_vpc_mng_msg_head head;
    unsigned long va;
    u32 page_size;
    u32 stage; /* use in device page fault, stage 0: query vm pa list, stage 1: free vm page */
    struct devmm_pa_list pa_list;
    u64 resv[4];    /* 4:reserved num */
};

struct devmm_vpc_mng_msg_agent_mem_map {
    struct devmm_vpc_mng_msg_head head;
    u64 va;
    u64 size;
    u32 pg_type;

    u32 module_id;
    int phy_addr_blk_id;
    u32 offset_pg_num;
    u64 phy_addr_blk_pg_num;
    u32 result;
    u32 resv1;
    u64 resv2[3];    /* 3:reserved num */
};


#endif
